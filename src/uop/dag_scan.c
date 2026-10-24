// uop/dag_scan.c - read-side scanners over a kernel's lifted UOp DAG
// (cached_lift.store_root): dtype and reduce gates, GEMM, contraction and conv
// classifiers, and the axis and address-coefficient readers behind hand_opts.
// A `root` of 0 (the lift declined) gets the safe default, 0 or uniform.  The
// entry points have external linkage: backend/metal/_.m is a separate TU.

// Lifted DAGs are acyclic: the recursive scans need no visited set.

// 1 iff every BUFFER, CONST and CAST/BITCAST target under `t` has dtype `dt`; an
// op not listed below reads as a mismatch.
int uop_dag_dtype_uniform(Term t, u32 dt) {
  if (t == 0) return 1;
  if (term_tag(t) != TAG_UOP) return 1;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_BUFFER:
      return uop_buffer_dtype(t) == dt;
    case UOP_CONST: {
      Term inner = heap_read(loc + 0);
      if (term_tag(inner) != TAG_NUM) return 0;
      return term_ext(inner) == dt;
    }
    case UOP_CAST: case UOP_BITCAST: {
      // [src, NUM(dst_dtype)]: src must be `dt` too, so a converting cast
      // reads as non-uniform.
      Term ddtype = heap_read(loc + 1);
      if (term_tag(ddtype) != TAG_NUM) return 0;
      if (term_val(ddtype) != dt) return 0;
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt);
    }
    // Index-domain: no float dtype to check.
    case UOP_RANGE: case UOP_INVALID:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
      return 1;
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:  case UOP_SIN:
    case UOP_REDUCE:
    case UOP_OPT:
    case UOP_LOAD:
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt);
    case UOP_ADD:  case UOP_MUL:
    case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 1), dt);
    case UOP_IWHERE:
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 1), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 2), dt);
    case UOP_STORE:
      // [buf, addr, value]; the address is index-domain.
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 2), dt);
    case UOP_AFTER:
      return uop_dag_dtype_uniform(heap_read(loc + 0), dt)
          && uop_dag_dtype_uniform(heap_read(loc + 1), dt);
    default:
      return 0;
  }
}

// propose's reduce-axis size: the extent (slot 2) of the first RANGE with
// axis_type KAX_REDUCE under `t`, else 0.
u32 uop_dag_reduce_axis_extent(Term t) {
  if (t == 0 || term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    Term axt = heap_read(loc + 1);
    Term ext = heap_read(loc + 2);
    if (term_tag(axt) != TAG_NUM || term_tag(ext) != TAG_NUM) return 0;
    if (term_val(axt) != 1 /*KAX_REDUCE*/) return 0;
    return term_val(ext);
  }
  switch (op) {
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
    case UOP_ADD:  case UOP_MUL:  case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E: case UOP_AFTER: {
      u32 a = uop_dag_reduce_axis_extent(heap_read(loc + 0));
      if (a) return a;
      return uop_dag_reduce_axis_extent(heap_read(loc + 1));
    }
    case UOP_NEG:   case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2:  case UOP_SQRT:  case UOP_SIN:
    case UOP_CAST:  case UOP_BITCAST:
    case UOP_OPT:   case UOP_REDUCE:
    case UOP_LOAD:
      return uop_dag_reduce_axis_extent(heap_read(loc + 0));
    case UOP_IWHERE:
      {
        u32 a = uop_dag_reduce_axis_extent(heap_read(loc + 0));
        if (a) return a;
        a = uop_dag_reduce_axis_extent(heap_read(loc + 1));
        if (a) return a;
        return uop_dag_reduce_axis_extent(heap_read(loc + 2));
      }
    case UOP_STORE:
      {
        // [buf, addr, value]: the RANGEs live in the address and the value.
        u32 a = uop_dag_reduce_axis_extent(heap_read(loc + 1));
        if (a) return a;
        return uop_dag_reduce_axis_extent(heap_read(loc + 2));
      }
    default:
      return 0;
  }
}

// Clears *out_ok at an op outside the cases below; sets *out_has_reduce at any
// REDUCE, whatever its kind.
static void uop_dag_reduce_unroll_walk(Term t, int *out_ok,
                                       int *out_has_reduce) {
  if (t == 0 || term_tag(t) != TAG_UOP) return;
  if (!*out_ok) return;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_REDUCE:
      *out_has_reduce = 1;
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      return;
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_INDEX_E:
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 1), out_ok, out_has_reduce);
      return;
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2: case UOP_LOG2:
    case UOP_SQRT: case UOP_SIN: case UOP_OPT: case UOP_LOAD:
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      return;
    case UOP_IWHERE:
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 1), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 2), out_ok, out_has_reduce);
      return;
    case UOP_STORE:
      uop_dag_reduce_unroll_walk(heap_read(loc + 1), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 2), out_ok, out_has_reduce);
      return;
    case UOP_AFTER:
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      uop_dag_reduce_unroll_walk(heap_read(loc + 1), out_ok, out_has_reduce);
      return;
    case UOP_BUFFER: case UOP_CONST:
    case UOP_RANGE: case UOP_INVALID:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
    case UOP_ISHR:
      return;
    case UOP_CAST: case UOP_BITCAST:
      // propose.c has already required an all-f32 DAG (uop_dag_dtype_uniform),
      // so this cast is f32 to f32.
      uop_dag_reduce_unroll_walk(heap_read(loc + 0), out_ok, out_has_reduce);
      return;
    default:
      *out_ok = 0;
      return;
  }
}

// The structural half of propose_metal_reduce_unroll_kernel's gate (propose.c
// checks the dtypes).
int uop_dag_is_reduce_unroll_kernel(Term t) {
  int ok = 1;
  int has_reduce = 0;
  uop_dag_reduce_unroll_walk(t, &ok, &has_reduce);
  return ok && has_reduce;
}

// === Shims for the Metal TU ===
// backend/metal/_.m walks the DAG through these: the heap, term and UOp
// accessors are `fn` (static inline) and invisible to another TU.

int uop_dag_decode_uop(Term t, u32 *out_op, u64 *out_loc) {
  if (term_tag(t) != TAG_UOP) return 0;
  if (out_op  != NULL) *out_op  = term_ext(t);
  if (out_loc != NULL) *out_loc = term_val(t);
  return 1;
}

// 0 also for a non-BUFFER, and 0 is the output's instance: check the op first.
u32 uop_dag_buffer_instance(Term t) {
  return uop_buffer_inst_get(t);
}

int uop_dag_const_payload(Term t, u32 *out_dtype, u32 *out_bits) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term inner = heap_read(term_val(t) + 0);
  if (term_tag(inner) != TAG_NUM) return 0;
  if (out_dtype != NULL) *out_dtype = term_ext(inner);
  if (out_bits  != NULL) *out_bits  = (u32)term_val(inner);
  return 1;
}

Term uop_dag_heap_read(u64 loc, u32 offset) {
  return heap_read(loc + offset);
}

int uop_dag_is_unary_ew (u32 op) { return uop_is_unary_elementwise ((u8)op); }
int uop_dag_is_binary_ew(u32 op) { return uop_is_binary_elementwise((u8)op); }

// === GEMM shape ===
// For CPU BLAS, hand_opts, propose and the Metal and CUDA tensor-core paths.

static Term udg_peel_tc_opt(Term value) {
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_OPT) return value;
  if (uop_opt_kind(value) != UOP_OPT_TC) return value;
  return uop_opt_target(value);
}

// uop_classify_matmul wants a bare REDUCE under the STORE, so a TC-wrapped value
// is classified through a transient STORE built without the OPT.
static int udg_classify_matmul_store(Term store, u32 *out_k_extent,
                                     Term *out_a_idx, Term *out_b_idx,
                                     u32 *out_unit_axis) {
  if (term_tag(store) != TAG_UOP || term_ext(store) != UOP_STORE) return 0;
  Term buf_out  = heap_read(term_val(store) + 0);
  Term addr_out = heap_read(term_val(store) + 1);
  Term value    = heap_read(term_val(store) + 2);
  Term peeled   = udg_peel_tc_opt(value);
  Term canon = (peeled == value) ? store : uop_store(buf_out, addr_out, peeled);
  if (!uop_classify_matmul(canon, out_k_extent, out_unit_axis)) return 0;
  // STORE(buf, addr, REDUCE(MUL(INDEX_E(A, addr_a), INDEX_E(B, addr_b))))
  u64 sloc = term_val(canon);
  Term reduce = heap_read(sloc + 2);
  Term mul    = heap_read(term_val(reduce) + 0);
  // Peel a widening or dequant CAST, as uop_classify_matmul does.
  if (out_a_idx != NULL) *out_a_idx = rec_tc_peel_cast(heap_read(term_val(mul) + 0));
  if (out_b_idx != NULL) *out_b_idx = rec_tc_peel_cast(heap_read(term_val(mul) + 1));
  return 1;
}

// A matmul operand's address is IADD(arm, arm), one arm per axis it reads (a
// broadcast axis has none); an arm is RANGE or IMUL(RANGE, CONST), either order.
static int udg_extract_addr_arm(Term arm, u32 *out_axis_id, u32 *out_coeff) {
  if (term_tag(arm) != TAG_UOP) return 0;
  u32 op = term_ext(arm);
  if (op == UOP_RANGE) {
    *out_axis_id = uop_range_axis_id(arm);
    *out_coeff   = 1;
    return 1;
  }
  if (op != UOP_IMUL) return 0;
  Term lhs = heap_read(term_val(arm) + 0);
  Term rhs = heap_read(term_val(arm) + 1);
  Term r = 0; Term c = 0;
  if (term_tag(lhs) == TAG_UOP && term_ext(lhs) == UOP_RANGE
      && term_tag(rhs) == TAG_UOP && term_ext(rhs) == UOP_CONST) {
    r = lhs; c = rhs;
  } else if (term_tag(rhs) == TAG_UOP && term_ext(rhs) == UOP_RANGE
             && term_tag(lhs) == TAG_UOP && term_ext(lhs) == UOP_CONST) {
    r = rhs; c = lhs;
  } else {
    return 0;
  }
  Term cnum = heap_read(term_val(c) + 0);
  if (term_tag(cnum) != TAG_NUM) return 0;
  u32 cv = (u32)term_val(cnum);
  if (cv == 0) return 0;
  *out_axis_id = uop_range_axis_id(r);
  *out_coeff   = cv;
  return 1;
}

// The reduce arm's coefficient and the other arm's coefficient and axis; 0 unless
// `addr` is IADD(arm, arm) with exactly one arm on `red_axis_id`.
int uop_dag_extract_matmul_strides_from_addr(Term addr, u32 red_axis_id,
                                             u32 *out_red_coeff,
                                             u32 *out_other_coeff,
                                             u32 *out_other_axis_id) {
  if (out_red_coeff != NULL)     *out_red_coeff = 0;
  if (out_other_coeff != NULL)   *out_other_coeff = 0;
  if (out_other_axis_id != NULL) *out_other_axis_id = 0;
  if (term_tag(addr) != TAG_UOP || term_ext(addr) != UOP_IADD) return 0;
  Term arm1 = heap_read(term_val(addr) + 0);
  Term arm2 = heap_read(term_val(addr) + 1);
  u32 axis1 = 0, axis2 = 0, c1 = 0, c2 = 0;
  if (!udg_extract_addr_arm(arm1, &axis1, &c1)) return 0;
  if (!udg_extract_addr_arm(arm2, &axis2, &c2)) return 0;
  if (axis1 == axis2) return 0;

  u32 k_coeff, other_coeff, other_axis;
  if (axis1 == red_axis_id) {
    k_coeff = c1; other_coeff = c2; other_axis = axis2;
  } else if (axis2 == red_axis_id) {
    k_coeff = c2; other_coeff = c1; other_axis = axis1;
  } else {
    return 0;
  }

  if (out_red_coeff != NULL)     *out_red_coeff = k_coeff;
  if (out_other_coeff != NULL)   *out_other_coeff = other_coeff;
  if (out_other_axis_id != NULL) *out_other_axis_id = other_axis;
  return 1;
}

