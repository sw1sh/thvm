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
    if (tid == 0 || tid >= TENS_NEXT) {
      in_ptrs[i] = CPU_BUFS[in_buf_ids[i]].data;
      continue;
    }
    TenDesc const *td = &TENS[tid];
    // Pre-materialize when the public view is non-contig OR when
    // the ShapeTracker has a chain (multi-view composition).  Both
    // cases need the index walked through tendesc_strided_index.
    if (td->view.contiguous && td->nviews == 0) {
      in_ptrs[i] = CPU_BUFS[in_buf_ids[i]].data;
      continue;
    }
    View const *v = &td->view;
    void *src = CPU_BUFS[in_buf_ids[i]].data;
    // Packed nibble dtypes: unpack the underlying byte buffer to i8,
    // gather under the view (i8 indexing), then repack to nibbles.
    if (dtype_is_packed(td->dtype)) {
      i32 max_idx = v->offset;
      for (u32 k = 0; k < v->shape.ndim; k++) {
        if (v->shape.dims[k] > 1 && v->strides[k] > 0)
          max_idx += (i32)(v->shape.dims[k] - 1) * v->strides[k];
      }
      u32 src_logical = (u32)max_idx + 1;
      i8 *unpacked = (i8 *)malloc(src_logical);
      if (td->dtype == DT_INT4)
        unpack_int4 (unpacked, (u8 const *)src, src_logical);
      else
        unpack_uint4((u8 *)unpacked, (u8 const *)src, src_logical);
      i8 *gathered = (i8 *)malloc(v->numel);
      for (u32 k = 0; k < v->numel; k++)
        gathered[k] = unpacked[tendesc_strided_index(td, k)];
      free(unpacked);
      void *packed = malloc((size_t)dtype_storage_bytes(td->dtype, v->numel));
      if (td->dtype == DT_INT4)
        pack_int4 ((u8 *)packed, gathered, v->numel);
      else
        pack_uint4((u8 *)packed, (u8 *)gathered, v->numel);
      free(gathered);
      in_ptrs  [i] = packed;
      temp_bufs[i] = packed;
      continue;
    }
    u32   esz = dtype_itemsize(td->dtype);
    void *tmp = malloc((size_t)dtype_storage_bytes(td->dtype, v->numel));
    switch (esz) {
      case 1: {
        u8 *d = (u8 *)tmp, *s = (u8 *)src;
        for (u32 k = 0; k < v->numel; k++) d[k] = s[tendesc_strided_index(td, k)];
        break;
      }
      case 2: {
        u16 *d = (u16 *)tmp, *s = (u16 *)src;
        for (u32 k = 0; k < v->numel; k++) d[k] = s[tendesc_strided_index(td, k)];
        break;
      }
      case 4: {
        u32 *d = (u32 *)tmp, *s = (u32 *)src;
        for (u32 k = 0; k < v->numel; k++) d[k] = s[tendesc_strided_index(td, k)];
        break;
      }
      case 8: {
        u64 *d = (u64 *)tmp, *s = (u64 *)src;
        for (u32 k = 0; k < v->numel; k++) d[k] = s[tendesc_strided_index(td, k)];
        break;
      }
      default:
        fprintf(stderr, "cpu_interpret: view pre-mat itemsize %u unsupported\n", esz);
        abort();
    }
    in_ptrs  [i] = tmp;
    temp_bufs[i] = tmp;
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
    u64 nbytes = dtype_storage_bytes(p->dtype, n_elem);

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
      case UOP_CAST:    cpu_op_cast   (dst, srcs, src_numels, p, n_elem); break;
      case UOP_BITCAST: cpu_op_bitcast(dst, srcs, src_numels, p, n_elem); break;
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

// === Phase B/C: scalar-UOp interpreter ===============================
//
// Recursive evaluator over ke->scalar_uops.  Each op produces a u64
// value (range iter, address, or f32-bit-cast scalar) that is
// either:
//   - immediate-recomputed via eval_scalar (for ops whose value
//     depends on the current iter context -- including REDUCE
//     bodies)
//   - or read from `range_iter[op_id]` for S_RANGE ops, which the
//     dispatcher mutates at loop boundaries.
//
// The dispatcher iterates LOOP-typed ranges in their canonical
// row-major order, sets the per-range iter values, then evaluates
// the kernel's S_STORE op at each iteration.  REDUCE-typed ranges
// are nested INSIDE the eval (S_REDUCE_SUM/_MAX op opens an inner
// loop, mutates its range's iter, accumulates).
//
// f32 only for now; bit-cast through u32 stored in the low 32 bits
// of the returned u64.

typedef struct {
  KernelEntry *ke;
  void       **in_ptrs;
  void        *out_p;
  u32         *range_iter;     // per-op-id; only S_RANGE slots are used
  u32          odtype;
} ScalarCtx;

static u64 eval_scalar(ScalarCtx *c, u32 op_id);

