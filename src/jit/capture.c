// jit/capture.c - record and replay a sequence of kernel dispatches.
//
// Once a step's graph is stable (same shapes, new input bytes), rerunning the
// scheduler costs more than the kernels; replay skips it.  jit_capture_begin
// claims a slot, kernel_fire_by_id records each dispatch while
// jit_is_capturing(), jit_capture_end* finalizes, and jit_replay re-dispatches
// every op against the buffer ids it had at capture.
//
// Replay checks no shapes: the WL wrapper (Jit.wl) decides when a capture is
// stale.  The ids stay valid while the inputs are pre-allocated tensors that
// TSet rewrites in place between replays.

#define JIT_CAPTURE_OP_CAP    65536  // per-capture op cap
#define JIT_CAPTURE_OP_INIT   256    // initial ops array size, then doubled
// One slot per live TJit closure: 64 fits per-block captures of 28 diffusion
// transformer blocks for both guidance branches, plus the sampler's closures.
#define JIT_CAPTURE_NSLOTS    64
#define JIT_CAPTURE_RETAIN_CAP 131072

// JIT_OP_ASSIGN replays a TAssign copy (interact_assign_with), as optimizer
// steps update weights that way.  JIT_OP_GATHER replays a host strided gather
// (materialize_root_alias) whose source changes under replay (a re-fired
// kernel output, an appended KV cache), since the eager gather baked the
// capture step's bytes.  It copies the view: the alias tensor is rewound.
typedef enum {
  JIT_OP_DISPATCH = 0,
  JIT_OP_ASSIGN   = 1,
  JIT_OP_GATHER   = 2
} JitOpKind;

// Fused kernels can have dozens of inputs; more go to heap_in_buf_ids.
#define JIT_OP_INLINE_INPUTS 64

typedef struct {
  JitOpKind kind;
  u8   replay_skip;
  u8   replay_packed;
  // The output adopted a capture-recycled buffer, so replay zeroes it first
  // as the pop did.  Unlike replay_packed, finalize keeps it.
  u8   replay_zero;
  // DISPATCH
  u32  kid;
  u32  out_buf_id;
  u32  n_inputs;
  u32  in_buf_ids[JIT_OP_INLINE_INPUTS];
  u32 *heap_in_buf_ids;
  // ASSIGN
  u32  assign_dst_tid;
  u32  assign_src_tid;
  // GATHER: source read through the copied view (and chain) into the
  // contiguous destination.
  void    *gather_backend;
  u32      gather_src_buf;
  u32      gather_dst_buf;
  u32      gather_dtype;
  View     gather_view;
  u8       gather_nviews;
  View    *gather_prior;       // [gather_nviews]; NULL when nviews == 0
} JitCaptureOp;

typedef struct {
  Backend *backend;
  u32      buf_id;
} JitCaptureBufRef;

// A captured (op, position) that reads a declared JIT input; each replay
// writes the call's fresh buffer id there (tinygrad input_replace,
// engine/jit.py:124-126).  The sentinel positions mark a dispatch output and
// a gather's source or destination.
#define JIT_INPUT_OUT_POS  0xFFFFFFFFu
#define JIT_GATHER_SRC_POS 0xFFFFFFFEu
#define JIT_GATHER_DST_POS 0xFFFFFFFDu
typedef struct {
  u32 op_index;
  u32 pos;          // input position within the op, or a *_POS sentinel
  u32 slot_index;   // index into JitCapture.input_buf_ids
} JitInputSite;

typedef struct {
  u32               in_use;        // 0 = free slot
  u32               n_ops;
  u32               ops_cap;
  JitCaptureOp     *ops;
  u32               n_retained;
  JitCaptureBufRef *retained;      // buffers owned by this replay slot
  // The buffer ids the declared inputs had at capture, and every site that
  // reads one, built on the first jit_replay_with_inputs.
  u32               n_inputs_decl;
  u32              *input_buf_ids;  // [n_inputs_decl]
  u32               sites_built;
  u32               n_sites;
  JitInputSite     *sites;          // [n_sites]
  // A live non-tile dispatch reads a strided view: per-op dispatch gathers it
  // into a contiguous temp first and the ICB would not, so the ICB declines.
  u32               metal_graph_unsafe;
  // Two live dispatches write one output buffer (pool or packer reuse).  The
  // ICB declines it unless THVM_JIT_GRAPH_FORCE_ICB=1, as on M3 its
  // setBarrier does not reliably order that write-after-write.
  u32               metal_graph_recycle;
  // The cached CUDA graph bakes input device addresses and its hit skips the
  // per-op rebind, so jit_replay_with_inputs re-captures it when an address
  // moves.  Addresses, not ids: a freed id can come back at a new address.
  u32              *graph_input_ids;    // [graph_input_n]; for the diag trace
  u64              *graph_input_addrs;  // [graph_input_n]; as baked
  u32               graph_input_n;
  // THVM_JIT_TRACE_CAPTURE, latched at begin: record without the eager Metal
  // submit, and one replay after finalize fills the results.
  u32               trace_capture_pending;
  // Some dispatch did skip its eager submit.  The end replay needs this, not
  // just pending: the skip is Metal-only, and replaying a CPU or CUDA capture
  // would run every kernel twice (wrong for an in-place custom kernel).
  u32               trace_capture_skipped;
} JitCapture;

static JitCapture JIT_CAPTURES[JIT_CAPTURE_NSLOTS];

static u32 JIT_ACTIVE_SLOT = 0;     // 0 = idle, else the slot being filled
static u32 JIT_PAUSE_DEPTH = 0;     // nested pauses for autotune bench fires

static void jit_capture_finalize(u32 slot, const Term *roots, u32 n_roots);
fn u32 jit_replay(u32 slot);
fn int jit_trace_capture_enabled(void);
static int jit_bufref_contains(JitCaptureBufRef const *refs, u32 n,
                               Backend *b, u32 buf_id);
static Backend *jit_dispatch_output_backend(JitCaptureOp const *op);
static Backend *jit_dispatch_input_backend(JitCaptureOp const *op, u32 i);
static int jit_dispatch_op_custom(JitCaptureOp const *op);

static void jit_capture_mark_buf(Backend *b, u32 buf_id) {
  if (b == NULL || buf_id == 0) {
    return;
  }
  switch (b->id) {
    case 1:
      cpu_buf_mark_preserved(buf_id);
      break;
    case 2:
      thvm_metal_buf_mark_preserved(buf_id);
      break;
#ifdef THVM_HAS_CUDA
    case 3:
      cuda_buf_mark_preserved(buf_id);
      break;
#endif
    default:
      break;
  }
}

static void jit_capture_release_retained(JitCapture *c) {
  if (c == NULL || c->retained == NULL) {
    return;
  }
  for (u32 i = 0; i < c->n_retained; i++) {
    Backend *b = c->retained[i].backend;
    u32 buf_id = c->retained[i].buf_id;
    if (b == NULL || buf_id == 0) continue;
    if (b->buf_jit_unpin != NULL) b->buf_jit_unpin(buf_id);
    if (b->buf_decref != NULL)    b->buf_decref(buf_id);
  }
  c->n_retained = 0;
}

// Release every retained buffer not in `keep`, leaving the kept ones' ref and
// pin untouched.  Releasing all and re-retaining would drop a buffer held only
// by the capture to refcount 0 and hard-free it, and since jit_pinned is a
// flag, not a count, a re-pin then unpin would clear the new pin.
static void jit_capture_release_retained_except(JitCapture *c,
                                                JitCaptureBufRef const *keep,
                                                u32 n_keep) {
  if (c == NULL || c->retained == NULL) {
    return;
  }
  u32 kept = 0;
  for (u32 i = 0; i < c->n_retained; i++) {
    Backend *b = c->retained[i].backend;
    u32 buf_id = c->retained[i].buf_id;
    if (b == NULL || buf_id == 0) continue;
    if (jit_bufref_contains(keep, n_keep, b, buf_id)) {
      c->retained[kept].backend = b;
      c->retained[kept].buf_id  = buf_id;
      kept++;
      continue;
    }
    if (b->buf_jit_unpin != NULL) b->buf_jit_unpin(buf_id);
    if (b->buf_decref != NULL)    b->buf_decref(buf_id);
  }
  c->n_retained = kept;
}

static void jit_capture_retain_buf(JitCapture *c, Backend *b, u32 buf_id) {
  if (c == NULL || b == NULL || buf_id == 0) {
    return;
  }
  for (u32 i = 0; i < c->n_retained; i++) {
    if (c->retained[i].backend == b && c->retained[i].buf_id == buf_id) {
      jit_capture_mark_buf(b, buf_id);
      return;
    }
  }
  if (c->retained == NULL || c->n_retained >= JIT_CAPTURE_RETAIN_CAP) {
    return;
  }
  if (b->buf_incref != NULL) {
    b->buf_incref(buf_id);
  }
  // `preserved` is cleared at the end of each realize, after which the next
  // realize's rollback would freelist the buffer; jit_pin outlives that.
  if (b->buf_jit_pin != NULL && getenv("THVM_JIT_NO_PIN") == NULL) {
    b->buf_jit_pin(buf_id);
  }
  // A buffer parked on the freelist (refcount 0) would be handed to a later
  // alloc; take it back.
  if (b->buf_freelist_remove != NULL) {
    b->buf_freelist_remove(buf_id);
  }
  c->retained[c->n_retained].backend = b;
  c->retained[c->n_retained].buf_id  = buf_id;
  c->n_retained++;
  jit_capture_mark_buf(b, buf_id);
}

static void jit_capture_retain_dispatch_bufs(JitCapture *c,
                                             JitCaptureOp const *op) {
  if (c == NULL || op == NULL || op->kind != JIT_OP_DISPATCH) {
    return;
  }
  if (op->kid == 0 || op->kid >= KERNELS_NEXT) {
    return;
  }
  KernelEntry *ke = &KERNELS[op->kid];
  Backend *out_backend = NULL;
  if (ke->output_tid != 0 && ke->output_tid < TENS_NEXT) {
    out_backend = TENS[ke->output_tid].backend;
  }
  jit_capture_retain_buf(c, out_backend, op->out_buf_id);

  u32 const *ids = op->heap_in_buf_ids != NULL
                 ? op->heap_in_buf_ids
                 : op->in_buf_ids;
  for (u32 i = 0; i < op->n_inputs; i++) {
    Backend *in_backend = out_backend;
    if (ke->input_tids != NULL && i < ke->n_inputs) {
      u32 tid = ke->input_tids[i];
      if (tid != 0 && tid < TENS_NEXT && TENS[tid].backend != NULL) {
        in_backend = TENS[tid].backend;
      }
    }
    jit_capture_retain_buf(c, in_backend, ids[i]);
  }
}

static void jit_capture_retain_tensor_buf(JitCapture *c, u32 tid) {
  if (c == NULL || tid == 0 || tid >= TENS_NEXT) {
    return;
  }
  TenDesc *td = &TENS[tid];
  jit_capture_retain_buf(c, td->backend, td->buf_id);
}

// Capture-time recycle pool, the capture half of tinygrad's JIT memory planner
// (engine/jit.py:72 memory_plan_rewrite, schedule/memory.py:24-53).  Recording
// pins every dispatched buffer and the Metal freelist refuses pinned ones, so
// a cold capture would keep every intermediate.  mem_plan_push_dead parks a
// dead pinned intermediate of the active capture here, and the next same-size
// captured output alloc adopts it (tensor_alloc_adopt); replay repeats the
// reuse in record order, with replay_zero for the zero fill.  A parked buffer
// stays dead for the whole capture: push_dead takes only single-consumer
// non-sink boundaries past their last consumer, and on Metal no later realize
// re-reads an internal boundary.  Drained at each pass end, reset at capture
// begin, end and drop.
#define JIT_CAPRECYCLE_CAP 8192
typedef struct {
  Backend *backend;
  u32      buf_id;
  u64      nbytes;
} JitCapRecycleEntry;
static JitCapRecycleEntry JIT_CAPRECYCLE[JIT_CAPRECYCLE_CAP];
static u32 JIT_CAPRECYCLE_LEN = 0;
// Pool-served ids whose next recorded producer gets replay_zero.  Sized to
// the pool: a pass pops before any of its records fire, so marks pile up one
// per adoption, and a dropped mark would skip a needed zero fill.
#define JIT_CAPRECYCLE_PENDING_CAP JIT_CAPRECYCLE_CAP
static u32 JIT_CAPRECYCLE_PENDING[JIT_CAPRECYCLE_PENDING_CAP];
static u32 JIT_CAPRECYCLE_PENDING_LEN = 0;
// Release pool, the cross-realize half (tinygrad engine/jit.py:298-300
// held_bufs): an intermediate that roots its own small realize dies only once
// later realizes consume it, which the per-pass pool cannot see.
// jit_caprecycle_release_hook parks it here.  Survives pass boundaries.
static JitCapRecycleEntry JIT_CAPRELEASE[JIT_CAPRECYCLE_CAP];
static u32 JIT_CAPRELEASE_LEN = 0;
// THVM_JIT_CAPRECYCLE_DEBUG counters: adoptions, bytes kept unpinned, and
// which release-hook guard rejected a candidate.
static u64 JIT_CAPRECYCLE_DBG_POPS  = 0;
static u64 JIT_CAPRECYCLE_DBG_BYTES = 0;
static u64 JIT_CAPRELEASE_DBG_PARKS = 0;
static u64 JIT_CAPRELEASE_DBG_POPS  = 0;
static u64 JIT_CAPRELEASE_DBG_BYTES = 0;
static u64 JIT_CAPRELEASE_DBG_REJ_INACTIVE    = 0;
static u64 JIT_CAPRELEASE_DBG_REJ_UNPINNED    = 0;
static u64 JIT_CAPRELEASE_DBG_REJ_REFS        = 0;
static u64 JIT_CAPRELEASE_DBG_REJ_NOTRETAINED = 0;
static u64 JIT_CAPRELEASE_DBG_REJ_NOTWRITTEN  = 0;

static int jit_caprecycle_enabled(void) {
  static int known = 0, on = 1;
  if (!known) {
    char const *e = getenv("THVM_JIT_CAPTURE_RECYCLE");
    on = (e == NULL || e[0] != '0');   // default on
    known = 1;
  }
  return on;
}

// Forget the pass pool's entries; the buffers stay pinned for replay.
fn void jit_caprecycle_drain(void) {
  JIT_CAPRECYCLE_LEN = 0;
}

