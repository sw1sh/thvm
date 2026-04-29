// backend/cpu/interpret.c - tree-walker that executes a KernelEntry program.
//
// Analogue of tinygrad's ops_python.py PythonProgram: walks
// KernelEntry.program[] entry-by-entry, dispatching on opcode to the
// per-op files under src/backend/cpu/op/<op>.c.  Each op writes into
// a fresh per-step scratch buffer; the final op writes into the
// caller's out_buf_id.  No fusion, no memory reuse in v1 (step 14
// territory).
//
// Source slots can reference either an input tensor
// (KSRC_IS_INPUT(s)) or an earlier program slot (regs[KSRC_INDEX(s)]).
// Broadcast is handled per-op by inspecting src_numels[].

fn int cpu_interpret(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  // Resolve each input buffer's raw pointer once up front.
  // Non-contiguous inputs (sub-item f3c: view-only EXPAND aliases
  // with stride=0 broadcast) get pre-materialized into temp
  // contiguous buffers via view_strided_index so per-op kernels
  // can stay flat-buffer-simple.
  // Dynamically sized to ke->n_inputs (was static [KERNEL_MAX_INPUT]
  // when that was a small fixed cap; now KernelEntry's input arrays
  // are heap-grown so we mirror that here via stack-malloc).
  u32   n_inputs = ke->n_inputs;
  void *in_ptrs_buf  [n_inputs ? n_inputs : 1];
  void *temp_bufs_buf[n_inputs ? n_inputs : 1];
  void **in_ptrs   = in_ptrs_buf;
  void **temp_bufs = temp_bufs_buf;
  for (u32 i = 0; i < n_inputs; i++) temp_bufs[i] = NULL;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    u32 tid = ke->input_tids[i];
    if (tid != 0 && tid < TENS_NEXT && !TENS[tid].view.contiguous) {
      View const *v = &TENS[tid].view;
      void *src = CPU_BUFS[in_buf_ids[i]].data;
      void *tmp = malloc((size_t)v->numel * 4);
      if (TENS[tid].dtype == DT_F32) {
        f32 *d = (f32 *)tmp; f32 *s = (f32 *)src;
        for (u32 k = 0; k < v->numel; k++) d[k] = s[view_strided_index(v, k)];
      } else {
        i32 *d = (i32 *)tmp; i32 *s = (i32 *)src;
        for (u32 k = 0; k < v->numel; k++) d[k] = s[view_strided_index(v, k)];
      }
      in_ptrs  [i] = tmp;
      temp_bufs[i] = tmp;
    } else {
      in_ptrs[i] = CPU_BUFS[in_buf_ids[i]].data;
    }
  }

  // Allocate one scratch slot per program op.  The last op writes
  // into the real output buffer; all earlier ops write into their
  // own f32/i32 scratch of the right size.  Sized to ke->n_ops
  // (was static [KPROG_MAX_OPS] when that was a small fixed cap).
  u32   n_ops_local = ke->n_ops;
  void *regs_buf  [n_ops_local ? n_ops_local : 1];
  u64   rbytes_buf[n_ops_local ? n_ops_local : 1];
  u32   rsize_buf [n_ops_local ? n_ops_local : 1];
  void **regs   = regs_buf;
  u64   *rbytes = rbytes_buf;
  u32   *rsize  = rsize_buf;
  for (u32 i = 0; i < n_ops_local; i++) {
    regs  [i] = NULL;
    rbytes[i] = 0;
    rsize [i] = 0;
  }

  int rc = 0;
  for (u32 step = 0; step < ke->n_ops; step++) {
    KProgOp *p = &ke->program[step];
    u32 n_elem = p->numel ? p->numel : 1;
    u64 nbytes = (u64)n_elem * 4;     // DT_F32 + DT_I32 are 4B

    // Assemble source pointers + numels for this op.
    void *srcs      [MAX_UOP_SRC] = {0};
    u32   src_numels[MAX_UOP_SRC] = {0};
    for (u8 s = 0; s < p->n_src; s++) {
      u32 raw = p->src[s];
      if (KSRC_IS_INPUT(raw)) {
        u32 idx = KSRC_INDEX(raw);
        srcs[s]       = in_ptrs[idx];
        src_numels[s] = ke->input_numels[idx];
      } else {
        u32 idx = KSRC_INDEX(raw);
        srcs[s]       = regs[idx];
        src_numels[s] = rsize[idx];
      }
    }

    // Decide where to write.  Last step goes to out_buf_id; others
    // land in a scratch.
    void *dst;
    if (step + 1 == ke->n_ops) {
      dst = CPU_BUFS[out_buf_id].data;
    } else {
      regs  [step] = malloc((size_t)nbytes);
      rbytes[step] = nbytes;
      rsize [step] = n_elem;
      dst = regs[step];
    }

    switch (p->opcode) {
      case UOP_CONST: cpu_op_const(dst, srcs, src_numels, p, n_elem); break;
      case UOP_ADD:   cpu_op_add  (dst, srcs, src_numels, p, n_elem); break;
      case UOP_MUL:   cpu_op_mul  (dst, srcs, src_numels, p, n_elem); break;
      case UOP_NEG:   cpu_op_neg  (dst, srcs, src_numels, p, n_elem); break;
      case UOP_RECIP: cpu_op_recip(dst, srcs, src_numels, p, n_elem); break;
      case UOP_SQRT:  cpu_op_sqrt (dst, srcs, src_numels, p, n_elem); break;
      case UOP_EXP2:  cpu_op_exp2 (dst, srcs, src_numels, p, n_elem); break;
      case UOP_LOG2:  cpu_op_log2 (dst, srcs, src_numels, p, n_elem); break;
      case UOP_CMPLT: cpu_op_cmplt(dst, srcs, src_numels, p, n_elem); break;
      case UOP_CMPEQ: cpu_op_cmpeq(dst, srcs, src_numels, p, n_elem); break;
      case UOP_REDUCE:cpu_op_reduce(dst, srcs, src_numels, p, n_elem); break;
      case UOP_EXPAND:cpu_op_expand(dst, srcs, src_numels, p, n_elem); break;
      case UOP_RESHAPE:cpu_op_reshape(dst, srcs, src_numels, p, n_elem); break;
      case UOP_LOAD:   cpu_op_load  (dst, srcs, src_numels, p, n_elem); break;
      case UOP_FLIP:  cpu_op_flip  (dst, srcs, src_numels, p, n_elem); break;
      case UOP_PAD:   cpu_op_pad   (dst, srcs, src_numels, p, n_elem); break;
      case UOP_SHRINK:cpu_op_shrink(dst, srcs, src_numels, p, n_elem); break;
      case UOP_PERMUTE: cpu_op_permute(dst, srcs, src_numels, p, n_elem); break;
      default:
        rc = -1;
        goto cleanup;
    }
  }