// M and N from the output BUFFER, K from uop_classify_matmul, input slots from
// BUFFER.instance, ld and trans from the operand addresses; 0 if any fails.
int uop_dag_classify_matmul_shape(Term root,
                                  struct KernelEntry const *ke,
                                  UopDagGemmShape *out) {
  if (root == 0 || ke == NULL || out == NULL) return 0;
  if (ke->n_inputs < 2) return 0;

  Term a_idx = 0, b_idx = 0;
  u32 k_extent = 0;
  u32 unit_axis = 0;
  if (!udg_classify_matmul_store(root, &k_extent, &a_idx, &b_idx, &unit_axis))
    return 0;
  if (k_extent == 0) return 0;
  // K == 1 leaves A's address a bare RANGE(m), so transA is ambiguous (and the
  // product is an outer product).
  if (k_extent == 1) return 0;

  Term buf_out = heap_read(term_val(root) + 0);
  if (term_tag(buf_out) != TAG_UOP || term_ext(buf_out) != UOP_BUFFER) return 0;
  if (uop_buffer_ndim(buf_out) != 2) return 0;
  u32 M = uop_buffer_dim(buf_out, 0);
  u32 N = uop_buffer_dim(buf_out, 1);
  if (M == 0 || N == 0) return 0;
  // A collapsed unit axis (A's M, or B's N) needs that output dim to be 1, so a
  // dense matmul whose address lost an arm to another rewrite cannot pass.
  if (unit_axis == 1 && M != 1) return 0;
  if (unit_axis == 2 && N != 1) return 0;

  // BUFFER.instance is the input slot + 1 (lift_input_buffer).
  Term buf_a = heap_read(term_val(a_idx) + 0);
  Term buf_b = heap_read(term_val(b_idx) + 0);
  if (term_tag(buf_a) != TAG_UOP || term_ext(buf_a) != UOP_BUFFER) return 0;
  if (term_tag(buf_b) != TAG_UOP || term_ext(buf_b) != UOP_BUFFER) return 0;
  u32 inst_a = uop_buffer_inst_get(buf_a);
  u32 inst_b = uop_buffer_inst_get(buf_b);
  if (inst_a == 0 || inst_b == 0) return 0;
  u32 a_input = inst_a - 1;
  u32 b_input = inst_b - 1;
  if (a_input >= ke->n_inputs || b_input >= ke->n_inputs) return 0;
  if (a_input == b_input) return 0;

  // One reduce axis; a multi-axis K is uop_dag_classify_contraction_shape's.
  Term value = heap_read(term_val(root) + 2);
  Term reduce = udg_peel_tc_opt(value);
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) return 0;
  if (uop_reduce_n_axes(reduce) != 1) return 0;
  u32 red_axis_id = uop_reduce_axis(reduce, 0);

  Term addr_a = heap_read(term_val(a_idx) + 1);
  Term addr_b = heap_read(term_val(b_idx) + 1);

  u32 ldA = 0, ldB = 0;
  u32 transA = 0, transB = 0;
  u32 a_red_coeff = 0, a_other_coeff = 0, axis_m = 0;
  u32 b_red_coeff = 0, b_other_coeff = 0, axis_n = 0;
  int dag_a_ok = uop_dag_extract_matmul_strides_from_addr(
      addr_a, red_axis_id, &a_red_coeff, &a_other_coeff, &axis_m);
  int dag_b_ok = uop_dag_extract_matmul_strides_from_addr(
      addr_b, red_axis_id, &b_red_coeff, &b_other_coeff, &axis_n);

  if (unit_axis != 0) {
    // Rangeify folds the unit axis to CONST(0), so the collapsed operand's
    // address is a bare RANGE(k): a row-major {1,K} or {K,1} read whose ld is
    // set here; only the other operand decodes.
    if (unit_axis == 1) {
      if (!dag_b_ok) return 0;
      if (axis_n == red_axis_id) return 0;
      ldB    = (b_red_coeff > b_other_coeff) ? b_red_coeff : b_other_coeff;
      transB = (b_other_coeff != 1) ? 1 : 0;
      if (transB) { if (ldB != k_extent) return 0; }
      else        { if (ldB != N) return 0; }
      ldA    = k_extent;
      transA = 0;
    } else {
      // N == 1: blas.c declines it (N <= 1); other consumers get a correct shape.
      if (!dag_a_ok) return 0;
      if (axis_m == red_axis_id) return 0;
      ldA    = (a_red_coeff > a_other_coeff) ? a_red_coeff : a_other_coeff;
      transA = (a_red_coeff != 1) ? 1 : 0;
      if (transA) { if (ldA != M) return 0; }
      else        { if (ldA != k_extent) return 0; }
      ldB    = N;
      transB = 0;
    }
  } else if (!dag_a_ok || !dag_b_ok) {
    return 0;
  } else {
    if (axis_m == axis_n) return 0;
    // MUL operand 0 is taken as A, but WL's Orderless Times can write MUL(B, A):
    // the output address names the true M (coefficient N) and N (coefficient 1)
    // axes, and swapped operands are swapped back.
    Term addr_out = heap_read(term_val(root) + 1);
    u32 out_axis_M = 0xFFFFFFFFu, out_axis_N = 0xFFFFFFFFu;
    if (term_tag(addr_out) == TAG_UOP && term_ext(addr_out) == UOP_IADD) {
      Term oa1 = heap_read(term_val(addr_out) + 0);
      Term oa2 = heap_read(term_val(addr_out) + 1);
      u32 oax1 = 0, oax2 = 0, oc1 = 0, oc2 = 0;
      if (udg_extract_addr_arm(oa1, &oax1, &oc1)
          && udg_extract_addr_arm(oa2, &oax2, &oc2)
          && oax1 != oax2) {
        if (oc1 == N && oc2 == 1)        { out_axis_M = oax1; out_axis_N = oax2; }
        else if (oc2 == N && oc1 == 1)   { out_axis_M = oax2; out_axis_N = oax1; }
      }
    }
    if (out_axis_M != 0xFFFFFFFFu) {
      if (axis_m == out_axis_N && axis_n == out_axis_M) {
        u32 tmp_input  = a_input;     a_input     = b_input;     b_input     = tmp_input;
        u32 tmp_red    = a_red_coeff; a_red_coeff = b_red_coeff; b_red_coeff = tmp_red;
        u32 tmp_other  = a_other_coeff; a_other_coeff = b_other_coeff; b_other_coeff = tmp_other;
        u32 tmp_axis   = axis_m;      axis_m      = axis_n;      axis_n      = tmp_axis;
      } else if (axis_m != out_axis_M || axis_n != out_axis_N) {
        return 0;
      }
    }
    // (reduce, other) coefficients: A {M,K} row-major (1, K), transposed (M, 1);
    // B {K,N} row-major (N, 1), transposed (1, K).  ld is the larger one.
    ldA    = (a_red_coeff > a_other_coeff) ? a_red_coeff : a_other_coeff;
    ldB    = (b_red_coeff > b_other_coeff) ? b_red_coeff : b_other_coeff;
    transA = (a_red_coeff   != 1) ? 1 : 0;
    transB = (b_other_coeff != 1) ? 1 : 0;
    if (transA) { if (ldA != M) return 0; }
    else        { if (ldA != k_extent) return 0; }
    if (transB) { if (ldB != k_extent) return 0; }
    else        { if (ldB != N) return 0; }
  }

  // bf16, and a weight-only quant B (int8, fp8) behind its peeled dequant CAST,
  // are for the tensor-core paths (hand_opts' TC section); blas.c takes only f32
  // and f64.  out->dtype is the compute dtype.
  u32 dt = uop_buffer_dtype(buf_out);
  if (dt != DT_FP32 && dt != DT_FP64 && dt != DT_BF16) return 0;
  if (uop_buffer_dtype(buf_a) != dt) return 0;
  u32 dt_b = uop_buffer_dtype(buf_b);
  if (dt_b != dt && dt_b != DT_INT8
      && dt_b != DT_FP8E4M3 && dt_b != DT_FP8E5M2) return 0;

  out->dtype   = dt;
  out->M       = M;
  out->N       = N;
  out->K       = k_extent;
  out->a_input = a_input;
  out->b_input = b_input;
  out->ldA     = ldA;
  out->ldB     = ldB;
  out->flags   = (transA ? 1u : 0u) | (transB ? 2u : 0u);
  return 1;
}

// === conv2d-flat ===
// The structural gate of tile_analyze_conv2d_flat (schedule/tile.c): a STORE of
// a bare REDUCE_SUM.  tile.c reads the shape from the decoder below, else from
// input_views.
int uop_dag_classify_conv2d_flat_shape(Term root) {
  if (root == 0) return 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;
  Term value = heap_read(term_val(root) + 2);
  if (term_tag(value) != TAG_UOP || term_ext(value) != UOP_REDUCE) return 0;
  Term kind = heap_read(term_val(value) + 1);
  if (term_tag(kind) != TAG_NUM) return 0;
  if (term_val(kind) != REDUCE_SUM) return 0;
  return 1;
}

// The address decoder inverts the IDIV/IMOD split of r_out (co, bi, oh, ow)
// and of r_q (ci, kh_v, kw_v) in the W and X addresses.

static int udg_match_idiv_const(Term t, Term *out_num, u32 *out_div) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IDIV) return 0;
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_CONST) return 0;
  Term bnum = heap_read(term_val(b) + 0);
  if (term_tag(bnum) != TAG_NUM) return 0;
  *out_num = a;
  *out_div = (u32)term_val(bnum);
  return 1;
}

static int udg_match_imod_const(Term t, Term *out_num, u32 *out_div) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IMOD) return 0;
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  if (term_tag(b) != TAG_UOP || term_ext(b) != UOP_CONST) return 0;
  Term bnum = heap_read(term_val(b) + 0);
  if (term_tag(bnum) != TAG_NUM) return 0;
  *out_num = a;
  *out_div = (u32)term_val(bnum);
  return 1;
}

// Either operand order; *out_t is the non-CONST side.
static int udg_match_imul_const(Term t, Term *out_t, u32 *out_coeff) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IMUL) return 0;
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  Term other = 0; u32 coeff = 0;
  if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_CONST) {
    Term anum = heap_read(term_val(a) + 0);
    if (term_tag(anum) != TAG_NUM) return 0;
    coeff = (u32)term_val(anum); other = b;
  } else if (term_tag(b) == TAG_UOP && term_ext(b) == UOP_CONST) {
    Term bnum = heap_read(term_val(b) + 0);
    if (term_tag(bnum) != TAG_NUM) return 0;
    coeff = (u32)term_val(bnum); other = a;
  } else {
    return 0;
  }
  *out_t = other; *out_coeff = coeff;
  return 1;
}

static int udg_is_range_axis(Term t, u32 want_axis_id) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) return 0;
  return uop_range_axis_id(t) == want_axis_id;
}

static int udg_match_iconst_signed(Term t, i32 *out_v) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  Term num = heap_read(term_val(t) + 0);
  if (term_tag(num) != TAG_NUM) return 0;
  *out_v = (i32)term_val(num);
  return 1;
}

#define UDG_CONV_ADDR_MAX_LEAVES 12
// The leaves of an IADD tree, or 0 when there are more than `cap`.
static u32 udg_flatten_iadd(Term t, Term *leaves, u32 cap) {
  if (cap == 0) return 0;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_IADD) {
    leaves[0] = t;
    return 1;
  }
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  u32 na = udg_flatten_iadd(a, leaves, cap);
  if (na == 0) return 0;
  if (na >= cap) return 0;
  u32 nb = udg_flatten_iadd(b, leaves + na, cap - na);
  if (nb == 0) return 0;
  return na + nb;
}