// Pool stats for the memory-ceiling abort path, which never reaches the
// end-of-capture summary.
fn void jit_caprecycle_stats_dump(void) {
  if (getenv("THVM_JIT_CAPRECYCLE_DEBUG") == NULL) return;
  fprintf(stderr,
          "[jit_caprecycle mid] adoptions=%llu bytes_unpinned=%.1fMB "
          "parked_now=%u | release parks=%llu adoptions=%llu "
          "bytes_unpinned=%.1fMB parked_now=%u | pending=%u "
          "active_slot=%u pause=%u\n",
          (unsigned long long)JIT_CAPRECYCLE_DBG_POPS,
          JIT_CAPRECYCLE_DBG_BYTES / 1.0e6, JIT_CAPRECYCLE_LEN,
          (unsigned long long)JIT_CAPRELEASE_DBG_PARKS,
          (unsigned long long)JIT_CAPRELEASE_DBG_POPS,
          JIT_CAPRELEASE_DBG_BYTES / 1.0e6, JIT_CAPRELEASE_LEN,
          JIT_CAPRECYCLE_PENDING_LEN, JIT_ACTIVE_SLOT, JIT_PAUSE_DEPTH);
  fprintf(stderr,
          "[jit_caprelease rej] inactive=%llu unpinned=%llu refs=%llu "
          "notretained=%llu notwritten=%llu\n",
          (unsigned long long)JIT_CAPRELEASE_DBG_REJ_INACTIVE,
          (unsigned long long)JIT_CAPRELEASE_DBG_REJ_UNPINNED,
          (unsigned long long)JIT_CAPRELEASE_DBG_REJ_REFS,
          (unsigned long long)JIT_CAPRELEASE_DBG_REJ_NOTRETAINED,
          (unsigned long long)JIT_CAPRELEASE_DBG_REJ_NOTWRITTEN);
}

// Reset at capture begin, end and drop: no later alloc may adopt a captured
// buffer, and no pending mark may leak into the next capture.
static void jit_caprecycle_reset(void) {
  if (getenv("THVM_JIT_CAPRECYCLE_DEBUG") != NULL
      && (JIT_CAPRECYCLE_DBG_POPS > 0 || JIT_CAPRECYCLE_LEN > 0
          || JIT_CAPRELEASE_DBG_PARKS > 0
          || JIT_CAPRELEASE_DBG_REJ_INACTIVE + JIT_CAPRELEASE_DBG_REJ_UNPINNED
             + JIT_CAPRELEASE_DBG_REJ_REFS + JIT_CAPRELEASE_DBG_REJ_NOTRETAINED
             + JIT_CAPRELEASE_DBG_REJ_NOTWRITTEN > 0)) {
    fprintf(stderr,
            "[jit_caprecycle] adoptions=%llu bytes_unpinned=%.1fMB "
            "parked_now=%u | release parks=%llu adoptions=%llu "
            "bytes_unpinned=%.1fMB parked_now=%u\n",
            (unsigned long long)JIT_CAPRECYCLE_DBG_POPS,
            JIT_CAPRECYCLE_DBG_BYTES / 1.0e6, JIT_CAPRECYCLE_LEN,
            (unsigned long long)JIT_CAPRELEASE_DBG_PARKS,
            (unsigned long long)JIT_CAPRELEASE_DBG_POPS,
            JIT_CAPRELEASE_DBG_BYTES / 1.0e6, JIT_CAPRELEASE_LEN);
    fprintf(stderr,
            "[jit_caprelease rej] inactive=%llu unpinned=%llu refs=%llu "
            "notretained=%llu notwritten=%llu\n",
            (unsigned long long)JIT_CAPRELEASE_DBG_REJ_INACTIVE,
            (unsigned long long)JIT_CAPRELEASE_DBG_REJ_UNPINNED,
            (unsigned long long)JIT_CAPRELEASE_DBG_REJ_REFS,
            (unsigned long long)JIT_CAPRELEASE_DBG_REJ_NOTRETAINED,
            (unsigned long long)JIT_CAPRELEASE_DBG_REJ_NOTWRITTEN);
  }
  JIT_CAPRECYCLE_DBG_POPS  = 0;
  JIT_CAPRECYCLE_DBG_BYTES = 0;
  JIT_CAPRECYCLE_LEN = 0;
  JIT_CAPRECYCLE_PENDING_LEN = 0;
  JIT_CAPRELEASE_DBG_PARKS = 0;
  JIT_CAPRELEASE_DBG_POPS  = 0;
  JIT_CAPRELEASE_DBG_BYTES = 0;
  JIT_CAPRELEASE_LEN = 0;
  JIT_CAPRELEASE_DBG_REJ_INACTIVE    = 0;
  JIT_CAPRELEASE_DBG_REJ_UNPINNED    = 0;
  JIT_CAPRELEASE_DBG_REJ_REFS        = 0;
  JIT_CAPRELEASE_DBG_REJ_NOTRETAINED = 0;
  JIT_CAPRELEASE_DBG_REJ_NOTWRITTEN  = 0;
}

// Park a dead captured intermediate; 1 iff parked.  Metal only (CPU and CUDA
// use their arena planner), and never a buffer another capture retains or a
// declared input of this one, whose bytes replay must keep.
fn int jit_caprecycle_push(Backend *b, u32 buf_id) {
#ifdef THVM_HAS_METAL
  if (!jit_caprecycle_enabled()) return 0;
  if (JIT_ACTIVE_SLOT == 0 || JIT_PAUSE_DEPTH > 0) return 0;
  if (b != &METAL_BACKEND || buf_id == 0) return 0;
  JitCapture *c = &JIT_CAPTURES[JIT_ACTIVE_SLOT];
  if (!c->in_use) return 0;
  for (u32 s = 1; s < JIT_CAPTURE_NSLOTS; s++) {
    if (s == JIT_ACTIVE_SLOT) continue;
    JitCapture *o = &JIT_CAPTURES[s];
    if (o->in_use && jit_bufref_contains(o->retained, o->n_retained, b, buf_id)) {
      return 0;
    }
  }
  if (c->input_buf_ids != NULL) {
    for (u32 k = 0; k < c->n_inputs_decl; k++) {
      if (c->input_buf_ids[k] == buf_id) return 0;
    }
  }
  u64 nbytes = 0;
  u32 rc = 0;
  thvm_metal_buf_get(buf_id, &nbytes, &rc);
  if (nbytes == 0) return 0;
  if (JIT_CAPRECYCLE_LEN >= JIT_CAPRECYCLE_CAP) return 0;
  JIT_CAPRECYCLE[JIT_CAPRECYCLE_LEN].backend = b;
  JIT_CAPRECYCLE[JIT_CAPRECYCLE_LEN].buf_id  = buf_id;
  JIT_CAPRECYCLE[JIT_CAPRECYCLE_LEN].nbytes  = nbytes;
  JIT_CAPRECYCLE_LEN++;
  return 1;
#else
  (void)b; (void)buf_id;
  return 0;
#endif
}

// Exact-size adoption for a captured output alloc.  Zeroes the buffer on the
// GPU, as metal_buf_freelist_try_pop does, and queues its replay_zero mark.
fn u32 jit_caprecycle_pop(Backend *b, u64 nbytes) {
#ifdef THVM_HAS_METAL
  if (JIT_ACTIVE_SLOT == 0 || JIT_PAUSE_DEPTH > 0) return 0;
  if (b != &METAL_BACKEND || nbytes == 0) return 0;
  // A full pending list would drop this adoption's zero mark: allocate fresh.
  if (JIT_CAPRECYCLE_PENDING_LEN >= JIT_CAPRECYCLE_PENDING_CAP) return 0;
  for (u32 i = 0; i < JIT_CAPRECYCLE_LEN; i++) {
    if (JIT_CAPRECYCLE[i].backend != b || JIT_CAPRECYCLE[i].nbytes != nbytes) {
      continue;
    }
    u32 bid = JIT_CAPRECYCLE[i].buf_id;
    JIT_CAPRECYCLE[i] = JIT_CAPRECYCLE[JIT_CAPRECYCLE_LEN - 1];
    JIT_CAPRECYCLE_LEN--;
    thvm_metal_buf_gpu_zero(bid);
    JIT_CAPRECYCLE_PENDING[JIT_CAPRECYCLE_PENDING_LEN++] = bid;
    JIT_CAPRECYCLE_DBG_POPS++;
    JIT_CAPRECYCLE_DBG_BYTES += nbytes;
    return bid;
  }
  // Release pool second.  Its entries hold only the capture's ref;
  // tensor_alloc_adopt adds the adopter's, whose release re-parks the id.
  for (u32 i = 0; i < JIT_CAPRELEASE_LEN; i++) {
    if (JIT_CAPRELEASE[i].backend != b || JIT_CAPRELEASE[i].nbytes != nbytes) {
      continue;
    }
    u32 bid = JIT_CAPRELEASE[i].buf_id;
    JIT_CAPRELEASE[i] = JIT_CAPRELEASE[JIT_CAPRELEASE_LEN - 1];
    JIT_CAPRELEASE_LEN--;
    thvm_metal_buf_gpu_zero(bid);
    JIT_CAPRECYCLE_PENDING[JIT_CAPRECYCLE_PENDING_LEN++] = bid;
    JIT_CAPRELEASE_DBG_POPS++;
    JIT_CAPRELEASE_DBG_BYTES += nbytes;
    return bid;
  }
  return 0;
#else
  (void)b; (void)nbytes;
  return 0;
#endif
}

// Release-pool entry, called by metal_buf_decref when a pinned buffer falls to
// the capture's own ref, and by the realize-end sweep.  Parks the buffer iff a
// capture is recording, only the active slot retains it, and a recorded
// dispatch rewrites it: a leaf input (weight, mask) keeps its capture-time
// bytes, and ASSIGN or host-gather writes are host copies not ordered against
// a GPU adopter.  Adoption aliases ids in record order, so replay stays sound.
static int jit_static_pack_enabled(void);

fn void jit_caprecycle_release_hook(Backend *b, u32 buf_id) {
#ifdef THVM_HAS_METAL
  if (!jit_caprecycle_enabled()) return;
  // Only under THVM_JIT_STATIC_PACK: refcount 1 with the capture's retain
  // means no live tensor names the buffer, so no later record op reads it.
  if (!jit_static_pack_enabled()) return;
  if (JIT_ACTIVE_SLOT == 0 || JIT_PAUSE_DEPTH > 0) {
    JIT_CAPRELEASE_DBG_REJ_INACTIVE++;
    return;
  }
  if (b != &METAL_BACKEND || buf_id == 0) return;
  u32 meta = thvm_metal_buf_meta(buf_id);
  if ((meta & 1u) != 0) return;                     // borrowed mmap wrap
  if (((meta >> 1) & 1u) == 0) {                    // not jit_pinned
    JIT_CAPRELEASE_DBG_REJ_UNPINNED++;
    return;
  }
  if (((meta >> 2) & 1u) == 0) return;              // no owned storage
  // A count above the capture's own retain means a live tensor still reads it.
  if ((meta >> 8) != 1) {
    JIT_CAPRELEASE_DBG_REJ_REFS++;
    return;
  }
  JitCapture *c = &JIT_CAPTURES[JIT_ACTIVE_SLOT];
  if (!c->in_use) return;
  if (!jit_bufref_contains(c->retained, c->n_retained, b, buf_id)) {
    JIT_CAPRELEASE_DBG_REJ_NOTRETAINED++;
    return;
  }
  for (u32 s = 1; s < JIT_CAPTURE_NSLOTS; s++) {
    if (s == JIT_ACTIVE_SLOT) continue;
    JitCapture *o = &JIT_CAPTURES[s];
    if (o->in_use && jit_bufref_contains(o->retained, o->n_retained, b, buf_id)) {
      return;
    }
  }
  int written = 0;
  for (u32 i = 0; i < c->n_ops; i++) {
    if (c->ops[i].kind == JIT_OP_DISPATCH && c->ops[i].out_buf_id == buf_id) {
      written = 1;
      break;
    }
  }
  if (!written) {
    JIT_CAPRELEASE_DBG_REJ_NOTWRITTEN++;
    return;
  }
  for (u32 i = 0; i < JIT_CAPRELEASE_LEN; i++) {
    if (JIT_CAPRELEASE[i].buf_id == buf_id) return;  // already parked
  }
  // Never park an id the pass pool holds: two adopters would share it.
  for (u32 i = 0; i < JIT_CAPRECYCLE_LEN; i++) {
    if (JIT_CAPRECYCLE[i].buf_id == buf_id) return;
  }
  if (JIT_CAPRELEASE_LEN >= JIT_CAPRECYCLE_CAP) return;
  u64 nbytes = 0;
  u32 rc = 0;
  thvm_metal_buf_get(buf_id, &nbytes, &rc);
  if (nbytes == 0) return;
  JIT_CAPRELEASE[JIT_CAPRELEASE_LEN].backend = b;
  JIT_CAPRELEASE[JIT_CAPRELEASE_LEN].buf_id  = buf_id;
  JIT_CAPRELEASE[JIT_CAPRELEASE_LEN].nbytes  = nbytes;
  JIT_CAPRELEASE_LEN++;
  JIT_CAPRELEASE_DBG_PARKS++;
#else
  (void)b; (void)buf_id;
#endif
}

// Extern, as the Metal translation unit cannot see static fn symbols.  Armed
// by the first jit_capture_begin and left set, as the hook no-ops when idle.
void (*thvm_metal_caprelease_hook)(u32 buf_id) = NULL;

#ifdef THVM_HAS_METAL
static void jit_caprecycle_release_hook_metal(u32 buf_id) {
  jit_caprecycle_release_hook(&METAL_BACKEND, buf_id);
}
#endif

// Consume out_buf_id's pending zero mark: 1 iff a pool served it since its
// last record.
static int jit_caprecycle_take_pending(u32 out_buf_id) {
  for (u32 i = 0; i < JIT_CAPRECYCLE_PENDING_LEN; i++) {
    if (JIT_CAPRECYCLE_PENDING[i] == out_buf_id) {
      JIT_CAPRECYCLE_PENDING[i] =
          JIT_CAPRECYCLE_PENDING[JIT_CAPRECYCLE_PENDING_LEN - 1];
      JIT_CAPRECYCLE_PENDING_LEN--;
      return 1;
    }
  }
  return 0;
}

static void jit_capture_clear_input_sites(JitCapture *c) {
  if (c == NULL) {
    return;
  }
  free(c->input_buf_ids);
  c->input_buf_ids = NULL;
  c->n_inputs_decl = 0;
  free(c->sites);
  c->sites = NULL;
  c->n_sites = 0;
  c->sites_built = 0;
  free(c->graph_input_ids);
  c->graph_input_ids = NULL;
  free(c->graph_input_addrs);
  c->graph_input_addrs = NULL;
  c->graph_input_n = 0;
}

static void jit_capture_clear_ops(JitCapture *c) {
  if (c == NULL) {
    return;
  }
  jit_capture_release_retained(c);
  jit_capture_clear_input_sites(c);
  if (c->ops == NULL) {
    return;
  }
  for (u32 i = 0; i < c->n_ops; i++) {
    free(c->ops[i].heap_in_buf_ids);
    c->ops[i].heap_in_buf_ids = NULL;
    if (c->ops[i].kind == JIT_OP_GATHER) {
      free(c->ops[i].gather_prior);
      c->ops[i].gather_prior = NULL;
    }
  }
  c->n_ops = 0;
}

fn int jit_is_capturing(void) {
  return JIT_ACTIVE_SLOT != 0 && JIT_PAUSE_DEPTH == 0;
}

// kernel_fire_by_id records but skips the eager Metal submit when this holds.
// A paused capture (an autotune bench) still dispatches eagerly.
fn int jit_capture_trace_pending(void) {
  if (JIT_ACTIVE_SLOT == 0 || JIT_PAUSE_DEPTH != 0) return 0;
  return JIT_CAPTURES[JIT_ACTIVE_SLOT].trace_capture_pending != 0;
}