// Decode S_INDEX: address = sum(range_iter[src[1+d]] * stride[d]).
// Returns the byte-offset address (input element index, output
// element index, etc.) plus the slot id in the high 32 bits when
// the buffer is a DEFINE_PARAM.
static u64 eval_index(ScalarCtx *c, ScalarUop const *u) {
  u32 buf_id = u->src[0];
  ScalarUop const *bu = &c->ke->scalar_uops[buf_id];
  u64 strides_packed = u->extra;
  u32 addr = 0;
  u32 nrng = (u32)u->src_count - 1;
  for (u32 d = 0; d < nrng; d++) {
    u32 rng_id = u->src[1 + d];
    u32 iter   = c->range_iter[rng_id];
    u32 stride = (u32)((strides_packed >> (16 * d)) & 0xFFFFu);
    addr += iter * stride;
  }
  if (bu->op == S_DEFINE_OUTPUT) return (u64)addr;
  // Input: stash the slot id in the high half so S_LOAD can find the
  // buffer pointer.
  u32 slot = (u32)bu->extra;
  return ((u64)slot << 32) | (u64)addr;
}

static u64 eval_scalar(ScalarCtx *c, u32 op_id) {
  if (op_id == 0) return 0;
  ScalarUop const *u = &c->ke->scalar_uops[op_id];
  switch (u->op) {
    case S_RANGE:           return (u64)c->range_iter[op_id];
    case S_DEFINE_PARAM:    return u->extra;
    case S_DEFINE_OUTPUT:   return 0;
    case S_CONST:           return u->extra & 0xFFFFFFFFu;
    case S_INDEX:           return eval_index(c, u);
    case S_LOAD: {
      u64 idx_r = eval_scalar(c, u->src[0]);
      u32 slot  = (u32)(idx_r >> 32);
      u32 off   = (u32)(idx_r & 0xFFFFFFFFu);
      void *p   = c->in_ptrs[slot];
      f32   v;
      memcpy(&v, (const u8 *)p + off * 4, 4);
      u32 bits; memcpy(&bits, &v, 4);
      return (u64)bits;
    }
    case S_REDUCE_SUM:
    case S_REDUCE_MAX: {
      // src[0] = body; src[1] = REDUCE-typed range.
      u32 rng_id = u->src[1];
      ScalarUop const *r = &c->ke->scalar_uops[rng_id];
      u32 extent = (u32)(r->extra & 0xFFFFFFFFu);
      f32 acc    = (u->op == S_REDUCE_MAX) ? -INFINITY : 0.0f;
      u32 saved  = c->range_iter[rng_id];
      for (u32 ri = 0; ri < extent; ri++) {
        c->range_iter[rng_id] = ri;
        u32 b = (u32)eval_scalar(c, u->src[0]);
        f32 v; memcpy(&v, &b, 4);
        if (u->op == S_REDUCE_MAX) acc = (v > acc ? v : acc);
        else                       acc += v;
      }
      c->range_iter[rng_id] = saved;
      u32 rb; memcpy(&rb, &acc, 4);
      return (u64)rb;
    }
    case S_ADD: {
      f32 a, b;
      u32 ab = (u32)eval_scalar(c, u->src[0]); memcpy(&a, &ab, 4);
      u32 bb = (u32)eval_scalar(c, u->src[1]); memcpy(&b, &bb, 4);
      f32 r = a + b; u32 rb; memcpy(&rb, &r, 4); return rb;
    }
    case S_MUL: {
      f32 a, b;
      u32 ab = (u32)eval_scalar(c, u->src[0]); memcpy(&a, &ab, 4);
      u32 bb = (u32)eval_scalar(c, u->src[1]); memcpy(&b, &bb, 4);
      f32 r = a * b; u32 rb; memcpy(&rb, &r, 4); return rb;
    }
    case S_NEG: {
      f32 a; u32 ab = (u32)eval_scalar(c, u->src[0]); memcpy(&a, &ab, 4);
      f32 r = -a; u32 rb; memcpy(&rb, &r, 4); return rb;
    }
    case S_RECIP: {
      f32 a; u32 ab = (u32)eval_scalar(c, u->src[0]); memcpy(&a, &ab, 4);
      f32 r = 1.0f / a; u32 rb; memcpy(&rb, &r, 4); return rb;
    }
    case S_SQRT: {
      f32 a; u32 ab = (u32)eval_scalar(c, u->src[0]); memcpy(&a, &ab, 4);
      f32 r = sqrtf(a); u32 rb; memcpy(&rb, &r, 4); return rb;
    }
    case S_EXP2: {
      f32 a; u32 ab = (u32)eval_scalar(c, u->src[0]); memcpy(&a, &ab, 4);
      f32 r = exp2f(a); u32 rb; memcpy(&rb, &r, 4); return rb;
    }
    case S_LOG2: {
      f32 a; u32 ab = (u32)eval_scalar(c, u->src[0]); memcpy(&a, &ab, 4);
      f32 r = log2f(a); u32 rb; memcpy(&rb, &r, 4); return rb;
    }
    case S_CMPLT: {
      f32 a, b;
      u32 ab = (u32)eval_scalar(c, u->src[0]); memcpy(&a, &ab, 4);
      u32 bb = (u32)eval_scalar(c, u->src[1]); memcpy(&b, &bb, 4);
      f32 r = (a < b) ? 1.0f : 0.0f;
      u32 rb; memcpy(&rb, &r, 4); return rb;
    }
    case S_CMPEQ: {
      f32 a, b;
      u32 ab = (u32)eval_scalar(c, u->src[0]); memcpy(&a, &ab, 4);
      u32 bb = (u32)eval_scalar(c, u->src[1]); memcpy(&b, &bb, 4);
      f32 r = (a == b) ? 1.0f : 0.0f;
      u32 rb; memcpy(&rb, &r, 4); return rb;
    }
    default:
      return 0;
  }
}