// W address: [w_offset +] (r_out / patches) * w_s0 + r_q * w_s1, an IMUL by 1
// omitted.  Also returns the r_out and r_q axis ids for the X decode.
static int udg_decode_conv_w_addr(Term addr,
                                  u32 *out_axis_r_out,
                                  u32 *out_axis_r_q,
                                  u32 *out_patches,
                                  u32 *out_w_stride0,
                                  u32 *out_w_stride1,
                                  i32 *out_w_offset) {
  Term leaves[UDG_CONV_ADDR_MAX_LEAVES] = {0};
  u32  n_leaves = udg_flatten_iadd(addr, leaves, UDG_CONV_ADDR_MAX_LEAVES);
  if (n_leaves == 0 || n_leaves > 3) return 0;

  i32 w_off = 0;
  Term arm_co = 0, arm_q = 0;
  for (u32 i = 0; i < n_leaves; i++) {
    Term t = leaves[i];
    i32 cv;
    if (udg_match_iconst_signed(t, &cv)) {
      w_off = cv;
      continue;
    }
    Term idiv_num = 0; u32 idiv_div = 0;
    Term mul_inner = 0; u32 mul_coeff = 1;
    if (udg_match_imul_const(t, &mul_inner, &mul_coeff)
        && udg_match_idiv_const(mul_inner, &idiv_num, &idiv_div)) {
      if (term_tag(idiv_num) != TAG_UOP || term_ext(idiv_num) != UOP_RANGE) continue;
      arm_co = t;
      *out_axis_r_out = uop_range_axis_id(idiv_num);
      *out_patches    = idiv_div;
      *out_w_stride0  = mul_coeff;
      continue;
    }
    if (udg_match_idiv_const(t, &idiv_num, &idiv_div)) {
      if (term_tag(idiv_num) != TAG_UOP || term_ext(idiv_num) != UOP_RANGE) continue;
      arm_co = t;
      *out_axis_r_out = uop_range_axis_id(idiv_num);
      *out_patches    = idiv_div;
      *out_w_stride0  = 1;
      continue;
    }
    if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_RANGE) {
      arm_q = t;
      *out_axis_r_q   = uop_range_axis_id(t);
      *out_w_stride1  = 1;
      continue;
    }
    Term qmul_inner = 0; u32 qmul_coeff = 0;
    if (udg_match_imul_const(t, &qmul_inner, &qmul_coeff)
        && term_tag(qmul_inner) == TAG_UOP
        && term_ext(qmul_inner) == UOP_RANGE) {
      arm_q = t;
      *out_axis_r_q   = uop_range_axis_id(qmul_inner);
      *out_w_stride1  = qmul_coeff;
      continue;
    }
    return 0;
  }
  if (arm_co == 0 || arm_q == 0) return 0;
  if (*out_patches == 0 || *out_w_stride0 == 0 || *out_w_stride1 == 0) return 0;
  if (*out_axis_r_out == *out_axis_r_q) return 0;
  *out_w_offset = w_off;
  return 1;
}

// The X-address arms.  Nothing reads these tags: the decoders below match each
// arm directly, and an unrecognised arm fails the decode.
typedef enum {
  UDG_CONV_ARM_NONE   = 0,
  UDG_CONV_ARM_CONST  = 1,  // x_offset
  UDG_CONV_ARM_CI     = 2,  // ci * x_stride2
  UDG_CONV_ARM_BI     = 3,  // bi * x_stride_b
  UDG_CONV_ARM_H      = 4,  // (oh + kh_v) * x_stride0
  UDG_CONV_ARM_W      = 5,  // (ow + kw_v) * x_stride1
} UdgConvArmKind;

// oh = (r_out / w_out) % h_out.
static int udg_match_oh(Term t, u32 axis_r_out,
                        u32 *out_w_out, u32 *out_h_out) {
  Term mod_num = 0; u32 mod_div = 0;
  if (!udg_match_imod_const(t, &mod_num, &mod_div)) return 0;
  Term div_num = 0; u32 div_div = 0;
  if (!udg_match_idiv_const(mod_num, &div_num, &div_div)) return 0;
  if (!udg_is_range_axis(div_num, axis_r_out)) return 0;
  *out_w_out = div_div;
  *out_h_out = mod_div;
  return 1;
}

// ow = r_out % w_out.
static int udg_match_ow(Term t, u32 axis_r_out, u32 *out_w_out) {
  Term mod_num = 0; u32 mod_div = 0;
  if (!udg_match_imod_const(t, &mod_num, &mod_div)) return 0;
  if (!udg_is_range_axis(mod_num, axis_r_out)) return 0;
  *out_w_out = mod_div;
  return 1;
}

// bi = (r_out / spatial) % batch.
static int udg_match_bi(Term t, u32 axis_r_out,
                        u32 *out_spatial, u32 *out_batch) {
  Term mod_num = 0; u32 mod_div = 0;
  if (udg_match_imod_const(t, &mod_num, &mod_div)) {
    Term div_num = 0; u32 div_div = 0;
    if (udg_match_idiv_const(mod_num, &div_num, &div_div)
        && udg_is_range_axis(div_num, axis_r_out)) {
      *out_spatial = div_div;
      *out_batch   = mod_div;
      return 1;
    }
  }
  return 0;
}

// ci = (r_q / kw) / kh as two IDIVs.  index_simplify folds such a pair into
// r_q / (kw * kh), which does not match.
static int udg_match_ci(Term t, u32 axis_r_q, u32 *out_kw, u32 *out_kh) {
  Term outer_num = 0; u32 outer_div = 0;
  if (!udg_match_idiv_const(t, &outer_num, &outer_div)) return 0;
  Term inner_num = 0; u32 inner_div = 0;
  if (!udg_match_idiv_const(outer_num, &inner_num, &inner_div)) return 0;
  if (!udg_is_range_axis(inner_num, axis_r_q)) return 0;
  *out_kw = inner_div;
  *out_kh = outer_div;
  return 1;
}

// kh_v = (r_q / kw) % kh.
static int udg_match_kh_v(Term t, u32 axis_r_q, u32 *out_kw, u32 *out_kh) {
  Term mod_num = 0; u32 mod_div = 0;
  if (!udg_match_imod_const(t, &mod_num, &mod_div)) return 0;
  Term div_num = 0; u32 div_div = 0;
  if (!udg_match_idiv_const(mod_num, &div_num, &div_div)) return 0;
  if (!udg_is_range_axis(div_num, axis_r_q)) return 0;
  *out_kw = div_div;
  *out_kh = mod_div;
  return 1;
}

// kw_v = r_q % kw.
static int udg_match_kw_v(Term t, u32 axis_r_q, u32 *out_kw) {
  Term mod_num = 0; u32 mod_div = 0;
  if (!udg_match_imod_const(t, &mod_num, &mod_div)) return 0;
  if (!udg_is_range_axis(mod_num, axis_r_q)) return 0;
  *out_kw = mod_div;
  return 1;
}

// H arm: (oh + kh_v) * x_s0, either IADD order.  A bare oh (kh == 1) and a
// missing IMUL (x_s0 == 1) do not match.
static int udg_decode_h_arm(Term t, u32 axis_r_out, u32 axis_r_q,
                            u32 *out_x_s0, u32 *out_w_out, u32 *out_h_out,
                            u32 *out_kw, u32 *out_kh) {
  Term inner = 0; u32 coeff = 0;
  if (!udg_match_imul_const(t, &inner, &coeff)) return 0;
  if (coeff == 0) return 0;
  *out_x_s0 = coeff;
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_IADD) {
    return 0;
  }
  Term lhs = heap_read(term_val(inner) + 0);
  Term rhs = heap_read(term_val(inner) + 1);
  u32 kw1 = 0, kh1 = 0;
  if (udg_match_oh(lhs, axis_r_out, out_w_out, out_h_out)
      && udg_match_kh_v(rhs, axis_r_q, &kw1, &kh1)) {
    *out_kw = kw1; *out_kh = kh1;
    return 1;
  }
  if (udg_match_oh(rhs, axis_r_out, out_w_out, out_h_out)
      && udg_match_kh_v(lhs, axis_r_q, &kw1, &kh1)) {
    *out_kw = kw1; *out_kh = kh1;
    return 1;
  }
  return 0;
}

// CI arm: ci * x_s2, or a bare ci when x_s2 == 1.
static int udg_decode_ci_arm(Term t, u32 axis_r_q,
                             u32 *out_x_s2, u32 *out_kw, u32 *out_kh) {
  u32 kw1 = 0, kh1 = 0;
  if (udg_match_ci(t, axis_r_q, &kw1, &kh1)) {
    *out_x_s2 = 1; *out_kw = kw1; *out_kh = kh1; return 1;
  }
  Term inner = 0; u32 coeff = 0;
  if (!udg_match_imul_const(t, &inner, &coeff)) return 0;
  if (coeff == 0) return 0;
  if (!udg_match_ci(inner, axis_r_q, &kw1, &kh1)) return 0;
  *out_x_s2 = coeff; *out_kw = kw1; *out_kh = kh1;
  return 1;
}

// BI arm: bi * x_sb, or a bare bi when x_sb == 1.
static int udg_decode_bi_arm(Term t, u32 axis_r_out,
                             u32 *out_x_sb, u32 *out_spatial, u32 *out_batch) {
  u32 sp = 0, bt = 0;
  if (udg_match_bi(t, axis_r_out, &sp, &bt)) {
    *out_x_sb = 1; *out_spatial = sp; *out_batch = bt; return 1;
  }
  Term inner = 0; u32 coeff = 0;
  if (!udg_match_imul_const(t, &inner, &coeff)) return 0;
  if (coeff == 0) return 0;
  if (!udg_match_bi(inner, axis_r_out, &sp, &bt)) return 0;
  *out_x_sb = coeff; *out_spatial = sp; *out_batch = bt;
  return 1;
}

// 1x1 conv (kh == kw == 1): the X address keeps no IDIV/IMOD on r_q, and its H
// and BI arms are both IMUL(IMOD(IDIV(r_out, a), b), c), told apart only by
// their divisors.  A separate parser takes exactly that case.

// 1 iff IDIV or IMOD(RANGE(axis_r_q), c >= 2) occurs under `t`, searched
// through integer ops and IWHERE to depth 16.
static int udg_x_has_rq_divmod(Term t, u32 axis_r_q, int depth) {
  if (depth > 16) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  if (op == UOP_IDIV || op == UOP_IMOD) {
    Term num = heap_read(term_val(t) + 0);
    Term den = heap_read(term_val(t) + 1);
    if (term_tag(num) == TAG_UOP && term_ext(num) == UOP_RANGE
        && uop_range_axis_id(num) == axis_r_q
        && term_tag(den) == TAG_UOP && term_ext(den) == UOP_CONST) {
      Term dnum = heap_read(term_val(den) + 0);
      if (term_tag(dnum) == TAG_NUM && (u32)term_val(dnum) >= 2) return 1;
    }
  }
  if (op == UOP_RANGE || op == UOP_CONST || op == UOP_BUFFER) return 0;
  if (op == UOP_IWHERE) {
    if (udg_x_has_rq_divmod(heap_read(term_val(t) + 0), axis_r_q, depth + 1)) return 1;
    if (udg_x_has_rq_divmod(heap_read(term_val(t) + 1), axis_r_q, depth + 1)) return 1;
    return udg_x_has_rq_divmod(heap_read(term_val(t) + 2), axis_r_q, depth + 1);
  }
  if (op == UOP_IADD || op == UOP_IMUL || op == UOP_ISUB || op == UOP_IDIV
      || op == UOP_IMOD || op == UOP_ILT  || op == UOP_IAND
      || op == UOP_IOR  || op == UOP_IXOR || op == UOP_ISHR) {
    Term a = heap_read(term_val(t) + 0);
    Term b = heap_read(term_val(t) + 1);
    if (udg_x_has_rq_divmod(a, axis_r_q, depth + 1)) return 1;
    return udg_x_has_rq_divmod(b, axis_r_q, depth + 1);
  }
  return 0;
}

static int udg_x_addr_is_1x1(Term addr_x, u32 axis_r_q) {
  return !udg_x_has_rq_divmod(addr_x, axis_r_q, 0);
}

