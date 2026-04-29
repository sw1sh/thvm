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

#define JIT_OP_INLINE_INPUTS 16

typedef struct {
  JitOpKind kind;
  // DISPATCH path:
  u32  kid;
  u32  out_buf_id;     // also doubles as dst-buf for ASSIGN
  u32  n_inputs;       // also doubles as src-tid for ASSIGN (in_buf_ids[0])
  u32  in_buf_ids[JIT_OP_INLINE_INPUTS];
  // ASSIGN path:
  u32  assign_dst_tid;
  u32  assign_src_tid;
} JitCaptureOp;

typedef struct {
  u32           in_use;        // 0 = free slot
  u32           n_ops;
  JitCaptureOp *ops;           // calloc'd JIT_CAPTURE_OP_CAP entries
} JitCapture;

static JitCapture JIT_CAPTURES[JIT_CAPTURE_NSLOTS];

static u32 JIT_ACTIVE_SLOT = 0;     // 0 = not capturing; otherwise the
                                    // 1-indexed slot being filled

fn int jit_is_capturing(void) {
  return JIT_ACTIVE_SLOT != 0;
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
      JIT_CAPTURES[i].in_use   = 1;
      JIT_CAPTURES[i].n_ops    = 0;
      JIT_ACTIVE_SLOT          = i;
      return i;
    }
  }
  return 0;
}

fn void jit_capture_end(void) {
  JIT_ACTIVE_SLOT = 0;
}

fn void jit_capture_drop(u32 slot) {
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) return;
  JIT_CAPTURES[slot].in_use = 0;
  JIT_CAPTURES[slot].n_ops  = 0;
  // Keep the ops buffer allocated -- the slot can be reused for
  // another capture without re-malloc.
}

fn u32 jit_capture_op_count(u32 slot) {
  if (slot == 0 || slot >= JIT_CAPTURE_NSLOTS) return 0;
  return JIT_CAPTURES[slot].n_ops;
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
  if (n_inputs > JIT_OP_INLINE_INPUTS) {
    fprintf(stderr,
        "thvm: jit_capture_record -- kernel kid=%u has %u inputs, cap is %u\n",
        kid, n_inputs, JIT_OP_INLINE_INPUTS);
    return;
  }
  JitCaptureOp *op = &c->ops[c->n_ops++];
  op->kind       = JIT_OP_DISPATCH;
  op->kid        = kid;
  op->out_buf_id = out_buf_id;
  op->n_inputs   = n_inputs;
  for (u32 i = 0; i < n_inputs; i++) op->in_buf_ids[i] = in_buf_ids[i];
}

// Called from interact_assign_with just before the memcpy when
// JIT_ACTIVE_SLOT != 0.  Records the (src_tid, dst_tid) so replay can
// re-do the memcpy without needing to walk the heap or fire WNF.
fn void jit_capture_record_assign(u32 dst_tid, u32 src_tid) {
  if (JIT_ACTIVE_SLOT == 0) return;
  JitCapture *c = &JIT_CAPTURES[JIT_ACTIVE_SLOT];
  if (c->n_ops >= JIT_CAPTURE_OP_CAP) return;
  JitCaptureOp *op = &c->ops[c->n_ops++];
  op->kind           = JIT_OP_ASSIGN;
  op->assign_dst_tid = dst_tid;
  op->assign_src_tid = src_tid;
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
  for (u32 i = 0; i < c->n_ops; i++) {
    JitCaptureOp *op = &c->ops[i];
    switch (op->kind) {
      case JIT_OP_DISPATCH: {
        if (op->kid == 0 || op->kid >= KERNELS_NEXT) continue;
        KernelEntry *ke = &KERNELS[op->kid];
        if (ke->spliced)                              continue;
        Backend *b = TENS[ke->output_tid].backend;
        if (b == NULL || b->dispatch_kernel == NULL)  continue;
        b->dispatch_kernel(ke, op->in_buf_ids, op->out_buf_id);
        ITRS++;
        break;
      }
      case JIT_OP_ASSIGN: {
        u32 dst = op->assign_dst_tid, src = op->assign_src_tid;
        if (dst == 0 || dst >= TENS_NEXT) continue;
        if (src == 0 || src >= TENS_NEXT) continue;
        TenDesc *dd = &TENS[dst], *sd = &TENS[src];
        if (dd->backend == NULL || dd->backend != sd->backend) continue;
        u32 numel = dd->view.numel;
        if (sd->view.numel != numel) continue;
        u32 elem_bytes = (dd->dtype == DT_F32 || dd->dtype == DT_I32) ? 4 : 0;
        if (elem_bytes == 0) continue;
        u64 nbytes = (u64)numel * (u64)elem_bytes;
        void *tmp = malloc((size_t)nbytes);
        if (tmp == NULL) continue;
        dd->backend->buf_read (sd->buf_id, tmp, nbytes);
        dd->backend->buf_write(dd->buf_id, tmp, nbytes);
        free(tmp);
        ITRS++;
        break;
      }
    }
  }
  return c->n_ops;
}

// Reset every slot.  Called from thvm_init so a re-init cleanly
// drops captures from a prior session.
fn void jit_capture_reset_all(void) {
  for (u32 i = 0; i < JIT_CAPTURE_NSLOTS; i++) {
    JIT_CAPTURES[i].in_use = 0;
    JIT_CAPTURES[i].n_ops  = 0;
  }
  JIT_ACTIVE_SLOT = 0;
}