fn int cpu_dispatch_scalar(KernelEntry *ke, u32 *in_buf_ids, u32 out_buf_id) {
  if (ke->scalar_uops == NULL || ke->n_scalar_uops < 2) return -1;
  // Find the S_BUFFERIZE root.  Its src[0] is the kernel's S_STORE.
  u32 buf_id = 0;
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    if (ke->scalar_uops[i].op == S_BUFFERIZE) { buf_id = i; break; }
  }
  if (buf_id == 0) return -1;
  u32 store_id = ke->scalar_uops[buf_id].src[0];
  if (ke->scalar_uops[store_id].op != S_STORE) return -1;

  // Collect LOOP ranges + their extents from the BUFFERIZE root.
  // BUFFERIZE.src[0..n) = STORE then 1+n LOOP ranges in axis order.
  ScalarUop *bu = &ke->scalar_uops[buf_id];
  u32 n_loops = (u32)bu->src_count - 1;
  u32 loop_ids    [4];
  u32 loop_extents[4];
  for (u32 d = 0; d < n_loops && d < 4; d++) {
    u32 r = bu->src[1 + d];
    loop_ids    [d] = r;
    loop_extents[d] = (u32)(ke->scalar_uops[r].extra & 0xFFFFFFFFu);
  }

  // Resolve raw input pointers.  We bailed at lower-time on non-
  // contig inputs, so no view pre-mat is needed.
  u32 n_inputs = ke->n_inputs;
  void *in_ptrs_buf[n_inputs ? n_inputs : 1];
  void **in_ptrs = in_ptrs_buf;
  for (u32 i = 0; i < n_inputs; i++) {
    in_ptrs[i] = CPU_BUFS[in_buf_ids[i]].data;
  }

  // Per-op iter slot.  Only S_RANGE op ids are read; reusing one
  // u32 array indexed by op id keeps the dispatcher branch-free.
  u32 *range_iter = (u32 *)calloc(ke->n_scalar_uops, sizeof(u32));
  if (range_iter == NULL) return -1;

  ScalarCtx ctx = {
    .ke          = ke,
    .in_ptrs     = in_ptrs,
    .out_p       = CPU_BUFS[out_buf_id].data,
    .range_iter  = range_iter,
    .odtype      = ke->output_dtype,
  };

  // Outer LOOP nest: iterate every LOOP range.  For Phase B/C we
  // support up to 3 LOOP dims; the BUFFERIZE captured them in
  // canonical row-major order so we can iterate flat by k and
  // decode strides.
  u32 onum = ke->output_numel;
  // Precompute the LOOP strides for k -> per-range-iter decoding.
  u32 loop_strides[4] = {0, 0, 0, 0};
  if (n_loops > 0) {
    loop_strides[n_loops - 1] = 1;
    for (i32 d = (i32)n_loops - 2; d >= 0; d--)
      loop_strides[d] = loop_strides[d + 1] * loop_extents[d + 1];
  }
  for (u32 k = 0; k < onum; k++) {
    for (u32 d = 0; d < n_loops; d++) {
      u32 ext = loop_extents[d];
      u32 str = loop_strides[d];
      range_iter[loop_ids[d]] = (str > 0) ? ((k / str) % ext) : 0;
    }
    // Evaluate the STORE: writes one element to out_p.
    ScalarUop const *st = &ke->scalar_uops[store_id];
    u64 idx_r   = eval_scalar(&ctx, st->src[0]);
    u32 off     = (u32)(idx_r & 0xFFFFFFFFu);
    u32 bits    = (u32)eval_scalar(&ctx, st->src[1]);
    if (ctx.odtype == DT_FP32) {
      f32 v; memcpy(&v, &bits, 4);
      memcpy((u8 *)ctx.out_p + off * 4, &v, 4);
    } else {
      memcpy((u8 *)ctx.out_p + off * 4, &bits, 4);
    }
  }

  free(range_iter);
  return 0;
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
  // 0. Phase B: if this kernel was lowered to scalar form, run the
  //    scalar interpreter.  Bypasses BLAS / JIT / KProgOp[] -- the
  //    scalar form is the authoritative program when present.
  if (ke->scalar_uops != NULL && ke->n_scalar_uops > 1) {
    int rc = cpu_dispatch_scalar(ke, in_buf_ids, out_buf_id);
    cg_profile_record(kid, KDISPATCH_INTERPRETER, cg_now_us() - t0);
    return rc;
  }
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
