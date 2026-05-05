// Phase F shadow-render counters.  Instrument every cg_emit_tile_metal
// call: try kernel_lift_to_uop alongside the existing path and report
// what fraction of real workload kernels the lifter covers.  Read
// via kernel_lift_attempts() / kernel_lift_successes() / etc.
//
// Counters are global (single-threaded scheduling).  Reset by
// thvm_init / thvm_free.
static u64 KERNEL_LIFT_ATTEMPTS;
static u64 KERNEL_LIFT_SUCCESSES;
static u64 KERNEL_LIFT_COMPILES;       // shadow-rendered MSL also compiled
static u64 KERNEL_LIFT_COMPILE_FAILS;

fn u64 kernel_lift_attempts(void)        { return KERNEL_LIFT_ATTEMPTS; }
fn u64 kernel_lift_successes(void)       { return KERNEL_LIFT_SUCCESSES; }
fn u64 kernel_lift_compiles(void)        { return KERNEL_LIFT_COMPILES; }
fn u64 kernel_lift_compile_fails(void)   { return KERNEL_LIFT_COMPILE_FAILS; }

fn void kernel_lift_counters_reset(void) {
  KERNEL_LIFT_ATTEMPTS = 0;
  KERNEL_LIFT_SUCCESSES = 0;
  KERNEL_LIFT_COMPILES = 0;
  KERNEL_LIFT_COMPILE_FAILS = 0;
}

// Increment helpers for callers in earlier translation units (codegen/
// render_metal.c is #include'd before this file; they can't reach the
// static globals directly).
fn void kernel_lift_count_attempt (void) { KERNEL_LIFT_ATTEMPTS++; }
fn void kernel_lift_count_success (void) { KERNEL_LIFT_SUCCESSES++; }
fn void kernel_lift_count_compile (int ok) {
  if (ok) KERNEL_LIFT_COMPILES++;
  else    KERNEL_LIFT_COMPILE_FAILS++;
}

// schedule/kernel_lift.c - lift a scheduled kernel's ScalarUop arena
// to a UOp DAG root suitable for cg_render_uop_kernel (Phase C wedge).
//
// The migration plan calls for KernelEntry.program / scalar_uops[] to
// disappear in Phase C, replaced by a direct UOp DAG reference.
// kernel_lift_to_uop is the bridge: it takes a fully-scheduled kernel
// and builds the equivalent UOp DAG on the heap, returning a UOP_STORE
// root + UOP_BUFFER terms for output and inputs.
//
// Once the renderer rewrite proper flips render_metal to consume UOp
// DAG, every kernel scheduled by the system flows through this lifter
// (during the Phase C migration period).  After Phase C lands and
// scalar_uops[] disappears, this file deletes -- the kernel is born
// as a UOp DAG, no lifting required.
//
// Coverage:
//   S_CONST                  -> UOP_CONST
//   S_LOAD                   -> UOP_INDEX_E(buffer, addr)
//   S_INDEX(buf, ranges...)  -> linearised symbolic addr expr
//   S_ADD / MUL              -> UOP_ADD / UOP_MUL
//   S_NEG / RECIP / EXP2 /
//   LOG2 / SQRT              -> UOP_NEG / UOP_RECIP / etc.
//   S_CMPLT / CMPEQ          -> UOP_CMPLT / UOP_CMPEQ
//   S_CAST                   -> UOP_CAST
//   S_REDUCE_SUM/MAX(body, ranges...)
//                            -> UOP_REDUCE(lifted_body, kind, axis)
//   S_STORE(INDEX, value)    -> UOP_STORE(buf, addr, lifted_value)
//   S_BUFFERIZE(STORE, ranges...) -> kernel root; produces lifter result
//   S_RANGE                  -> UOP_RANGE (via UopRangeMap)
//   S_ICONST / S_I*          -> existing scalar_to_uop path
//
// Returns 0 on unsupported shape (S_RESHAPE_V wrappers, S_PAD/SHRINK,
// etc. -- the lifter stays strict so unsupported subtrees fall back
// to the legacy renderer until Phase B3-finish removes them).

// KernelUopLift is declared in thvm.h.

