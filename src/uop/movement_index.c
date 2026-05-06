// uop/movement_index.c - per-USE movement-chain INDEX resolver.
//
// `uop_resolve_movement_chain` walks a movement-op chain outside-in,
// transforming the consumer's iter context to the iter context the
// underlying buffer expects.  Mirrors tinygrad's apply_movement_op
// (indexing.py:128-145): each movement op's effect on a consumer's
// iters is a small, local rewrite.
//
// Movement ops stay in the UOp DAG.  This helper is called per-USE
// during lowering so each consumer sees its own resolved INDEX
// expression.  Eager DAG-layer rewriting would split buffer
// materializations the schedule wants to keep.
//
// Caller contract:
//   - Pass `src` = the term seen at the consumer's input slot
//     (may be a chain of UOP_PERMUTE / RESHAPE / EXPAND / PAD /
//     SHRINK / FLIP wrapping any non-movement bottom).
//   - Pass `iters[ndim]` = consumer's per-axis iter expressions
//     (typically UOP_RANGE leaves, but any UOp tree is fine).
//   - Pass `valid_mask_io` = pointer to a Term holding the running
//     PAD bounds-check expression.  Initialise to 0 (no constraint);
//     PAD steps accumulate IAND'd ILT checks.  Pass NULL to disallow
//     PAD (helper bails when it hits one).
//   - On return, `iters[*ndim_io]` is the iter context for the
//     bottom buffer; the function value is that bottom term.
//   - Returns 0 on a shape mismatch / unsupported op so the caller
//     can bail.

// === UOP_PERMUTE ===
// perm[d] = which input axis becomes output axis d.  Going from
// out_iters to in_iters: in_iters[perm[d]] = out_iters[d].
static int uop_apply_permute_iters(Term src, Term *iters, u32 *ndim_io) {
  u64 loc = term_val(src);
  Term ndim_cell = heap_read(loc + 1);
  if (term_tag(ndim_cell) != TAG_NUM) return 0;
  u32 ndim = (u32)term_val(ndim_cell);
  if (ndim != *ndim_io || ndim > MAX_DIM) return 0;
  Term scratch[MAX_DIM] = {0};
  for (u32 d = 0; d < ndim; d++) {
    Term p_cell = heap_read(loc + 2 + d);
    if (term_tag(p_cell) != TAG_NUM) return 0;
    u32 src_axis = (u32)term_val(p_cell);
    if (src_axis >= ndim) return 0;
    scratch[src_axis] = iters[d];
  }
  for (u32 d = 0; d < ndim; d++) iters[d] = scratch[d];
  return 1;
}

// === UOP_EXPAND ===
// Per axis, if src_dim != out_dim then the axis is broadcast: the
// inner sees iter=0 along that axis.  Rank may shrink (out_ndim >
// src_ndim) -- trailing iters drop.
static int uop_apply_expand_iters(Term src, Term *iters, u32 *ndim_io) {
  u64 loc = term_val(src);
  u32 ndim = (u32)term_val(heap_read(loc + 1));
  if (ndim != *ndim_io || ndim > MAX_DIM) return 0;
  Term inner = heap_read(loc + 0);
  Shape src_shape;
  if (!term_shape_in(inner, 0, &src_shape)) return 0;
  if (src_shape.ndim > MAX_DIM || src_shape.ndim > ndim) return 0;
  Term zero = uop_const(DT_INT32, 0);
  for (u32 d = 0; d < src_shape.ndim; d++) {
    u32 out_dim = (u32)term_val(heap_read(loc + 2 + d));
    if (src_shape.dims[d] != out_dim) iters[d] = zero;
  }
  *ndim_io = src_shape.ndim;
  return 1;
}

