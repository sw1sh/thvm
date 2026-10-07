// jit/capture.c -- record + replay a sequence of kernel dispatches.
//
// Phase 7 of the tinygrad-parity arc.  Per-iteration cost in a tight
// training loop is dominated by re-running the scheduler (realize_classify,
// topo-sort, materialize) -- not by the actual kernel work.  Once the
// graph stabilises (i.e. the same forward/backward shape on every step,
// just different input bytes), capture+replay lets us bypass the
// scheduler entirely on subsequent calls and dispatch the recorded
// kernel sequence directly.
//
// Three primitives:
//   jit_capture_begin()             -- reset the active capture buffer
//                                       and flip the recording flag
//   jit_capture_record(kid, ins,    -- pushed by uop_kernel's
//                      n_in, out)      kernel_fire_by_id when
//                                       JIT_CAPTURING is set
//   jit_capture_end() -> capture id
//   jit_replay(capture id)          -- walk the captured sequence and
//                                       re-dispatch each KernelEntry
//                                       against the SAME (in_buf_ids,
//                                       out_buf_id) tuple it had at
//                                       capture time
//
// Replay is intentionally dumb: no shape check, no cache invalidation,
// no input-shape comparison.  The WL wrapper (Jit.wl) is responsible
// for deciding when a capture is stale; this file just stores +
// dispatches the sequence.
//
// Buffer reuse: the captured (kid, in_buf_ids, out_buf_id) tuple
// references buf ids that are stable across iterations as long as the
// scheduler emits the same KernelEntry sequence + the inputs are
// pre-allocated TenDescs whose buf_id stays stable (no fresh
// TTensorCreate per iter).  TSet (= TAssign) into those buf_ids
// between replays mutates the input bytes in place, so the captured
// kernels read fresh data.

#define JIT_CAPTURE_OP_CAP    65536  // per-capture op count HARD cap (grow limit)
#define JIT_CAPTURE_OP_INIT   256    // per-slot ops array START size (grows 2x)
// Per-block JIT captures (the Krea velocity-net memory fix: 28 block closures
// instead of one whole-net capture) hold one slot each for the closure's life,
// so the pool must fit every block PLUS the sampler's other closures (condFn,
// text-fusion) with headroom.  64 covers 28 DiT blocks x2 (cond/uncond CFG is
// possible) with room; each unused slot is a zeroed struct (no ops array until
// first use -- allocated at JIT_CAPTURE_OP_INIT, ~120 KB, and grown per record).
#define JIT_CAPTURE_NSLOTS    64     // max simultaneous captures (paired
                                     // with TJit closures held WL-side)
#define JIT_CAPTURE_RETAIN_CAP 131072

// Two op kinds covered by the capture buffer:
//   JIT_OP_DISPATCH  -- normal kernel dispatch through Backend.dispatch_kernel.
//   JIT_OP_ASSIGN    -- a memcpy from src tid -> dst tid (interact_assign_with).
//                       The TJit-friendly optimiser-step idiom mutates weight
//                       tensors via TAssign, so we have to replay those writes
//                       too or the captured kernels would read stale weights.
// JIT_OP_GATHER -- a host-side strided gather (materialize_root_alias):
//   read the SOURCE buffer through a (possibly broadcast / chained) view
//   and write a fresh contiguous DST buffer.  Captured when a movement-op
//   realize root resolves to an alias over a VOLATILE buffer (a kernel
//   output that re-fires, or a KV cache an append ASSIGN mutates): the
//   eager capture-time gather is a one-shot host memcpy that bakes the
//   capture-step bytes, so we re-run it per replay off the re-derived
//   source.  Stores the source View (+ chain) inline so replay survives
//   the post-realize TENS rewind that drops the transient alias TenDesc.
typedef enum {
  JIT_OP_DISPATCH = 0,
  JIT_OP_ASSIGN   = 1,
  JIT_OP_GATHER   = 2
} JitOpKind;

// Full fused forward kernels can legitimately have dozens of input
// buffers (beautiful_mnist hits 26 today).  Keep capture records inline
// for replay speed; switch to a sidecar input table if this grows again.
#define JIT_OP_INLINE_INPUTS 64