cleanup:
  for (u32 i = 0; i < ke->n_ops; i++) if (regs[i]) free(regs[i]);
  for (u32 i = 0; i < ke->n_inputs; i++) if (temp_bufs[i]) free(temp_bufs[i]);
  return rc;
}

// Forward decls: defined in backend/cpu/{blas,jit}.c (included after
// this file in thvm.c, so declare here for the dispatcher).
fn int cpu_blas_dispatch(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);
fn int cpu_jit_dispatch (KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id);

fn int cpu_dispatch_kernel(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  // Recover kid by pointer arithmetic into KERNELS[].  Used for
  // per-kid profiling (cg_profile_record).
  u32 kid = (u32)(ke - KERNELS);
  u64 t0  = cg_now_us();
  // 1. BLAS first: matmul / matvec / dot patterns get cblas_*
  //    (Accelerate on macOS) -- 10-100x faster than anything we can
  //    JIT-compile near-term.
  int blas_kind = cpu_blas_dispatch(ke, in_buf_ids, out_buf_id);
  if (blas_kind) {
    cg_profile_record(kid, (KDispatchKind)blas_kind, cg_now_us() - t0);
    return 0;
  }
  // 2. JIT next: clang-compiled fused inner loop for elementwise
  //    chains, cached by program hash.
  if (cpu_jit_dispatch(ke, in_buf_ids, out_buf_id)) {
    cg_profile_record(kid, KDISPATCH_JIT, cg_now_us() - t0);
    return 0;
  }
  // 3. Interpreter fallback.
  int rc = cpu_interpret(ke, in_buf_ids, out_buf_id);
  cg_profile_record(kid, KDISPATCH_INTERPRETER, cg_now_us() - t0);
  return rc;
}