// Called when a Metal fire skipped its eager submit; arms the end replay.
fn void jit_capture_note_trace_skip(void) {
  if (JIT_ACTIVE_SLOT != 0) {
    JIT_CAPTURES[JIT_ACTIVE_SLOT].trace_capture_skipped = 1;
  }
}

fn void jit_capture_pause(void) {
  if (JIT_ACTIVE_SLOT != 0) {
    JIT_PAUSE_DEPTH++;
  }
}

fn void jit_capture_resume(void) {
  if (JIT_PAUSE_DEPTH != 0) {
    JIT_PAUSE_DEPTH--;
  }
}

// Claim a free slot: its id (>= 1), or 0 when all are in use.  The WL TJit
// closure holds it until jit_capture_drop.
fn u32 jit_capture_begin(void) {
  for (u32 i = 1; i <= JIT_CAPTURE_NSLOTS - 1; i++) {
    if (!JIT_CAPTURES[i].in_use) {
      if (JIT_CAPTURES[i].ops == NULL) {
        // Start small: JIT_CAPTURE_OP_CAP rows in each of 64 slots would
        // pin a lot of memory, mostly unused.
        JIT_CAPTURES[i].ops = (JitCaptureOp *)calloc(
            JIT_CAPTURE_OP_INIT, sizeof(JitCaptureOp));
        if (JIT_CAPTURES[i].ops == NULL) return 0;
        JIT_CAPTURES[i].ops_cap = JIT_CAPTURE_OP_INIT;
      }
      if (JIT_CAPTURES[i].retained == NULL) {
        JIT_CAPTURES[i].retained = (JitCaptureBufRef *)calloc(
            JIT_CAPTURE_RETAIN_CAP, sizeof(JitCaptureBufRef));
        if (JIT_CAPTURES[i].retained == NULL) {
          free(JIT_CAPTURES[i].ops);
          JIT_CAPTURES[i].ops = NULL;
          return 0;
        }
      }
      jit_capture_clear_ops(&JIT_CAPTURES[i]);
      JIT_CAPTURES[i].in_use   = 1;
      // Latched so the per-fire query and the end replay agree.
      JIT_CAPTURES[i].trace_capture_pending =
          jit_trace_capture_enabled() ? 1u : 0u;
      JIT_CAPTURES[i].trace_capture_skipped = 0;
      JIT_ACTIVE_SLOT          = i;
      JIT_PAUSE_DEPTH          = 0;
      jit_caprecycle_reset();
#ifdef THVM_HAS_METAL
      thvm_metal_caprelease_hook = jit_caprecycle_release_hook_metal;
#endif
      // The capture's realizes share one loc->tid cache, so a kernel an
      // earlier realize emitted is reused, not emitted and recorded again.
      materialized_loc_jit_span_begin();
      return i;
    }
  }
  return 0;
}

static void jit_capture_end_common(u32 done_slot, const Term *roots,
                                   u32 n_roots) {
  materialized_loc_jit_span_end();
  JIT_ACTIVE_SLOT = 0;
  JIT_PAUSE_DEPTH = 0;
  jit_caprecycle_reset();
  // Plan replay liveness, packing and ICB eligibility (tinygrad
  // graph_split_rewrite and GraphRunner).
  if (done_slot != 0) {
    jit_capture_finalize(done_slot, roots, n_roots);
  }
  // Trace capture skipped the eager submits, leaving the results unfilled.  The
  // recorded buffers are this call's, so one plain replay fills them at replay
  // cost.  Gated on skipped, not pending: a CPU or CUDA capture already ran.
  if (done_slot != 0
      && JIT_CAPTURES[done_slot].in_use
      && JIT_CAPTURES[done_slot].trace_capture_pending) {
    JIT_CAPTURES[done_slot].trace_capture_pending = 0;
    if (JIT_CAPTURES[done_slot].trace_capture_skipped) {
      JIT_CAPTURES[done_slot].trace_capture_skipped = 0;
      jit_replay(done_slot);
    }
  }
}

fn void jit_capture_end(void) {
  jit_capture_end_common(JIT_ACTIVE_SLOT, NULL, 0);
}

fn void jit_capture_end_with_result(Term root) {
  jit_capture_end_common(JIT_ACTIVE_SLOT, &root, root != 0 ? 1u : 0u);
}

// For a function returning several tensors (TGrad over several parameters):
// every returned buffer must be a root, or finalize treats it as dead, skips
// it on replay and lets the next step's arena reclaim it.
fn void jit_capture_end_with_results(const Term *roots, u32 n_roots) {
  jit_capture_end_common(JIT_ACTIVE_SLOT, roots, n_roots);
}

fn void jit_capture_drop(u32 slot) {
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) return;
  jit_capture_clear_ops(&JIT_CAPTURES[slot]);
  JIT_CAPTURES[slot].in_use = 0;
  if (JIT_ACTIVE_SLOT == slot) {
    JIT_ACTIVE_SLOT = 0;
    JIT_PAUSE_DEPTH = 0;
    jit_caprecycle_reset();
  }
#ifdef THVM_HAS_CUDA
  cuda_jit_graph_invalidate(slot);
#endif
  // The ops array stays allocated for the slot's next capture.
}

fn u32 jit_capture_op_count(u32 slot) {
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) return 0;
  return JIT_CAPTURES[slot].n_ops;
}

static int jit_metal_graph_replay_enabled(void) {
  char const *e = getenv("THVM_METAL_GRAPH_REPLAY");
  return e == NULL || e[0] != '0';
}

// THVM_JIT_TRACE_CAPTURE (default on; =0 in the process environment disables):
// Metal recording skips the eager submits, and one replay after finalize fills
// the first results the way warm calls do.  The replay needs only the record,
// the kernels' DAGs and the buffer table, all ready at fire time.  Default on
// because WL SetEnvironment does not reliably reach this library's getenv.
fn int jit_trace_capture_enabled(void) {
  char const *e = getenv("THVM_JIT_TRACE_CAPTURE");
  return (e == NULL || e[0] != '0');
}

// THVM_JIT_REPLAY_NOSKIP=1 replays every dispatch, needed or not: a measurement
// aid for a bench that discards its result, whose replay would do no GPU work.
static int jit_replay_noskip(void) {
  static int known = 0;
  static int v = 0;
  if (!known) {
    char const *e = getenv("THVM_JIT_REPLAY_NOSKIP");
    v = (e != NULL && e[0] == '1');
    known = 1;
  }
  return v;
}

static u32 jit_metal_graph_max_dispatches(void) {
  static int known = 0;
  static u32 limit = 256;
  if (!known) {
    char const *e = getenv("THVM_METAL_GRAPH_MAX_DISPATCHES");
    if (e != NULL && e[0] != '\0') {
      u64 v = strtoull(e, NULL, 10);
      if (v >= 2 && v <= 512) {
        limit = (u32)v;
      }
    }
    known = 1;
  }
  return limit;
}

static int jit_replay_pack_enabled(void) {
  char const *e = getenv("THVM_JIT_REPLAY_PACK");
  return e == NULL || e[0] != '0';
}

// THVM_JIT_STATIC_PACK=1: tinygrad's offline planner (engine/memory.py) over
// every capture-internal buffer.  Liveness in a record is static and replay
// runs it in order, so buffers with disjoint [def, last use] intervals alias.
// It drops the default packer's tile-kernel-only gate and its rule keeping an
// output no op reads pinned, taking tinygrad's view that a JIT's internals are
// never host-read.  Inputs, result roots, multi-writer buffers, other
// captures' buffers and ASSIGN or gather destinations stay excluded.
static int jit_static_pack_enabled(void) {
  char const *e = getenv("THVM_JIT_STATIC_PACK");
  return e != NULL && e[0] == '1';
}

// THVM_JIT_GRAPH_FORCE_ICB=1 batches a buffer-recycling capture into the ICB
// anyway.  tinygrad's MetalGraph orders such writes with setBarrier
// (runtime/graph/metal.py:39), but its recycled buffers are slices of one
// arena; thvm recycles whole buffers, which setBarrier does not reliably order
// on M3.  Per-op replay does, so recycling streams decline the ICB by default
// (flux_jit_replay.wlt).
static int jit_metal_graph_force_icb(void) {
  static int known = 0;
  static int v = 0;
  if (!known) {
    char const *e = getenv("THVM_JIT_GRAPH_FORCE_ICB");
    v = (e != NULL && e[0] == '1');
    known = 1;
  }
  return v;
}

// The lifted DAG's UOp count for jit_capture_export_ops, leaves excluded.  A
// shared node counts once per use; the walk stops at depth 256.
static u32 jit_capture_dag_op_count_walk(Term t, u32 depth) {
  if (depth > 256 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID
      || op == UOP_RANGE) return 0;
  u32 count = 1;
  u64 loc = term_val(t);
  if (op == UOP_OPT) {
    return count + jit_capture_dag_op_count_walk(uop_opt_target(t), depth + 1);
  }
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term child = heap_read(loc + i);
    if (term_tag(child) == TAG_UOP) {
      count += jit_capture_dag_op_count_walk(child, depth + 1);
    }
  }
  return count;
}

static u32 jit_capture_kernel_op_count(KernelEntry const *ke) {
  if (ke == NULL || ke->cached_lift.store_root == 0) return 0;
  return jit_capture_dag_op_count_walk(ke->cached_lift.store_root, 0);
}

// Structural content hash of a lifted DAG (Merkle style), keying the Metal
// pipeline cache and autotune.  A root's heap location is no key: garbage
// collection and hash-consing move nodes.  Every payload that changes the
// generated code is hashed, since a collision would reuse a wrong pipeline.
// Memoised per call by location, so shared subgraphs hash once.

#define UOP_CONTENT_HASH_CACHE_CAP 8192    // power of two
typedef struct {
  u32 gen;
  u64 loc;
  u64 hash;
} UopContentHashSlot;
static UopContentHashSlot UOP_CONTENT_HASH_CACHE[UOP_CONTENT_HASH_CACHE_CAP];
static u32 UOP_CONTENT_HASH_GEN = 0;

static inline u64 uop_content_mix(u64 h, u64 v) {
  h ^= v;
  h *= 0x100000001b3ULL;
  return h;
}

static inline u32 uop_content_cache_hash(u64 loc) {
  loc ^= loc >> 33; loc *= 0xff51afd7ed558ccdULL;
  loc ^= loc >> 33; loc *= 0xc4ceb9fe1a85ec53ULL;
  loc ^= loc >> 33;
  return (u32)loc & (UOP_CONTENT_HASH_CACHE_CAP - 1);
}

static int uop_content_cache_lookup(u64 loc, u64 *out) {
  u32 h = uop_content_cache_hash(loc);
  for (u32 probe = 0; probe < UOP_CONTENT_HASH_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (UOP_CONTENT_HASH_CACHE_CAP - 1);
    UopContentHashSlot *s = &UOP_CONTENT_HASH_CACHE[i];
    if (s->gen != UOP_CONTENT_HASH_GEN) return 0;     // empty for this gen
    if (s->loc == loc) { *out = s->hash; return 1; }
  }
  return 0;
}

static void uop_content_cache_insert(u64 loc, u64 hash) {
  u32 h = uop_content_cache_hash(loc);
  for (u32 probe = 0; probe < UOP_CONTENT_HASH_CACHE_CAP; probe++) {
    u32 i = (h + probe) & (UOP_CONTENT_HASH_CACHE_CAP - 1);
    UopContentHashSlot *s = &UOP_CONTENT_HASH_CACHE[i];
    if (s->gen != UOP_CONTENT_HASH_GEN || s->loc == loc) {
      s->gen = UOP_CONTENT_HASH_GEN;
      s->loc = loc;
      s->hash = hash;
      return;
    }
  }
  // Full table: the node is recomputed on its next visit.
}

// A non-UOp term hashes by tag, ext and val, none of which a collection moves.
static inline u64 uop_content_hash_atom(u64 h, Term t) {
  h = uop_content_mix(h, (u64)term_tag(t));
  h = uop_content_mix(h, (u64)term_ext(t));
  h = uop_content_mix(h, (u64)term_val(t));
  return h;
}