// Build per-axis-id maps from BUFFERIZE's range slots.  axis_id is
// the position in BUFFERIZE.src[1..n] (0-based).
typedef struct {
  u32  axis_id;
  u32  scalar_id;     // S_RANGE slot
  Term axis_uop;      // UOP_RANGE term
} LiftRangeMap;

static Term lift_scalar_value(KernelEntry const *ke, u32 sid,
                              LiftRangeMap const *ranges, u32 n_ranges,
                              Term out_buf, Term const *in_bufs,
                              u32 n_inputs);

static Term lift_lookup_range(LiftRangeMap const *ranges, u32 n_ranges,
                              u32 scalar_id) {
  for (u32 i = 0; i < n_ranges; i++) {
    if (ranges[i].scalar_id == scalar_id) return ranges[i].axis_uop;
  }
  return 0;
}

// Build the symbolic address expression for an S_INDEX(buf, ranges...)
// node.  Linearises: addr = sum(rng_i * stride_i) where stride_i is
// the row-major stride based on the surrounding buffer shape.  For
// row-major contiguous buffers this matches the expected layout.
//
// Returns the addr Term; *out_buf_term is filled with the lifted
// UOP_BUFFER for the source buffer.
static Term lift_scalar_index(KernelEntry const *ke, u32 sid,
                              LiftRangeMap const *ranges, u32 n_ranges,
                              Term out_buf, Term const *in_bufs,
                              u32 n_inputs, Term *out_buf_term) {
  if (sid == 0 || sid >= ke->n_scalar_uops) return 0;
  ScalarUop const *u = &ke->scalar_uops[sid];
  if (u->op != S_INDEX || u->src_count < 1) return 0;
  u32 buf_sid = u->src[0];
  ScalarUop const *bu = &ke->scalar_uops[buf_sid];
  Term buf = 0;
  if (bu->op == S_DEFINE_PARAM) {
    u32 slot = (u32)bu->extra;
    if (slot >= n_inputs) return 0;
    buf = in_bufs[slot];
  } else if (bu->op == S_DEFINE_OUTPUT) {
    buf = out_buf;
  } else {
    return 0;
  }
  if (out_buf_term != NULL) *out_buf_term = buf;

  // Walk the RANGE srcs and emit `sum(r_i * stride_i)`.  Strides come
  // from the buffer's shape (row-major).  For now we use uop_buffer
  // dims as the shape -- this matches the lifter's UOP_BUFFER setup.
  u32 ndim = uop_buffer_ndim(buf);
  if (ndim == 0 || ndim != (u32)u->src_count - 1) return 0;
  Term acc = 0;
  for (u32 d = 0; d < ndim; d++) {
    u32 r_sid = u->src[1 + d];
    Term r_uop = lift_lookup_range(ranges, n_ranges, r_sid);
    if (r_uop == 0) return 0;
    // Stride = product of dims[d+1..ndim).
    u32 stride = 1;
    for (u32 e = d + 1; e < ndim; e++) stride *= uop_buffer_dim(buf, e);
    Term term = (stride == 1) ? r_uop
              : uop_int_binary(UOP_IMUL, r_uop,
                               uop_const(DT_INT32, stride));
    acc = (acc == 0) ? term
        : uop_int_binary(UOP_IADD, acc, term);
  }
  if (acc == 0) acc = uop_const(DT_INT32, 0);
  return acc;
}

static u32 lift_scalar_unary_op(u8 sop) {
  switch (sop) {
    case S_NEG:   return UOP_NEG;
    case S_RECIP: return UOP_RECIP;
    case S_EXP2:  return UOP_EXP2;
    case S_LOG2:  return UOP_LOG2;
    case S_SQRT:  return UOP_SQRT;
    default:      return 0;
  }
}

static u32 lift_scalar_binary_op(u8 sop) {
  switch (sop) {
    case S_ADD:   return UOP_ADD;
    case S_MUL:   return UOP_MUL;
    case S_CMPLT: return UOP_CMPLT;
    case S_CMPEQ: return UOP_CMPEQ;
    default:      return 0;
  }
}

