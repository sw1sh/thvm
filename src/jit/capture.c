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

#define JIT_CAPTURE_OP_CAP    65536  // per-capture op count cap
#define JIT_CAPTURE_NSLOTS    16     // max simultaneous captures (paired
                                     // with TJit closures held WL-side)
#define JIT_CAPTURE_RETAIN_CAP 131072

// Two op kinds covered by the capture buffer:
//   JIT_OP_DISPATCH  -- normal kernel dispatch through Backend.dispatch_kernel.
//   JIT_OP_ASSIGN    -- a memcpy from src tid -> dst tid (interact_assign_with).
//                       The TJit-friendly optimiser-step idiom mutates weight
//                       tensors via TAssign, so we have to replay those writes
//                       too or the captured kernels would read stale weights.
typedef enum {
  JIT_OP_DISPATCH = 0,
  JIT_OP_ASSIGN   = 1
} JitOpKind;

// Full fused forward kernels can legitimately have dozens of input
// buffers (beautiful_mnist hits 26 today).  Keep capture records inline
// for replay speed; switch to a sidecar input table if this grows again.
#define JIT_OP_INLINE_INPUTS 64

typedef struct {
  JitOpKind kind;
  u8   replay_skip;
  u8   replay_packed;
  // DISPATCH path:
  u32  kid;
  u32  out_buf_id;     // also doubles as dst-buf for ASSIGN
  u32  n_inputs;       // also doubles as src-tid for ASSIGN (in_buf_ids[0])
  u32  in_buf_ids[JIT_OP_INLINE_INPUTS];
  u32 *heap_in_buf_ids;
  // ASSIGN path:
  u32  assign_dst_tid;
  u32  assign_src_tid;
} JitCaptureOp;

typedef struct {
  Backend *backend;
  u32      buf_id;
} JitCaptureBufRef;

typedef struct {
  u32               in_use;        // 0 = free slot
  u32               n_ops;
  JitCaptureOp     *ops;           // calloc'd JIT_CAPTURE_OP_CAP entries
  u32               n_retained;
  JitCaptureBufRef *retained;      // buffers owned by this replay slot
} JitCapture;

static JitCapture JIT_CAPTURES[JIT_CAPTURE_NSLOTS];

static u32 JIT_ACTIVE_SLOT = 0;     // 0 = not capturing; otherwise the
                                    // 1-indexed slot being filled
static u32 JIT_PAUSE_DEPTH = 0;     // nested suppression for internal
                                    // benchmark fires (autotune, variants)

static void jit_capture_finalize(u32 slot, Term root);

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
    if (b != NULL && b->buf_decref != NULL && buf_id != 0) {
      b->buf_decref(buf_id);
    }
  }
  c->n_retained = 0;
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

static void jit_capture_clear_ops(JitCapture *c) {
  if (c == NULL) {
    return;
  }
  jit_capture_release_retained(c);
  if (c->ops == NULL) {
    return;
  }
  for (u32 i = 0; i < c->n_ops; i++) {
    free(c->ops[i].heap_in_buf_ids);
    c->ops[i].heap_in_buf_ids = NULL;
  }
  c->n_ops = 0;
}

fn int jit_is_capturing(void) {
  return JIT_ACTIVE_SLOT != 0 && JIT_PAUSE_DEPTH == 0;
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
        JIT_CAPTURES[i].ops = (JitCaptureOp *)calloc(
            JIT_CAPTURE_OP_CAP, sizeof(JitCaptureOp));
        if (JIT_CAPTURES[i].ops == NULL) return 0;
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
      JIT_ACTIVE_SLOT          = i;
      JIT_PAUSE_DEPTH          = 0;
      return i;
    }
  }
  return 0;
}

fn void jit_capture_end(void) {
  if (JIT_ACTIVE_SLOT != 0) {
    jit_capture_finalize(JIT_ACTIVE_SLOT, 0);
  }
  JIT_ACTIVE_SLOT = 0;
  JIT_PAUSE_DEPTH = 0;
}

fn void jit_capture_end_with_result(Term root) {
  if (JIT_ACTIVE_SLOT != 0) {
    jit_capture_finalize(JIT_ACTIVE_SLOT, root);
  }
  JIT_ACTIVE_SLOT = 0;
  JIT_PAUSE_DEPTH = 0;
}

fn void jit_capture_drop(u32 slot) {
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) return;
  jit_capture_clear_ops(&JIT_CAPTURES[slot]);
  JIT_CAPTURES[slot].in_use = 0;
  if (JIT_ACTIVE_SLOT == slot) {
    JIT_ACTIVE_SLOT = 0;
    JIT_PAUSE_DEPTH = 0;
  }
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
    }
  }
  return 2 + n * row_width;
}