static u64 uop_content_hash_rec(Term t, u32 depth) {
  if (depth > 512) return 0xDEADBEEFULL;          // pathological guard
  if (term_tag(t) != TAG_UOP) {
    return uop_content_hash_atom(0xcbf29ce484222325ULL, t);
  }

  u64 loc = term_val(t);
  u64 cached;
  if (uop_content_cache_lookup(loc, &cached)) return cached;

  u32 op = term_ext(t);
  u64 h = 0xcbf29ce484222325ULL;
  h = uop_content_mix(h, 0x55554F50ULL);          // "UOP" domain tag
  h = uop_content_mix(h, (u64)op);

  switch (op) {
    case UOP_CONST: {
      // [NUM(bits)]; the NUM's ext carries the dtype.
      Term num = heap_read(loc + 0);
      h = uop_content_hash_atom(h, num);
      break;
    }
    case UOP_RANGE: {
      // [NUM(axis_id), NUM(axis_type), NUM(extent)].  A symbolic extent is a
      // token holding the variable id, so it hashes alike at every batch size.
      h = uop_content_mix(h, (u64)uop_range_axis_id(t));
      h = uop_content_mix(h, (u64)uop_range_axis_type(t));
      h = uop_content_mix(h, (u64)uop_range_extent(t));
      break;
    }
    case UOP_BUFFER: {
      // The instance (kernel slot) says WHICH input an operand reads:
      // without it x*x+y and x*y+y hash equal and share a pipeline.
      h = uop_content_mix(h, (u64)uop_buffer_scope(t));
      h = uop_content_mix(h, (u64)uop_buffer_dtype(t));
      h = uop_content_mix(h, (u64)uop_buffer_inst_get(t));
      u32 ndim = uop_buffer_ndim(t);
      h = uop_content_mix(h, (u64)ndim);
      for (u32 d = 0; d < ndim; d++) {
        h = uop_content_mix(h, (u64)uop_buffer_dim(t, d));
      }
      break;
    }
    case UOP_INVALID:
      break;                                        // [NUM(0)] sentinel
    case UOP_DEFINE_VAR:
      h = uop_content_hash_atom(h, heap_read(loc + 0));  // [NUM(packed_extent)]
      break;

    case UOP_OPT: {
      h = uop_content_mix(h, (u64)uop_opt_kind(t));
      h = uop_content_mix(h, (u64)uop_opt_factor(t));
      h = uop_content_mix(h, uop_content_hash_rec(uop_opt_target(t), depth + 1));
      break;
    }

    case UOP_REDUCE: {
      h = uop_content_mix(h, (u64)uop_reduce_kind(t));
      u32 n_axes = uop_reduce_n_axes(t);
      h = uop_content_mix(h, (u64)n_axes);
      for (u32 i = 0; i < n_axes; i++) {
        h = uop_content_mix(h, (u64)uop_reduce_axis(t, i));
      }
      h = uop_content_mix(h, uop_content_hash_rec(uop_reduce_src(t), depth + 1));
      break;
    }

    case UOP_BUFFERIZE: {
      h = uop_content_mix(h, (u64)uop_bufferize_addrspace(t));
      // loc+2 holds the removable flag.
      h = uop_content_hash_atom(h, heap_read(loc + 2));
      u32 nr = uop_bufferize_n_ranges(t);
      h = uop_content_mix(h, (u64)nr);
      h = uop_content_mix(h,
            uop_content_hash_rec(uop_bufferize_value(t), depth + 1));
      for (u32 i = 0; i < nr; i++) {
        h = uop_content_mix(h,
              uop_content_hash_rec(uop_bufferize_range_at(t, i), depth + 1));
      }
      break;
    }

    // Movement ops [src, NUM(ndim), payload]: ndim payload NUMs, or 2*ndim
    // for PAD and SHRINK.
    case UOP_RESHAPE: case UOP_EXPAND: case UOP_PERMUTE:
    case UOP_PAD:     case UOP_SHRINK: {
      h = uop_content_mix(h, uop_content_hash_rec(heap_read(loc + 0), depth + 1));
      u32 ndim = (u32)term_val(heap_read(loc + 1));
      if (ndim > MAX_DIM) ndim = MAX_DIM;     // clamp defensively
      u32 per_dim = (op == UOP_PAD || op == UOP_SHRINK) ? 2u : 1u;
      u32 npay = ndim * per_dim;
      h = uop_content_mix(h, (u64)ndim);
      for (u32 i = 0; i < npay; i++) {
        h = uop_content_hash_atom(h, heap_read(loc + 2 + i));
      }
      break;
    }

    // FLIP [src, NUM(mask)]; CAST/BITCAST [src, NUM(dst_dtype)];
    // COPY [src, NUM(device+1)].
    case UOP_FLIP: case UOP_CAST: case UOP_BITCAST: case UOP_COPY: {
      h = uop_content_mix(h, uop_content_hash_rec(heap_read(loc + 0), depth + 1));
      h = uop_content_hash_atom(h, heap_read(loc + 1));
      break;
    }

    // GATHER [src, idx, NUM(dim)]; SCATTER [base, idx, src, NUM(dim),
    // NUM(kind), NUM(include_self)].  The payload is hashed too, or another
    // axis or scatter kind would collide.
    case UOP_GATHER: case UOP_SCATTER: {
      u32 n_src  = (op == UOP_GATHER) ? 2u : 3u;
      u32 n_cell = (op == UOP_GATHER) ? 3u : 6u;
      for (u32 i = 0; i < n_src; i++) {
        h = uop_content_mix(h, uop_content_hash_rec(heap_read(loc + i), depth + 1));
      }
      for (u32 i = n_src; i < n_cell; i++) {
        h = uop_content_hash_atom(h, heap_read(loc + i));
      }
      break;
    }

    // AFTER [node, after_node] orders side effects in a multi-store kernel.
    // uop_arity is 0 for it, so both children are hashed explicitly.
    case UOP_AFTER: {
      h = uop_content_mix(h, uop_content_hash_rec(heap_read(loc + 0), depth + 1));
      h = uop_content_mix(h, uop_content_hash_rec(heap_read(loc + 1), depth + 1));
      break;
    }

    default: {
      u8 ar = uop_arity((u8)op);
      for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
        h = uop_content_mix(h, uop_content_hash_rec(heap_read(loc + i), depth + 1));
      }
      break;
    }
  }

  uop_content_cache_insert(loc, h);
  return h;
}

// Bumping the generation empties the memo without a memset; the heap may have
// moved since the last call.
u64 uop_dag_content_hash(Term root) {
  if (root == 0) return 0;
  UOP_CONTENT_HASH_GEN++;
  if (UOP_CONTENT_HASH_GEN == 0) UOP_CONTENT_HASH_GEN = 1;  // skip sentinel
  return uop_content_hash_rec(root, 0);
}

// The capture as a flat table for WL profiling: header {n_ops, row_width},
// then per op {kind, kid, dispatch_kind, n_inputs, out_buf_id, input0, input1,
// 0, output_numel, uop_count, 0, 0, assign_dst_tid, assign_src_tid,
// replay_skip, replay_packed}.
fn u32 jit_capture_export_ops(u32 slot, u64 *out, u32 cap_words) {
  if (out == NULL || cap_words < 2) {
    return 0;
  }
  out[0] = 0;
  out[1] = JIT_CAPTURE_EXPORT_ROW_WIDTH;
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) {
    return 2;
  }
  JitCapture *c = &JIT_CAPTURES[slot];
  if (!c->in_use || c->n_ops == 0) {
    return 2;
  }

  u32 row_width = JIT_CAPTURE_EXPORT_ROW_WIDTH;
  u32 max_ops = (cap_words - 2) / row_width;
  u32 n = c->n_ops < max_ops ? c->n_ops : max_ops;
  out[0] = n;
  out[1] = row_width;
  for (u32 i = 0; i < n; i++) {
    JitCaptureOp const *op = &c->ops[i];
    u64 *row = &out[2 + i * row_width];
    for (u32 j = 0; j < row_width; j++) {
      row[j] = 0;
    }
    row[0] = (u64)op->kind;
    row[14] = op->replay_skip;
    if (row_width >= 16) {
      row[15] = op->replay_packed;
    }
    switch (op->kind) {
      case JIT_OP_DISPATCH: {
        row[1] = op->kid;
        row[2] = cg_kernel_dispatch_kind(op->kid);
        row[3] = op->n_inputs;
        row[4] = op->out_buf_id;
        u32 const *ids = op->heap_in_buf_ids != NULL
                       ? op->heap_in_buf_ids
                       : op->in_buf_ids;
        row[5] = op->n_inputs > 0 ? ids[0] : 0;
        row[6] = op->n_inputs > 1 ? ids[1] : 0;
        if (op->kid > 0 && op->kid < KERNELS_NEXT) {
          KernelEntry const *ke = &KERNELS[op->kid];
          row[8]  = ke->output_numel;
          row[9]  = jit_capture_kernel_op_count(ke);
          row[10] = 0;
          row[11] = 0;
        }
        break;
      }
      case JIT_OP_ASSIGN:
        row[12] = op->assign_dst_tid;
        row[13] = op->assign_src_tid;
        break;
      case JIT_OP_GATHER:
        row[3] = 1;
        row[4] = op->gather_dst_buf;
        row[5] = op->gather_src_buf;
        row[8] = op->gather_view.numel;
        break;
    }
  }
  return 2 + n * row_width;
}

// Grow c->ops to `need` rows by doubling, zeroing the new tail; 0 on OOM or
// past JIT_CAPTURE_OP_CAP.  Callers take &c->ops[n] after it, as realloc moves.
static int jit_capture_ops_reserve(JitCapture *c, u32 need) {
  if (need <= c->ops_cap) return 1;
  if (need > JIT_CAPTURE_OP_CAP) return 0;
  u32 nc = c->ops_cap ? c->ops_cap : JIT_CAPTURE_OP_INIT;
  while (nc < need) {
    nc = (nc > JIT_CAPTURE_OP_CAP / 2) ? JIT_CAPTURE_OP_CAP : nc * 2;
  }
  JitCaptureOp *ne = (JitCaptureOp *)realloc(
      c->ops, (size_t)nc * sizeof(JitCaptureOp));
  if (ne == NULL) return 0;
  memset(ne + c->ops_cap, 0,
         (size_t)(nc - c->ops_cap) * sizeof(JitCaptureOp));
  c->ops = ne;
  c->ops_cap = nc;
  return 1;
}

// Called by kernel_fire_by_id and kernel_custom_fire before the dispatch.
fn void jit_capture_record(u32 kid, u32 const *in_buf_ids,
                           u32 n_inputs, u32 out_buf_id) {
  if (JIT_ACTIVE_SLOT == 0) return;
  JitCapture *c = &JIT_CAPTURES[JIT_ACTIVE_SLOT];
  if (!jit_capture_ops_reserve(c, c->n_ops + 1)) {
    fprintf(stderr,
        "thvm: jit_capture_record -- buffer full at %u ops, dropping\n",
        JIT_CAPTURE_OP_CAP);
    return;
  }
  JitCaptureOp *op = &c->ops[c->n_ops++];
  op->kind          = JIT_OP_DISPATCH;
  op->replay_skip   = 0;
  op->replay_packed = 0;
  op->replay_zero   = jit_caprecycle_take_pending(out_buf_id) ? 1 : 0;
  op->kid           = kid;
  op->out_buf_id    = out_buf_id;
  op->n_inputs      = n_inputs;
  op->heap_in_buf_ids = NULL;
  if (n_inputs <= JIT_OP_INLINE_INPUTS) {
    for (u32 i = 0; i < n_inputs; i++) op->in_buf_ids[i] = in_buf_ids[i];
  } else {
    op->heap_in_buf_ids = (u32 *)malloc((size_t)n_inputs * sizeof(u32));
    if (op->heap_in_buf_ids == NULL) {
      c->n_ops--;
      return;
    }
    for (u32 i = 0; i < n_inputs; i++) {
      op->heap_in_buf_ids[i] = in_buf_ids[i];
    }
  }
  jit_capture_retain_dispatch_bufs(c, op);
}

// Called by interact_assign_with before its copy.
fn void jit_capture_record_assign(u32 dst_tid, u32 src_tid) {
  if (JIT_ACTIVE_SLOT == 0) return;
  JitCapture *c = &JIT_CAPTURES[JIT_ACTIVE_SLOT];
  if (!jit_capture_ops_reserve(c, c->n_ops + 1)) return;
  JitCaptureOp *op = &c->ops[c->n_ops++];
  op->kind            = JIT_OP_ASSIGN;
  op->replay_skip     = 0;
  op->replay_packed   = 0;
  op->replay_zero     = 0;
  op->heap_in_buf_ids = NULL;
  op->assign_dst_tid  = dst_tid;
  op->assign_src_tid  = src_tid;
  // Replay copies between the live tensors (weights, KV caches).
  jit_capture_retain_tensor_buf(c, dst_tid);
  jit_capture_retain_tensor_buf(c, src_tid);
}

// Called by materialize_root_alias after a host strided gather whose source
// changes under replay.  `src_tid` is the alias tensor, whose view and chain
// say how to read the buffer; `dst_buf` is the contiguous destination.
fn void jit_capture_record_gather(u32 src_tid, u32 dst_buf) {
  if (JIT_ACTIVE_SLOT == 0) return;
  if (src_tid == 0 || src_tid >= TENS_NEXT) return;
  JitCapture *c = &JIT_CAPTURES[JIT_ACTIVE_SLOT];
  if (!jit_capture_ops_reserve(c, c->n_ops + 1)) return;
  TenDesc const *sd = &TENS[src_tid];
  JitCaptureOp *op = &c->ops[c->n_ops++];
  op->kind            = JIT_OP_GATHER;
  op->replay_skip     = 0;
  op->replay_packed   = 0;
  op->replay_zero     = 0;
  op->heap_in_buf_ids = NULL;
  op->gather_backend  = (void *)sd->backend;
  op->gather_src_buf  = sd->buf_id;
  op->gather_dst_buf  = dst_buf;
  op->gather_dtype    = sd->dtype;
  op->gather_view     = sd->view;
  op->gather_nviews   = sd->nviews;
  op->gather_prior    = NULL;
  if (sd->nviews > 0 && sd->prior_views != NULL) {
    op->gather_prior = (View *)malloc(sizeof(View) * sd->nviews);
    if (op->gather_prior != NULL)
      memcpy(op->gather_prior, sd->prior_views, sizeof(View) * sd->nviews);
    else
      op->gather_nviews = 0;   // OOM: drop the chain
  }
  jit_capture_retain_buf(c, sd->backend, sd->buf_id);
  jit_capture_retain_buf(c, sd->backend, dst_buf);
}

fn void jit_capture_mark_preserved(void) {
  for (u32 i = 1; i < JIT_CAPTURE_NSLOTS; i++) {
    JitCapture *c = &JIT_CAPTURES[i];
    if (!c->in_use || c->retained == NULL) {
      continue;
    }
    for (u32 j = 0; j < c->n_retained; j++) {
      jit_capture_mark_buf(c->retained[j].backend,
                           c->retained[j].buf_id);
    }
  }
}

static int jit_bufref_contains(JitCaptureBufRef const *refs, u32 n,
                               Backend *b, u32 buf_id) {
  if (b == NULL || buf_id == 0) {
    return 0;
  }
  for (u32 i = 0; i < n; i++) {
    if (refs[i].backend == b && refs[i].buf_id == buf_id) {
      return 1;
    }
  }
  return 0;
}

static void jit_bufref_add(JitCaptureBufRef *refs, u32 *n,
                           Backend *b, u32 buf_id) {
  if (refs == NULL || n == NULL || b == NULL || buf_id == 0) {
    return;
  }
  if (jit_bufref_contains(refs, *n, b, buf_id)) {
    return;
  }
  if (*n >= JIT_CAPTURE_RETAIN_CAP) {
    return;
  }
  refs[*n].backend = b;
  refs[*n].buf_id  = buf_id;
  (*n)++;
}

static Backend *jit_dispatch_output_backend(JitCaptureOp const *op) {
  if (op == NULL || op->kind != JIT_OP_DISPATCH) {
    return NULL;
  }
  if (op->kid == 0 || op->kid >= KERNELS_NEXT) {
    return NULL;
  }
  KernelEntry *ke = &KERNELS[op->kid];
  if (ke->output_tid == 0 || ke->output_tid >= TENS_NEXT) {
    return NULL;
  }
  return TENS[ke->output_tid].backend;
}

static Backend *jit_dispatch_input_backend(JitCaptureOp const *op, u32 i) {
  Backend *fallback = jit_dispatch_output_backend(op);
  if (op == NULL || op->kid == 0 || op->kid >= KERNELS_NEXT) {
    return fallback;
  }
  KernelEntry *ke = &KERNELS[op->kid];
  if (ke->input_tids == NULL || i >= ke->n_inputs) {
    return fallback;
  }
  u32 tid = ke->input_tids[i];
  if (tid == 0 || tid >= TENS_NEXT || TENS[tid].backend == NULL) {
    return fallback;
  }
  return TENS[tid].backend;
}

// A dispatch of a raw injected program (kernel_emit_custom_program) is opaque
// to the planner: always live, every argument buffer held, as tinygrad
// treats a raw PROGRAM's buffers as read and written (codegen/__init__.py
// do_to_program).
static int jit_dispatch_op_custom(JitCaptureOp const *op) {
  return op != NULL && op->kind == JIT_OP_DISPATCH
      && op->kid != 0 && op->kid < KERNELS_NEXT
      && kernel_is_custom(&KERNELS[op->kid]);
}

static void jit_capture_root_needed(Term root,
                                    JitCaptureBufRef *needed,
                                    u32 *n_needed) {
  if (root == 0) {
    return;
  }
  Term r = term_resolve(root);
  switch (term_tag(r)) {
    case TAG_TEN: {
      u32 tid = (u32)term_val(r);
      if (tid != 0 && tid < TENS_NEXT) {
        TenDesc *td = &TENS[tid];
        jit_bufref_add(needed, n_needed, td->backend, td->buf_id);
      }
      return;
    }
    case TAG_CTR: {
      u32 n = term_ctr_n(r);
      for (u32 i = 0; i < n; i++) {
        jit_capture_root_needed(term_ctr_at(r, i), needed, n_needed);
      }
      return;
    }
    default:
      return;
  }
}