static Term lift_scalar_value(KernelEntry const *ke, u32 sid,
                              LiftRangeMap const *ranges, u32 n_ranges,
                              Term out_buf, Term const *in_bufs,
                              u32 n_inputs) {
  if (sid == 0 || sid >= ke->n_scalar_uops) return 0;
  ScalarUop const *u = &ke->scalar_uops[sid];
  switch (u->op) {
    case S_CONST: {
      u32 bits = (u32)u->extra;
      return uop_const(u->dtype, bits);
    }
    case S_LOAD: {
      if (u->src_count != 1) return 0;
      Term buf = 0;
      Term addr = lift_scalar_index(ke, u->src[0], ranges, n_ranges,
                                    out_buf, in_bufs, n_inputs, &buf);
      if (addr == 0 || buf == 0) return 0;
      return uop_index_e(buf, addr);
    }
    case S_NEG: case S_RECIP: case S_EXP2:
    case S_LOG2: case S_SQRT: {
      if (u->src_count != 1) return 0;
      u32 op = lift_scalar_unary_op(u->op);
      Term src = lift_scalar_value(ke, u->src[0], ranges, n_ranges,
                                   out_buf, in_bufs, n_inputs);
      if (src == 0) return 0;
      return uop_unary(op, src);
    }
    case S_ADD: case S_MUL: case S_CMPLT: case S_CMPEQ: {
      if (u->src_count != 2) return 0;
      u32 op = lift_scalar_binary_op(u->op);
      Term a = lift_scalar_value(ke, u->src[0], ranges, n_ranges,
                                 out_buf, in_bufs, n_inputs);
      Term b = lift_scalar_value(ke, u->src[1], ranges, n_ranges,
                                 out_buf, in_bufs, n_inputs);
      if (a == 0 || b == 0) return 0;
      return uop_binary(op, a, b);
    }
    case S_REDUCE_SUM: case S_REDUCE_MAX: {
      if (u->src_count < 1) return 0;
      Term body = lift_scalar_value(ke, u->src[0], ranges, n_ranges,
                                    out_buf, in_bufs, n_inputs);
      if (body == 0) return 0;
      // The reduce axis_id == the first range's axis_id (RANGE
      // ordering at BUFFERIZE matches semantic axis order).  For
      // multi-axis reductions we'd need to nest; F1e renderer only
      // handles single-axis, so we match that.
      if (u->src_count < 2) return 0;
      Term r_uop = lift_lookup_range(ranges, n_ranges, u->src[1]);
      if (r_uop == 0) return 0;
      u32 axis_id = term_val(heap_read(term_val(r_uop) + 0));
      u32 kind = (u->op == S_REDUCE_SUM) ? REDUCE_SUM : REDUCE_MAX;
      return uop_reduce(kind, axis_id, body);
    }
    case S_CAST: {
      if (u->src_count != 1) return 0;
      Term src = lift_scalar_value(ke, u->src[0], ranges, n_ranges,
                                   out_buf, in_bufs, n_inputs);
      if (src == 0) return 0;
      return uop_cast(src, u->dtype);
    }
    case S_RANGE:
      // Bare RANGE in a value position would be unusual but well-
      // defined: emit the corresponding UOP_RANGE.  Used for
      // index-like scalar values feeding into another expression.
      return lift_lookup_range(ranges, n_ranges, sid);
    case S_ICONST: case S_IADD: case S_ISUB: case S_IMUL:
    case S_IDIV: case S_IMOD: case S_ILT: case S_IAND:
    case S_IWHERE:
      // Integer-side scalar ops -- handled by scalar_to_uop with a
      // UopRangeMap built from the same ranges.
      {
        UopRangeMap srm[MAX_DIM];
        u32 n = (n_ranges < MAX_DIM) ? n_ranges : MAX_DIM;
        for (u32 i = 0; i < n; i++) {
          srm[i].scalar_id = ranges[i].scalar_id;
          srm[i].axis_uop  = ranges[i].axis_uop;
        }
        return scalar_to_uop(ke, sid, srm, n);
      }
    default:
      return 0;
  }
}