// (r_out / inner) % outer: the 1x1 H arm (w_out, h_out) or BI arm (spatial,
// batch); the caller tells them apart.
static int udg_match_imod_idiv_range(Term t, u32 axis_r_out,
                                     u32 *out_inner, u32 *out_outer) {
  Term mod_num = 0; u32 mod_div = 0;
  if (!udg_match_imod_const(t, &mod_num, &mod_div)) return 0;
  Term div_num = 0; u32 div_div = 0;
  if (!udg_match_idiv_const(mod_num, &div_num, &div_div)) return 0;
  if (!udg_is_range_axis(div_num, axis_r_out)) return 0;
  *out_inner = div_div;
  *out_outer = mod_div;
  return 1;
}

// r_out % w_out: the 1x1 W arm.
static int udg_match_imod_range(Term t, u32 axis_r_out, u32 *out_div) {
  Term mod_num = 0; u32 mod_div = 0;
  if (!udg_match_imod_const(t, &mod_num, &mod_div)) return 0;
  if (!udg_is_range_axis(mod_num, axis_r_out)) return 0;
  *out_div = mod_div;
  return 1;
}

// X leaves: CONST, CI (r_q * x_s2), W ((r_out % w_out) * x_s1) and up to two
// H/BI candidates, assigned so that spatial == h_out * w_out and batch * spatial
// == patches.  Each IMUL is absent at stride 1.
static int uop_dag_extract_conv2d_flat_shape_1x1(
    Term addr_w, Term addr_x,
    UopDagConv2dFlatShape *out) {
  u32 axis_r_out = 0, axis_r_q = 0;
  u32 patches = 0, w_s0 = 0, w_s1 = 0;
  i32 w_off = 0;
  if (!udg_decode_conv_w_addr(addr_w, &axis_r_out, &axis_r_q,
                              &patches, &w_s0, &w_s1, &w_off)) {
    return 0;
  }

  Term x_leaves[UDG_CONV_ADDR_MAX_LEAVES] = {0};
  u32 n_x = udg_flatten_iadd(addr_x, x_leaves, UDG_CONV_ADDR_MAX_LEAVES);
  if (n_x == 0) return 0;

  i32 x_off = 0;
  int got_ci = 0; u32 x_s2 = 0;
  int got_w = 0; u32 x_s1 = 0; u32 w_out_w = 0;
  struct { Term t; u32 inner; u32 outer; u32 coeff; int has_coeff; } cand[2];
  u32 n_cand = 0;

  for (u32 i = 0; i < n_x; i++) {
    Term t = x_leaves[i];
    i32 cv;
    if (udg_match_iconst_signed(t, &cv)) { x_off = cv; continue; }

    if (!got_ci) {
      if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_RANGE
          && uop_range_axis_id(t) == axis_r_q) {
        x_s2 = 1; got_ci = 1; continue;
      }
      Term inner_t = 0; u32 inner_c = 0;
      if (udg_match_imul_const(t, &inner_t, &inner_c)
          && term_tag(inner_t) == TAG_UOP && term_ext(inner_t) == UOP_RANGE
          && uop_range_axis_id(inner_t) == axis_r_q) {
        x_s2 = inner_c; got_ci = 1; continue;
      }
    }

    if (!got_w) {
      u32 w_out_t = 0;
      if (udg_match_imod_range(t, axis_r_out, &w_out_t)) {
        x_s1 = 1; w_out_w = w_out_t; got_w = 1; continue;
      }
      Term inner_t = 0; u32 inner_c = 0;
      if (udg_match_imul_const(t, &inner_t, &inner_c)
          && udg_match_imod_range(inner_t, axis_r_out, &w_out_t)) {
        x_s1 = inner_c; w_out_w = w_out_t; got_w = 1; continue;
      }
    }

    {
      u32 inner_d = 0, outer_d = 0;
      if (udg_match_imod_idiv_range(t, axis_r_out, &inner_d, &outer_d)) {
        if (n_cand >= 2) return 0;
        cand[n_cand].t         = t;
        cand[n_cand].inner     = inner_d;
        cand[n_cand].outer     = outer_d;
        cand[n_cand].coeff     = 1;
        cand[n_cand].has_coeff = 0;
        n_cand++;
        continue;
      }
      Term inner_t = 0; u32 inner_c = 0;
      if (udg_match_imul_const(t, &inner_t, &inner_c)
          && udg_match_imod_idiv_range(inner_t, axis_r_out,
                                       &inner_d, &outer_d)) {
        if (n_cand >= 2) return 0;
        cand[n_cand].t         = t;
        cand[n_cand].inner     = inner_d;
        cand[n_cand].outer     = outer_d;
        cand[n_cand].coeff     = inner_c;
        cand[n_cand].has_coeff = 1;
        n_cand++;
        continue;
      }
    }

    return 0;
  }

  // w_out == 1 folds the W arm away; not handled.
  if (!got_w) return 0;
  if (n_cand == 0) return 0;
  if (!got_ci) return 0;

  // H is (w_out, h_out) and BI (spatial, batch); batch == 1 leaves only H.
  u32 batch = 0, spatial = 0, h_out = 0, w_out = 0, x_sb = 0;
  int got_bi = 0;

  if (n_cand == 1) {
    h_out = cand[0].outer;
    w_out = cand[0].inner;
    if (w_out != w_out_w) return 0;
    if (h_out == 0 || w_out == 0) return 0;
    if ((u64)h_out * (u64)w_out != patches) return 0;
    batch = 1;
    spatial = patches;
    x_sb = 0;
  } else {
    // Exactly one pairing may fit.
    int hits = 0;
    int h_idx = -1;
    for (int try_h = 0; try_h < 2; try_h++) {
      int b_idx = 1 - try_h;
      u32 try_w_out = cand[try_h].inner;
      u32 try_h_out = cand[try_h].outer;
      u32 try_spatial = cand[b_idx].inner;
      u32 try_batch   = cand[b_idx].outer;
      if (try_w_out == 0 || try_h_out == 0 || try_batch == 0) continue;
      if (try_w_out != w_out_w) continue;
      if ((u64)try_h_out * (u64)try_w_out != try_spatial) continue;
      if ((u64)try_batch * (u64)try_spatial != patches) continue;
      hits++;
      h_idx = try_h;
      h_out = try_h_out;
      w_out = try_w_out;
      spatial = try_spatial;
      batch = try_batch;
    }
    if (hits != 1) return 0;
    int b_idx = 1 - h_idx;
    x_sb = cand[b_idx].has_coeff ? cand[b_idx].coeff : 1;
    if (cand[h_idx].has_coeff) {
      // x_s0 is read below.
    }
    got_bi = 1;
  }

  u32 x_s0 = 0;
  if (n_cand == 1) {
    x_s0 = cand[0].has_coeff ? cand[0].coeff : 1;
  } else {
    int h_idx = (cand[0].inner == w_out && cand[0].outer == h_out) ? 0 : 1;
    x_s0 = cand[h_idx].has_coeff ? cand[h_idx].coeff : 1;
  }

  u32 r_out_ext = rec_tc_find_range_extent(addr_w, axis_r_out, 0);
  u32 r_q_ext   = rec_tc_find_range_extent(addr_w, axis_r_q,   0);
  if (r_out_ext == 0 || r_q_ext == 0) return 0;
  if (r_out_ext % patches != 0) return 0;
  u32 c_out = r_out_ext / patches;
  // KRED = c_in * kh * kw = c_in.
  u32 c_in  = r_q_ext;

  out->c_out     = c_out;
  out->c_in      = c_in;
  out->kh        = 1;
  out->kw        = 1;
  out->h_out     = h_out;
  out->w_out     = w_out;
  out->batch     = batch;
  out->patches   = patches;
  out->spatial_patches = spatial;
  out->w_offset  = w_off;
  out->w_stride0 = (i32)w_s0;
  out->w_stride1 = (i32)w_s1;
  out->x_offset  = x_off;
  out->x_stride_b = (i32)x_sb;
  out->x_stride0  = (i32)x_s0;
  out->x_stride1  = (i32)x_s1;
  out->x_stride2  = (i32)x_s2;
  out->axis_r_out = axis_r_out;
  out->axis_r_q   = axis_r_q;
  (void)got_bi;
  return 1;
}

// The conv shape from the W and X INDEX_E addresses; 0 when either does not
// decode (tile.c then reads input_views).
int uop_dag_extract_conv2d_flat_shape(Term addr_w, Term addr_x,
                                      UopDagConv2dFlatShape *out) {
  if (out == NULL) return 0;
  memset(out, 0, sizeof(*out));

  u32 axis_r_out = 0, axis_r_q = 0;
  u32 patches = 0, w_s0 = 0, w_s1 = 0;
  i32 w_off = 0;
  if (!udg_decode_conv_w_addr(addr_w, &axis_r_out, &axis_r_q,
                              &patches, &w_s0, &w_s1, &w_off)) {
    return 0;
  }

  if (udg_x_addr_is_1x1(addr_x, axis_r_q)) {
    return uop_dag_extract_conv2d_flat_shape_1x1(addr_w, addr_x, out);
  }

  Term x_leaves[UDG_CONV_ADDR_MAX_LEAVES] = {0};
  u32 n_x = udg_flatten_iadd(addr_x, x_leaves, UDG_CONV_ADDR_MAX_LEAVES);
  if (n_x == 0) return 0;

  i32 x_off    = 0;
  u32 x_sb     = 0,  batch     = 0,  spatial = 0;
  u32 x_s2     = 0,  kw_ci     = 0,  kh_ci   = 0;
  u32 x_s0     = 0,  h_out_h   = 0,  w_out_h = 0,  kw_h = 0,  kh_h = 0;
  u32 x_s1     = 0,  w_out_w   = 0,  kw_w    = 0;
  int got_h    = 0,  got_w     = 0,  got_ci  = 0,  got_bi = 0;

  // X leaves: CONST (x_offset) and the H, W, CI and BI arms.  At x_s1 == 1 the
  // W arm ow + kw_v has no IMUL, so the flatten splits it into two leaves.
  Term ow_leaf = 0, kw_v_leaf = 0;
  u32 ow_w_out = 0, kw_v_kw = 0;

  for (u32 i = 0; i < n_x; i++) {
    Term t = x_leaves[i];
    i32 cv;
    if (udg_match_iconst_signed(t, &cv)) { x_off = cv; continue; }
    if (!got_h && udg_decode_h_arm(t, axis_r_out, axis_r_q,
                                   &x_s0, &w_out_h, &h_out_h, &kw_h, &kh_h)) {
      got_h = 1; continue;
    }
    // W arm: (ow + kw_v) * x_s1, either IADD order.
    if (!got_w) {
      Term inner_imul = 0; u32 inner_coeff = 0;
      if (udg_match_imul_const(t, &inner_imul, &inner_coeff)
          && term_tag(inner_imul) == TAG_UOP
          && term_ext(inner_imul) == UOP_IADD) {
        u32 wo = 0, kw1 = 0;
        Term lhs = heap_read(term_val(inner_imul) + 0);
        Term rhs = heap_read(term_val(inner_imul) + 1);
        if ((udg_match_ow(lhs, axis_r_out, &wo)
             && udg_match_kw_v(rhs, axis_r_q, &kw1))
            || (udg_match_ow(rhs, axis_r_out, &wo)
                && udg_match_kw_v(lhs, axis_r_q, &kw1))) {
          x_s1 = inner_coeff; w_out_w = wo; kw_w = kw1; got_w = 1; continue;
        }
      }
    }
    if (!got_ci && udg_decode_ci_arm(t, axis_r_q, &x_s2, &kw_ci, &kh_ci)) {
      got_ci = 1; continue;
    }
    if (!got_bi && udg_decode_bi_arm(t, axis_r_out,
                                     &x_sb, &spatial, &batch)) {
      got_bi = 1; continue;
    }
    if (ow_leaf == 0 && udg_match_ow(t, axis_r_out, &ow_w_out)) {
      ow_leaf = t; continue;
    }
    if (kw_v_leaf == 0 && udg_match_kw_v(t, axis_r_q, &kw_v_kw)) {
      kw_v_leaf = t; continue;
    }
    return 0;
  }

  if (!got_w && ow_leaf != 0 && kw_v_leaf != 0) {
    x_s1 = 1; w_out_w = ow_w_out; kw_w = kw_v_kw; got_w = 1;
  } else if (ow_leaf != 0 || kw_v_leaf != 0) {
    return 0;
  }

  // H and W are required; CI may be absent only when c_in == 1, BI only when
  // batch == 1 (checked below).
  if (!got_h || !got_w) return 0;

  if (w_out_h != w_out_w) return 0;
  if (kw_h != kw_w) return 0;
  if (got_ci) {
    if (kw_ci != kw_h) return 0;
    if (kh_ci != kh_h) return 0;
  }

  u32 kw = kw_h;
  u32 kh = kh_h;
  u32 h_out = h_out_h;
  u32 w_out = w_out_h;
  if (kw == 0 || kh == 0) return 0;
  if (h_out == 0 || w_out == 0) return 0;

  u32 r_out_ext = 0, r_q_ext = 0;
  r_out_ext = rec_tc_find_range_extent(addr_w, axis_r_out, 0);
  r_q_ext   = rec_tc_find_range_extent(addr_w, axis_r_q,   0);
  if (r_out_ext == 0 || r_q_ext == 0) return 0;
  if (r_out_ext % patches != 0) return 0;
  u32 c_out = r_out_ext / patches;
  u32 KRED  = r_q_ext;

  // KRED = c_in * kh * kw.
  if (KRED == 0 || kh * kw == 0) return 0;
  if (KRED % (kh * kw) != 0) return 0;
  u32 c_in = KRED / (kh * kw);
  if (c_in == 0) return 0;
  if (c_in > 1 && !got_ci) return 0;

  if (!got_bi) {
    batch   = 1;
    spatial = patches;
  } else {
    if (spatial == 0 || batch == 0) return 0;
    if (spatial * batch != patches) return 0;
  }
  if (spatial != h_out * w_out) return 0;

  // No BI arm: x_stride_b 0, as tile.c's input_views reader gives a 3-D X.
  if (!got_bi) x_sb = 0;

  out->c_out     = c_out;
  out->c_in      = c_in;
  out->kh        = kh;
  out->kw        = kw;
  out->h_out     = h_out;
  out->w_out     = w_out;
  out->batch     = batch;
  out->patches   = patches;
  out->spatial_patches = spatial;
  out->w_offset  = w_off;
  out->w_stride0 = (i32)w_s0;
  out->w_stride1 = (i32)w_s1;
  out->x_offset  = x_off;
  out->x_stride_b = (i32)x_sb;
  out->x_stride0  = (i32)x_s0;
  out->x_stride1  = (i32)x_s1;
  out->x_stride2  = (i32)x_s2;
  out->axis_r_out = axis_r_out;
  out->axis_r_q   = axis_r_q;
  return 1;
}