static void jit_capture_drop_dead_output(JitCaptureOp const *op) {
  Backend *b = jit_dispatch_output_backend(op);
  if (b == NULL || b->buf_decref == NULL || op->out_buf_id == 0) {
    return;
  }
  b->buf_decref(op->out_buf_id);
}

static int jit_assign_sink_safe(JitCaptureOp const *producer,
                                u32 dst_tid, u32 dst_buf_id) {
  if (producer == NULL || producer->kind != JIT_OP_DISPATCH
      || producer->kid == 0 || producer->kid >= KERNELS_NEXT) {
    return 0;
  }
  KernelEntry const *ke = &KERNELS[producer->kid];
  if (ke->output_numel == 0 || dst_tid == 0 || dst_tid >= TENS_NEXT) {
    return 0;
  }
  TenDesc const *dst = &TENS[dst_tid];
  if (dst->buf_id != dst_buf_id || dst->backend == NULL) {
    return 0;
  }
#ifdef THVM_HAS_METAL
  if (dst->backend->id != METAL_BACKEND.id) {
    return 0;
  }
#else
  return 0;
#endif
  if (cg_kernel_dispatch_kind(producer->kid) != KDISPATCH_METAL_TILE) {
    return 0;
  }
  if (dst->nviews != 0 || !dst->view.contiguous || dst->view.offset != 0
      || dst->view.numel != ke->output_numel) {
    return 0;
  }
  // The producer must fill exactly output_numel contiguous elements,
  // approximated as its non-reduce axis extents multiplying to output_numel.
  Term root = ke->cached_lift.store_root;
  if (root == 0 || term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) {
    return 0;
  }
  u32 axis_ids  [MAX_DIM];
  u32 axis_types[MAX_DIM];
  u32 extents   [MAX_DIM];
  u32 n_axes = uop_dag_collect_axes(root, axis_ids, axis_types, extents, MAX_DIM);
  if (n_axes == 0) return 0;
  u64 product = 1;
  for (u32 i = 0; i < n_axes; i++) {
    if (axis_types[i] == KAX_REDUCE) continue;
    if (extents[i] == 0) return 0;
    product *= extents[i];
    if (product > 0xFFFFFFFFu) return 0;
  }
  if ((u32)product != ke->output_numel) return 0;
  return 1;
}

static int jit_capture_op_refs_buf(JitCaptureOp const *op,
                                   Backend *backend, u32 buf_id) {
  if (op == NULL || op->replay_skip || backend == NULL || buf_id == 0) {
    return 0;
  }
  switch (op->kind) {
    case JIT_OP_DISPATCH: {
      if (jit_dispatch_output_backend(op) == backend
          && op->out_buf_id == buf_id) {
        return 1;
      }
      u32 const *ids = op->heap_in_buf_ids != NULL
                     ? op->heap_in_buf_ids
                     : op->in_buf_ids;
      for (u32 i = 0; i < op->n_inputs; i++) {
        if (jit_dispatch_input_backend(op, i) == backend && ids[i] == buf_id) {
          return 1;
        }
      }
      return 0;
    }
    case JIT_OP_ASSIGN:
      if (op->assign_dst_tid != 0 && op->assign_dst_tid < TENS_NEXT) {
        TenDesc *td = &TENS[op->assign_dst_tid];
        if (td->backend == backend && td->buf_id == buf_id) {
          return 1;
        }
      }
      if (op->assign_src_tid != 0 && op->assign_src_tid < TENS_NEXT) {
        TenDesc *td = &TENS[op->assign_src_tid];
        if (td->backend == backend && td->buf_id == buf_id) {
          return 1;
        }
      }
      return 0;
    case JIT_OP_GATHER:
      // A gather re-reads its source each replay, so the dispatch feeding it
      // must not be sunk into an assign's buffer.
      return (Backend *)op->gather_backend == backend
          && (op->gather_src_buf == buf_id || op->gather_dst_buf == buf_id);
  }
  return 0;
}

static void jit_capture_sink_assigns(JitCapture *c, Term root) {
  if (c == NULL || root != 0) {
    return;
  }
  for (u32 i = 1; i < c->n_ops; i++) {
    JitCaptureOp *op = &c->ops[i];
    if (op->kind != JIT_OP_ASSIGN || op->replay_skip) {
      continue;
    }
    u32 dst_tid = op->assign_dst_tid;
    u32 src_tid = op->assign_src_tid;
    if (dst_tid == 0 || src_tid == 0
        || dst_tid >= TENS_NEXT || src_tid >= TENS_NEXT) {
      continue;
    }
    TenDesc *dst = &TENS[dst_tid];
    TenDesc *src = &TENS[src_tid];
    if (dst->backend == NULL || dst->backend != src->backend
        || dst->buf_id == 0 || src->buf_id == 0
        || dst->dtype != src->dtype
        || dst->view.numel != src->view.numel) {
      continue;
    }
    if (src->nviews != 0 || !src->view.contiguous || src->view.offset != 0) {
      continue;
    }
    u32 p = i;
    while (p > 0) {
      p--;
      if (!c->ops[p].replay_skip) {
        break;
      }
    }
    if (c->ops[p].kind != JIT_OP_DISPATCH || c->ops[p].replay_skip
        || c->ops[p].out_buf_id != src->buf_id) {
      continue;
    }
    int future_use = 0;
    for (u32 j = i + 1; j < c->n_ops; j++) {
      if (jit_capture_op_refs_buf(&c->ops[j], src->backend, src->buf_id)) {
        future_use = 1;
        break;
      }
    }
    if (future_use) {
      continue;
    }
    if (!jit_assign_sink_safe(&c->ops[p], dst_tid, dst->buf_id)) {
      continue;
    }
    u32 old_out = c->ops[p].out_buf_id;
    c->ops[p].out_buf_id = dst->buf_id;
    op->replay_skip = 1;
    if (src->backend->buf_decref != NULL && old_out != dst->buf_id) {
      src->backend->buf_decref(old_out);
    }
  }
}

// A dispatch's output size from its KernelEntry, valid after the eager buffer
// is freed; it matches the eager alloc, so exact-size reuse lines up.
static u64 jit_dispatch_output_nbytes(JitCaptureOp const *op) {
  if (op == NULL || op->kind != JIT_OP_DISPATCH
      || op->kid == 0 || op->kid >= KERNELS_NEXT) {
    return 0;
  }
  KernelEntry const *ke = &KERNELS[op->kid];
  if (ke->output_numel == 0 || ke->output_tid == 0
      || ke->output_tid >= TENS_NEXT) {
    return 0;
  }
  return dtype_storage_bytes(ke->output_dtype, ke->output_numel);
}

#if defined(THVM_HAS_METAL) || defined(THVM_HAS_CUDA)
typedef struct {
  Backend *backend;
  u32      buf_id;
  u64      nbytes;
  u32      last_use;
} JitReplaySlot;

// Release one buffer the packer redirected every reference off.  Unpin before
// the decref: a pinned buffer decref'd to 0 is hard-freed and its id handed to
// the next alloc unzeroed, under a sibling capture's replay; unpinned, it parks
// on the freelist until a same-size alloc pops it zeroed.
static void jit_capture_release_one_retained(JitCapture *c, Backend *b,
                                             u32 buf_id) {
  if (c == NULL || b == NULL || buf_id == 0) {
    return;
  }
  for (u32 i = 0; i < c->n_retained; i++) {
    if (c->retained[i].backend == b && c->retained[i].buf_id == buf_id) {
      if (b->buf_jit_unpin != NULL) b->buf_jit_unpin(buf_id);
      if (b->buf_decref != NULL)    b->buf_decref(buf_id);
      c->retained[i] = c->retained[c->n_retained - 1];
      c->n_retained--;
      return;
    }
  }
  // Not retained (already removed): still drop the pin and the ref.
  if (b->buf_jit_unpin != NULL) b->buf_jit_unpin(buf_id);
  if (b->buf_decref != NULL)    b->buf_decref(buf_id);
}

// Drop the eager tensor's ref on a packed-away output, or every packed
// intermediate stays resident.  Replay writes the new slot and no tensor
// outside the capture reads a packed intermediate.  tinygrad frees the
// originals once they become arena slices (schedule/memory.py:51-59).
static void jit_capture_release_packed_heap_output(JitCaptureOp const *op,
                                                   Backend *b, u32 old_out) {
  if (op == NULL || b == NULL || old_out == 0 || b->buf_decref == NULL) {
    return;
  }
  if (op->kid == 0 || op->kid >= KERNELS_NEXT) {
    return;
  }
  u32 tid = KERNELS[op->kid].output_tid;
  if (tid == 0 || tid >= TENS_NEXT) {
    return;
  }
  TenDesc *td = &TENS[tid];
  // Only if the descriptor still owns old_out (a view alias may have taken it).
  if (td->backend != b || td->buf_id != old_out) {
    return;
  }
  // Clear buf_id so a later tensor_release does not decref again, but keep
  // backend: jit_replay finds the output's backend through it.
  b->buf_decref(old_out);
  td->buf_id = 0;
}


static int jit_capture_op_reads_buf(JitCaptureOp const *op,
                                    Backend *backend, u32 buf_id) {
  if (op == NULL || op->replay_skip || backend == NULL || buf_id == 0) {
    return 0;
  }
  switch (op->kind) {
    case JIT_OP_DISPATCH: {
      u32 const *ids = op->heap_in_buf_ids != NULL
                     ? op->heap_in_buf_ids
                     : op->in_buf_ids;
      for (u32 i = 0; i < op->n_inputs; i++) {
        if (jit_dispatch_input_backend(op, i) == backend && ids[i] == buf_id) {
          return 1;
        }
      }
      return 0;
    }
    case JIT_OP_ASSIGN:
      if (op->assign_src_tid != 0 && op->assign_src_tid < TENS_NEXT) {
        TenDesc *td = &TENS[op->assign_src_tid];
        return td->backend == backend && td->buf_id == buf_id;
      }
      return 0;
    case JIT_OP_GATHER:
      // A gather re-reads its source each replay; unseen, the planner would
      // recycle the source before the gather fires.
      return (Backend *)op->gather_backend == backend
          && op->gather_src_buf == buf_id;
  }
  return 0;
}

static int jit_capture_op_writes_buf(JitCaptureOp const *op,
                                     Backend *backend, u32 buf_id) {
  if (op == NULL || op->replay_skip || backend == NULL || buf_id == 0) {
    return 0;
  }
  switch (op->kind) {
    case JIT_OP_DISPATCH:
      return jit_dispatch_output_backend(op) == backend
          && op->out_buf_id == buf_id;
    case JIT_OP_ASSIGN:
      if (op->assign_dst_tid != 0 && op->assign_dst_tid < TENS_NEXT) {
        TenDesc *td = &TENS[op->assign_dst_tid];
        return td->backend == backend && td->buf_id == buf_id;
      }
      return 0;
    case JIT_OP_GATHER:
      // A gather writes its destination each replay.
      return (Backend *)op->gather_backend == backend
          && op->gather_dst_buf == buf_id;
  }
  return 0;
}

static void jit_capture_replace_future_dispatch_inputs(JitCapture *c,
                                                       u32 start,
                                                       Backend *backend,
                                                       u32 old_buf_id,
                                                       u32 new_buf_id) {
  if (c == NULL || backend == NULL || old_buf_id == 0 || new_buf_id == 0) {
    return;
  }
  for (u32 i = start + 1; i < c->n_ops; i++) {
    JitCaptureOp *op = &c->ops[i];
    if (op->replay_skip) {
      continue;
    }
    if (op->kind == JIT_OP_DISPATCH) {
      u32 *ids = op->heap_in_buf_ids != NULL
               ? op->heap_in_buf_ids
               : op->in_buf_ids;
      for (u32 j = 0; j < op->n_inputs; j++) {
        if (jit_dispatch_input_backend(op, j) == backend
            && ids[j] == old_buf_id) {
          ids[j] = new_buf_id;
        }
      }
    } else if (op->kind == JIT_OP_GATHER) {
      // Redirect a gather's source too, or it replays off the freed buffer.
      if ((Backend *)op->gather_backend == backend
          && op->gather_src_buf == old_buf_id) {
        op->gather_src_buf = new_buf_id;
      }
    }
  }
}

static int jit_capture_replay_packable_output(JitCaptureOp const *op,
                                              Backend **out_backend,
                                              u64 *out_nbytes) {
  if (op == NULL || op->kind != JIT_OP_DISPATCH || op->replay_skip
      || op->kid == 0 || op->kid >= KERNELS_NEXT) {
    return 0;
  }
  KernelEntry const *ke = &KERNELS[op->kid];
  if (ke->output_tid == 0 || ke->output_tid >= TENS_NEXT) {
    return 0;
  }
  TenDesc const *td = &TENS[ke->output_tid];
  Backend *backend = td->backend;
  if (backend == NULL || backend->buf_decref == NULL
      || td->buf_id != op->out_buf_id) {
    return 0;
  }
  // GPU outputs on their own backend only: a Metal tile or CUDA JIT dispatch,
  // or any GPU dispatch under THVM_JIT_STATIC_PACK.  The CPU frees its
  // transients during materialize.
  u32 dk = cg_kernel_dispatch_kind(op->kid);
  int static_pack = jit_static_pack_enabled();
  int ok = 0;
#ifdef THVM_HAS_METAL
  if (backend->id == METAL_BACKEND.id
      && (dk == KDISPATCH_METAL_TILE || static_pack)) ok = 1;
#endif
#ifdef THVM_HAS_CUDA
  if (backend->id == CUDA_BACKEND.id
      && (dk == KDISPATCH_CUDA_JIT || static_pack)) ok = 1;
#endif
  if (!ok) return 0;
  u64 nbytes = jit_dispatch_output_nbytes(op);
  if (nbytes == 0) {
    return 0;
  }
  if (out_backend != NULL) {
    *out_backend = backend;
  }
  if (out_nbytes != NULL) {
    *out_nbytes = nbytes;
  }
  return 1;
}

static int jit_capture_replay_lifetime(JitCapture *c, u32 idx,
                                       Backend *backend, u32 buf_id,
                                       u32 *last_use) {
  if (c == NULL || backend == NULL || buf_id == 0 || last_use == NULL) {
    return 0;
  }
  u32 last = idx;
  for (u32 j = idx + 1; j < c->n_ops; j++) {
    JitCaptureOp const *op = &c->ops[j];
    if (op->replay_skip) {
      continue;
    }
    if (op->kind == JIT_OP_ASSIGN
        && jit_capture_op_reads_buf(op, backend, buf_id)) {
      return 0;
    }
    if (jit_capture_op_writes_buf(op, backend, buf_id)) {
      return 0;
    }
    if (jit_capture_op_reads_buf(op, backend, buf_id)) {
      last = j;
    }
  }
  if (last == idx) {
    // No later reader: keep it pinned, as the host may read it, except under
    // THVM_JIT_STATIC_PACK, where internals are never host-read and the
    // interval is [idx, idx].  Roots and inputs are already held.
    if (!jit_static_pack_enabled()) {
      return 0;
    }
  }
  *last_use = last;
  return 1;
}