// === UOP_RESHAPE ===
// Compose flat = sum_d (out_iters[d] * out_strides[d]) row-major,
// then decompose into in_iters via in_strides + src_dims.  Mirrors
// rngs_ctx_reshape in rangeify.c.
static int uop_apply_reshape_iters(Term src, Term *iters, u32 *ndim_io) {
  u64 loc = term_val(src);
  u32 out_ndim = (u32)term_val(heap_read(loc + 1));
  if (out_ndim != *ndim_io || out_ndim == 0 || out_ndim > MAX_DIM) return 0;
  Term inner = heap_read(loc + 0);
  Shape src_shape;
  if (!term_shape_in(inner, 0, &src_shape)) return 0;
  if (src_shape.ndim == 0 || src_shape.ndim > MAX_DIM) return 0;

  u32 out_dims[MAX_DIM];
  for (u32 d = 0; d < out_ndim; d++) {
    out_dims[d] = (u32)term_val(heap_read(loc + 2 + d));
  }
  // Row-major strides: stride[d] = product(dims[d+1..]).
  u32 out_strides[MAX_DIM];
  out_strides[out_ndim - 1] = 1;
  for (u32 d = out_ndim - 1; d > 0; d--) {
    out_strides[d - 1] = out_strides[d] * out_dims[d];
  }
  // Build flat = sum (iters[d] * stride[d]).  0 sentinel = "empty so far".
  Term flat = 0;
  for (u32 d = 0; d < out_ndim; d++) {
    Term contrib = iters[d];
    if (out_strides[d] != 1) {
      Term s = uop_const(DT_INT32, out_strides[d]);
      contrib = uop_int_binary(UOP_IMUL, contrib, s);
    }
    flat = (flat == 0) ? contrib
                       : uop_int_binary(UOP_IADD, flat, contrib);
  }
  if (flat == 0) flat = uop_const(DT_INT32, 0);

  u32 in_strides[MAX_DIM];
  in_strides[src_shape.ndim - 1] = 1;
  for (u32 d = src_shape.ndim - 1; d > 0; d--) {
    in_strides[d - 1] = in_strides[d] * src_shape.dims[d];
  }
  for (u32 d = 0; d < src_shape.ndim; d++) {
    Term coord = flat;
    if (in_strides[d] != 1) {
      Term s = uop_const(DT_INT32, in_strides[d]);
      coord = uop_int_binary(UOP_IDIV, coord, s);
    }
    if (d != 0) {
      Term dc = uop_const(DT_INT32, src_shape.dims[d]);
      coord = uop_int_binary(UOP_IMOD, coord, dc);
    }
    iters[d] = coord;
  }
  *ndim_io = src_shape.ndim;
  return 1;
}

// === UOP_PAD ===
// Per axis, in_iter[d] = out_iter[d] - begin[d], plus a bounds check
// `(out_iter >= begin) & (out_iter < begin + src_dim)` accumulated
// into *valid_mask_io.  Rank-preserving.  Mirrors rngs_ctx_pad +
// emit_pad_bounds_mask.
static int uop_apply_pad_iters(Term src, Term *iters, u32 *ndim_io,
                               Term *valid_mask_io) {
  if (valid_mask_io == NULL) return 0;
  u64 loc = term_val(src);
  u32 ndim = (u32)term_val(heap_read(loc + 1));
  if (ndim != *ndim_io || ndim > MAX_DIM) return 0;
  Term inner = heap_read(loc + 0);
  Shape src_shape;
  if (!term_shape_in(inner, 0, &src_shape)) return 0;
  if (src_shape.ndim != ndim) return 0;
  Term one = uop_const(DT_INT32, 1);
  for (u32 d = 0; d < ndim; d++) {
    u32 begin   = (u32)term_val(heap_read(loc + 2 + 2 * d));
    u32 src_dim = src_shape.dims[d];
    Term ref = iters[d];
    Term src_ref = ref;
    if (begin != 0) {
      Term c = uop_const(DT_INT32, begin);
      src_ref = uop_int_binary(UOP_ISUB, ref, c);
    }
    iters[d] = src_ref;
    Term hi_lim  = uop_const(DT_INT32, begin + src_dim);
    Term axis_ok = uop_int_binary(UOP_ILT, ref, hi_lim);
    if (begin > 0) {
      Term lo_lim = uop_const(DT_INT32, begin);
      Term lt_lo  = uop_int_binary(UOP_ILT, ref, lo_lim);
      Term ge_lo  = uop_int_binary(UOP_ISUB, one, lt_lo);
      axis_ok     = uop_int_binary(UOP_IAND, axis_ok, ge_lo);
    }
    *valid_mask_io = (*valid_mask_io == 0)
                       ? axis_ok
                       : uop_int_binary(UOP_IAND, *valid_mask_io, axis_ok);
  }
  return 1;
}