// The distinct RANGEs under `root` as (axis_id, axis_type, extent), sorted by
// axis_id, at most `cap`.  An AFTER is a leaf here (uop_arity 0).
u32 uop_dag_collect_axes(Term root, u32 *out_axis_id, u32 *out_axis_type,
                         u32 *out_extent, u32 cap) {
  if (root == 0 || term_tag(root) != TAG_UOP || cap == 0) return 0;
  // Dedup by axis_id, the first RANGE found winning: a well-formed DAG has one
  // RANGE per axis_id.
  UOpWalk w = {0};
  uop_walk_queue(&w, root);
  u32  n = 0;
  Term t;
  while ((t = uop_walk_take(&w)) != 0) {
    u32 op = term_ext(t);
    if (op == UOP_RANGE) {
      u32 aid  = uop_range_axis_id(t);
      u32 at   = uop_range_axis_type(t);
      u32 ext  = uop_range_extent(t);
      int seen = 0;
      for (u32 i = 0; i < n; i++) if (out_axis_id[i] == aid) { seen = 1; break; }
      if (!seen && n < cap) {
        out_axis_id[n]   = aid;
        out_axis_type[n] = at;
        out_extent[n]    = ext;
        n++;
      }
      continue;
    }
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID) continue;
    if (op == UOP_OPT) {
      uop_walk_queue(&w, uop_opt_target(t));
      continue;
    }
    u8  ar  = uop_arity(op);
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
      uop_walk_queue(&w, heap_read(loc + i));
    }
  }
  uop_walk_free(&w);
  for (u32 i = 1; i < n; i++) {
    u32 aid = out_axis_id[i], at = out_axis_type[i], ext = out_extent[i];
    i32 j = (i32)i - 1;
    while (j >= 0 && out_axis_id[j] > aid) {
      out_axis_id[j + 1]   = out_axis_id[j];
      out_axis_type[j + 1] = out_axis_type[j];
      out_extent[j + 1]    = out_extent[j];
      j--;
    }
    out_axis_id[j + 1]   = aid;
    out_axis_type[j + 1] = at;
    out_extent[j + 1]    = ext;
  }
  return n;
}

// === Address coefficients and the contraction classifiers ===
// An INDEX_E address read as sum(coeff * RANGE) + offset, for hand_opts and the
// BLAS contraction classifiers below.
#define UDG_CONTRACT_MAX_AXES UOP_DAG_ADDR_MAX_AXES

static int udg_addr_accumulate(UopDagAddrCoeffsView *a, u32 axis_id, u32 coeff) {
  for (u32 i = 0; i < a->n_axes; i++) {
    if (a->axis_ids[i] == axis_id) {
      // An axis in several arms sums their coefficients.
      a->coeffs[i] += coeff;
      return 1;
    }
  }
  if (a->n_axes >= UDG_CONTRACT_MAX_AXES) return 0;
  a->axis_ids[a->n_axes] = axis_id;
  a->coeffs  [a->n_axes] = coeff;
  a->n_axes++;
  return 1;
}

// Add coeff * t: RANGE, CONST, IADD, OPT and IMUL by a CONST.  index_simplify
// does not distribute IMUL over IADD (tinygrad symbolic.py:249 does), so this
// does: (oh + kh) * W gives oh and kh one coefficient, the im2col fingerprint.
static int udg_addr_decode_leaf(Term t, u32 coeff, UopDagAddrCoeffsView *out) {
  if (term_tag(t) == TAG_UOP) {
    u32 op = term_ext(t);
    if (op == UOP_RANGE) {
      return udg_addr_accumulate(out, uop_range_axis_id(t), coeff);
    }
    if (op == UOP_IADD) {
      Term a = heap_read(term_val(t) + 0);
      Term b = heap_read(term_val(t) + 1);
      if (!udg_addr_decode_leaf(a, coeff, out)) return 0;
      if (!udg_addr_decode_leaf(b, coeff, out)) return 0;
      return 1;
    }
    if (op == UOP_OPT) {
      // A renderer hint, transparent here: apply_opt_dag wraps the inner
      // RANGE of each UPCAST or UNROLL split in one.
      return udg_addr_decode_leaf(uop_opt_target(t), coeff, out);
    }
    Term inner = 0; u32 c = 0;
    if (udg_match_imul_const(t, &inner, &c)) {
      u64 prod = (u64)coeff * (u64)c;
      if (prod > 0xFFFFFFFFu) return 0;
      return udg_addr_decode_leaf(inner, (u32)prod, out);
    }
  }
  i32 ival = 0;
  if (udg_match_iconst_signed(t, &ival)) {
    out->offset += (i32)((i64)ival * (i64)coeff);
    return 1;
  }
  return 0;
}

// Per-axis coefficients and the element offset of an INDEX_E address; out->ok
// is 0 when some leaf, such as an IDIV, IMOD or IWHERE, does not decode.
void uop_dag_decode_addr_coeffs(Term addr, UopDagAddrCoeffsView *out) {
  out->n_axes = 0;
  out->offset = 0;
  out->ok     = 0;
  if (udg_addr_decode_leaf(addr, 1, out)) out->ok = 1;
}

u32 uop_dag_addr_coeff_lookup(UopDagAddrCoeffsView const *a, u32 axis_id) {
  for (u32 i = 0; i < a->n_axes; i++) {
    if (a->axis_ids[i] == axis_id) return a->coeffs[i];
  }
  return 0;
}

// Static extent (a symbolic one's upper bound) of axis `want_axis_id`, or 0.
static u32 udg_axis_extent(Term root, u32 want_axis_id) {
  if (root == 0 || term_tag(root) != TAG_UOP) return 0;
  UOpWalk w = {0};
  uop_walk_queue(&w, root);
  u32  extent = 0;
  Term t;
  while ((t = uop_walk_take(&w)) != 0) {
    u32 op = term_ext(t);
    if (op == UOP_RANGE) {
      if (uop_range_axis_id(t) == want_axis_id) {
        extent = uop_range_static_extent(t);
        break;
      }
      continue;
    }
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID) continue;
    if (op == UOP_OPT) {
      uop_walk_queue(&w, uop_opt_target(t));
      continue;
    }
    u8  ar  = uop_arity(op);
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
      uop_walk_queue(&w, heap_read(loc + i));
    }
  }
  uop_walk_free(&w);
  return extent;
}