typedef struct {
  JitOpKind kind;
  u8   replay_skip;
  u8   replay_packed;
  // Set at RECORD time when this dispatch's output adopted a capture-recycled
  // buffer (jit_caprecycle_pop): the replay must zero-fill the slot before the
  // dispatch (mirror of the pop-time gpu_zero) or a partial-write kernel
  // leaves the prior occupant's bytes.  Distinct from replay_packed, which the
  // finalize packer resets + re-derives; this flag survives finalize.
  u8   replay_zero;
  // DISPATCH path:
  u32  kid;
  u32  out_buf_id;     // also doubles as dst-buf for ASSIGN
  u32  n_inputs;       // also doubles as src-tid for ASSIGN (in_buf_ids[0])
  u32  in_buf_ids[JIT_OP_INLINE_INPUTS];
  u32 *heap_in_buf_ids;
  // ASSIGN path:
  u32  assign_dst_tid;
  u32  assign_src_tid;
  // GATHER path: source buffer + the strided view to read it through,
  // and the destination buffer to fill.  The View (+ chain) is copied
  // inline so the replay doesn't depend on the transient alias TenDesc.
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

// Input-rebind site: a single (op, input-position) location in the
// captured stream that reads a JIT-declared input buffer.  On every
// replay the caller supplies the current call's fresh input buffer ids
// and we overwrite each site's buf_id with the matching fresh one before
// dispatching.  Mirrors tinygrad's `input_replace`
// (engine/jit.py:124-126): GraphRunner records each call-arg position
// that is an Ops.PARAM (a JIT input) and substitutes the fresh input
// rawbuffer there before the graph runs.  pos==U32_MAX marks the
// dispatch op's out_buf_id (a JIT input used in place as an output);
// the GATHER_* sentinels mark a JIT input read/written by a regather op.
#define JIT_INPUT_OUT_POS  0xFFFFFFFFu
#define JIT_GATHER_SRC_POS 0xFFFFFFFEu
#define JIT_GATHER_DST_POS 0xFFFFFFFDu
typedef struct {
  u32 op_index;
  u32 pos;          // input-position within the op, or JIT_INPUT_OUT_POS
  u32 slot_index;   // index into JitCapture.input_buf_ids
} JitInputSite;

typedef struct {
  u32               in_use;        // 0 = free slot
  u32               n_ops;
  u32               ops_cap;       // allocated capacity of `ops` (grows on demand)
  JitCaptureOp     *ops;           // grows from JIT_CAPTURE_OP_INIT up to _OP_CAP
  u32               n_retained;
  JitCaptureBufRef *retained;      // buffers owned by this replay slot
  // Input rebinding (tinygrad input_replace port).  input_buf_ids holds
  // the buf_ids the JIT-declared input tensors carried at capture time;
  // sites lists every captured (op, pos) that reads one of them.  Built
  // lazily on the first jit_replay_with_inputs call.
  u32               n_inputs_decl;
  u32              *input_buf_ids;  // [n_inputs_decl]
  u32               sites_built;
  u32               n_sites;
  JitInputSite     *sites;          // [n_sites]
  // Set by jit_capture_finalize when a captured dispatch reads a strided-view
  // input the per-op path pre-materialises into a contiguous temp: the ICB
  // encodes the raw input buffer and skips that gather, so the kernel would
  // read strided bytes as contiguous.  Genuinely unsafe -- always decline the
  // ICB for these (the FLUX double block's concat/slice views).
  u32               metal_graph_unsafe;
  // Set by jit_capture_finalize when two live captured dispatches write the
  // SAME output buf_id (the scheduler/arena recycled a buffer across the
  // stream -- the natural memory-bounding reuse).  tinygrad's MetalGraph
  // (runtime/graph/metal.py) emits exactly this kind of reuse and relies on
  // a per-command [cmd setBarrier] to order the write-after-write within the
  // ICB; thvm's metal_graph_build emits the same setBarrier.  Default: keep
  // the ICB (the barrier orders the recycle) so the replay stays batched AND
  // memory-bounded.  THVM_JIT_GRAPH_FORCE_ICB=0 reverts to the old per-op
  // decline for a recycling stream (still correct, scheduler-free, slower).
  u32               metal_graph_recycle;
  // CUDA cuGraph rebind guard: the cached CUgraphExec bakes input buffer
  // DEVICE ADDRESSES at capture, and the cuGraph cache-HIT path (jit_replay)
  // ignores the per-op input substitution.  So an input arriving at a FRESH
  // device address would replay stale bytes -- the batched-Qwen/VAE collapse:
  // warm items all reuse the last-baked prompt's encoding.  graph_input_addrs
  // records the dptrs the cached graph was baked with; jit_replay_with_inputs
  // invalidates the graph when this call's fresh dptrs differ (re-capture with
  // the fresh buffers).  An input whose buffer is REUSED + content-updated
  // in place (the velocity-net z within one sampler loop) keeps the same dptr
  // -> no invalidate -> stays a fast cuGraph hit.
  //
  // The comparison is on dptr, NOT buf_id: across a session's call boundary a
  // freed buf_id is recycled by the freelist, so a fresh input can reuse the
  // baked buf_id while sitting at a different dptr (or realloc to a new dptr
  // under the same id).  A buf_id match then wrongly skips invalidation and
  // the warm replay reads the OLD baked address (velocity-net cold!=warm).
  // graph_input_ids is kept only for the THVM_JIT_GRAPH_DIAG trace.
  u32              *graph_input_ids;    // [graph_input_n]; buf_ids (diag only)
  u64              *graph_input_addrs;  // [graph_input_n]; dptrs the cuGraph baked
  u32               graph_input_n;
  // Trace-capture (THVM_JIT_TRACE_CAPTURE): set when the recording pass skipped
  // the eager per-kernel GPU dispatch (recorded the op stream only).  The result
  // buffers are then UNFILLED at finalize -- so jit_capture_end_common runs one
  // jit_replay (ICB build + batched submit) right after finalize to produce the
  // FIRST result, collapsing the cold step from N eager submits to the ICB-replay
  // cost.  Default 0: the eager pass fills the buffers and no extra replay runs.
  u32               trace_capture_pending;
  // Set by the per-fire skip site (jit_capture_note_trace_skip) when at least
  // one recorded dispatch actually SKIPPED its eager submit.  The finalize-time
  // one-shot replay is gated on THIS, not just trace_capture_pending: the skip
  // is Metal-only, so a CPU/CUDA capture records with pending latched but skips
  // nothing -- an unconditional end replay would run every recorded dispatch a
  // SECOND time during the capture call.  Invisible for pure kernels (same
  // bytes rewritten, 2x cold cost), corrupting for non-idempotent custom
  // kernels (an in-place rope applied twice, a device counter bumped twice --
  // the DeepSeekV2 decode-step capture bug).
  u32               trace_capture_skipped;
} JitCapture;

static JitCapture JIT_CAPTURES[JIT_CAPTURE_NSLOTS];

static u32 JIT_ACTIVE_SLOT = 0;     // 0 = not capturing; otherwise the
                                    // 1-indexed slot being filled
static u32 JIT_PAUSE_DEPTH = 0;     // nested suppression for internal
                                    // benchmark fires (autotune, variants)

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

// Release every currently-retained buffer EXCEPT those named in `keep`
// (a bufref set the live, non-skipped ops still need).  The kept entries
// stay in c->retained with their original incref + jit_pin untouched, so a
// buffer needed across the recording->finalize boundary is NEVER momentarily
// decref'd to refcount 0 and hard-freed.  This is the fix for the FLUX
// re-finalize free: recording incref+pins every dispatch's weight buffer;
// the old unconditional release dropped a ~100MB bf16 weight to refcount 0
// (a transient realize tensor + the per-realize rollback already released
// its other ref), metal_buf_decref hard-freed the slot, and the subsequent
// re-retain's metal_buf_jit_pin no-op'd on the now-nil buffer -- so replay
// bound a nil MTLBuffer at the kernel's weight input.  Unpinning then
// re-pinning a still-live buffer (the naive reorder) is wrong: jit_pinned is
// a flag, not a count, so the trailing unpin would clear a pin the re-retain
// just set.  Diff-based release keeps the flag stable.
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
      // Still needed by a live op: leave its incref + jit_pin in place and
      // compact it down so c->retained holds exactly the surviving set.
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
  // STICKY pin: the per-realize `preserved` flag gets cleared at
  // end-of-realize (cuda_buf_clear_preserved), so a buf retained mid-
  // realize then handed off to a later replay loses its pin and the
  // NEXT realize's pool_rollback freelists it -- replay then hits
  // dptr=0 / dispatch returns -1 / loss reads stale.  jit_pin sets a
  // separate flag that survives clear_preserved and gets unpinned only
  // on jit_capture_drop.
  if (b->buf_jit_pin != NULL && getenv("THVM_JIT_NO_PIN") == NULL) {
    b->buf_jit_pin(buf_id);
  }
  // If the buf is currently parked on the backend's freelist (refcount==
  // 0 from a prior owner's decref), yank it back -- otherwise a later
  // alloc would pop+transfer-storage, leaving our captured op's buf_id
  // with dptr=0.  buf_freelist_remove no-ops if buf isn't on the list.
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

// === capture-time recycle pool (the JIT memory planner, capture side) =======
//
// Port of tinygrad's JIT memory planner (engine/jit.py:72 memory_plan_rewrite
// over the captured linear; schedule/memory.py:24-53 first/last-appearance +
// interval reuse).  tinygrad plans the WHOLE captured schedule so buffers with
// non-overlapping lifetimes share allocations; the replay working set is then
// ~peak-liveness, not sum-of-all-intermediates.
//
// thvm's problem shape: at RECORD time every dispatched buffer is jit_pinned
// (jit_capture_retain_dispatch_bufs), and metal_buf_freelist_push_impl REFUSES
// pinned buffers -- so the scheduler's own within-realize recycler
// (mem_plan_push_dead -> freelist push -> same-size pop at the next output
// alloc) is disabled for the whole capture and a cold capture accumulates
// EVERY intermediate (~8.4GB for the 128px Krea DiT on top of 24GB weights).
//
// The pool re-enables exactly that recycler for captured intermediates: when
// mem_plan_push_dead would push a dead pinned buffer of the ACTIVE capture, it
// parks it here instead (still allocated, still pinned); the next same-size
// captured kernel-output alloc adopts it (tensor_alloc_adopt).  The recorded
// stream then contains the reuse -- the same buf_id written by two dispatches
// -- which the replay already supports: in-order per-op fire, ICB WAW ordering
// via metal_graph_recycle's setBarrier, and the recycled-slot zero-fill
// (replay_zero, mirroring metal_buf_freelist_try_pop's memset-on-pop).
//
// SOUNDNESS (why a parked buffer is dead for the REST of the capture, not just
// this realize): mem_plan_push_dead only pushes single-consumer non-sink
// boundaries whose last in-realize consumer depth has passed, with cross-root
// shares excluded via the xpass sentinel (materialize.c:6536-6546).  Cross-
// REALIZE reads inside a capture flow through realize ROOTS (sinks never
// pushed: the last_use==0 guard) or through the materialized_loc span -- which
// is Metal-off (materialized_loc_span_holds returns 0 for METAL_BACKEND), so
// on Metal no later realize of the capture can re-read an internal boundary.
// The eager capture pass records fire order, so the replayed reuse ordering is
// the ordering the eager pass already executed correctly.
//
// The pool is drained (entries forgotten, buffers stay pinned + allocated for
// replay) at each pass end alongside mem_plan_drain_freelist, mirroring the
// freelist's pull-back of un-popped survivors, and reset at capture
// begin/end/drop so no post-capture alloc can adopt a captured buffer.
#define JIT_CAPRECYCLE_CAP 8192
typedef struct {
  Backend *backend;
  u32      buf_id;
  u64      nbytes;
} JitCapRecycleEntry;
static JitCapRecycleEntry JIT_CAPRECYCLE[JIT_CAPRECYCLE_CAP];
static u32 JIT_CAPRECYCLE_LEN = 0;
// buf ids served from the pool whose NEXT recorded producer needs the replay
// zero-fill (op->replay_zero): the eager pass got a gpu_zero at pop; the
// replay must re-zero before the producer fires or a partial-write kernel
// leaves the prior occupant's bytes in the unwritten region.
// Sized to the pool: at emit time EVERY pop of a pass happens before ANY
// record fires (kernels fire in a later realize-loop iteration), so pending
// marks accumulate one per adoption -- an overflow would silently drop a mark
// and the replay would skip a required zero-fill.
#define JIT_CAPRECYCLE_PENDING_CAP JIT_CAPRECYCLE_CAP
static u32 JIT_CAPRECYCLE_PENDING[JIT_CAPRECYCLE_PENDING_CAP];
static u32 JIT_CAPRECYCLE_PENDING_LEN = 0;
// Cross-realize half of the planner (tinygrad jit.py:298-300 held_bufs: only
// buffers still reachable from live Tensors are excluded from planning).  A
// velocity net built from per-projection TRealize calls (fxLinear) pins ~29
// {S,F} intermediates per DiT block that the within-pass pool above can never
// see: each is the ROOT of its own micro-realize (a per-pass sink), dead only
// once later realizes have consumed it.  On Metal that death has no refcount
// event (buffer liveness is mark-sweep: mark_gc_preserve + fwd_reclaim, which
// the capture path skips because pins exempt recorded buffers from every
// free) -- so the realize-end sweep thvm_metal_buf_park_unpreserved_pinned
// offers each dead pinned buffer to jit_caprecycle_release_hook, and this
// pool survives pass boundaries (reset only at capture begin/end/drop).
// Adoption grants the adopting TenDesc a real buffer reference
// (tensor_alloc_adopt increfs); a re-dead adopter is re-parked by a later
// sweep -- a closed reuse cycle whose steady-state liveness is ~the live
// window of the block stack instead of sum-of-all-intermediates.
static JitCapRecycleEntry JIT_CAPRELEASE[JIT_CAPRECYCLE_CAP];
static u32 JIT_CAPRELEASE_LEN = 0;
// THVM_JIT_CAPRECYCLE_DEBUG: one summary line per capture at end -- adoptions
// served and bytes NOT pinned thanks to the reuse (the capture-working-set
// reduction, the first number to read when a cold capture's resident set is
// too large).
static u64 JIT_CAPRECYCLE_DBG_POPS  = 0;
static u64 JIT_CAPRECYCLE_DBG_BYTES = 0;
static u64 JIT_CAPRELEASE_DBG_PARKS = 0;
static u64 JIT_CAPRELEASE_DBG_POPS  = 0;
static u64 JIT_CAPRELEASE_DBG_BYTES = 0;
// Hook reject taxonomy (THVM_JIT_CAPRECYCLE_DEBUG): which guard turns away
// last-ref releases -- the first numbers to read when release parks == 0.
static u64 JIT_CAPRELEASE_DBG_REJ_INACTIVE    = 0;
static u64 JIT_CAPRELEASE_DBG_REJ_UNPINNED    = 0;
static u64 JIT_CAPRELEASE_DBG_REJ_REFS        = 0;
static u64 JIT_CAPRELEASE_DBG_REJ_NOTRETAINED = 0;
static u64 JIT_CAPRELEASE_DBG_REJ_NOTWRITTEN  = 0;

static int jit_caprecycle_enabled(void) {
  static int known = 0, on = 1;
  if (!known) {
    char const *e = getenv("THVM_JIT_CAPTURE_RECYCLE");
    on = (e == NULL || e[0] != '0');   // default ON; =0 opts out for A/B
    known = 1;
  }
  return on;
}

// Forget pool entries (pass boundary).  Parked buffers stay pinned+allocated
// -- they are stream-referenced intermediates the replay still rewrites.
fn void jit_caprecycle_drain(void) {
  JIT_CAPRECYCLE_LEN = 0;
}

// Mid-flight pool stats (the ceiling death path calls this; the normal
// summary only prints at capture end, which an OOM'd capture never reaches).
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

// Full reset at capture begin/end/drop: no post-capture alloc may adopt a
// captured buffer, and stale pending marks must not leak into the next
// capture's records.
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

// Park a dead captured intermediate for same-size reuse by a later captured
// kernel-output alloc.  Returns 1 iff parked (the caller marks its MemPlan
// entry pushed).  Guards: an active un-paused capture, Metal only (CPU/CUDA
// ride their arena planner), never a buffer another in-use capture's replay
// still needs, never one of the active capture's declared replay inputs
// (their content must be stable at replay).
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

// Exact-size adoption pop for a captured kernel-output alloc.  Zero-fills the
// slot (GPU blit, hazard-ordered after any in-flight producer) so the eager
// pass sees fresh-alloc semantics -- the mirror of metal_buf_freelist_try_pop's
// memset-on-pop -- and queues the replay_zero mark for the adopting op.
fn u32 jit_caprecycle_pop(Backend *b, u64 nbytes) {
#ifdef THVM_HAS_METAL
  if (JIT_ACTIVE_SLOT == 0 || JIT_PAUSE_DEPTH > 0) return 0;
  if (b != &METAL_BACKEND || nbytes == 0) return 0;
  // A full pending list would silently drop the replay-zero mark for this
  // adoption (the replay then skips a required zero-fill on a partial-write
  // kernel) -- refuse the pop instead; the caller falls back to a fresh alloc.
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
  // Release-pool second: cross-realize entries hold refcount == 1 (the
  // capture's retain only).  The adopting TenDesc's own reference is granted
  // by tensor_alloc_adopt (uniformly for both pools) -- its later
  // tensor_release then re-parks the id via the hook instead of dropping the
  // buffer to 0 under the capture.
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

// Mark-park entry (cross-realize parking).  Called ONLY from the realize-end
// sweep (thvm_metal_buf_park_unpreserved_pinned, via the extern fn pointer):
// the caller has PROVEN buf_id dead -- mark_gc_preserve just marked true
// reachability (result + WL wrappers + DEFS + heap TAG_TEN overlay) and the
// buffer is jit_pinned yet unpreserved, the same proof fwd_reclaim uses to
// hard-free when not capturing.  Parks it in the release pool iff:
//   - an active un-paused capture (never during replay/eager);
//   - retained by the ACTIVE slot on Metal and not by any other in-use slot;
//   - REWRITTEN in-stream: some recorded JIT_OP_DISPATCH has it as out_buf_id,
//     so the replay regenerates its contents (a leaf input -- rope table,
//     mask, weight -- must keep its capture-time bytes and is refused; ASSIGN
//     dsts and host-GATHER dsts are refused too: their replay writes are host
//     memcpys, not queue-ordered against a GPU adopter).
// Replay ordering stays sound for the same reason the within-pass pool is
// sound: adoption only aliases ids in record order (writer -> readers ->
// re-writer), which the per-op replay executes in order and the ICB orders
// with metal_graph_recycle's setBarrier.
static int jit_static_pack_enabled(void);   // fwd (defined below)

fn void jit_caprecycle_release_hook(Backend *b, u32 buf_id) {
#ifdef THVM_HAS_METAL
  if (!jit_caprecycle_enabled()) return;
  // Online refcount-death recycling is the THVM_JIT_STATIC_PACK path: the hook
  // fires from metal_buf_decref the instant a captured intermediate's LAST
  // non-retain reference drops (refcount reaches 1 == only the capture's own
  // pin).  rc==1-with-capture-retain is DEFINITIVE static death: no live
  // TenDesc names the buffer, so no future record op can read it, and the
  // replay re-runs the record in order -- exactly the aliasing the finalize
  // packer computes, applied online so the COLD capture peak stays at max-live
  // instead of the sum of every intermediate.  Gated so every other JIT path
  // is byte-identical.
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
  // Only the capture's own retain may remain (refcount == 1).  A higher count
  // means a live TenDesc / view still reads it -- NOT dead, do not park.
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
  // Never double-park: the id may sit in the WITHIN-PASS pool right now (its
  // TenDesc released mid-realize, after mem_plan parked it at emit) -- two
  // pool entries would serve the same id to two overlapping adopters.
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

// metal_buf_decref's callback slot (extern: the Metal TU can't see static
// `fn` symbols).  NULL until the first jit_capture_begin arms it; the hook
// itself no-ops outside an active capture, so it stays armed for the session.
void (*thvm_metal_caprelease_hook)(u32 buf_id) = NULL;

#ifdef THVM_HAS_METAL
static void jit_caprecycle_release_hook_metal(u32 buf_id) {
  jit_caprecycle_release_hook(&METAL_BACKEND, buf_id);
}
#endif

// Consume a pending replay-zero mark for the op that adopted a pooled buffer
// (called from jit_capture_record).  Returns 1 iff out_buf_id was pool-served
// since the last record of it.
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

// True iff we are actively recording (jit_is_capturing) AND the active capture
// armed trace-capture (THVM_JIT_TRACE_CAPTURE).  kernel_fire_by_id reads this to
// RECORD the op but SKIP the eager GPU dispatch; the one-shot ICB replay in
// jit_capture_end_common then produces the first result.  Returns 0 when not
// capturing or when a nested benchmark fire paused the capture (so a paused
// autotune bench still dispatches eagerly on its isolated scratch).
fn int jit_capture_trace_pending(void) {
  if (JIT_ACTIVE_SLOT == 0 || JIT_PAUSE_DEPTH != 0) return 0;
  return JIT_CAPTURES[JIT_ACTIVE_SLOT].trace_capture_pending != 0;
}

// Called by the per-fire skip site (kernel_fire_by_id's Metal trace-capture
// branch) when an eager dispatch was actually SKIPPED: arms the finalize-time
// one-shot replay.  Captures that skip nothing (CPU/CUDA recording) leave this
// unset, so no end replay double-runs their recorded stream.
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

// Allocate a fresh capture slot.  Returns 1-indexed slot id (>=1) or
// 0 if every slot is in use.  The caller (the WL TJit closure) holds
// the slot id for the lifetime of the closure; jit_capture_drop
// frees it.
fn u32 jit_capture_begin(void) {
  for (u32 i = 1; i <= JIT_CAPTURE_NSLOTS - 1; i++) {
    if (!JIT_CAPTURES[i].in_use) {
      if (JIT_CAPTURES[i].ops == NULL) {
        // Start small; jit_capture_ops_reserve grows 2x per record up to
        // JIT_CAPTURE_OP_CAP.  With 64 slots for per-block captures a fixed
        // 65536-entry preallocation would pin ~900 MB of mostly-unused op
        // rows -- the growth keeps a 29-op block at ~120 KB.
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
      // Trace-capture: arm the eager-dispatch skip for THIS capture span only.
      // Latched at begin so the per-fire query (jit_capture_trace_pending) and
      // the finalize-time one-shot replay agree, even if the env var read races.
      JIT_CAPTURES[i].trace_capture_pending =
          jit_trace_capture_enabled() ? 1u : 0u;
      JIT_CAPTURES[i].trace_capture_skipped = 0;
      JIT_ACTIVE_SLOT          = i;
      JIT_PAUSE_DEPTH          = 0;
      jit_caprecycle_reset();
#ifdef THVM_HAS_METAL
      // Arm the decref-site parking callback (idempotent; no-ops when no
      // capture is active, so it can stay set for the whole session).
      thvm_metal_caprelease_hook = jit_caprecycle_release_hook_metal;
#endif
      // Open the realize-dedup span:
      // the multiple realizes of this captured step share one loc->tid
      // cache so a kernel emitted by realize-1 is reused (not re-emitted
      // + re-recorded) by realize-2/3.  Closed in jit_capture_end{,_with_result}.
      materialized_loc_jit_span_begin();
      return i;
    }
  }
  return 0;
}

// Shared capture-end body: close the realize-dedup span, clear the active
// slot, dissolve the capture-recycle pool, then finalize the recorded stream.
static void jit_capture_end_common(u32 done_slot, const Term *roots,
                                   u32 n_roots) {
  // Close the span (reverts the substitution journal, wipes loc->tid).
  materialized_loc_jit_span_end();
  JIT_ACTIVE_SLOT = 0;
  JIT_PAUSE_DEPTH = 0;
  // Dissolve the capture-recycle pool: no post-capture alloc may adopt a
  // captured (stream-referenced, pinned) buffer.
  jit_caprecycle_reset();
  // Finalize (tinygrad graph_split_rewrite + GraphRunner): plan the replay
  // liveness / pack and the ICB eligibility.
  if (done_slot != 0) {
    jit_capture_finalize(done_slot, roots, n_roots);
  }
  // TRACE-CAPTURE one-shot: the recording pass SKIPPED the eager per-kernel GPU
  // dispatch (kernel_fire_by_id under jit_capture_trace_pending), so the result
  // buffers are unfilled.  Run ONE jit_replay -- the exact path warm calls take
  // (ICB build + batched submit when eligible; per-op dispatch otherwise) -- to
  // produce the FIRST result against the captured input buffers.  No input
  // substitution: the captured (in_buf_ids, out_buf_id) ARE the first call's
  // buffers, so a plain jit_replay reproduces what the eager pass would have
  // computed, bit-identically, but at the ICB-replay cost (the cold-start win).
  // Disarm after firing so the slot's warm replays don't re-trigger it.
  // Gated on trace_capture_skipped: the per-fire skip is Metal-only, so a
  // CPU/CUDA capture records with pending latched but every dispatch already
  // ran eagerly -- an unconditional replay here would execute the whole
  // recorded stream a SECOND time inside the capture call (harmless-looking
  // for pure kernels, corrupting for non-idempotent custom kernels).
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

// Multi-root variant: a JIT-captured function whose return is a LIST (or
// nested structure) of tensor handles -- e.g. TGrad over several params --
// must mark EVERY returned buffer as needed, else they're treated as dead
// (replay_skip + not pinned), reclaimed by the next step's arena, and read
// back as freed-memory garbage on the first call AND not re-dispatched on
// replay (issue #5).  The single-root end_with_result only covered a bare
// _TTerm return.  roots[] are the resolved result Terms (one per handle).
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
  // Keep the ops buffer allocated -- the slot can be reused for
  // another capture without re-malloc.
}

fn u32 jit_capture_op_count(u32 slot) {
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) return 0;
  return JIT_CAPTURES[slot].n_ops;
}

static int jit_metal_graph_replay_enabled(void) {
  char const *e = getenv("THVM_METAL_GRAPH_REPLAY");
  return e == NULL || e[0] != '0';
}

// THVM_JIT_TRACE_CAPTURE=1 (default OFF) -- build the replay ICB during/right-
// after capture and produce the FIRST result via an ICB replay, WITHOUT the
// eager per-kernel GPU dispatch the recording pass otherwise runs.
//
// thvm's capture is execute-and-record: kernel_fire_by_id records each (kid,
// in_buf_ids, out_buf_id) tuple, then eagerly dispatches it on a standalone
// command buffer (the slow ~4s-on-FLUX-velocity cold step).  Subsequent calls
// REPLAY the recorded stream through the batched Metal ICB (~0.6s).  The ICB is
// built by metal_graph_build purely from the recorded tuple + KERNELS[kid]'s
// lifted DAG (PSO compiled on demand) + the buffer table -- NONE of which needs
// the eager submit (the output buffer is allocated at fire time, before
// dispatch).  So under this gate the recording pass SKIPS the eager dispatch
// (kernel_fire_by_id, gated on jit_is_capturing()) and jit_capture_end_common
// runs one jit_replay after finalize to fill the result buffers via the ICB --
// the same path warm replays take, so the first result is bit-identical to the
// eager pass but pays only the ICB build + one batched submit.
//
// Default ON.  Only the Metal ICB path benefits (CPU/CUDA keep the eager pass,
// the per-fire skip is Metal-gated in kernel_fire_by_id) and it is
// eligibility-gated -- ICB-ineligible captures (arena-recycled / non-static /
// M3 setBarrier) fall back to the eager pass, so default-on is correctness-safe.
// Default-on (not env-gated-default-off) because WL SetEnvironment (the
// fxBoundMemory default) does NOT reliably reach this dylib's getenv, so an
// in-script default-on never fired.  THVM_JIT_TRACE_CAPTURE=0 in the PROCESS env
// (before launch) disables.  Not memoised (getenv per capture-begin, ~3/gen).
fn int jit_trace_capture_enabled(void) {
  char const *e = getenv("THVM_JIT_TRACE_CAPTURE");
  return (e == NULL || e[0] != '0');
}

// THVM_JIT_REPLAY_NOSKIP=1 -- disable the liveness-based replay_skip
// marking in jit_capture_finalize, so EVERY captured dispatch re-fires
// on replay even when its output isn't observably needed.  This is a
// measurement aid: the beautiful_mnist `forward` bench discards the
// loss TTerm, so the normal liveness pass marks all 25 dispatches
// replay_skip=1 and steady-state replay does zero GPU work (wall=0.0ms
// is pure WL overhead, not GPU compute).  With NOSKIP the replay path
// re-dispatches all 25 each step -> wall (after metal_dispatch_flush's
// waitUntilCompleted) reflects real GPU exec time.  Memoised.
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

// THVM_JIT_STATIC_PACK (default OFF): full static-liveness interval packing of
// EVERY capture-internal buffer, tinygrad's offline memory planner
// (engine/memory.py over ScheduleItems).  For a captured schedule liveness is
// STATIC -- a buffer written by op i and last READ by op j in the record is
// dead after j; the replay re-runs the record in order, so aliasing buffers
// with disjoint [def,lastuse] intervals is sound (per-op replay + the ICB
// setBarrier order the WAW).  This drops two conservative vetoes the default
// packer keeps for a *reachability*-based safety margin:
//   (1) the KDISPATCH_METAL_TILE-only packability gate -- the real Krea DiT
//       produces most of its 12.98MB block activations from NON-tile kernels
//       (RMSNorm reduces, attention, modulations, residual adds), which the
//       default packer leaves pinned (the census: 810 unpacked singles);
//   (2) the "no in-record reader -> keep pinned" veto (a dead-after-def write
//       is assumed possibly host-read).
// The WL semantic adopted under the gate is tinygrad's: you do NOT host-read a
// JIT capture's internals, so extern pins / heap-reachable cells are IGNORED
// for interval eligibility.  KreaGenerate sets the env for the gen; default off
// keeps every other JIT path byte-identical.  The correctness exclusions STAY:
// declared inputs + result roots (held), multi-writer victims, cross-capture
// retained bufs, ASSIGN/host-GATHER dsts, exact-size-only reuse + replay_zero.
static int jit_static_pack_enabled(void) {
  char const *e = getenv("THVM_JIT_STATIC_PACK");
  return e != NULL && e[0] == '1';
}

// THVM_JIT_GRAPH_FORCE_ICB (default OFF) -- batch a buffer-recycling capture
// (two live dispatches writing one physical buffer) into the ICB anyway,
// relying on the per-command [cmd setBarrier] in metal_graph_build to order the
// write-after-write.  tinygrad's MetalGraph (runtime/graph/metal.py:39) emits
// the same setBarrier, but tinygrad runs memory_plan_rewrite first so its
// recycled buffers are SLICEs into one arena whose accesses Apple's dependency
// tracker serializes; thvm recycles whole buf_ids and on Apple9 (M3) the ICB's
// setBarrier does NOT reliably order those write-after-write recycles across
// concurrent indirect commands -- the FLUX VAE collapse (two distinct latents
// decode to a byte-identical image: input rel-L2 1.22 -> output 0.0).  Per-op
// replay hazard-tracks each dispatch correctly AND keeps the packer's recycle
// (memory stays bounded), so a recycling stream declines the ICB by default.
// jit_capture_finalize sets metal_graph_recycle; the flux_jit_replay.wlt
// regression covers this.  THVM_JIT_GRAPH_FORCE_ICB=1 re-enables the batched
// ICB for measuring the (unsafe-on-M3) batched path.  Memoised.
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

// Count distinct UOP nodes reachable from a lifted-DAG root.  Used by
// jit_capture_export_ops to expose an "OpCount" surrogate (program[]
// has been retired; the DAG is now the only kernel representation).
// Bounded recursion via a small visited stack; the lifted DAG is
// finite.
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

// === Stable structural content hash of a lifted-DAG (Merkle-style) ===
//
// The Metal PSO cache and the in-memory tile-JIT cache key a kernel by
// its lifted UOp DAG.  Keying on the raw heap location of store_root is
// WRONG: hash-consing/GC relocates DAG nodes mid-session (the autotune
// BEAM search churns the shared heap), so a captured kernel's root loc
// silently changes and warm replay misses or collides on the PSO cache
// -> recompiles the wrong / no PSO -> non-finite output.
//
// This computes a GC-invariant content hash: hash(node) =
// mix(op, dtype, payload values, then hash of each child), folded
// recursively over the DAG.  Two structurally-identical kernels hash
// equal (they SHOULD share a PSO); the danger is two DIFFERENT kernels
// hashing equal (a real collision -> wrong PSO -> silent wrong
// results), so we cover everything that affects the generated MSL:
// op codes, leaf const bits + dtype, RANGE axis/type/extent (incl.
// kvar-bound extents), movement-op dims/perms/pads, REDUCE kind+axes,
// OPT kind+factor, CAST/BITCAST/COPY payload, buffer scope/dtype/shape,
// and child structure recursively.
//
// Per-call memoization (loc-keyed, generation-stamped) keeps it linear
// over the DAG even when shared subgraphs are referenced many times.

#define UOP_CONTENT_HASH_CACHE_CAP 8192    // power of two, > deepest DAG
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
  // table full -- caller falls back to recomputing this node (correct,
  // just slower); never wrong.
}

// Hash a non-UOP term (NUM / TEN / leaf) by its identity-bearing bits:
// tag + ext (dtype/opcode) + val (raw bits / id).  These are
// GC-stable: NUM bits, TEN tensor-id, etc. don't move under GC.
static inline u64 uop_content_hash_atom(u64 h, Term t) {
  h = uop_content_mix(h, (u64)term_tag(t));
  h = uop_content_mix(h, (u64)term_ext(t));
  h = uop_content_mix(h, (u64)term_val(t));
  return h;
}

static u64 uop_content_hash_rec(Term t, u32 depth) {
  if (depth > 512) return 0xDEADBEEFULL;          // pathological guard
  if (term_tag(t) != TAG_UOP) {
    // Non-UOP leaf reached via a child slot (NUM payload, TEN handle,
    // ERA sentinel, ...).  Hash its identity-bearing bits.
    return uop_content_hash_atom(0xcbf29ce484222325ULL, t);
  }

  u64 loc = term_val(t);
  u64 cached;
  if (uop_content_cache_lookup(loc, &cached)) return cached;

  u32 op = term_ext(t);
  // Seed with the opcode AND the term ext (dtype lives in ext for some
  // ops; opcode == ext for UOP, so this also seeds the op).
  u64 h = 0xcbf29ce484222325ULL;
  h = uop_content_mix(h, 0x55554F50ULL);          // "UOP" domain tag
  h = uop_content_mix(h, (u64)op);

  switch (op) {
    // --- leaves: hash all distinguishing NUM payload directly -------
    case UOP_CONST: {
      // [NUM(bits)]; ext of the wrapping NUM carries the dtype.
      Term num = heap_read(loc + 0);
      h = uop_content_hash_atom(h, num);
      break;
    }
    case UOP_RANGE: {
      // [NUM(axis_id), NUM(axis_type), NUM(extent)].  The extent may be
      // a kvar-bound token (kvar_extent_is_var); we hash the raw extent
      // value, which is the stable token, so symbolic kernels at
      // different BS still hash identically (the var id is in the token).
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

    // --- opt directive: kind + factor + the wrapped target ----------
    case UOP_OPT: {
      h = uop_content_mix(h, (u64)uop_opt_kind(t));
      h = uop_content_mix(h, (u64)uop_opt_factor(t));
      h = uop_content_mix(h, uop_content_hash_rec(uop_opt_target(t), depth + 1));
      break;
    }

    // --- reduce: kind + each axis + the reduced source --------------
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

    // --- bufferize: addrspace/removable/n_ranges + value + ranges ---
    case UOP_BUFFERIZE: {
      h = uop_content_mix(h, (u64)uop_bufferize_addrspace(t));
      // removable lives at heap loc+2; hash it raw for completeness.
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

    // --- movement ops [src, NUM(ndim), payload NUMs...] -------------
    // RESHAPE/EXPAND/PERMUTE carry `ndim` payload NUMs; PAD/SHRINK
    // carry `2*ndim`.  We read ndim from loc+1 and hash the right span.
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

    // --- single-source ops with one trailing payload NUM ------------
    // FLIP [src, NUM(mask)]; CAST/BITCAST [src, NUM(dst_dtype)];
    // COPY [src, NUM(device+1)].
    case UOP_FLIP: case UOP_CAST: case UOP_BITCAST: case UOP_COPY: {
      h = uop_content_mix(h, uop_content_hash_rec(heap_read(loc + 0), depth + 1));
      h = uop_content_hash_atom(h, heap_read(loc + 1));
      break;
    }

    // --- indexed ops: Term children, then the NUM payload -----------
    // GATHER [src, idx, NUM(dim)]; SCATTER [base, idx, src, NUM(dim),
    // NUM(kind), NUM(include_self)].  The generic descent walks only the
    // children, so a gather along another axis or a scatter of another
    // kind would hash equal.
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

    // --- UOP_AFTER [node, after_node]: ordering between sibling -----
    // side-effects in a multi-store kernel.  uop_arity returns 0 for
    // AFTER (it's a boundary-ish marker for the generic rewriter), so
    // the default fixed-arity descent would miss both children -- hash
    // them explicitly to stay collision-safe on multi-store DAGs.
    case UOP_AFTER: {
      h = uop_content_mix(h, uop_content_hash_rec(heap_read(loc + 0), depth + 1));
      h = uop_content_mix(h, uop_content_hash_rec(heap_read(loc + 1), depth + 1));
      break;
    }

    // --- generic fixed-arity ops: recurse every Term child ----------
    // ADD/MUL/CMP/NEG/RECIP/EXP2/LOG2/SQRT/LOAD/DETACH, the integer ALU
    // (IADD..ISHR/ILT), INDEX_E, IWHERE, STORE, ASSIGN, ...
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

// Public entry: GC-invariant structural content hash of the UOp DAG
// rooted at `root`.  Returns 0 for an empty root.  Bumps the per-call
// memo generation so prior cache entries become invisible without a
// memset (the heap may have been relocated between calls).
u64 uop_dag_content_hash(Term root) {
  if (root == 0) return 0;
  UOP_CONTENT_HASH_GEN++;
  if (UOP_CONTENT_HASH_GEN == 0) UOP_CONTENT_HASH_GEN = 1;  // skip sentinel
  return uop_content_hash_rec(root, 0);
}

// Export the capture sequence as a flat table for WL-side profiling.
// Header: {n_ops, row_width}.  Row width is JIT_CAPTURE_EXPORT_ROW_WIDTH:
// {kind, kid, dispatch_kind, n_inputs, out_buf_id, input0, input1,
//  program_key, output_numel, n_ops, _unused, _unused2,
//  assign_dst_tid, assign_src_tid, replay_skip, replay_packed}.
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

// Called from kernel_fire_by_id just before the actual dispatch when
// JIT_ACTIVE_SLOT != 0.  Records the (kid, in_buf_ids, out_buf_id)
// tuple so jit_replay can re-dispatch the SAME work without a new
// materialize pass.
// Grow the active capture's ops array to hold at least `need` entries (2x
// doubling, zeroing the new tail so a fresh row starts clean).  Returns 0 on
// OOM or the hard cap (caller drops the record).  The append sites take their
// &c->ops[n] pointer AFTER this call, so a realloc here never dangles.
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
  // Capture-recycled output: the replay must zero the slot before this
  // dispatch (the pop-time gpu_zero's replay mirror).
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
  // Own every dispatched buffer for the capture's lifetime.
  jit_capture_retain_dispatch_bufs(c, op);
}

// Called from interact_assign_with just before the memcpy when
// JIT_ACTIVE_SLOT != 0.  Records the (src_tid, dst_tid) so replay can
// re-do the memcpy without needing to walk the heap or fire WNF.
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
  // ASSIGN endpoints are replayed against the LIVE TenDescs (weights /
  // KV caches) -- retained so they survive to every replay.
  jit_capture_retain_tensor_buf(c, dst_tid);
  jit_capture_retain_tensor_buf(c, src_tid);
}

// Called from materialize_root_alias right after a host-side strided
// gather, when the SOURCE buffer is volatile under replay (a kernel
// output that re-fires, or an append-mutated KV cache).  Records the
// (src buf, src view, dst buf) so jit_replay re-runs the same gather off
// the freshly produced source bytes each step.  `src` is the alias
// TenDesc (its view + chain describe how to read the underlying buffer);
// `dst_buf` is the contiguous gather destination.
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
      op->gather_nviews = 0;   // OOM: degrade to single-view (still correct
                               // for the nviews==0 broadcast case)
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

// A recorded dispatch whose kid carries a raw injected program
// (KernelEntry.custom_* -- kernel_emit_custom_program).  Opaque to
// every liveness / packing / dedup heuristic: its explicit outs set
// says which argument slots it WRITES (any of which may escape the
// capture), so the planner must treat the op as always-live and every
// argument buffer as held.  Mirrors tinygrad codegen/__init__.py
// do_to_program: a raw PROGRAM's buffers are conservatively all
// read+written, else graph replay recycles its scratch mid-step.
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
  // The producer must write a contiguous row-major fill covering exactly
  // output_numel positions in its output buffer.  We approximate that by
  // walking cached_lift.store_root and verifying the product of its
  // non-reduce axes' extents equals output_numel: a METAL_TILE kernel
  // whose lifted DAG addresses each output element exactly once via the
  // standard rangeified addr expression.  uop_dag_collect_axes returns
  // every UOP_RANGE leaf in the DAG; we skip KAX_REDUCE axes (those
  // appear in the reduce body, not the output addressing).
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
      // A replayable gather re-reads its source every step: sinking a
      // dispatch it reads into an assign's buffer would leave it replaying
      // the capture step's bytes.
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

// Logical output size of a recorded dispatch, derived from the KernelEntry
// (dtype x numel) -- stable at finalize even after the eager buffer was freed
// (the replay packer sizes its slots from this; the eager alloc at fire time
// used the same formula, so exact-size reuse matches eager layout).
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

// Drop ONE buffer from this slot's retained set: unpin it, decref it, and
// compact it out of c->retained.  Used by the replay memory planner when it
// recycles a packed-away output buffer (jit_capture_pack_replay_temporaries):
// the recording pinned every dispatch output, but a buffer the packer redirects
// every reference off is no longer needed by the capture, so its pin must be
// released.  Crucially the UNPIN must precede the decref: a still-pinned buffer
// decref'd to refcount 0 is refused by the freelist and HARD-FREED (the slot is
// vacated, then immediately reused by the next alloc WITHOUT the freelist's
// memset-on-pop) -- across two sibling captures in one context that recycles a
// buf_id under a buffer a later replay reads (the FLUX qwJit->velJit velocity
// collapse).  Unpinning first lets the decref park the buffer on the freelist,
// where the slot stays reserved until a same-size alloc pops it with a clean
// memset -- per-capture buffer ownership preserved, memory bound intact.
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
  // Not in the retained set (e.g. the buffer was the capture result root, or a
  // duplicate already removed): still drop the pin + ref the packer no longer
  // needs, so it can be freelisted cleanly rather than hard-freed.
  if (b->buf_jit_unpin != NULL) b->buf_jit_unpin(buf_id);
  if (b->buf_decref != NULL)    b->buf_decref(buf_id);
}

// Release the LIVE-HEAP reference on a packed-away recording intermediate.
//
// thvm executes the cold forward eagerly: every kernel output becomes a live
// TenDesc in TENS holding a backend buffer ref (TenDesc lifetime decoupled from
// the buffer's via tensor/release.c).  When the replay memory planner packs an
// op's output `old_out` onto a reused slot `new_out`, it redirects every replay
// reference and drops the CAPTURE's pin+ref -- but the recording-time TenDesc
// at KERNELS[op->kid].output_tid still pins `old_out` at refcount 1, so the
// buffer never frees and the cold capture's whole intermediate set stays
// resident (the FLUX 15.9GB-at-256 footprint).  That recording intermediate is
// dead: replay writes `new_out`, and no live tensor outside the capture reads a
// packed intermediate (the result root is excluded from packing).  So drop the
// heap's hold the same way tensor_release does at refcount 0 -- decref the
// buffer and clear the descriptor's buf_id so a later tensor_release of that
// TenDesc won't double-decref.  Mirrors tinygrad's planner freeing the
// originals once buffers become arena SLICEs (schedule/memory.py:51-59); here
// the eager-then-capture model needs the heap ref dropped explicitly.
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
  // Only release if this descriptor still owns the packed-away buffer on this
  // backend (a view-alias may have already migrated it).
  if (td->backend != b || td->buf_id != old_out) {
    return;
  }
  // Drop the heap's premature buffer hold (decref parks old_out on the recycle
  // freelist if this was its last ref), then clear ONLY the descriptor's
  // buf_id so a later tensor_release of this TenDesc won't double-decref the
  // buffer.  KEEP td->backend: jit_replay's DISPATCH path reads
  // TENS[ke->output_tid].backend to find the backend vtable (capture.c:2676)
  // and writes to the redirected op->out_buf_id (capture.c:2686) -- it never
  // reads td->buf_id for the output -- so nulling backend would make replay
  // skip the dispatch and leave the output buffer unwritten (garbage replay).
  // Freelisting (not hard-freeing) old_out is safe: the packer redirected
  // every reference off it and it is never a slot's canonical reuse target, so
  // the captured stream no longer names old_out; a replay fresh-alloc that
  // pops the slot reuses memory the capture genuinely abandoned.
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
      // A replayable gather re-reads gather_src_buf each step.  The replay
      // memory planner MUST see this read or it recycles the source slot
      // before the gather fires (the FLUX VAE conv_shortcut over a TRealize'd
      // up-block output: src buffer reused, gather reads corrupt bytes).
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
      // The gather writes its contiguous dst buffer on every replay; the
      // planner must treat that as a write so it neither recycles the dst's
      // slot out from under a downstream reader nor reuses the dst for an
      // earlier buffer whose lifetime overlaps the gather.
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
      // A gather re-reads old_buf_id as its source: redirect it to the
      // recycled slot too, or the gather replays off the freed buffer.
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
  // Only pack genuine GPU-kernel-produced intermediates whose storage is
  // recycle-safe: a Metal tile dispatch or a CUDA nvrtc dispatch on its
  // own backend.  (CPU JIT already frees transients during materialize,
  // so it doesn't need this; CPU stays on its existing path.)
  //
  // THVM_JIT_STATIC_PACK: accept EVERY GPU dispatch output on its own backend,
  // not just the tile-JIT kind -- the real DiT's block activations come from
  // non-tile kernels (norm reduces, attention, elementwise) too, and static
  // record-order intervals make them just as recycle-safe (exact-size reuse +
  // replay_zero on the packed slot).  Restricting to the tile kind is what
  // leaves the census's 810 non-tile activations pinned.
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
    // No later reader in the record.  Default: keep pinned (the buffer might be
    // host-read -- a WL anonymous-builder reading a JIT internal).  Under
    // THVM_JIT_STATIC_PACK we adopt tinygrad's "no host-read of JIT internals"
    // semantic: a dead-after-def write is aliasable (interval [idx,idx]) -- as
    // a victim it redirects onto an earlier dead slot (the write is dead), as a
    // target its slot is free for any op after idx.  The result roots + declared
    // inputs are already excluded by `held`, so this only reaches genuine dead
    // internals.
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
  // The replay memory planner (tinygrad's memory_plan_rewrite,
  // schedule/memory.py): reuse a dead-lifetime temporary buffer for a later op
  // of the same size+backend so the capture's resident set is bounded to its
  // live working set, not the sum of every intermediate.  For the FLUX velocity
  // net this collapses ~6.75GB of 25-block activations to ~one block's worth --
  // the lever for a <10GB full-forward.  The reuse makes two ops write one
  // physical buffer; the per-command [cmd setBarrier] in metal_graph_build
  // orders that WAW so the batched ICB stays correct (confirmed on M3 by
  // flux_jit_replay.wlt).  Runs for with-result captures too by default;
  // THVM_JIT_PACK_RESULT=0 disables.
  //
  // `held` is the tinygrad memory_plan_rewrite `held_bufs` set (memory.py:13
  // _can_plan): buffers that must NEVER be recycled -- the capture's result
  // roots and its declared replay inputs.  A held buffer escapes the capture
  // (the WL/py caller reads the root after replay; an input is re-substituted
  // each replay), so packing it away (redirecting its producer to another
  // slot) or recycling its slot for a later op corrupts the value a downstream
  // reader -- INCLUDING ops still inside this same capture -- depends on.  The
  // FLUX VAE decode is a with-result capture whose root-feeding intermediates
  // were being recycled, so warm replay (gen2+) read a slot a later op had
  // already overwritten -> NaN image; gen1 is the eager pass and never touches
  // the packed assignment, so it masked the bug.
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
  // THVM_JIT_PACK_DEBUG (gated, like THVM_JIT_REPLAY_TRACE): one summary line
  // per finalized capture -- packable intermediates, how many the planner
  // recycled, and the bytes reclaimed.  The recording-buffer footprint a cold
  // capture pins is exactly (packable - packed) plus the result roots, so this
  // is the first number to read when a capture's resident set is too large.
  int pack_dbg = getenv("THVM_JIT_PACK_DEBUG") != NULL;
  u64 dbg_n_packable = 0, dbg_n_packed = 0;
  u64 dbg_bytes_packable = 0, dbg_bytes_packed = 0;
  // Multi-writer exclusion: an out_buf_id written by MORE THAN ONE live
  // dispatch (the capture-recycle pool's same-size reuse, or a scheduler/arena
  // recycle baked into the stream) must never be a pack VICTIM.
  // jit_capture_replay_lifetime already rejects an op with a LATER writer of
  // the same id; without this earlier-writer check the id's LAST writer would
  // pass the lifetime test, get packed away, and the pin release below would
  // unpin the id while the EARLIER lifetime's ops still reference it -- the
  // post-capture rollback then frees a stream-referenced buffer.
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
    // tinygrad _can_plan (memory.py:13): a held buffer (result root or declared
    // replay input) is neither recycled nor packed away.  Skipping it as a
    // VICTIM here keeps it in its own slot (never redirected) AND keeps it out
    // of `slots[]`, so no later op recycles it as a TARGET either.
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
      // EXACT-size reuse only.  A larger dead slot reused for a smaller output
      // leaves stale bytes in the slot tail [nbytes, slots[s].nbytes): the new
      // kernel writes only its own `nbytes`, so the [nbytes, slot) tail keeps
      // the previous occupant's data.  thvm's eager-then-replay model never
      // re-zeros a directly-reused slot (unlike metal_buf_freelist_try_pop,
      // which memsets on pop AND already matches size EXACTLY -- _.m:688), so a
      // consumer that reads past the new output's numel sees that stale tail.
      // The FLUX.2 VAE fused conv (TConv2DIm2ColPool, Conv.wl) reads its
      // operands through strided _pool-unfold + broadcast EXPAND views whose
      // max-reachable byte index runs into the tail; on warm replay a 512KB
      // f32 slot reused for a 128KB bf16 output exposed 384KB of stale f32
      // bytes reinterpreted as bf16 -> inf/NaN -> the all-NaN decoded image.
      // The GEMM lowering is immune only because it realizes a fully-written
      // contiguous im2col.  Exact-size reuse keeps the velocity-net's
      // same-shape block-activation collapse (the actual footprint lever)
      // while never handing a view a slot longer than its own logical numel --
      // matching tinygrad's planner, which slices each buffer into the shared
      // arena as a SLICE of its own exact `buf.arg` extent (schedule/memory.py
      // build_replace_map, :57-59), never handing a buffer a region wider than
      // its logical numel.
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
    // Release old_out's recording-time JIT pin BEFORE the decref.  The packer
    // redirected every live reference off old_out, so the capture no longer
    // needs it -- but it was jit_pinned by jit_capture_retain_dispatch_bufs at
    // record time.  A jit_pinned buffer decref'd to refcount 0 is REFUSED by
    // the freelist (metal_buf_freelist_push_impl skips pinned) and HARD-FREED
    // instead: metal_buf_free nils the MTLBuffer and vacates the slot, so the
    // very next metal_buf_alloc / wrap_external reuses that buf_id for a fresh
    // (different) buffer WITHOUT the freelist's memset-on-pop.  Across two
    // sibling captures in one context (FLUX qwJit then velJit) that recycled
    // buf_id lands under a buffer a later replay reads, so the replay diverges
    // from the capture (the velocity-net collapse).  Unpinning first lets the
    // decref land old_out on the freelist, where it stays parked (slot NOT
    // reused) until a same-size alloc pops it with a clean memset -- safe
    // reuse, per-capture buffer ownership preserved, memory bound intact.
    jit_capture_release_one_retained(c, backend, old_out);
    // Drop the recording intermediate's LIVE-HEAP buffer ref too, or the cold
    // capture's whole intermediate set stays resident (see helper comment).
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
  // `has_result` mirrors the old single-Term `root != 0` flag: the
  // sink-assigns / pack-temporaries passes below only run for the
  // no-result (jit_capture_end) case.
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
        // Raw injected program: out_buf_id names only the FIRST written
        // slot; the remaining outs-mask writes live in in_buf_ids and
        // any of them may escape (a multi-out scratch chain, a caller
        // reading a written argument after replay).  Always replay.
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
        // Live only if the gathered dst is needed downstream; if so, its
        // SOURCE buffer must re-fire too, so add it to `needed` (reconnects
        // the liveness chain the eager one-shot gather would have severed).
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
  // Build the tinygrad memory_plan_rewrite `held_bufs` set (memory.py:13): the
  // capture's result-root buffers plus its declared replay inputs.  These
  // escape the capture and must never be recycled or packed away by the
  // planner.  The root bufs are exactly the ones jit_capture_root_needed seeds
  // BEFORE the reverse-liveness loop pollutes `needed` with intermediates, so
  // re-derive them cleanly here.
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
        // A declared input's backend is whichever op reads it; record under
        // every backend a captured op uses it on (cheap -- bufref dedups).
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
    // Raw injected programs: hold EVERY argument buffer (the outs-mask
    // writes AND the reads) so the packer neither recycles a
    // custom-referenced scratch nor packs a custom output away.  The
    // memory planner cannot see inside the opaque kernel body; this is
    // thvm's version of tinygrad's conservative all-read+written
    // treatment for raw PROGRAM ops (codegen/__init__.py do_to_program).
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

  // Build the set of buffers the live (non-skipped) ops still need, then
  // release only the recording-time retained buffers that DROPPED OUT of that
  // set (skipped/dead).  A buffer still needed keeps its original incref +
  // jit_pin across the boundary -- it is never decref'd to refcount 0, so a
  // ~100MB FLUX weight held only by the capture (its transient realize tensor
  // already rolled back) cannot be hard-freed here and re-pinned as nil.  See
  // jit_capture_release_retained_except.
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
    jit_capture_release_retained(c);   // OOM: fall back to the unconditional
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
  // SSA-rename recycled Metal dispatch outputs: the SECOND+ writer of a buffer
  // gets a fresh same-size buffer so each physical buffer is written exactly
  // once, eliminating within-batch write-after-write on a shared buffer.
  //
  // OBSOLETE by default (THVM_JIT_GRAPH_RENAME=1 to re-enable for debugging).
  // Its sole purpose was avoiding double-writes the ICB couldn't order -- but
  // the per-command [cmd setBarrier] in metal_graph_build DOES order them
  // (tinygrad's MetalGraph invariant, confirmed on this M3 by
  // flux_jit_replay.wlt with rename off).  Renaming also FIGHTS the packer
  // (jit_capture_pack_replay_temporaries above): it fresh-allocates the very
  // buffers the packer just reused, undoing the memory bound.  With the
  // packer as the memory planner + setBarrier ordering the recycle, the
  // rename is pure overhead.  Pure-DISPATCH captures only.
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
      // The actual result roots (just the returned tensors' buf_ids, NOT the
      // transitive `needed` closure the liveness pass built above).  These name
      // the live results and must never be renamed; every other recycled buffer
      // is fair game.
      JitCaptureBufRef *root_bufs = (JitCaptureBufRef *)calloc(
          JIT_CAPTURE_RETAIN_CAP, sizeof(JitCaptureBufRef));
      u32 n_root_bufs = 0;
      if (root_bufs != NULL) {
        for (u32 ri = 0; ri < n_roots; ri++) {
          jit_capture_root_needed(roots[ri], root_bufs, &n_root_bufs);
        }
      }
      // last_op[ob] = index of the LAST live dispatch writing ob.  The last
      // write keeps the original buffer; only EARLIER writes get renamed.
      // cur[ob] = the buffer the latest write to ob landed in.
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
          // Rename every write that is NOT the last write of ob, so each
          // physical buffer is written exactly once (the last write keeps the
          // original buffer -- it is the live result the tensor handle names).
          // A buffer written only once has last_op == this index -> not renamed.
          // This holds for ROOT buffers too: only the LAST write to a root is
          // the result WL reads; earlier writes are intermediates whose reads
          // are redirected to the fresh buffer via cur[] below, while the last
          // write keeps the root's buf_id (last_op[ob]==i+1 -> not renamed).
          // Renaming a root's non-last writes is what lets the FLUX multi-root
          // double block (img/txt sharing the joint-attention sub-DAG, each
          // gated-residual-written more than once) replay through the ICB
          // instead of tripping the recycle hazard below.
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

  // Mark the Metal ICB replay path unsafe if any output buf_id is written by
  // two live dispatches (the scheduler recycled it across the captured stream).
  // After the rename pass above this should no longer trip for pure-DISPATCH
  // Metal captures.  O(n^2) over live dispatches, but n is the per-step kernel
  // count (~hundreds) and this runs once at capture.
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

  // Decline the ICB when a live DAG-path (non-tile) dispatch has a strided-view
  // input.  The per-op DAG encoder (metal_dispatch_kernel) PRE-MATERIALISES such
  // an input into a contiguous temp via a CPU gather before the kernel reads it;
  // the ICB encodes the raw input buffer and skips that gather, so a DAG kernel
  // would read the strided bytes as contiguous -> garbage.
  //
  // Generated-tile kernels are EXEMPT: the tile path (metal_tile_jit_encode and
  // the ICB's own metal_graph_build) binds the ORIGINAL strided buffer and BAKES
  // the view strides into the kernel's address expressions, so a strided input is
  // resolved in-kernel -- identical whether dispatched per-op or via the ICB.  The
  // ICB run loop (jit_replay_try_metal_graph_run) only ever batches
  // KDISPATCH_METAL_TILE kernels and breaks on any other kind, so a strided input
  // on a DAG-path op already declines the ICB run for that op.  Scoping this flag
  // to non-tile dispatches keeps every captured FLUX block (whose attention /
  // concat / RoPE views all feed tile-path matmuls) ICB-eligible instead of
  // declining the whole capture to per-op dispatch (the warm-replay lever:
  // ~7x on the 4-step velocity net).  Viewless recycling streams stay ICB-eligible
  // via the [cmd setBarrier] WAW ordering (the metal_graph_recycle path above).
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
// THVM_METAL_SERIAL_REPLAY (default ON; =0 disables) -- replay a captured
// dispatch run through ONE serial compute encoder (thvm_metal_serial_run)
// instead of per-op dispatch.  Serial dispatch order makes the WAW/RAW chain
// correct by construction, so unlike the ICB this path needs neither the
// metal_graph_recycle nor the strided-view (metal_graph_unsafe) declines:
// tile kernels bake views in-kernel and custom kernels read raw buffers.
// Ops the serial encoder cannot carry break the run and fall to per-op:
// MPS / DAG-route kernels (pre-materialise or self-encode) and
// replay_packed/replay_zero ops (their blit zero-fill needs its own encoder).
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
    // Packer/capture-recycled slot: zero it on the encoder before the
    // dispatch (the per-op path's b->buf_zero mirror).
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
  // Below a few encodes the flush + commit + wait overhead beats per-op.
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
  // THVM_JIT_GRAPH_DIAG: report once per slot (at its first op) whether the
  // capture is ICB-batch-eligible and, if not, why -- the VAE-batching probe.
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
  // Strided-view captures are genuinely ICB-unsafe (the per-op path
  // pre-materialises the view; the ICB can't) -- always per-op.
  if (c->metal_graph_unsafe) {
    return 0;
  }
  // A buffer-recycling stream (two ICB commands writing one physical buffer)
  // is ordered by the per-command [cmd setBarrier] in metal_graph_build --
  // tinygrad's MetalGraph invariant.  Keep the batched ICB by default; only
  // fall to per-op when THVM_JIT_GRAPH_FORCE_ICB=0.
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
  // Recycled-slot zero-fill (see the per-op DISPATCH path in jit_replay): a
  // packer-recycled output (replay_packed) that a partial-write kernel only
  // covers in part must be cleared before the batch fires, or the unwritten
  // region keeps the prior occupant's stale bytes.  The blit fillBuffer is
  // hazard-ordered before the ICB's dispatches that write the same slot.
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

// Declare the buf_ids the JIT-input tensors carried at capture time.
// Called by the WL/py TinyJit wrapper right after jit_capture_end with
// the (realized) argument tensors' buf_ids in argument order.  These are
// the baseline ids jit_replay_with_inputs substitutes away on each
// replay -- the tinygrad input_replace baseline (engine/jit.py:124).
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

// Build the input-rebind site table (tinygrad input_replace).  Walks
// every captured op once and records each (op, input-position) -- and
// each dispatch out_buf_id -- whose buf_id matches a declared input.
// Lazy + cached: only runs on the first jit_replay_with_inputs.
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
      // A regather over a JIT-declared input buffer must rebind its src
      // (and dst) to the fresh per-call buffer, the same way a dispatch
      // input does -- otherwise the regather re-reads the capture-time
      // input on every replay.
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
      // A JIT input used directly as a kernel output (rare, e.g. an
      // in-place ASSIGN target captured as a dispatch out) also needs
      // rebinding so the fresh call writes the fresh buffer.
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
  // THVM_JIT_SITES_TRACE=1: per declared JIT input, how many rebind sites
  // were found.  A declared input with 0 sites is NOT rebound on replay --
  // it silently keeps its capture-time bytes.  The usual cause: the input
  // is a lazy / non-contiguous tensor that a kernel stages into an internal
  // copy whose tid differs from the declared input, so input_replace finds
  // no matching buf_id (realize the input contiguous to fix).
  if (getenv("THVM_JIT_SITES_TRACE") != NULL) {
    fprintf(stderr, "[jit-sites] n_inputs_decl=%u n_sites=%u n_ops=%u\n",
            c->n_inputs_decl, c->n_sites, c->n_ops);
    for (u32 k = 0; k < c->n_inputs_decl; k++) {
      u32 sc = 0;
      for (u32 s = 0; s < c->n_sites; s++) if (c->sites[s].slot_index == k) sc++;
      fprintf(stderr, "  decl[%u] buf=%u -> %u site(s):", k, c->input_buf_ids[k], sc);
      for (u32 s = 0; s < c->n_sites; s++)
        if (c->sites[s].slot_index == k)
          fprintf(stderr, " op%u/pos%d", c->sites[s].op_index, (int)c->sites[s].pos);
      fprintf(stderr, "\n");
    }
  }
}

// Rebind the captured input sites to this call's fresh input buffers,
// then replay.  `new_ids[k]` is the buf_id of the k-th JIT-input tensor
// for the CURRENT call; we overwrite every recorded site that reads
// input-slot k with new_ids[k] before dispatch.  Idempotent across
// calls: sites are positions (op, pos), never the stale buf_ids
// themselves, so each replay overwrites from scratch.  This is thvm's
// port of tinygrad's resolve_params substitution (engine/jit.py:126).
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
  // Invalidate the cached cuGraph when this call's input DEVICE ADDRESSES
  // differ from the ones it was baked with -- the cuGraph cache-hit ignores
  // the per-op substitution above and would replay stale baked buffers
  // (batched Qwen/VAE warm collapse; velocity-net cold!=warm).  Compare dptr,
  // NOT buf_id: across a call boundary the freelist recycles a freed input's
  // buf_id, so a fresh input can carry the baked id while sitting at a new
  // dptr -- a buf_id match then wrongly keeps the stale graph.  An input whose
  // buffer is reused + content-updated in place (velocity-net z within one
  // sampler loop) keeps the same dptr -> no invalidate -> fast cuGraph hit.
  if (new_ids != NULL && n > 0) {
    int diag = getenv("THVM_JIT_GRAPH_DIAG") != NULL;
    if (c->graph_input_addrs != NULL) {
      int diff = (c->graph_input_n != n);
      for (u32 i = 0; i < n; i++) {
        u64 a = cuda_buf_addr(new_ids[i]);
        if (i < c->graph_input_n) {
          int moved = (a != c->graph_input_addrs[i]);
          if (moved) diff = 1;
          // Only trace the rare MOVED case -- a per-input "same" line on every
          // jit_replay_with_inputs call floods stderr across a diffusion loop.
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

// Walk the captured sequence and re-dispatch each KernelEntry against
// the recorded (in_buf_ids, out_buf_id).  Caller is responsible for
// ensuring the input buffers carry fresh data (typically via TSet
// into pre-allocated input TenDescs between calls).  Returns the
// number of ops dispatched, or 0 on a no-op slot.
fn u32 jit_replay(u32 slot) {
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) return 0;
  JitCapture *c = &JIT_CAPTURES[slot];
  if (!c->in_use)    return 0;
  if (c->n_ops == 0) return 0;
  HOT_JIT_REPLAY_CALLS++;
  backend_dispatch_begin_all();
  int trace = getenv("THVM_JIT_REPLAY_TRACE") != NULL;
  if (trace) fprintf(stderr, "[replay] start n_ops=%u\n", c->n_ops);

#ifdef THVM_HAS_CUDA
  // CUDA Graph fast path: if every op is a CUDA-backend dispatch/copy
  // and the slot already has a cached CUgraphExec, cuGraphLaunch it
  // instead of looping over per-op cuLaunchKernel.  Cache miss: capture
  // this replay's per-op work into a graph, then launch.  See
  // cuda_jit_graph_replay / cuda_jit_graph_capture_begin in backend/cuda/jit.c.
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
    if (trace) fprintf(stderr, "[replay] CUDA graph hit (n_ops=%u)\n", c->n_ops);
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
    // Cache miss -- capture into a CUgraph as we go through the op loop.
    if (cuda_jit_graph_capture_begin() == 0) {
      cuda_capturing = 1;
      if (trace) fprintf(stderr, "[replay] CUDA graph capturing slot=%u\n", slot);
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
        if (trace) fprintf(stderr, "[replay] op%u DISPATCH kid=%u out_buf=%u\n",
                           i, op->kid, op->out_buf_id);
        if (op->kid == 0 || op->kid >= KERNELS_NEXT) {
          if (trace) fprintf(stderr, "[replay]   skip: invalid kid\n");
          i++;
          continue;
        }
        KernelEntry *ke = &KERNELS[op->kid];
        Backend *b = TENS[ke->output_tid].backend;
        if (b == NULL || b->dispatch_kernel == NULL) {
          if (trace) fprintf(stderr, "[replay]   skip: ke->output_tid=%u backend=%p\n",
                             ke->output_tid, (void*)b);
          i++;
          continue;
        }
        u32 *ids = op->heap_in_buf_ids != NULL
                 ? op->heap_in_buf_ids
                 : op->in_buf_ids;
        // Recycled-slot zero-fill: a packer-recycled output buffer
        // (replay_packed -- jit_capture_pack_replay_temporaries reused a
        // dead-lifetime slot for this op) may be written only partially by a
        // kernel whose store covers fewer elements than the slot holds (a
        // windowed/strided store, e.g. the FLUX VAE up-block op).  In the eager
        // capture pass the output was a FRESH zero buffer, so the unwritten
        // region read as zeros; on warm replay the recycled slot still holds
        // the PRIOR occupant's bytes (a different tensor, possibly a different
        // dtype -- f32 bytes reinterpreted as bf16 are NaN), and a downstream
        // op that reads the full buffer then sees garbage -> the NaN decoded
        // image on gen2+.  Zero the slot first (GPU blit fill, hazard-ordered
        // before this dispatch, no host sync), mirroring
        // metal_buf_freelist_try_pop's memset-on-pop -- the recycle-zero
        // invariant the direct packer reuse otherwise skipped.
        if ((op->replay_packed || op->replay_zero) && b->buf_zero != NULL) {
          b->buf_zero(op->out_buf_id);
        }
        int rc = b->dispatch_kernel(ke, ids, op->out_buf_id);
        if (trace) fprintf(stderr, "[replay]   dispatch rc=%d\n", rc);
        if (rc == 0) {
          HOT_KERNEL_FIRES++;
          HOT_JIT_REPLAY_DISPATCHES++;
          ITRS++;
        }
        break;
      }
      case JIT_OP_ASSIGN: {
        HOT_JIT_REPLAY_ASSIGNS++;
        u32 dst = op->assign_dst_tid, src = op->assign_src_tid;
        if (trace) fprintf(stderr, "[replay] op%u ASSIGN dst_tid=%u src_tid=%u\n",
                           i, dst, src);
        if (dst == 0 || dst >= TENS_NEXT) break;
        if (src == 0 || src >= TENS_NEXT) break;
        if (trace) fprintf(stderr, "[replay]   dst_buf=%u src_buf=%u\n",
                           TENS[dst].buf_id, TENS[src].buf_id);
        TenDesc *dd = &TENS[dst], *sd = &TENS[src];
        if (dd->backend == NULL || dd->backend != sd->backend) break;
        if (sd->view.numel != dd->view.numel) break;
        // Offset-aware write (mirrors interact_assign_with).  A KV-cache append
        // dst (assign_kvar_id != 0) re-resolves its row from the LIVE kvar at
        // FIRE -- so a step captured at start_pos=2 replays at the rebound
        // start_pos=3, landing at row 3 (the offset baked into view.offset at
        // capture is only that one step's row).  Element offset = row *
        // leading-stride.  A plain weight ASSIGN (assign_kvar_id 0, view.offset
        // 0) writes the whole buffer at 0.
        u64 off_elems = (dd->assign_kvar_id != 0)
            ? (u64)kvar_runtime(dd->assign_kvar_id) * (u32)dd->view.strides[0]
            : (u64)(u32)dd->view.offset;
        if (tensor_assign(dst, src, off_elems) == 0) {
          ITRS++;
        }
        break;
      }
      case JIT_OP_GATHER: {
        // Re-run the host-side strided gather off the LIVE source bytes
        // (its producer kernel re-fired earlier in this replay, or the KV
        // cache was just re-appended) into the contiguous dst buffer.
        Backend *b = (Backend *)op->gather_backend;
        if (b == NULL || b->buf_read == NULL || b->buf_write == NULL) break;
        // The gather is a pure strided byte permutation (dst[k] = src[strided(k)]),
        // so it is valid for ANY dtype whose itemsize the copy switch handles
        // (1/2/4/8 bytes) -- it never interprets the values.  Gate on itemsize,
        // NOT a dtype allow-list: a bf16/f16 (2-byte) source -- every Metal VAE
        // conv im2col, whose weights+activations are bf16 -- would otherwise hit
        // the old DT_FP32/DT_INT32-only guard, skip the replay, and leave the
        // contiguous dst holding the CAPTURE-step bytes (the VAE up-block
        // conv_shortcut baked its first prompt's input on every warm replay).
        u32 esz = dtype_itemsize(op->gather_dtype);
        if (esz != 1 && esz != 2 && esz != 4 && esz != 8) break;
        // Rebuild a TenDesc shell from the stored view + chain so the
        // shared strided-index walker maps each dst element to its source
        // offset (handles broadcast strides and ShapeTracker chains).
        TenDesc sh = {0};
        sh.dtype       = op->gather_dtype;
        sh.view        = op->gather_view;
        sh.nviews      = op->gather_nviews;
        sh.prior_views = op->gather_prior;
        // GPU-side gather first (the eager pre-materialize hook, e.g.
        // metal_gather_strided): encodes into the open batch with NO host
        // sync.  The host loop below buf_read-DRAINS the whole GPU queue on
        // every gather -- the DSV2 metal decode replays 4 gathers/layer, and
        // those drains dominated its warm step.  The hook declines (flips,
        // 8-byte dtypes, long chains) into the host fallback.
        if (b->gather_strided != NULL
            && b->gather_strided(op->gather_dst_buf, op->gather_src_buf, &sh) == 0) {
          ITRS++;
          break;
        }
        u32 numel = sh.view.numel;
        // Source byte ceiling = max reachable index + 1 (matches the
        // capture-time sizing in materialize_root_alias).
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
        if (trace) fprintf(stderr, "[replay] op%u GATHER src=%u dst=%u numel=%u dt=%u\n",
                           i, op->gather_src_buf, op->gather_dst_buf, numel, op->gather_dtype);
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
      if (trace) fprintf(stderr, "[replay] CUDA graph capture FAILED -- per-op work already ran on capture stream, results may be missing\n");
    } else if (trace) {
      fprintf(stderr, "[replay] CUDA graph captured + launched slot=%u\n", slot);
    }
  }
#endif
  backend_dispatch_end_all();
  return c->n_ops;
}

// Reset every slot.  Called from thvm_init so a re-init cleanly
// drops captures from a prior session.
fn void jit_capture_reset_all(void) {
  for (u32 i = 0; i < JIT_CAPTURE_NSLOTS; i++) {
    jit_capture_clear_ops(&JIT_CAPTURES[i]);
    JIT_CAPTURES[i].in_use = 0;
  }
  JIT_ACTIVE_SLOT = 0;
  JIT_PAUSE_DEPTH = 0;
}