// === UOP_SHRINK ===
// Per axis, in_iter[d] = out_iter[d] + begin[d].  No bounds check
// (SHRINK never reads out-of-bounds).  Rank-preserving.
static int uop_apply_shrink_iters(Term src, Term *iters, u32 *ndim_io) {
  u64 loc = term_val(src);
  u32 ndim = (u32)term_val(heap_read(loc + 1));
  if (ndim != *ndim_io || ndim > MAX_DIM) return 0;
  for (u32 d = 0; d < ndim; d++) {
    u32 begin = (u32)term_val(heap_read(loc + 2 + 2 * d));
    if (begin == 0) continue;
    Term c = uop_const(DT_INT32, begin);
    iters[d] = uop_int_binary(UOP_IADD, iters[d], c);
  }
  return 1;
}

// === UOP_FLIP ===
// Per flipped axis, in_iter[d] = (extent - 1) - out_iter[d].
// extent comes from inner src's shape.  Rank-preserving.
static int uop_apply_flip_iters(Term src, Term *iters, u32 *ndim_io) {
  u64 loc = term_val(src);
  Term mask_cell = heap_read(loc + 1);
  if (term_tag(mask_cell) != TAG_NUM) return 0;
  u32 mask = (u32)term_val(mask_cell);
  if (mask == 0) return 1;  // identity flip; constructor would have elided
  Term inner = heap_read(loc + 0);
  Shape src_shape;
  if (!term_shape_in(inner, 0, &src_shape)) return 0;
  if (src_shape.ndim != *ndim_io || src_shape.ndim > MAX_DIM) return 0;
  for (u32 d = 0; d < src_shape.ndim; d++) {
    if ((mask & (1u << d)) == 0) continue;
    u32 ext = src_shape.dims[d] > 0 ? src_shape.dims[d] : 1;
    Term c = uop_const(DT_INT32, ext - 1);
    iters[d] = uop_int_binary(UOP_ISUB, c, iters[d]);
  }
  return 1;
}

// === Driver ===
fn Term uop_resolve_movement_chain(Term src, Term *iters, u32 *ndim_io,
                                   Term *valid_mask_io) {
  for (u32 guard = 0; guard < (1u << 20); guard++) {
    if (term_tag(src) != TAG_UOP) return src;
    u32 op = term_ext(src);
    int ok = 1;
    switch (op) {
      case UOP_PERMUTE: ok = uop_apply_permute_iters(src, iters, ndim_io); break;
      case UOP_EXPAND:  ok = uop_apply_expand_iters (src, iters, ndim_io); break;
      case UOP_RESHAPE: ok = uop_apply_reshape_iters(src, iters, ndim_io); break;
      case UOP_PAD:     ok = uop_apply_pad_iters    (src, iters, ndim_io,
                                                     valid_mask_io); break;
      case UOP_SHRINK:  ok = uop_apply_shrink_iters (src, iters, ndim_io); break;
      case UOP_FLIP:    ok = uop_apply_flip_iters   (src, iters, ndim_io); break;
      default: return src;
    }
    if (!ok) return 0;
    src = heap_read(term_val(src) + 0);
  }
  return 0;  // unreachable in practice; chain depth-limit guard.
}