// out[M, N] = sum_K A[K, N] * B[M, K]: N axes only in A, M axes only in B, K
// axes in both and not in the output; the M axis with the largest B stride may
// be a batch.  Only dense f32 layouts pass (thvm.h has the sgemm call).
int uop_dag_classify_contraction_shape(Term root,
                                       struct KernelEntry const *ke,
                                       UopDagContractionShape *out) {
  if (root == 0 || ke == NULL || out == NULL) return 0;
  if (ke->n_inputs < 2) return 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;

  Term buf_out  = heap_read(term_val(root) + 0);
  Term addr_out = heap_read(term_val(root) + 1);
  Term value    = heap_read(term_val(root) + 2);
  Term peeled   = udg_peel_tc_opt(value);
  Term reduce   = peeled;
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) return 0;
  u32 rkind = uop_reduce_kind(reduce);
  if (rkind != REDUCE_SUM) return 0;
  u32 n_K = uop_reduce_n_axes(reduce);
  if (n_K == 0 || n_K > UDG_CONTRACT_MAX_AXES) return 0;
  u32 red_axis_ids[UDG_CONTRACT_MAX_AXES];
  for (u32 i = 0; i < n_K; i++) red_axis_ids[i] = uop_reduce_axis(reduce, i);
  Term mul = uop_reduce_src(reduce);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  Term a_idx = heap_read(term_val(mul) + 0);
  Term b_idx = heap_read(term_val(mul) + 1);
  if (term_tag(a_idx) != TAG_UOP || term_ext(a_idx) != UOP_INDEX_E) return 0;
  if (term_tag(b_idx) != TAG_UOP || term_ext(b_idx) != UOP_INDEX_E) return 0;

  Term buf_a = heap_read(term_val(a_idx) + 0);
  Term buf_b = heap_read(term_val(b_idx) + 0);
  if (term_tag(buf_a) != TAG_UOP || term_ext(buf_a) != UOP_BUFFER) return 0;
  if (term_tag(buf_b) != TAG_UOP || term_ext(buf_b) != UOP_BUFFER) return 0;
  if (term_val(buf_a) == term_val(buf_b)) return 0;
  if (term_tag(buf_out) != TAG_UOP || term_ext(buf_out) != UOP_BUFFER) return 0;
  u32 inst_a = uop_buffer_inst_get(buf_a);
  u32 inst_b = uop_buffer_inst_get(buf_b);
  if (inst_a == 0 || inst_b == 0) return 0;
  u32 a_input = inst_a - 1;
  u32 b_input = inst_b - 1;
  if (a_input >= ke->n_inputs || b_input >= ke->n_inputs) return 0;

  Term addr_a = heap_read(term_val(a_idx) + 1);
  Term addr_b = heap_read(term_val(b_idx) + 1);

  UopDagAddrCoeffsView ca = {0}, cb = {0}, co = {0};
  uop_dag_decode_addr_coeffs(addr_a,  &ca);
  uop_dag_decode_addr_coeffs(addr_b,  &cb);
  uop_dag_decode_addr_coeffs(addr_out,&co);
  if (!ca.ok || !cb.ok || !co.ok) return 0;
  if (ca.offset != 0 || cb.offset != 0 || co.offset != 0) return 0;

  u32 K_extents[UDG_CONTRACT_MAX_AXES];
  u32 K_a_coeffs[UDG_CONTRACT_MAX_AXES];
  u32 K_b_coeffs[UDG_CONTRACT_MAX_AXES];
  u64 K_total = 1;
  for (u32 i = 0; i < n_K; i++) {
    u32 aid = red_axis_ids[i];
    u32 ac  = uop_dag_addr_coeff_lookup(&ca, aid);
    u32 bc  = uop_dag_addr_coeff_lookup(&cb, aid);
    u32 oc  = uop_dag_addr_coeff_lookup(&co, aid);
    if (ac == 0 || bc == 0 || oc != 0) return 0;
    u32 ext = udg_axis_extent(root, aid);
    if (ext == 0) return 0;
    K_extents [i] = ext;
    K_a_coeffs[i] = ac;
    K_b_coeffs[i] = bc;
    K_total *= ext;
  }
  if (K_total == 0 || K_total > 0x7fffffffULL) return 0;
  // Several K axes collapse into one when A and B nest them in the same order
  // and each is dense in both.
  u32 K_in_a_single = K_a_coeffs[0];
  u32 K_in_b_single = K_b_coeffs[0];
  if (n_K > 1) {
    u32 ord_a[UDG_CONTRACT_MAX_AXES];
    for (u32 i = 0; i < n_K; i++) ord_a[i] = i;
    for (u32 i = 1; i < n_K; i++) {
      u32 tmp = ord_a[i]; i32 j = (i32)i - 1;
      while (j >= 0 && K_a_coeffs[ord_a[j]] < K_a_coeffs[tmp]) {
        ord_a[j + 1] = ord_a[j]; j--;
      }
      ord_a[j + 1] = tmp;
    }
    u32 ord_b[UDG_CONTRACT_MAX_AXES];
    for (u32 i = 0; i < n_K; i++) ord_b[i] = i;
    for (u32 i = 1; i < n_K; i++) {
      u32 tmp = ord_b[i]; i32 j = (i32)i - 1;
      while (j >= 0 && K_b_coeffs[ord_b[j]] < K_b_coeffs[tmp]) {
        ord_b[j + 1] = ord_b[j]; j--;
      }
      ord_b[j + 1] = tmp;
    }
    for (u32 i = 0; i < n_K; i++) if (ord_a[i] != ord_b[i]) return 0;
    K_in_a_single = K_a_coeffs[ord_a[n_K - 1]];
    K_in_b_single = K_b_coeffs[ord_b[n_K - 1]];
    u64 acc_a = K_in_a_single;
    u64 acc_b = K_in_b_single;
    for (i32 i = (i32)n_K - 2; i >= 0; i--) {
      acc_a *= K_extents[ord_a[i + 1]];
      acc_b *= K_extents[ord_b[i + 1]];
      if (K_a_coeffs[ord_a[i]] != (u32)acc_a) return 0;
      if (K_b_coeffs[ord_b[i]] != (u32)acc_b) return 0;
    }
  }
  // From here K is one axis of extent K_total at the innermost K stride.
  u32 K_in_a = K_in_a_single;
  u32 K_in_b = K_in_b_single;
  u32 K_extent = (u32)K_total;

  // An output axis read by A only is an N axis, by B only an M axis.
  typedef struct { u32 axis_id; u32 extent; u32 b_coeff; u32 out_coeff; } UdgMAxis;
  typedef struct { u32 axis_id; u32 extent; u32 a_coeff; u32 out_coeff; } UdgNAxis;
  UdgMAxis M_axes[UDG_CONTRACT_MAX_AXES];
  UdgNAxis N_axes[UDG_CONTRACT_MAX_AXES];
  u32 n_M = 0, n_N = 0;
  for (u32 i = 0; i < co.n_axes; i++) {
    u32 aid = co.axis_ids[i];
    u32 oc  = co.coeffs  [i];
    if (oc == 0) return 0;
    u32 a_c = uop_dag_addr_coeff_lookup(&ca, aid);
    u32 b_c = uop_dag_addr_coeff_lookup(&cb, aid);
    u32 ext = udg_axis_extent(root, aid);
    if (ext == 0) return 0;
    if (a_c != 0 && b_c == 0) {
      if (n_N >= UDG_CONTRACT_MAX_AXES) return 0;
      N_axes[n_N].axis_id   = aid;
      N_axes[n_N].extent    = ext;
      N_axes[n_N].a_coeff   = a_c;
      N_axes[n_N].out_coeff = oc;
      n_N++;
    } else if (b_c != 0 && a_c == 0) {
      if (n_M >= UDG_CONTRACT_MAX_AXES) return 0;
      M_axes[n_M].axis_id   = aid;
      M_axes[n_M].extent    = ext;
      M_axes[n_M].b_coeff   = b_c;
      M_axes[n_M].out_coeff = oc;
      n_M++;
    } else {
      return 0;
    }
  }
  if (n_M == 0 || n_N == 0) return 0;

  for (u32 i = 0; i < ca.n_axes; i++) {
    u32 aid = ca.axis_ids[i];
    int is_K = 0;
    for (u32 j = 0; j < n_K; j++) if (red_axis_ids[j] == aid) { is_K = 1; break; }
    if (is_K) continue;
    int found = 0;
    for (u32 j = 0; j < n_N; j++) if (N_axes[j].axis_id == aid) { found = 1; break; }
    if (!found) return 0;
  }
  for (u32 i = 0; i < cb.n_axes; i++) {
    u32 aid = cb.axis_ids[i];
    int is_K = 0;
    for (u32 j = 0; j < n_K; j++) if (red_axis_ids[j] == aid) { is_K = 1; break; }
    if (is_K) continue;
    int found = 0;
    for (u32 j = 0; j < n_M; j++) if (M_axes[j].axis_id == aid) { found = 1; break; }
    if (!found) return 0;
  }

  u64 N_total = 1;
  for (u32 i = 0; i < n_N; i++) N_total *= N_axes[i].extent;
  if (N_total == 0 || N_total > 0x7fffffffULL) return 0;

  // A is (K, N) row-major: K's stride is N_total, and the N axes are dense with
  // the same strides in A and the output.
  if ((u32)N_total != K_in_a) return 0;
  for (u32 i = 1; i < n_N; i++) {
    UdgNAxis tmp = N_axes[i];
    i32 j = (i32)i - 1;
    while (j >= 0 && N_axes[j].a_coeff < tmp.a_coeff) {
      N_axes[j + 1] = N_axes[j];
      j--;
    }
    N_axes[j + 1] = tmp;
  }
  {
    u64 acc = 1;
    for (i32 i = (i32)n_N - 1; i >= 0; i--) {
      if (N_axes[i].a_coeff != (u32)acc) return 0;
      if (N_axes[i].out_coeff != (u32)acc) return 0;
      acc *= N_axes[i].extent;
    }
    if (acc != N_total) return 0;
  }

  // B is (batch, K, inner_M) row-major; with several M axes the one with the
  // largest B stride is the batch.
  for (u32 i = 1; i < n_M; i++) {
    UdgMAxis tmp = M_axes[i];
    i32 j = (i32)i - 1;
    while (j >= 0 && M_axes[j].b_coeff < tmp.b_coeff) {
      M_axes[j + 1] = M_axes[j];
      j--;
    }
    M_axes[j + 1] = tmp;
  }
  u64 inner_M_total = 1;
  u32 batch         = 1;
  if (n_M == 1) {
    inner_M_total = M_axes[0].extent;
    batch         = 1;
    // A transposed B (M stride K, K stride 1) is not routed.
    if (M_axes[0].b_coeff != 1) return 0;
    if (K_in_b != inner_M_total) return 0;
  } else {
    batch = M_axes[0].extent;
    for (u32 i = 1; i < n_M; i++) inner_M_total *= M_axes[i].extent;
    u64 acc = 1;
    for (i32 i = (i32)n_M - 1; i >= 1; i--) {
      if (M_axes[i].b_coeff != (u32)acc) return 0;
      acc *= M_axes[i].extent;
    }
    if (acc != inner_M_total) return 0;
    if (K_in_b != inner_M_total) return 0;
    u64 expect_batch_stride_b = (u64)K_extent * inner_M_total;
    if (M_axes[0].b_coeff != (u32)expect_batch_stride_b) return 0;
  }
  if (inner_M_total == 0 || inner_M_total > 0x7fffffffULL) return 0;
  u64 M_total = (u64)batch * inner_M_total;
  if (M_total > 0x7fffffffULL) return 0;

  // The output is (batch, inner_M, N) row-major.
  {
    u64 acc = N_total;
    for (i32 i = (i32)n_M - 1; i >= 0; i--) {
      if (M_axes[i].out_coeff != (u32)acc) return 0;
      acc *= M_axes[i].extent;
    }
  }

  u64 a_needed = (u64)K_extent * N_total;
  u64 b_needed = (u64)batch * K_extent * inner_M_total;
  u64 c_needed = (u64)batch * inner_M_total * N_total;
  (void)c_needed;
  if (uop_buffer_ndim(buf_a) == 0 || uop_buffer_ndim(buf_b) == 0) return 0;
  u64 a_buf_prod = 1, b_buf_prod = 1;
  for (u32 d = 0; d < uop_buffer_ndim(buf_a); d++)
    a_buf_prod *= uop_buffer_dim(buf_a, d);
  for (u32 d = 0; d < uop_buffer_ndim(buf_b); d++)
    b_buf_prod *= uop_buffer_dim(buf_b, d);
  if (a_buf_prod < a_needed) return 0;
  if (b_buf_prod < b_needed) return 0;

  u32 dt = uop_buffer_dtype(buf_out);
  if (dt != DT_FP32) return 0;
  if (uop_buffer_dtype(buf_a) != dt) return 0;
  if (uop_buffer_dtype(buf_b) != dt) return 0;

  out->dtype          = dt;
  out->a_input        = a_input;
  out->b_input        = b_input;
  out->K              = K_extent;
  out->N              = (u32)N_total;
  out->inner_M        = (u32)inner_M_total;
  out->batch          = batch;
  out->ldA            = (u32)N_total;          // A's row stride in (K, N)
  out->ldB            = (u32)inner_M_total;    // B's row stride in (K, inner_M)
  out->ldC            = (u32)N_total;          // C's row stride in (inner_M, N)
  out->batch_stride_a = 0;                     // A has no batch axis
  out->batch_stride_b = (batch == 1) ? 0 : (u32)((u64)K_extent * inner_M_total);
  out->batch_stride_c = (batch == 1) ? 0 : (u32)(inner_M_total * N_total);
  out->flags          = 0;
  return 1;
}