static void jit_capture_pack_replay_temporaries(JitCapture *c, Term root,
                                                JitCaptureBufRef const *held,
                                                u32 n_held) {
  // The replay memory planner (tinygrad memory_plan_rewrite,
  // schedule/memory.py): a later op of the same size and backend reuses a dead
  // temporary, bounding the capture to its live working set.  Two ops then
  // write one buffer, which the per-op and serial replays order (the ICB
  // declines it, see jit_metal_graph_force_icb).  THVM_JIT_PACK_RESULT=0 skips
  // captures with a result.
  //
  // `held` is tinygrad's held_bufs (memory.py:13 _can_plan): the result roots
  // and declared inputs escape the capture, so they are never packed away or
  // recycled.
  static int pack_result = -1;
  if (pack_result < 0) {
    char const *e = getenv("THVM_JIT_PACK_RESULT");
    pack_result = (e == NULL || e[0] != '0') ? 1 : 0;
  }
  if (!jit_replay_pack_enabled() || c == NULL || (root != 0 && !pack_result)
      || c->n_ops == 0) {
    return;
  }
  JitReplaySlot *slots = (JitReplaySlot *)calloc(
      c->n_ops, sizeof(JitReplaySlot));
  if (slots == NULL) {
    return;
  }
  // THVM_JIT_PACK_DEBUG: one line per finalize with packable and packed counts
  // and bytes; a cold capture pins packable minus packed, plus the roots.
  int pack_dbg = getenv("THVM_JIT_PACK_DEBUG") != NULL;
  u64 dbg_n_packable = 0, dbg_n_packed = 0;
  u64 dbg_bytes_packable = 0, dbg_bytes_packed = 0;
  // A buffer several live dispatches write is never a victim.  The lifetime
  // check sees only later writers; packing the last writer away would unpin
  // the id while earlier ops still name it.
  JitCaptureBufRef *writes = (JitCaptureBufRef *)calloc(
      c->n_ops, sizeof(JitCaptureBufRef));
  u32 n_writes = 0;
  u32 n_slots = 0;
  for (u32 i = 0; i < c->n_ops; i++) {
    JitCaptureOp *op = &c->ops[i];
    int written_before = 0;
    if (writes != NULL && op->kind == JIT_OP_DISPATCH && !op->replay_skip
        && op->out_buf_id != 0) {
      Backend *ob = jit_dispatch_output_backend(op);
      if (ob != NULL) {
        written_before = jit_bufref_contains(writes, n_writes, ob,
                                             op->out_buf_id);
        if (!written_before) {
          jit_bufref_add(writes, &n_writes, ob, op->out_buf_id);
        }
      }
    }
    Backend *backend = NULL;
    u64 nbytes = 0;
    if (!jit_capture_replay_packable_output(op, &backend, &nbytes)) {
      continue;
    }
    if (written_before) {
      continue;
    }
    // Held: kept in its own buffer and out of slots[], so nothing recycles it.
    if (jit_bufref_contains(held, n_held, backend, op->out_buf_id)) {
      continue;
    }
    if (pack_dbg) { dbg_n_packable++; dbg_bytes_packable += nbytes; }
    u32 last_use = i;
    if (!jit_capture_replay_lifetime(c, i, backend, op->out_buf_id,
                                     &last_use)) {
      continue;
    }

    u32 slot_idx = n_slots;
    for (u32 s = 0; s < n_slots; s++) {
      // Exact size only: a larger slot keeps its previous occupant's bytes in
      // the tail, which a strided view reading past the output's numel (the
      // pooled im2col convolution) would see.  tinygrad also slices each
      // buffer to its exact extent (schedule/memory.py:57-59).
      if (slots[s].backend != backend || slots[s].nbytes != nbytes
          || slots[s].last_use >= i) {
        continue;
      }
      slot_idx = s;
      break;
    }
    if (slot_idx == n_slots) {
      slots[n_slots].backend  = backend;
      slots[n_slots].buf_id   = op->out_buf_id;
      slots[n_slots].nbytes   = nbytes;
      slots[n_slots].last_use = last_use;
      n_slots++;
      continue;
    }

    u32 old_out = op->out_buf_id;
    u32 new_out = slots[slot_idx].buf_id;
    op->out_buf_id = new_out;
    op->replay_packed = 1;
    jit_capture_replace_future_dispatch_inputs(c, i, backend,
                                               old_out, new_out);
    // No op names old_out now: drop the capture's and the tensor's refs.
    jit_capture_release_one_retained(c, backend, old_out);
    jit_capture_release_packed_heap_output(op, backend, old_out);
    slots[slot_idx].last_use = last_use;
    if (pack_dbg) { dbg_n_packed++; dbg_bytes_packed += nbytes; }
  }
  if (pack_dbg) {
    fprintf(stderr,
      "[jit_pack] ops=%u packable=%llu (%.1fMB) packed=%llu (%.1fMB reclaimed) "
      "root=%d\n",
      c->n_ops,
      (unsigned long long)dbg_n_packable, dbg_bytes_packable / 1.0e6,
      (unsigned long long)dbg_n_packed, dbg_bytes_packed / 1.0e6,
      (int)(root != 0));
  }
  free(writes);
  free(slots);
}
#else
static void jit_capture_pack_replay_temporaries(JitCapture *c, Term root,
                                                JitCaptureBufRef const *held,
                                                u32 n_held) {
  (void)c;
  (void)root;
  (void)held;
  (void)n_held;
}
#endif

static void jit_capture_finalize(u32 slot, const Term *roots, u32 n_roots) {
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) {
    return;
  }
  // Assign sinking runs only without a result; packing runs either way unless
  // THVM_JIT_PACK_RESULT=0.
  Term has_result = n_roots > 0 ? 1 : 0;
  JitCapture *c = &JIT_CAPTURES[slot];
  if (!c->in_use || c->ops == NULL) {
    return;
  }

  JitCaptureBufRef *needed = (JitCaptureBufRef *)calloc(
      JIT_CAPTURE_RETAIN_CAP, sizeof(JitCaptureBufRef));
  if (needed == NULL) {
    return;
  }
  u32 n_needed = 0;
  for (u32 ri = 0; ri < n_roots; ri++) {
    jit_capture_root_needed(roots[ri], needed, &n_needed);
  }

  int noskip = jit_replay_noskip();
  for (u32 rev = c->n_ops; rev > 0; rev--) {
    JitCaptureOp *op = &c->ops[rev - 1];
    op->replay_skip = 0;
    op->replay_packed = 0;
    switch (op->kind) {
      case JIT_OP_DISPATCH: {
        Backend *out_backend = jit_dispatch_output_backend(op);
        int output_needed = noskip || jit_bufref_contains(needed, n_needed,
                                                          out_backend,
                                                          op->out_buf_id);
        // A raw program's out_buf_id is only its first written argument; the
        // others are in in_buf_ids and may escape, so it always replays.
        if (jit_dispatch_op_custom(op)) output_needed = 1;
        if (!output_needed) {
          op->replay_skip = 1;
          break;
        }
        u32 const *ids = op->heap_in_buf_ids != NULL
                       ? op->heap_in_buf_ids
                       : op->in_buf_ids;
        for (u32 i = 0; i < op->n_inputs; i++) {
          jit_bufref_add(needed, &n_needed,
                         jit_dispatch_input_backend(op, i),
                         ids[i]);
        }
        break;
      }
      case JIT_OP_ASSIGN: {
        if (op->assign_dst_tid != 0 && op->assign_dst_tid < TENS_NEXT) {
          TenDesc *td = &TENS[op->assign_dst_tid];
          jit_bufref_add(needed, &n_needed, td->backend, td->buf_id);
        }
        if (op->assign_src_tid != 0 && op->assign_src_tid < TENS_NEXT) {
          TenDesc *td = &TENS[op->assign_src_tid];
          jit_bufref_add(needed, &n_needed, td->backend, td->buf_id);
        }
        break;
      }
      case JIT_OP_GATHER: {
        // Live only if its destination is needed, and then so is its source.
        Backend *gb = (Backend *)op->gather_backend;
        int dst_needed = noskip || jit_bufref_contains(needed, n_needed,
                                                       gb, op->gather_dst_buf);
        if (!dst_needed) {
          op->replay_skip = 1;
          break;
        }
        jit_bufref_add(needed, &n_needed, gb, op->gather_src_buf);
        break;
      }
    }
  }

  jit_capture_sink_assigns(c, has_result);
  // held_bufs (tinygrad memory.py:13): the result roots and declared inputs,
  // re-derived as `needed` now holds intermediates too.
  JitCaptureBufRef *held = (JitCaptureBufRef *)calloc(
      JIT_CAPTURE_RETAIN_CAP, sizeof(JitCaptureBufRef));
  u32 n_held = 0;
  if (held != NULL) {
    for (u32 ri = 0; ri < n_roots; ri++) {
      jit_capture_root_needed(roots[ri], held, &n_held);
    }
    if (c->input_buf_ids != NULL) {
      for (u32 k = 0; k < c->n_inputs_decl; k++) {
        u32 ib = c->input_buf_ids[k];
        if (ib == 0) continue;
        // Held under every backend an op reads it on.
        for (u32 oi = 0; oi < c->n_ops; oi++) {
          JitCaptureOp *op = &c->ops[oi];
          if (op->kind != JIT_OP_DISPATCH) continue;
          u32 const *ids = op->heap_in_buf_ids != NULL
                         ? op->heap_in_buf_ids : op->in_buf_ids;
          for (u32 j = 0; j < op->n_inputs; j++) {
            if (ids[j] == ib) {
              jit_bufref_add(held, &n_held, jit_dispatch_input_backend(op, j), ib);
            }
          }
        }
      }
    }
    // Raw programs hold every argument buffer (see jit_dispatch_op_custom).
    for (u32 oi = 0; oi < c->n_ops; oi++) {
      JitCaptureOp *cop = &c->ops[oi];
      if (!jit_dispatch_op_custom(cop) || cop->replay_skip) continue;
      jit_bufref_add(held, &n_held, jit_dispatch_output_backend(cop),
                     cop->out_buf_id);
      u32 const *cids = cop->heap_in_buf_ids != NULL
                      ? cop->heap_in_buf_ids : cop->in_buf_ids;
      for (u32 j = 0; j < cop->n_inputs; j++) {
        jit_bufref_add(held, &n_held, jit_dispatch_input_backend(cop, j),
                       cids[j]);
      }
    }
  }
  jit_capture_pack_replay_temporaries(c, has_result, held, n_held);
  free(held);

  // Release only the retained buffers no live op needs; the rest keep their
  // ref and pin (see jit_capture_release_retained_except).
  JitCaptureBufRef *live = (JitCaptureBufRef *)calloc(
      JIT_CAPTURE_RETAIN_CAP, sizeof(JitCaptureBufRef));
  u32 n_live = 0;
  if (live != NULL) {
    for (u32 i = 0; i < c->n_ops; i++) {
      JitCaptureOp *op = &c->ops[i];
      if (op->replay_skip) continue;
      switch (op->kind) {
        case JIT_OP_DISPATCH: {
          Backend *out_b = jit_dispatch_output_backend(op);
          jit_bufref_add(live, &n_live, out_b, op->out_buf_id);
          u32 const *ids = op->heap_in_buf_ids != NULL
                         ? op->heap_in_buf_ids
                         : op->in_buf_ids;
          for (u32 k = 0; k < op->n_inputs; k++) {
            jit_bufref_add(live, &n_live, jit_dispatch_input_backend(op, k),
                           ids[k]);
          }
          break;
        }
        case JIT_OP_ASSIGN: {
          if (op->assign_dst_tid != 0 && op->assign_dst_tid < TENS_NEXT) {
            TenDesc *td = &TENS[op->assign_dst_tid];
            jit_bufref_add(live, &n_live, td->backend, td->buf_id);
          }
          if (op->assign_src_tid != 0 && op->assign_src_tid < TENS_NEXT) {
            TenDesc *td = &TENS[op->assign_src_tid];
            jit_bufref_add(live, &n_live, td->backend, td->buf_id);
          }
          break;
        }
        case JIT_OP_GATHER: {
          Backend *gb = (Backend *)op->gather_backend;
          jit_bufref_add(live, &n_live, gb, op->gather_src_buf);
          jit_bufref_add(live, &n_live, gb, op->gather_dst_buf);
          break;
        }
      }
    }
  }
  if (live != NULL) {
    jit_capture_release_retained_except(c, live, n_live);
  } else {
    jit_capture_release_retained(c);   // OOM: release all
  }
  for (u32 i = 0; i < c->n_ops; i++) {
    JitCaptureOp *op = &c->ops[i];
    if (op->kind == JIT_OP_DISPATCH && op->replay_skip) {
      jit_capture_drop_dead_output(op);
    }
  }
  for (u32 i = 0; i < c->n_ops; i++) {
    JitCaptureOp *op = &c->ops[i];
    if (op->replay_skip) {
      continue;
    }
    switch (op->kind) {
      case JIT_OP_DISPATCH:
        jit_capture_retain_dispatch_bufs(c, op);
        break;
      case JIT_OP_ASSIGN:
        jit_capture_retain_tensor_buf(c, op->assign_dst_tid);
        jit_capture_retain_tensor_buf(c, op->assign_src_tid);
        break;
      case JIT_OP_GATHER:
        jit_capture_retain_buf(c, (Backend *)op->gather_backend, op->gather_src_buf);
        jit_capture_retain_buf(c, (Backend *)op->gather_backend, op->gather_dst_buf);
        break;
    }
  }
  free(live);