// Called from kernel_fire_by_id just before the actual dispatch when
// JIT_ACTIVE_SLOT != 0.  Records the (kid, in_buf_ids, out_buf_id)
// tuple so jit_replay can re-dispatch the SAME work without a new
// materialize pass.
fn void jit_capture_record(u32 kid, u32 const *in_buf_ids,
                           u32 n_inputs, u32 out_buf_id) {
  if (JIT_ACTIVE_SLOT == 0) return;
  JitCapture *c = &JIT_CAPTURES[JIT_ACTIVE_SLOT];
  if (c->n_ops >= JIT_CAPTURE_OP_CAP) {
    fprintf(stderr,
        "thvm: jit_capture_record -- buffer full at %u ops, dropping\n",
        JIT_CAPTURE_OP_CAP);
    return;
  }
  JitCaptureOp *op = &c->ops[c->n_ops++];
  op->kind          = JIT_OP_DISPATCH;
  op->replay_skip   = 0;
  op->replay_packed = 0;
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

// Called from interact_assign_with just before the memcpy when
// JIT_ACTIVE_SLOT != 0.  Records the (src_tid, dst_tid) so replay can
// re-do the memcpy without needing to walk the heap or fire WNF.
fn void jit_capture_record_assign(u32 dst_tid, u32 src_tid) {
  if (JIT_ACTIVE_SLOT == 0) return;
  JitCapture *c = &JIT_CAPTURES[JIT_ACTIVE_SLOT];
  if (c->n_ops >= JIT_CAPTURE_OP_CAP) return;
  JitCaptureOp *op = &c->ops[c->n_ops++];
  op->kind            = JIT_OP_ASSIGN;
  op->replay_skip     = 0;
  op->replay_packed   = 0;
  op->heap_in_buf_ids = NULL;
  op->assign_dst_tid  = dst_tid;
  op->assign_src_tid  = src_tid;
  jit_capture_retain_tensor_buf(c, dst_tid);
  jit_capture_retain_tensor_buf(c, src_tid);
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

#ifdef THVM_HAS_METAL
typedef struct {
  Backend *backend;
  u32      buf_id;
  u64      nbytes;
  u32      last_use;
} JitReplaySlot;

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
    if (op->replay_skip || op->kind != JIT_OP_DISPATCH) {
      continue;
    }
    u32 *ids = op->heap_in_buf_ids != NULL
             ? op->heap_in_buf_ids
             : op->in_buf_ids;
    for (u32 j = 0; j < op->n_inputs; j++) {
      if (jit_dispatch_input_backend(op, j) == backend
          && ids[j] == old_buf_id) {
        ids[j] = new_buf_id;
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
  if (cg_kernel_dispatch_kind(op->kid) != KDISPATCH_METAL_TILE) {
    return 0;
  }
  KernelEntry const *ke = &KERNELS[op->kid];
  if (ke->output_tid == 0 || ke->output_tid >= TENS_NEXT) {
    return 0;
  }
  TenDesc const *td = &TENS[ke->output_tid];
  Backend *backend = td->backend;
  if (backend == NULL || backend->id != METAL_BACKEND.id
      || backend->buf_decref == NULL || td->buf_id != op->out_buf_id) {
    return 0;
  }
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
    return 0;
  }
  *last_use = last;
  return 1;
}

static void jit_capture_pack_replay_temporaries(JitCapture *c, Term root) {
  if (!jit_replay_pack_enabled() || c == NULL || root != 0
      || c->n_ops == 0) {
    return;
  }
  JitReplaySlot *slots = (JitReplaySlot *)calloc(
      c->n_ops, sizeof(JitReplaySlot));
  if (slots == NULL) {
    return;
  }
  u32 n_slots = 0;
  for (u32 i = 0; i < c->n_ops; i++) {
    JitCaptureOp *op = &c->ops[i];
    Backend *backend = NULL;
    u64 nbytes = 0;
    if (!jit_capture_replay_packable_output(op, &backend, &nbytes)) {
      continue;
    }
    u32 last_use = i;
    if (!jit_capture_replay_lifetime(c, i, backend, op->out_buf_id,
                                     &last_use)) {
      continue;
    }

    u32 slot_idx = n_slots;
    u64 best_size = UINT64_MAX;
    for (u32 s = 0; s < n_slots; s++) {
      if (slots[s].backend != backend || slots[s].nbytes < nbytes
          || slots[s].last_use >= i) {
        continue;
      }
      if (slots[s].nbytes < best_size) {
        slot_idx = s;
        best_size = slots[s].nbytes;
      }
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
    backend->buf_decref(old_out);
    slots[slot_idx].last_use = last_use;
  }
  free(slots);
}
#else
static void jit_capture_pack_replay_temporaries(JitCapture *c, Term root) {
  (void)c;
  (void)root;
}
#endif

static void jit_capture_finalize(u32 slot, Term root) {
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) {
    return;
  }
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
  jit_capture_root_needed(root, needed, &n_needed);

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
    }
  }

  jit_capture_sink_assigns(c, root);
  jit_capture_pack_replay_temporaries(c, root);

  jit_capture_release_retained(c);
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
    }
  }
  free(needed);
}

#ifdef THVM_HAS_METAL
static u32 jit_replay_try_metal_graph_run(u32 slot, JitCapture *c, u32 start) {
  if (!jit_metal_graph_replay_enabled()) {
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
  if (thvm_metal_jit_replay_run(slot, start, recs, n) != 0) {
    return 0;
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
  for (u32 i = 0; i < c->n_ops;) {
#ifdef THVM_HAS_METAL
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
        if (b->dispatch_kernel(ke, ids, op->out_buf_id) == 0) {
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
        u32 numel = dd->view.numel;
        if (sd->view.numel != numel) break;
        // Phase A: dtype_storage_bytes aborts on unwired dtypes; gate
        // on the kinds we actually fire ASSIGN for (everything wired
        // through Phase B onward goes here too once enabled).
        if (dd->dtype != DT_FP32 && dd->dtype != DT_INT32) break;
        u64 nbytes = dtype_storage_bytes(dd->dtype, numel);
        if (dd->backend->buf_copy != NULL
            && dd->backend->buf_copy(dd->buf_id, sd->buf_id, nbytes) == 0) {
          ITRS++;
          break;
        }
        void *tmp = malloc((size_t)nbytes);
        if (tmp == NULL) break;
        dd->backend->buf_read (sd->buf_id, tmp, nbytes);
        dd->backend->buf_write(dd->buf_id, tmp, nbytes);
        free(tmp);
        ITRS++;
        break;
      }
    }
    i++;
  }
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