// C{batch,M,N} = A{batch,M,K} . B{batch,K,N}, the batch axis in all three
// (attention's per-head bmm).  Operands may be permuted views, so ld, trans and
// batch strides come from the address coefficients, with no layout check.
int uop_dag_classify_batched_gemm(Term root, struct KernelEntry const *ke,
                                  UopDagBatchedGemmShape *out) {
  if (root == 0 || ke == NULL || out == NULL) return 0;
  if (ke->n_inputs < 2) return 0;
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;
  // Checked here, not by udg_classify_matmul_store: uop_classify_matmul rejects
  // an operand that reads three axes.
  Term reduce = udg_peel_tc_opt(heap_read(term_val(root) + 2));
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) return 0;
  if (uop_reduce_kind(reduce) != REDUCE_SUM) return 0;
  if (uop_reduce_n_axes(reduce) != 1) return 0;
  u32 red_axis = uop_reduce_axis(reduce, 0);
  Term mul = uop_reduce_src(reduce);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) return 0;
  Term a_idx = heap_read(term_val(mul) + 0);
  Term b_idx = heap_read(term_val(mul) + 1);
  if (term_tag(a_idx) != TAG_UOP || term_ext(a_idx) != UOP_INDEX_E) return 0;
  if (term_tag(b_idx) != TAG_UOP || term_ext(b_idx) != UOP_INDEX_E) return 0;
  u32 k_extent = udg_axis_extent(root, red_axis);
  if (k_extent < 2) return 0;
  Term buf_a   = heap_read(term_val(a_idx) + 0);
  Term buf_b   = heap_read(term_val(b_idx) + 0);
  Term buf_out = heap_read(term_val(root) + 0);
  if (term_tag(buf_a)   != TAG_UOP || term_ext(buf_a)   != UOP_BUFFER) return 0;
  if (term_tag(buf_b)   != TAG_UOP || term_ext(buf_b)   != UOP_BUFFER) return 0;
  if (term_tag(buf_out) != TAG_UOP || term_ext(buf_out) != UOP_BUFFER) return 0;
  if (term_val(buf_a) == term_val(buf_b)) return 0;
  u32 inst_a = uop_buffer_inst_get(buf_a), inst_b = uop_buffer_inst_get(buf_b);
  if (inst_a == 0 || inst_b == 0) return 0;
  u32 a_input = inst_a - 1, b_input = inst_b - 1;
  if (a_input >= ke->n_inputs || b_input >= ke->n_inputs || a_input == b_input) return 0;

  UopDagAddrCoeffsView ca = {0}, cb = {0}, co = {0};
  uop_dag_decode_addr_coeffs(heap_read(term_val(a_idx) + 1), &ca);
  uop_dag_decode_addr_coeffs(heap_read(term_val(b_idx) + 1), &cb);
  uop_dag_decode_addr_coeffs(heap_read(term_val(root)  + 1), &co);
  if (!ca.ok || !cb.ok || !co.ok) return 0;
  if (ca.offset != 0 || cb.offset != 0 || co.offset != 0) return 0;
  if (uop_dag_addr_coeff_lookup(&ca, red_axis) == 0) return 0;
  if (uop_dag_addr_coeff_lookup(&cb, red_axis) == 0) return 0;
  if (uop_dag_addr_coeff_lookup(&co, red_axis) != 0) return 0;
  // Output axes: A only is M, B only is N, both is the batch; one of each.
  u32 m_axis = 0, n_axis = 0, batch_axis = 0, n_M = 0, n_N = 0, n_batch = 0;
  for (u32 i = 0; i < co.n_axes; i++) {
    u32 aid = co.axis_ids[i];
    if (aid == red_axis) return 0;
    u32 ac = uop_dag_addr_coeff_lookup(&ca, aid), bc = uop_dag_addr_coeff_lookup(&cb, aid);
    if      (ac != 0 && bc == 0) { m_axis = aid; n_M++; }
    else if (bc != 0 && ac == 0) { n_axis = aid; n_N++; }
    else if (ac != 0 && bc != 0) { batch_axis = aid; n_batch++; }
    else return 0;
  }
  if (n_M != 1 || n_N != 1 || n_batch != 1) return 0;
  for (u32 i = 0; i < ca.n_axes; i++) {
    u32 aid = ca.axis_ids[i];
    if (aid != red_axis && aid != m_axis && aid != batch_axis) return 0;
  }
  for (u32 i = 0; i < cb.n_axes; i++) {
    u32 aid = cb.axis_ids[i];
    if (aid != red_axis && aid != n_axis && aid != batch_axis) return 0;
  }
  u32 a_K = uop_dag_addr_coeff_lookup(&ca, red_axis), a_M = uop_dag_addr_coeff_lookup(&ca, m_axis);
  u32 b_K = uop_dag_addr_coeff_lookup(&cb, red_axis), b_N = uop_dag_addr_coeff_lookup(&cb, n_axis);
  u32 c_M = uop_dag_addr_coeff_lookup(&co, m_axis),   c_N = uop_dag_addr_coeff_lookup(&co, n_axis);
  if (a_K == 0 || a_M == 0 || b_K == 0 || b_N == 0 || c_M == 0 || c_N == 0) return 0;
  // ld and trans from whichever of an operand's two axes has unit stride.
  u32 transA, ldA;
  if      (a_K == 1) { transA = 0; ldA = a_M; }
  else if (a_M == 1) { transA = 1; ldA = a_K; }
  else return 0;
  u32 transB, ldB;
  if      (b_N == 1) { transB = 0; ldB = b_K; }
  else if (b_K == 1) { transB = 1; ldB = b_N; }
  else return 0;
  if (c_N != 1) return 0;
  u32 ldC = c_M;
  u32 M = udg_axis_extent(root, m_axis);
  u32 N = udg_axis_extent(root, n_axis);
  u32 batch = udg_axis_extent(root, batch_axis);
  if (M < 1 || N < 1 || batch < 2) return 0;   // batch<2 -> blas_try_gemm's job
  u32 dt = uop_buffer_dtype(buf_out);
  if (dt != DT_FP32) return 0;
  if (uop_buffer_dtype(buf_a) != dt || uop_buffer_dtype(buf_b) != dt) return 0;

  out->dtype   = dt;
  out->a_input = a_input; out->b_input = b_input;
  out->M = M; out->N = N; out->K = k_extent; out->batch = batch;
  out->ldA = ldA; out->ldB = ldB; out->ldC = ldC;
  out->transA = transA; out->transB = transB;
  out->batch_stride_a = uop_dag_addr_coeff_lookup(&ca, batch_axis);
  out->batch_stride_b = uop_dag_addr_coeff_lookup(&cb, batch_axis);
  out->batch_stride_c = uop_dag_addr_coeff_lookup(&co, batch_axis);
  // Per-batch element span (max addressed index + 1) for buffer bounds checks.
  out->a_span = (M - 1) * a_M + (k_extent - 1) * a_K + 1;
  out->b_span = (k_extent - 1) * b_K + (N - 1) * b_N + 1;
  out->c_span = (M - 1) * c_M + (N - 1) * c_N + 1;
  return 1;
}