#ifdef THVM_HAS_METAL
  // THVM_JIT_GRAPH_RENAME=1 (debug): give every write of a recycled Metal
  // buffer but the last a fresh same-size buffer, so each is written once.
  // Off by default, as it undoes the packer's reuse.  Pure-dispatch captures.
  {
    char const *re = getenv("THVM_JIT_GRAPH_RENAME");
    int rename_on = (re != NULL && re[0] == '1');   // default off; =1 re-enables
    int pure_dispatch = 1;
    for (u32 i = 0; i < c->n_ops; i++) {
      if (c->ops[i].replay_skip) continue;
      if (c->ops[i].kind != JIT_OP_DISPATCH) { pure_dispatch = 0; break; }
    }
    if (rename_on && pure_dispatch) {
      u32 nbufs = thvm_metal_buf_count();
      // Unused: a root's last writer keeps its buffer anyway.
      JitCaptureBufRef *root_bufs = (JitCaptureBufRef *)calloc(
          JIT_CAPTURE_RETAIN_CAP, sizeof(JitCaptureBufRef));
      u32 n_root_bufs = 0;
      if (root_bufs != NULL) {
        for (u32 ri = 0; ri < n_roots; ri++) {
          jit_capture_root_needed(roots[ri], root_bufs, &n_root_bufs);
        }
      }
      // last_op[ob]: 1 + the last live dispatch writing ob.  cur[ob]: the
      // buffer the latest write to ob landed in.
      u32 *last_op = (u32 *)calloc((size_t)nbufs + 1u, sizeof(u32));
      u32 *cur     = (u32 *)calloc((size_t)nbufs + 1u, sizeof(u32));
      if (last_op != NULL && cur != NULL) {
        for (u32 i = 0; i < c->n_ops; i++) {
          JitCaptureOp *op = &c->ops[i];
          if (op->kind != JIT_OP_DISPATCH || op->replay_skip) continue;
          u32 ob = op->out_buf_id;
          if (ob != 0 && ob <= nbufs) last_op[ob] = i + 1u;  // 1-based
        }
        for (u32 i = 0; i < c->n_ops; i++) {
          JitCaptureOp *op = &c->ops[i];
          if (op->kind != JIT_OP_DISPATCH || op->replay_skip) continue;
          Backend *be = jit_dispatch_output_backend(op);
          if (be == NULL || be->id != 2 /* Metal */) continue;
          // Redirect inputs to the current version of any recycled buffer.
          u32 *ids = op->heap_in_buf_ids != NULL ? op->heap_in_buf_ids
                                                 : op->in_buf_ids;
          for (u32 k = 0; k < op->n_inputs; k++) {
            u32 ib = ids[k];
            if (ib != 0 && ib <= nbufs && cur[ib] != 0 && cur[ib] != ib) {
              ids[k] = cur[ib];
            }
          }
          u32 ob = op->out_buf_id;
          if (ob == 0 || ob > nbufs) continue;
          // The last write keeps ob, the id the tensor handle names, so roots
          // need no exception; earlier writes' readers follow cur[].
          (void)root_bufs;
          (void)n_root_bufs;
          if (last_op[ob] != i + 1u) {
            u64 nbytes = 0;
            thvm_metal_buf_get(ob, &nbytes, NULL);
            u32 ob_new = (nbytes > 0 && be->buf_alloc != NULL)
                       ? be->buf_alloc(nbytes) : 0;
            if (ob_new != 0) {
              jit_capture_retain_buf(c, be, ob_new);  // incref + pin + unfreelist
              op->out_buf_id = ob_new;
              cur[ob] = ob_new;
            } else {
              cur[ob] = ob;
            }
          } else {
            cur[ob] = ob;  // first or last writer keeps its buffer
          }
        }
      }
      free(root_bufs);
      free(last_op);
      free(cur);
    }
  }
#endif

  // metal_graph_recycle: some output buffer has two live writers.  Quadratic,
  // but once per capture over a step's few hundred kernels.
  c->metal_graph_unsafe = 0;
  c->metal_graph_recycle = 0;
  for (u32 i = 0; i < c->n_ops && !c->metal_graph_recycle; i++) {
    if (c->ops[i].kind != JIT_OP_DISPATCH || c->ops[i].replay_skip) continue;
    u32 ob = c->ops[i].out_buf_id;
    if (ob == 0) continue;
    for (u32 j = i + 1; j < c->n_ops; j++) {
      if (c->ops[j].kind != JIT_OP_DISPATCH || c->ops[j].replay_skip) continue;
      if (c->ops[j].out_buf_id == ob) { c->metal_graph_recycle = 1; break; }
    }
  }

  // metal_graph_unsafe: a live non-tile dispatch reads a strided view, which
  // the per-op encoder gathers into a contiguous temp and the ICB would not.
  // Tile kernels bake view strides into their addressing, so they do not
  // count; this keeps captures whose views all feed tile kernels on the ICB.
  for (u32 i = 0; i < c->n_ops && !c->metal_graph_unsafe; i++) {
    JitCaptureOp *op = &c->ops[i];
    if (op->kind != JIT_OP_DISPATCH || op->replay_skip) continue;
    if (op->kid == 0 || op->kid >= KERNELS_NEXT) continue;
    if (cg_kernel_dispatch_kind(op->kid) == KDISPATCH_METAL_TILE) continue;
    KernelEntry *ke = &KERNELS[op->kid];
    for (u32 ii = 0; ii < ke->n_inputs; ii++) {
      int composed = (ke->input_chain_composed != NULL
                      && ke->input_chain_composed[ii]);
      u32 tid = ke->input_tids != NULL ? ke->input_tids[ii] : 0;
      if (composed || tid == 0 || tid >= TENS_NEXT) continue;
      TenDesc const *td = &TENS[tid];
      if (!td->view.contiguous || td->view.offset != 0 || td->nviews != 0) {
        c->metal_graph_unsafe = 1;
        break;
      }
    }
  }

  free(needed);
}

#ifdef THVM_HAS_METAL
// THVM_METAL_SERIAL_REPLAY (default on; =0 disables): replay a run of ops on
// one serial compute encoder.  Serial order makes every write-after-write and
// read-after-write correct, so neither ICB decline applies: tile kernels bake
// views in and custom kernels read raw buffers.  Recycled outputs are zeroed
// and gathers encoded on the same encoder; MPS and DAG-route kernels and
// ASSIGN ops end the run and go per-op.
static int jit_metal_serial_replay_enabled(void) {
  static int known = 0;
  static int v = 1;
  if (!known) {
    char const *e = getenv("THVM_METAL_SERIAL_REPLAY");
    v = (e == NULL || e[0] != '0');
    known = 1;
  }
  return v;
}

#define JIT_SERIAL_RUN_CAP 2048
static u32 jit_replay_try_metal_serial_run(JitCapture *c, u32 start) {
  if (!jit_metal_serial_replay_enabled()) return 0;
  if (start >= c->n_ops || c->ops[start].kind != JIT_OP_DISPATCH) return 0;
  static JitReplayDispatch recs[JIT_SERIAL_RUN_CAP];
  static u8  kinds[JIT_SERIAL_RUN_CAP];
  static u8  zf[JIT_SERIAL_RUN_CAP];
  static u32 gx[JIT_SERIAL_RUN_CAP];
  static u32 tx[JIT_SERIAL_RUN_CAP];
  static TenDesc gsh[JIT_SERIAL_RUN_CAP];
  u32 n = 0;
  u32 consumed = 0;
  u32 n_dispatch = 0;
  int why = 0;   /* collector exit reason for THVM_SERIAL_TRACE */
  for (u32 i = start; i < c->n_ops && n < JIT_SERIAL_RUN_CAP; i++) {
    JitCaptureOp *op = &c->ops[i];
    if (op->replay_skip) {
      consumed++;
      continue;
    }
    if (op->kind == JIT_OP_GATHER) {
      // Absorb the strided gather as a compute dispatch on the same serial
      // encoder (metal_gather_encode_on; same gates as the eager GPU gather).
      Backend *gb = (Backend *)op->gather_backend;
      if (gb == NULL || gb->id != METAL_BACKEND.id) { why = 9; break; }
      u32 esz = dtype_itemsize(op->gather_dtype);
      if (esz != 1 && esz != 2 && esz != 4) { why = 10; break; }
      JitReplayDispatch *r = &recs[n];
      r->kid = 0;
      r->n_inputs = 1;
      r->out_buf_id = op->gather_dst_buf;
      r->in_buf_ids[0] = op->gather_src_buf;
      TenDesc *sh = &gsh[n];
      memset(sh, 0, sizeof(*sh));
      sh->dtype = op->gather_dtype;
      sh->view = op->gather_view;
      sh->nviews = op->gather_nviews;
      sh->prior_views = op->gather_prior;
      kinds[n] = 2;
      zf[n] = 0;
      gx[n] = 0;
      tx[n] = 0;
      n++;
      consumed++;
      continue;
    }
    if (op->kind != JIT_OP_DISPATCH) { why = 1; break; }
    if (op->kid == 0 || op->kid >= KERNELS_NEXT) { why = 2; break; }
    if (op->n_inputs > JIT_REPLAY_MAX_INPUTS) { why = 4; break; }
    KernelEntry *ke = &KERNELS[op->kid];
    if (ke->output_tid == 0 || ke->output_tid >= TENS_NEXT) { why = 5; break; }
    Backend *b = TENS[ke->output_tid].backend;
    if (b == NULL || b->id != METAL_BACKEND.id) { why = 6; break; }
    u32 groups = 0;
    u32 threads = 0;
    u8 fc;
    if (kernel_is_custom(ke)) {
      fc = 1;
    } else if (cg_kernel_dispatch_kind(op->kid) == KDISPATCH_METAL_TILE) {
      if (!cg_tile_metal_dispatch_shape(ke, &groups, &threads)) { why = 7; break; }
      fc = 0;
    } else {
      why = 100 + cg_kernel_dispatch_kind(op->kid);   /* MPS / DAG / unstamped */
      break;
    }
    JitReplayDispatch *r = &recs[n];
    r->kid = op->kid;
    r->n_inputs = op->n_inputs;
    r->out_buf_id = op->out_buf_id;
    u32 const *ids = op->heap_in_buf_ids != NULL
                   ? op->heap_in_buf_ids
                   : op->in_buf_ids;
    for (u32 j = 0; j < op->n_inputs; j++) {
      r->in_buf_ids[j] = ids[j];
    }
    if (fc == 0 && !thvm_metal_jit_replay_dispatch_ready(r)) { why = 8; break; }
    kinds[n] = fc;
    // A recycled output is zeroed on the encoder first, as per-op does.
    zf[n] = (op->replay_packed || op->replay_zero) ? 1 : 0;
    gx[n] = groups;
    tx[n] = threads;
    n++;
    n_dispatch++;
    consumed++;
  }
  if (getenv("THVM_SERIAL_TRACE") != NULL) {
    fprintf(stderr, "[serial] start=%u n=%u consumed=%u why=%d\n", start, n, consumed, why);
  }
  // Under four ops, the flush, commit and wait cost more than per-op dispatch.
  if (n < 4) return 0;
  if (thvm_metal_serial_run(recs, n, kinds, zf, gx, tx, gsh) != 0) return 0;
  for (u32 k = 0; k < n_dispatch; k++) {
    HOT_KERNEL_FIRES++;
    HOT_JIT_REPLAY_DISPATCHES++;
    ITRS++;
  }
  return consumed;
}

static u32 jit_replay_try_metal_graph_run(u32 slot, JitCapture *c, u32 start) {
  // THVM_JIT_GRAPH_DIAG: report each slot's ICB eligibility once.
  static u64 diag_seen_slots = 0;
  if (start == 0 && getenv("THVM_JIT_GRAPH_DIAG") != NULL
      && slot < 64 && !((diag_seen_slots >> slot) & 1u)) {
    diag_seen_slots |= (1ull << slot);
    fprintf(stderr,
            "[graph-diag] slot=%u n_ops=%u replay_enabled=%d unsafe=%u recycle=%u force_icb=%d\n",
            slot, c->n_ops, jit_metal_graph_replay_enabled(),
            c->metal_graph_unsafe, c->metal_graph_recycle, jit_metal_graph_force_icb());
  }
  if (!jit_metal_graph_replay_enabled()) {
    return 0;
  }
  if (c->metal_graph_unsafe) {
    return 0;
  }
  // A recycling stream goes per-op unless THVM_JIT_GRAPH_FORCE_ICB=1.
  if (c->metal_graph_recycle && !jit_metal_graph_force_icb()) {
    return 0;
  }
  if (start >= c->n_ops || c->ops[start].kind != JIT_OP_DISPATCH) {
    return 0;
  }
  JitReplayDispatch recs[512];
  u32 limit = jit_metal_graph_max_dispatches();
  u32 n = 0;
  u32 consumed = 0;
  u32 live_consumed = 0;
  for (u32 i = start; i < c->n_ops && consumed < limit; i++) {
    JitCaptureOp *op = &c->ops[i];
    if (op->replay_skip) {
      consumed++;
      continue;
    }
    if (op->kind != JIT_OP_DISPATCH) {
      break;
    }
    if (op->kid == 0 || op->kid >= KERNELS_NEXT) {
      break;
    }
    if (op->n_inputs > JIT_REPLAY_MAX_INPUTS || op->n_inputs > 30) {
      break;
    }
    u32 dispatch_kind = cg_kernel_dispatch_kind(op->kid);
    if (dispatch_kind == KDISPATCH_METAL_ALIAS) {
      consumed++;
      live_consumed++;
      continue;
    }
    if (dispatch_kind != KDISPATCH_METAL_TILE) {
      break;
    }
    KernelEntry *ke = &KERNELS[op->kid];
    if (ke->output_tid == 0 || ke->output_tid >= TENS_NEXT) {
      break;
    }
    Backend *b = TENS[ke->output_tid].backend;
    if (b == NULL || b->id != METAL_BACKEND.id) {
      break;
    }
    u32 groups_x = 0;
    u32 threads_x = 0;
    if (!cg_tile_metal_dispatch_shape(ke, &groups_x, &threads_x)) {
      break;
    }

    if (n >= limit) {
      break;
    }
    JitReplayDispatch *r = &recs[n++];
    r->kid = op->kid;
    r->n_inputs = op->n_inputs;
    r->out_buf_id = op->out_buf_id;
    u32 const *ids = op->heap_in_buf_ids != NULL
                   ? op->heap_in_buf_ids
                   : op->in_buf_ids;
    for (u32 j = 0; j < op->n_inputs; j++) {
      r->in_buf_ids[j] = ids[j];
    }
    if (!thvm_metal_jit_replay_dispatch_ready(r)) {
      n--;
      break;
    }
    consumed++;
    live_consumed++;
  }

  if (n < 2 || consumed < 2) {
    return 0;
  }
  // Zero recycled outputs before the batch, as the per-op path does; the blit
  // is ordered before the ICB's writes to the same buffer.
  for (u32 i = start; i < start + consumed && i < c->n_ops; i++) {
    JitCaptureOp *po = &c->ops[i];
    if (po->kind == JIT_OP_DISPATCH && !po->replay_skip
        && (po->replay_packed || po->replay_zero))
      thvm_metal_buf_gpu_zero(po->out_buf_id);
  }
  if (thvm_metal_jit_replay_run(slot, start, recs, n) != 0) {
    return 0;
  }
  if (getenv("THVM_JIT_GRAPH_DIAG") != NULL) {
    static u64 diag_batch_seen = 0;
    if (start == 0 && slot < 64 && !((diag_batch_seen >> slot) & 1u)) {
      diag_batch_seen |= (1ull << slot);
      fprintf(stderr, "[graph-diag] slot=%u ICB-batch start=%u n=%u consumed=%u (of %u ops)\n",
              slot, start, n, consumed, c->n_ops);
    }
  }
  HOT_JIT_GRAPH_RUNS++;
  HOT_JIT_GRAPH_DISPATCHES += n;
  for (u32 i = 0; i < live_consumed; i++) {
    HOT_KERNEL_FIRES++;
    HOT_JIT_REPLAY_DISPATCHES++;
    ITRS++;
  }
  return consumed;
}
#endif

fn u32 jit_replay(u32 slot);

// The buffer ids the JIT-input tensors had at capture, in argument order, set
// by the wrapper after jit_capture_end; jit_replay_with_inputs substitutes
// them away (tinygrad input_replace, engine/jit.py:124).
fn void jit_capture_set_inputs(u32 slot, u32 const *ids, u32 n) {
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) return;
  JitCapture *c = &JIT_CAPTURES[slot];
  if (!c->in_use) return;
  free(c->input_buf_ids);
  c->input_buf_ids = NULL;
  c->n_inputs_decl = 0;
  c->sites_built = 0;
  free(c->sites);
  c->sites = NULL;
  c->n_sites = 0;
  if (n == 0 || ids == NULL) return;
  c->input_buf_ids = (u32 *)malloc((size_t)n * sizeof(u32));
  if (c->input_buf_ids == NULL) return;
  for (u32 i = 0; i < n; i++) c->input_buf_ids[i] = ids[i];
  c->n_inputs_decl = n;
}