// Walk the scalar arena to find an S_INDEX(slot, ranges...) usage
// for the given DEFINE_PARAM slot id, and read the per-axis extents
// from the referenced S_RANGE leaves.  Used as a fallback when the
// kernel's input_tids[slot] doesn't have a TenDesc (synthetic test
// kernels, or paths where TenDesc isn't yet wired).  Returns 1 on
// success, filling `*ndim_out` and `dims_out[]`.
static int infer_input_shape_from_usage(KernelEntry const *ke, u32 slot,
                                        u32 *ndim_out, u32 *dims_out) {
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    ScalarUop const *u = &ke->scalar_uops[i];
    if (u->op != S_INDEX || u->src_count < 1) continue;
    u32 buf_sid = u->src[0];
    if (buf_sid == 0 || buf_sid >= ke->n_scalar_uops) continue;
    ScalarUop const *bu = &ke->scalar_uops[buf_sid];
    if (bu->op != S_DEFINE_PARAM || (u32)bu->extra != slot) continue;
    u32 ndim = (u32)u->src_count - 1;
    if (ndim == 0 || ndim > MAX_DIM) continue;
    for (u32 d = 0; d < ndim; d++) {
      u32 r_sid = u->src[1 + d];
      if (r_sid == 0 || r_sid >= ke->n_scalar_uops) return 0;
      ScalarUop const *ru = &ke->scalar_uops[r_sid];
      if (ru->op != S_RANGE) return 0;
      dims_out[d] = (u32)(ru->extra & 0xFFFFFFFFu);
    }
    *ndim_out = ndim;
    return 1;
  }
  return 0;
}

// Build a UOP_BUFFER for kernel input slot `slot`.  Prefers the
// TenDesc shape when present; falls back to inferring from the
// S_INDEX usage in the scalar arena (test kernels, synthetic shapes).
//
// The `instance` field on UOP_BUFFER ensures distinct slots get
// distinct Terms even when shape collides; we use slot+1 (since
// instance=0 is the output's reserved value).
static Term lift_input_buffer(KernelEntry const *ke, u32 slot) {
  if (slot >= ke->n_inputs) return 0;
  u32 dtype = (ke->input_dtypes != NULL) ? ke->input_dtypes[slot] : DT_FP32;
  u32 tid = (ke->input_tids != NULL) ? ke->input_tids[slot] : 0;
  u32 inst = slot + 1;
  if (tid != 0 && tid < TENS_NEXT) {
    TenDesc const *td = &TENS[tid];
    return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype,
                           td->view.shape.ndim, td->view.shape.dims, inst);
  }
  u32 ndim = 0;
  u32 dims[MAX_DIM] = {0};
  if (infer_input_shape_from_usage(ke, slot, &ndim, dims)) {
    return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, ndim, dims, inst);
  }
  // No usage found -- 1D dummy buffer.
  u32 dummy[1] = { 1 };
  return uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, 1, dummy, inst);
}

// Walk the scalar arena for an S_INDEX rooted at S_DEFINE_OUTPUT and
// read per-axis extents from the referenced ranges.  Returns 1 on
// success.  Mirrors infer_input_shape_from_usage but for the output.
static int infer_output_shape_from_usage(KernelEntry const *ke,
                                         u32 *ndim_out, u32 *dims_out) {
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    ScalarUop const *u = &ke->scalar_uops[i];
    if (u->op != S_INDEX || u->src_count < 1) continue;
    u32 buf_sid = u->src[0];
    if (buf_sid == 0 || buf_sid >= ke->n_scalar_uops) continue;
    ScalarUop const *bu = &ke->scalar_uops[buf_sid];
    if (bu->op != S_DEFINE_OUTPUT) continue;
    u32 ndim = (u32)u->src_count - 1;
    if (ndim == 0 || ndim > MAX_DIM) continue;
    for (u32 d = 0; d < ndim; d++) {
      u32 r_sid = u->src[1 + d];
      if (r_sid == 0 || r_sid >= ke->n_scalar_uops) return 0;
      ScalarUop const *ru = &ke->scalar_uops[r_sid];
      if (ru->op != S_RANGE) return 0;
      dims_out[d] = (u32)(ru->extra & 0xFFFFFFFFu);
    }
    *ndim_out = ndim;
    return 1;
  }
  return 0;
}