// im2col conv contraction (shapes in thvm.h): the weight gradient or the forward
// conv, whose KH and KW axes share X strides with OH and OW; blas.c gathers the
// patches and runs one sgemm per batch.  tinygrad: _pool (mixin/movement.py:598).
int uop_dag_classify_im2col_contraction(Term root,
                                        struct KernelEntry const *ke,
                                        UopDagIm2colShape *out) {
#define IM2COL_REJ(tag) return 0
  if (root == 0 || ke == NULL || out == NULL) return 0;
  if (ke->n_inputs < 2) IM2COL_REJ("n_inputs<2");
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) IM2COL_REJ("not-store");

  Term buf_out  = heap_read(term_val(root) + 0);
  Term addr_out = heap_read(term_val(root) + 1);
  Term value    = heap_read(term_val(root) + 2);
  Term peeled   = udg_peel_tc_opt(value);
  Term reduce   = peeled;
  if (term_tag(reduce) != TAG_UOP || term_ext(reduce) != UOP_REDUCE) IM2COL_REJ("not-reduce");
  if (uop_reduce_kind(reduce) != REDUCE_SUM) IM2COL_REJ("not-sum");

  u32 n_K = uop_reduce_n_axes(reduce);
  if (n_K < 2 || n_K > UDG_CONTRACT_MAX_AXES) IM2COL_REJ("n_K-bounds");
  u32 red_axis_ids[UDG_CONTRACT_MAX_AXES];
  for (u32 i = 0; i < n_K; i++) red_axis_ids[i] = uop_reduce_axis(reduce, i);

  Term mul = uop_reduce_src(reduce);
  if (term_tag(mul) != TAG_UOP || term_ext(mul) != UOP_MUL) IM2COL_REJ("not-mul");
  Term a_idx = heap_read(term_val(mul) + 0);
  Term b_idx = heap_read(term_val(mul) + 1);
  if (term_tag(a_idx) != TAG_UOP || term_ext(a_idx) != UOP_INDEX_E) IM2COL_REJ("a-not-idx");
  if (term_tag(b_idx) != TAG_UOP || term_ext(b_idx) != UOP_INDEX_E) IM2COL_REJ("b-not-idx");

  Term buf_a = heap_read(term_val(a_idx) + 0);
  Term buf_b = heap_read(term_val(b_idx) + 0);
  if (term_tag(buf_a) != TAG_UOP || term_ext(buf_a) != UOP_BUFFER) IM2COL_REJ("a-not-buf");
  if (term_tag(buf_b) != TAG_UOP || term_ext(buf_b) != UOP_BUFFER) IM2COL_REJ("b-not-buf");
  if (term_val(buf_a) == term_val(buf_b)) IM2COL_REJ("a==b");
  if (term_tag(buf_out) != TAG_UOP || term_ext(buf_out) != UOP_BUFFER) IM2COL_REJ("out-not-buf");
  u32 inst_a = uop_buffer_inst_get(buf_a);
  u32 inst_b = uop_buffer_inst_get(buf_b);
  if (inst_a == 0 || inst_b == 0) IM2COL_REJ("inst-0");
  u32 a_input = inst_a - 1;
  u32 b_input = inst_b - 1;
  if (a_input >= ke->n_inputs || b_input >= ke->n_inputs) IM2COL_REJ("input-oob");

  Term addr_a = heap_read(term_val(a_idx) + 1);
  Term addr_b = heap_read(term_val(b_idx) + 1);
  UopDagAddrCoeffsView ca = {0}, cb = {0}, co = {0};
  uop_dag_decode_addr_coeffs(addr_a,  &ca);
  uop_dag_decode_addr_coeffs(addr_b,  &cb);
  uop_dag_decode_addr_coeffs(addr_out,&co);
  if (!ca.ok || !cb.ok || !co.ok) IM2COL_REJ("addr-decode");
  if (ca.offset != 0 || cb.offset != 0 || co.offset != 0) IM2COL_REJ("addr-offset");

  // An output axis with a K axis's A stride is its patch partner.  Sorted by A
  // stride, K is (outer, row, col): row and col are paired, outer is not.
  typedef struct {
    u32 aid; u32 a_coeff; u32 b_coeff; u32 extent;
    u32 patch_aid; u32 patch_extent; u32 patch_out_coeff;
    int has_patch;
  } KEntry;
  KEntry K[UDG_CONTRACT_MAX_AXES];
  for (u32 i = 0; i < n_K; i++) {
    K[i].aid = red_axis_ids[i];
    K[i].a_coeff = uop_dag_addr_coeff_lookup(&ca, K[i].aid);
    K[i].b_coeff = uop_dag_addr_coeff_lookup(&cb, K[i].aid);
    K[i].extent  = udg_axis_extent(root, K[i].aid);
    if (K[i].a_coeff == 0 || K[i].b_coeff == 0 || K[i].extent == 0) IM2COL_REJ("K-coeff-0");
    if (uop_dag_addr_coeff_lookup(&co, K[i].aid) != 0) IM2COL_REJ("K-also-out");
    K[i].has_patch = 0;
  }

  // Output axes: read by A only, a patch axis or else an N axis; by B only, M.
  typedef struct { u32 aid; u32 extent; u32 a_coeff; u32 out_coeff; } NAxisRec;
  typedef struct { u32 aid; u32 extent; u32 b_coeff; u32 out_coeff; } MAxisRec;
  NAxisRec N_axes[UDG_CONTRACT_MAX_AXES];
  MAxisRec M_axes[UDG_CONTRACT_MAX_AXES];
  u32 n_N = 0, n_M = 0, n_patch = 0;
  for (u32 i = 0; i < co.n_axes; i++) {
    u32 aid = co.axis_ids[i];
    u32 oc  = co.coeffs  [i];
    if (oc == 0) IM2COL_REJ("oc-0");
    u32 ac = uop_dag_addr_coeff_lookup(&ca, aid);
    u32 bc = uop_dag_addr_coeff_lookup(&cb, aid);
    u32 ext = udg_axis_extent(root, aid);
    if (ext == 0) IM2COL_REJ("oc-ext-0");
    if (ac != 0 && bc == 0) {
      int paired = 0;
      for (u32 j = 0; j < n_K; j++) {
        if (!K[j].has_patch && K[j].a_coeff == ac) {
          K[j].has_patch       = 1;
          K[j].patch_aid       = aid;
          K[j].patch_extent    = ext;
          K[j].patch_out_coeff = oc;
          n_patch++;
          paired = 1;
          break;
        }
      }
      if (!paired) {
        if (n_N >= UDG_CONTRACT_MAX_AXES) IM2COL_REJ("n_N-overflow");
        N_axes[n_N].aid       = aid;
        N_axes[n_N].extent    = ext;
        N_axes[n_N].a_coeff   = ac;
        N_axes[n_N].out_coeff = oc;
        n_N++;
      }
    } else if (bc != 0 && ac == 0) {
      if (n_M >= UDG_CONTRACT_MAX_AXES) IM2COL_REJ("n_M-overflow");
      M_axes[n_M].aid       = aid;
      M_axes[n_M].extent    = ext;
      M_axes[n_M].b_coeff   = bc;
      M_axes[n_M].out_coeff = oc;
      n_M++;
    } else {
      IM2COL_REJ("ac-bc-both-or-neither");
    }
  }
  // No patch axis: the plain contraction's case.
  if (n_patch == 0) IM2COL_REJ("n_patch=0");
  if (n_M != 1) IM2COL_REJ("n_M!=1");
  if (n_N > 1) IM2COL_REJ("n_N>1");
  if (n_patch > 2) IM2COL_REJ("n_patch>2");

  for (u32 i = 1; i < n_K; i++) {
    KEntry tmp = K[i];
    i32 j = (i32)i - 1;
    while (j >= 0 && K[j].a_coeff < tmp.a_coeff) {
      K[j+1] = K[j]; j--;
    }
    K[j+1] = tmp;
  }
  if (K[n_K-1].a_coeff != 1) IM2COL_REJ("K_col.a!=1");
  if (n_K != 3) IM2COL_REJ("n_K!=3");
  if (K[0].has_patch != 0) IM2COL_REJ("K0-paired");
  if (K[1].has_patch != 1) IM2COL_REJ("K1-unpaired");
  if (K[2].has_patch != 1) IM2COL_REJ("K2-unpaired");

  // BWD has K = (B, OH, OW), patch axes KH and KW, and Cout at dY stride OH*OW;
  // FWD has K = (Cin, KH, KW), patch axes OH and OW, and Cout at W stride
  // Cin*KH*KW.  Cout's stride in operand B picks the direction.
  u32 K_inner = K[2].extent;             // sort-order K[2] = KW (FWD) / OW (BWD)
  u32 K_middle = K[1].extent;            // sort-order K[1] = KH (FWD) / OH (BWD)
  u32 K_outer_ext = K[0].extent;
  if (K[2].b_coeff != 1) IM2COL_REJ("K2.b!=1");
  if (K[1].b_coeff != K_inner) IM2COL_REJ("K1.b!=K2.ext");
  u32 m_bcoeff_bwd = (u32)((u64)K_inner * K_middle);
  u32 m_bcoeff_fwd = (u32)((u64)K_outer_ext * K_inner * K_middle);
  int forward_conv;
  if (M_axes[0].b_coeff == m_bcoeff_bwd) {
    forward_conv = 0;
    if (K[0].b_coeff !=
        (u32)((u64)M_axes[0].extent * K_inner * K_middle)) IM2COL_REJ("bwd:K0.b!=M*OH*OW");
  } else if (M_axes[0].b_coeff == m_bcoeff_fwd) {
    forward_conv = 1;
    if (K[0].b_coeff != (u32)((u64)K_inner * K_middle)) IM2COL_REJ("fwd:K0.b!=KH*KW");
    if (n_N != 1) IM2COL_REJ("fwd:n_N!=1");  // BWD allows 0 or 1
  } else {
    IM2COL_REJ("M.b-no-match");
  }

  // Extents by role: OH, OW are output pixels and KH, KW the kernel window.
  u32 OH_ext, OW_ext, KH_ext, KW_ext;
  if (forward_conv) {
    OH_ext = K[1].patch_extent;  OW_ext = K[2].patch_extent;
    KH_ext = K_middle;           KW_ext = K_inner;
  } else {
    OH_ext = K_middle;           OW_ext = K_inner;
    KH_ext = K[1].patch_extent;  KW_ext = K[2].patch_extent;
  }

  // BWD: K_outer is the batch and the N axis (if any) Cin; FWD: the N axis is
  // the batch and K_outer Cin.
  u32 X_Cin_stride;       // X stride per Cin step (= H*W)
  u32 N_extent_used;      // Cin extent (1 if no explicit Cin axis)
  u32 X_outer_stride;     // X stride per outer-batch step (= Cin*H*W)
  u32 N_outer_extent;     // outer-batch loop bound (B in both forms)
  if (forward_conv) {
    X_Cin_stride   = K[0].a_coeff;       // K_outer = Cin axis
    N_extent_used  = K_outer_ext;
    X_outer_stride = N_axes[0].a_coeff;  // clean-N = B axis
    N_outer_extent = N_axes[0].extent;
  } else if (n_N == 1) {
    X_Cin_stride   = N_axes[0].a_coeff;  // clean-N = Cin axis
    N_extent_used  = N_axes[0].extent;
    X_outer_stride = K[0].a_coeff;       // K_outer = B axis
    N_outer_extent = K_outer_ext;
  } else {
    X_Cin_stride   = K[0].a_coeff;       // Cin=1 -> one H*W slab
    N_extent_used  = 1;
    X_outer_stride = K[0].a_coeff;
    N_outer_extent = K_outer_ext;
  }
  if (X_Cin_stride == 0 || X_outer_stride == 0) IM2COL_REJ("X-stride-0");

  u32 X_W = K[1].a_coeff;          // K_middle a-stride = W
  if (X_W == 0) IM2COL_REJ("X_W=0");
  if (X_Cin_stride % X_W != 0) IM2COL_REJ("X_Cin%X_W");
  u32 X_H = X_Cin_stride / X_W;
  if (X_H == 0) IM2COL_REJ("X_H=0");
  if (X_outer_stride % X_Cin_stride != 0) IM2COL_REJ("X_outer%X_Cin");
  if (X_outer_stride / X_Cin_stride != N_extent_used) IM2COL_REJ("X_outer/X_Cin!=N");
  if (OH_ext + KH_ext > X_H + 1) IM2COL_REJ("OH+KH>X_H+1");
  if (OW_ext + KW_ext > X_W + 1) IM2COL_REJ("OW+KW>X_W+1");

  // Row-major output: BWD dW (Cout, Cin, KH, KW), FWD out (B, Cout, OH, OW).
  u64 N_total = (u64)N_extent_used * KH_ext * KW_ext;     // sgemm N in BWD, K in FWD
  u64 K_pixels = (u64)OH_ext * OW_ext;                    // sgemm K in BWD, N in FWD
  if (N_total == 0 || N_total > 0x7fffffffULL) IM2COL_REJ("N_total-bounds");
  if (forward_conv) {
    if (M_axes[0].out_coeff != (u32)K_pixels)                  IM2COL_REJ("fwd:M.oc");
    if (N_axes[0].out_coeff != (u32)((u64)M_axes[0].extent * K_pixels))
                                                               IM2COL_REJ("fwd:N.oc");
    if (K[1].patch_out_coeff != OW_ext)                        IM2COL_REJ("fwd:OH.oc");
    if (K[2].patch_out_coeff != 1)                             IM2COL_REJ("fwd:OW.oc");
  } else {
    if (M_axes[0].out_coeff != (u32)N_total)                   IM2COL_REJ("bwd:M.oc");
    if (n_N == 1 && N_axes[0].out_coeff != (u32)((u64)KH_ext * KW_ext))
                                                               IM2COL_REJ("bwd:N.oc");
    if (K[1].patch_out_coeff != KW_ext)                        IM2COL_REJ("bwd:KH.oc");
    if (K[2].patch_out_coeff != 1)                             IM2COL_REJ("bwd:KW.oc");
  }

  // A is X (N_outer, Cin, H*W); B is dY (BWD) or W (FWD).
  u64 a_need = (u64)N_outer_extent * X_Cin_stride;
  u64 b_need = forward_conv ? (u64)M_axes[0].extent * N_total
                            : (u64)N_outer_extent * M_axes[0].extent * K_pixels;
  if (uop_buffer_ndim(buf_a) == 0 || uop_buffer_ndim(buf_b) == 0) IM2COL_REJ("buf-ndim-0");
  u64 a_buf_prod = 1, b_buf_prod = 1;
  for (u32 d = 0; d < uop_buffer_ndim(buf_a); d++) a_buf_prod *= uop_buffer_dim(buf_a, d);
  for (u32 d = 0; d < uop_buffer_ndim(buf_b); d++) b_buf_prod *= uop_buffer_dim(buf_b, d);
  if (a_buf_prod < a_need) IM2COL_REJ("a-buf-too-small");
  if (b_buf_prod < b_need) IM2COL_REJ("b-buf-too-small");

  u32 dt = uop_buffer_dtype(buf_out);
  if (dt != DT_FP32) IM2COL_REJ("dt!=fp32");
  if (uop_buffer_dtype(buf_a) != dt) IM2COL_REJ("a-dt");
  if (uop_buffer_dtype(buf_b) != dt) IM2COL_REJ("b-dt");

  // Role extents, not sort order, so one dispatcher serves BWD and FWD.
  out->dtype              = dt;
  out->a_input            = a_input;
  out->b_input            = b_input;
  out->M                  = M_axes[0].extent;
  out->N_patchless        = N_extent_used;
  out->KH_total           = (u32)((u64)KH_ext * KW_ext);
  out->N                  = (u32)N_total;
  out->K_row              = OH_ext;          // OH (output-pixel rows)
  out->K_col              = OW_ext;          // OW (output-pixel cols)
  out->K_outer            = K_outer_ext;
  out->X_W                = X_W;
  out->X_H                = X_H;
  out->X_Cin_stride       = X_Cin_stride;
  out->X_outer_stride     = X_outer_stride;
  out->Y_outer_stride     = forward_conv ? 0u : K[0].b_coeff;
  out->Y_M_stride         = M_axes[0].b_coeff;
  out->out_M_stride       = M_axes[0].out_coeff;
  out->patch_n            = 2;
  out->patch_extent[0]    = KH_ext;          // KH (kernel rows)
  out->patch_extent[1]    = KW_ext;          // KW (kernel cols)
  out->patch_out_coeff[0] = forward_conv ? OW_ext : KW_ext;  // OH.oc or KH.oc
  out->patch_out_coeff[1] = 1;
  out->patch_a_stride[0]  = X_W;             // KH stride in X (= image width)
  out->patch_a_stride[1]  = 1;               // KW stride in X
  out->forward_conv       = (u32)forward_conv;
  out->N_outer_extent     = N_outer_extent;
  out->N_outer_out_stride = forward_conv ? N_axes[0].out_coeff : 0u;
  return 1;
#undef IM2COL_REJ
}

// The distinct INDEX_E addresses under `root`, at most `cap`: the counterpart of
// tinygrad's Scheduler.bufs (codegen/opt/postrange.py:250).
u32 uop_dag_collect_index_e_addrs(Term root, Term *out_addrs, u32 cap) {
  if (root == 0 || term_tag(root) != TAG_UOP || out_addrs == NULL || cap == 0) {
    return 0;
  }
  // hand_opts iterates the addresses, so they keep a plain depth-first walk's
  // first-pop order (uop_walk_take).
  UOpWalk w = {0};
  uop_walk_queue(&w, root);
  u32  n = 0;
  Term t;
  while ((t = uop_walk_take(&w)) != 0) {
    u32 op = term_ext(t);
    if (op == UOP_INDEX_E) {
      Term addr = heap_read(term_val(t) + 1);
      int seen = 0;
      for (u32 i = 0; i < n; i++) if (out_addrs[i] == addr) { seen = 1; break; }
      if (!seen && n < cap) out_addrs[n++] = addr;
      // Keep descending: an address may hold a nested INDEX_E.
    }
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID || op == UOP_RANGE) {
      continue;
    }
    if (op == UOP_OPT) {
      uop_walk_queue(&w, uop_opt_target(t));
      continue;
    }
    u8  ar  = uop_arity(op);
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
      uop_walk_queue(&w, heap_read(loc + i));
    }
  }
  uop_walk_free(&w);
  return n;
}