// Every dispatch input or output and gather source or destination that names a
// declared input, built on the first jit_replay_with_inputs.
static void jit_capture_build_input_sites(JitCapture *c) {
  if (c == NULL || c->sites_built) return;
  c->sites_built = 1;
  c->n_sites = 0;
  free(c->sites);
  c->sites = NULL;
  if (c->n_inputs_decl == 0 || c->input_buf_ids == NULL) return;

  // Two passes: count, then fill.
  for (int pass = 0; pass < 2; pass++) {
    u32 count = 0;
    for (u32 i = 0; i < c->n_ops; i++) {
      JitCaptureOp const *op = &c->ops[i];
      // Unrebound, a gather would re-read the capture-time input.
      if (op->kind == JIT_OP_GATHER) {
        for (u32 k = 0; k < c->n_inputs_decl; k++) {
          if (op->gather_src_buf == c->input_buf_ids[k]) {
            if (pass == 1) {
              c->sites[count].op_index   = i;
              c->sites[count].pos        = JIT_GATHER_SRC_POS;
              c->sites[count].slot_index = k;
            }
            count++;
          }
          if (op->gather_dst_buf == c->input_buf_ids[k]) {
            if (pass == 1) {
              c->sites[count].op_index   = i;
              c->sites[count].pos        = JIT_GATHER_DST_POS;
              c->sites[count].slot_index = k;
            }
            count++;
          }
        }
        continue;
      }
      if (op->kind != JIT_OP_DISPATCH) continue;
      u32 const *ids = op->heap_in_buf_ids != NULL
                     ? op->heap_in_buf_ids
                     : op->in_buf_ids;
      for (u32 j = 0; j < op->n_inputs; j++) {
        for (u32 k = 0; k < c->n_inputs_decl; k++) {
          if (ids[j] != c->input_buf_ids[k]) continue;
          if (pass == 1) {
            c->sites[count].op_index   = i;
            c->sites[count].pos        = j;
            c->sites[count].slot_index = k;
          }
          count++;
          break;
        }
      }
      // An input written in place, as a dispatch output.
      for (u32 k = 0; k < c->n_inputs_decl; k++) {
        if (op->out_buf_id != c->input_buf_ids[k]) continue;
        if (pass == 1) {
          c->sites[count].op_index   = i;
          c->sites[count].pos        = JIT_INPUT_OUT_POS;
          c->sites[count].slot_index = k;
        }
        count++;
        break;
      }
    }
    if (pass == 0) {
      if (count == 0) { c->n_sites = 0; return; }
      c->sites = (JitInputSite *)malloc((size_t)count * sizeof(JitInputSite));
      if (c->sites == NULL) { c->n_sites = 0; return; }
    } else {
      c->n_sites = count;
    }
  }
}

// Write this call's input buffer ids into every site, then replay.  Sites are
// positions, not ids, so each call overwrites from scratch (tinygrad
// resolve_params, engine/jit.py:126).
fn u32 jit_replay_with_inputs(u32 slot, u32 const *new_ids, u32 n) {
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) return 0;
  JitCapture *c = &JIT_CAPTURES[slot];
  if (!c->in_use) return 0;
  jit_capture_build_input_sites(c);
  if (n == c->n_inputs_decl && new_ids != NULL) {
    for (u32 s = 0; s < c->n_sites; s++) {
      JitInputSite const *site = &c->sites[s];
      if (site->op_index >= c->n_ops) continue;
      if (site->slot_index >= n) continue;
      JitCaptureOp *op = &c->ops[site->op_index];
      u32 fresh = new_ids[site->slot_index];
      if (site->pos == JIT_INPUT_OUT_POS) {
        op->out_buf_id = fresh;
      } else if (site->pos == JIT_GATHER_SRC_POS) {
        op->gather_src_buf = fresh;
      } else if (site->pos == JIT_GATHER_DST_POS) {
        op->gather_dst_buf = fresh;
      } else if (site->pos < op->n_inputs) {
        u32 *ids = op->heap_in_buf_ids != NULL
                 ? op->heap_in_buf_ids
                 : op->in_buf_ids;
        ids[site->pos] = fresh;
      }
    }
  }
#ifdef THVM_HAS_CUDA
  // Drop the CUDA graph when an input's device address moved since it was
  // baked (see graph_input_addrs); an input updated in place stays a hit.
  if (new_ids != NULL && n > 0) {
    int diag = getenv("THVM_JIT_GRAPH_DIAG") != NULL;
    if (c->graph_input_addrs != NULL) {
      int diff = (c->graph_input_n != n);
      for (u32 i = 0; i < n; i++) {
        u64 a = cuda_buf_addr(new_ids[i]);
        if (i < c->graph_input_n) {
          int moved = (a != c->graph_input_addrs[i]);
          if (moved) diff = 1;
          // Moves only: a line per input per call floods a diffusion loop.
          if (diag && moved)
            fprintf(stderr, "[graph-diag] slot=%u in%u id=%u->%u addr=%#llx->%#llx MOVED\n",
                    slot, i, c->graph_input_ids ? c->graph_input_ids[i] : 0, new_ids[i],
                    (unsigned long long)c->graph_input_addrs[i], (unsigned long long)a);
        }
      }
      if (diff) {
        cuda_jit_graph_invalidate(slot);
        if (diag) fprintf(stderr, "[graph-diag] slot=%u INVALIDATE (input dptr moved)\n", slot);
      }
    }
    if (c->graph_input_n != n) {
      free(c->graph_input_ids);
      free(c->graph_input_addrs);
      c->graph_input_ids   = (u32 *)malloc((size_t)n * sizeof(u32));
      c->graph_input_addrs = (u64 *)malloc((size_t)n * sizeof(u64));
      c->graph_input_n = (c->graph_input_ids != NULL && c->graph_input_addrs != NULL) ? n : 0;
    }
    if (c->graph_input_ids != NULL && c->graph_input_addrs != NULL) {
      memcpy(c->graph_input_ids, new_ids, (size_t)n * sizeof(u32));
      for (u32 i = 0; i < n; i++) c->graph_input_addrs[i] = cuda_buf_addr(new_ids[i]);
    }
  }
#endif
  return jit_replay(slot);
}

// Re-run the captured ops against their recorded buffers; the caller writes
// fresh input bytes beforehand.  Returns the op count, 0 for an unused slot.
fn u32 jit_replay(u32 slot) {
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) return 0;
  JitCapture *c = &JIT_CAPTURES[slot];
  if (!c->in_use)    return 0;
  if (c->n_ops == 0) return 0;
  HOT_JIT_REPLAY_CALLS++;
  backend_dispatch_begin_all();

#ifdef THVM_HAS_CUDA
  // An all-CUDA stream launches its cached CUDA graph; on a miss this
  // replay's per-op work is captured into one (backend/cuda/jit.c).
  int cuda_only = 1;
  for (u32 i = 0; i < c->n_ops; i++) {
    JitCaptureOp *op = &c->ops[i];
    if (op->replay_skip) continue;
    Backend *b = NULL;
    if (op->kind == JIT_OP_DISPATCH) {
      if (op->kid >= KERNELS_NEXT) { cuda_only = 0; break; }
      if (KERNELS[op->kid].output_tid >= TENS_NEXT) { cuda_only = 0; break; }
      b = TENS[KERNELS[op->kid].output_tid].backend;
    } else if (op->kind == JIT_OP_ASSIGN) {
      if (op->assign_dst_tid >= TENS_NEXT) { cuda_only = 0; break; }
      b = TENS[op->assign_dst_tid].backend;
    }
    if (b == NULL || b->id != 3 /* CUDA */) { cuda_only = 0; break; }
  }
  int cuda_graph_replayed = 0;
  int cuda_capturing = 0;
  if (cuda_only && cuda_jit_graph_replay(slot) == 0) {
    cuda_graph_replayed = 1;
    HOT_JIT_GRAPH_RUNS++;
    for (u32 i = 0; i < c->n_ops; i++) {
      JitCaptureOp *op = &c->ops[i];
      if (op->replay_skip) continue;
      if (op->kind == JIT_OP_DISPATCH) {
        HOT_KERNEL_FIRES++;
        HOT_JIT_REPLAY_DISPATCHES++;
        HOT_JIT_GRAPH_DISPATCHES++;
      } else {
        HOT_JIT_REPLAY_ASSIGNS++;
      }
      ITRS++;
    }
  } else if (cuda_only) {
    // Miss: capture the op loop below into a graph.
    if (cuda_jit_graph_capture_begin() == 0) {
      cuda_capturing = 1;
    }
  }
  if (cuda_graph_replayed) {
    backend_dispatch_end_all();
    return c->n_ops;
  }
#endif

  for (u32 i = 0; i < c->n_ops;) {
#ifdef THVM_HAS_METAL
    u32 serial_run = jit_replay_try_metal_serial_run(c, i);
    if (serial_run != 0) {
      i += serial_run;
      continue;
    }
    u32 metal_run = jit_replay_try_metal_graph_run(slot, c, i);
    if (metal_run != 0) {
      i += metal_run;
      continue;
    }
#endif
    JitCaptureOp *op = &c->ops[i];
    if (op->replay_skip) {
      i++;
      continue;
    }
    switch (op->kind) {
      case JIT_OP_DISPATCH: {
        if (op->kid == 0 || op->kid >= KERNELS_NEXT) {
          i++;
          continue;
        }
        KernelEntry *ke = &KERNELS[op->kid];
        Backend *b = TENS[ke->output_tid].backend;
        if (b == NULL || b->dispatch_kernel == NULL) {
          i++;
          continue;
        }
        u32 *ids = op->heap_in_buf_ids != NULL
                 ? op->heap_in_buf_ids
                 : op->in_buf_ids;
        // A recycled output may be written only in part (a windowed store),
        // while the eager pass saw a fresh zeroed buffer: zero it first, as
        // metal_buf_freelist_try_pop does, or readers see the prior bytes.
        if ((op->replay_packed || op->replay_zero) && b->buf_zero != NULL) {
          b->buf_zero(op->out_buf_id);
        }
        int rc = b->dispatch_kernel(ke, ids, op->out_buf_id);
        if (rc == 0) {
          ke->dispatched = 1;
          HOT_KERNEL_FIRES++;
          HOT_JIT_REPLAY_DISPATCHES++;
          ITRS++;
        }
        break;
      }
      case JIT_OP_ASSIGN: {
        HOT_JIT_REPLAY_ASSIGNS++;
        u32 dst = op->assign_dst_tid, src = op->assign_src_tid;
        if (dst == 0 || dst >= TENS_NEXT) break;
        if (src == 0 || src >= TENS_NEXT) break;
        TenDesc *dd = &TENS[dst], *sd = &TENS[src];
        if (dd->backend == NULL || dd->backend != sd->backend) break;
        if (sd->view.numel != dd->view.numel) break;
        // As interact_assign_with: a KV-cache append target reads its row from
        // the live kvar, since view.offset holds only the capture step's row.
        u64 off_elems = (dd->assign_kvar_id != 0)
            ? (u64)kvar_runtime(dd->assign_kvar_id) * (u32)dd->view.strides[0]
            : (u64)(u32)dd->view.offset;
        if (tensor_assign(dst, src, off_elems) == 0) {
          ITRS++;
        }
        break;
      }
      case JIT_OP_GATHER: {
        Backend *b = (Backend *)op->gather_backend;
        if (b == NULL || b->buf_read == NULL || b->buf_write == NULL) break;
        // A pure byte permutation: any 1, 2, 4 or 8 byte dtype.
        u32 esz = dtype_itemsize(op->gather_dtype);
        if (esz != 1 && esz != 2 && esz != 4 && esz != 8) break;
        // A shell for the shared strided-index walker (broadcasts, chains).
        TenDesc sh = {0};
        sh.dtype       = op->gather_dtype;
        sh.view        = op->gather_view;
        sh.nviews      = op->gather_nviews;
        sh.prior_views = op->gather_prior;
        // GPU gather first: it encodes into the open batch, while the host
        // loop's buf_read drains the GPU queue.  The hook declines flips,
        // 8-byte dtypes and long chains.
        if (b->gather_strided != NULL
            && b->gather_strided(op->gather_dst_buf, op->gather_src_buf, &sh) == 0) {
          ITRS++;
          break;
        }
        u32 numel = sh.view.numel;
        // Source bytes up to the largest reachable index, as
        // materialize_root_alias sizes it.
        u32 max_idx = 0;
        for (u32 k = 0; k < numel; k++) {
          u32 bidx = tendesc_strided_index(&sh, k);
          if (bidx > max_idx) max_idx = bidx;
        }
        size_t src_bytes = (size_t)dtype_storage_bytes(op->gather_dtype, (u64)(max_idx + 1));
        void *raw = malloc(src_bytes);
        if (raw == NULL) break;
        b->buf_read(op->gather_src_buf, raw, src_bytes);
        size_t dst_bytes = (size_t)dtype_storage_bytes(op->gather_dtype, numel);
        void *dst = malloc(dst_bytes);
        if (dst == NULL) { free(raw); break; }
        switch (esz) {
          case 4: { u32 *o = (u32 *)dst; u32 const *s = (u32 const *)raw;
                    for (u32 k = 0; k < numel; k++) o[k] = s[tendesc_strided_index(&sh, k)]; break; }
          case 1: { u8  *o = (u8  *)dst; u8  const *s = (u8  const *)raw;
                    for (u32 k = 0; k < numel; k++) o[k] = s[tendesc_strided_index(&sh, k)]; break; }
          case 2: { u16 *o = (u16 *)dst; u16 const *s = (u16 const *)raw;
                    for (u32 k = 0; k < numel; k++) o[k] = s[tendesc_strided_index(&sh, k)]; break; }
          case 8: { u64 *o = (u64 *)dst; u64 const *s = (u64 const *)raw;
                    for (u32 k = 0; k < numel; k++) o[k] = s[tendesc_strided_index(&sh, k)]; break; }
          default: free(raw); free(dst); raw = NULL; dst = NULL; break;
        }
        if (dst != NULL) b->buf_write(op->gather_dst_buf, dst, dst_bytes);
        free(raw);
        free(dst);
        ITRS++;
        break;
      }
    }
    i++;
  }
#ifdef THVM_HAS_CUDA
  if (cuda_capturing) {
    if (cuda_jit_graph_capture_end_and_launch(slot) != 0) {
      fprintf(stderr, "thvm: CUDA graph capture failed for JIT slot %u; the "
                      "replayed results may be missing\n", slot);
    }
  }
#endif
  backend_dispatch_end_all();
  return c->n_ops;
}

// Called on (re)initialization to drop a prior session's captures.
fn void jit_capture_reset_all(void) {
  for (u32 i = 0; i < JIT_CAPTURE_NSLOTS; i++) {
    jit_capture_clear_ops(&JIT_CAPTURES[i]);
    JIT_CAPTURES[i].in_use = 0;
  }
  JIT_ACTIVE_SLOT = 0;
  JIT_PAUSE_DEPTH = 0;
}