// Build a UOP_BUFFER for the kernel's output, using the actual output
// INDEX expression to determine ndim+dims (not BUFFERIZE.src[1..],
// which counts ALL ranges including reduce axes that don't appear in
// the output index).
static Term lift_output_buffer(KernelEntry const *ke,
                               LiftRangeMap const *ranges, u32 n_ranges) {
  u32 ndim = 0;
  u32 dims[MAX_DIM] = {0};
  if (infer_output_shape_from_usage(ke, &ndim, dims)) {
    return uop_buffer_inst(UOP_SCOPE_GLOBAL, ke->output_dtype, ndim, dims, 0);
  }
  // Fallback: use all ranges' extents.  Conservative; may produce
  // higher-rank output than the actual store but the renderer will
  // still index it.
  u32 n = (n_ranges < MAX_DIM) ? n_ranges : MAX_DIM;
  for (u32 i = 0; i < n; i++) {
    Term r = ranges[i].axis_uop;
    dims[i] = term_val(heap_read(term_val(r) + 2));
  }
  return uop_buffer_inst(UOP_SCOPE_GLOBAL, ke->output_dtype, n, dims, 0);
}

// Find the BUFFERIZE root in scalar_uops and lift the whole kernel
// program to a UOp DAG.  Fills `out` with the rendered-ready
// store_root + buffer terms.
fn int kernel_lift_to_uop(KernelEntry const *ke, KernelUopLift *out) {
  if (ke == NULL || ke->scalar_uops == NULL || out == NULL) return 0;
  // Find the BUFFERIZE root.
  u32 buf_sid = 0;
  for (u32 i = 1; i < ke->n_scalar_uops; i++) {
    if (ke->scalar_uops[i].op == S_BUFFERIZE) { buf_sid = i; break; }
  }
  if (buf_sid == 0) return 0;
  ScalarUop const *bu = &ke->scalar_uops[buf_sid];
  if (bu->src_count < 1) return 0;

  // Build LiftRangeMap from BUFFERIZE.src[1..].
  u32 n_ranges = (u32)bu->src_count - 1;
  if (n_ranges > MAX_DIM) return 0;
  LiftRangeMap ranges[MAX_DIM];
  for (u32 i = 0; i < n_ranges; i++) {
    u32 r_sid = bu->src[1 + i];
    if (r_sid == 0 || r_sid >= ke->n_scalar_uops) return 0;
    ScalarUop const *ru = &ke->scalar_uops[r_sid];
    if (ru->op != S_RANGE) return 0;
    u32 axis_type = (u32)(ru->extra >> 32) & 0xFFu;
    u32 extent    = (u32)(ru->extra & 0xFFFFFFFFu);
    ranges[i].axis_id   = i;
    ranges[i].scalar_id = r_sid;
    ranges[i].axis_uop  = uop_range(i, axis_type, extent);
  }

  // Build buffers.
  if (ke->n_inputs > KERNEL_LIFT_MAX_INPUT) return 0;
  out->n_inputs = ke->n_inputs;
  for (u32 i = 0; i < ke->n_inputs; i++) {
    Term in_buf = lift_input_buffer(ke, i);
    if (in_buf == 0) return 0;
    out->in_bufs[i] = in_buf;
  }
  out->out_buf = lift_output_buffer(ke, ranges, n_ranges);
  if (out->out_buf == 0) return 0;

  // Lift the body STORE.
  u32 store_sid = bu->src[0];
  if (store_sid == 0 || store_sid >= ke->n_scalar_uops) return 0;
  ScalarUop const *su = &ke->scalar_uops[store_sid];
  if (su->op != S_STORE || su->src_count != 2) return 0;
  Term ignored;
  Term addr = lift_scalar_index(ke, su->src[0], ranges, n_ranges,
                                out->out_buf, out->in_bufs,
                                out->n_inputs, &ignored);
  if (addr == 0) return 0;
  Term value = lift_scalar_value(ke, su->src[1], ranges, n_ranges,
                                 out->out_buf, out->in_bufs,
                                 out->n_inputs);
  if (value == 0) return 0;
  out->store_root = uop_store(out->out_buf, addr, value);
  return 1;
}
