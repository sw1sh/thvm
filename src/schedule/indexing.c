// schedule/indexing.c - movement-op range swizzlers: the per-op cases of
// tinygrad's apply_movement_op (schedule/indexing.py:171-187 at 365f87a8a).
//
// Each maps the per-axis index expressions a consumer uses (out_rngs, one
// per output axis) to the producer's (in_rngs, filled here; never in place,
// PERMUTE would clobber it).  Only RESHAPE changes the rank, and ndim is at
// most MAX_DIM.  The constructors hash-cons, so equal swizzles yield the
// same Terms.

fn void apply_movement_op_shrink(u32 ndim,
                                  u32 const *begin_end,
                                  Term const *out_rngs,
                                  Term *in_rngs) {
  // indexing.py:173: a if off == 0 else a+off.  visit() (materialize.c) also
  // turns a SHRINK of a buffer into an offset view slot (view_apply_shrink),
  // but unified_fold_chain_scan flags that slot input_chain_composed, so the
  // backend binds the raw buffer and only this index applies the offset.
  for (u32 i = 0; i < ndim; i++) {
    // A kvar-packed begin (KVAR_FLAG) is a runtime row offset, as in a
    // decode's posTable[pos:pos+1]: use the bound value (kvar_hi if
    // unbound), as view_apply_shrink does; a literal begin passes through.
    u32 ss = kvar_extent_runtime(begin_end[2 * i]);
    if (ss == 0) {
      in_rngs[i] = out_rngs[i];
    } else {
      Term ss_const = uop_const(DT_INT32, ss);
      in_rngs[i] = uop_int_binary(UOP_IADD, out_rngs[i], ss_const);
    }
  }
}

fn void apply_movement_op_permute(u32 ndim,
                                   u32 const *perm,
                                   Term const *out_rngs,
                                   Term *in_rngs) {
  // indexing.py:174: rngs[p] for p in argsort(perm), that is
  // in_rngs[i] = out_rngs[perm^-1[i]].
  u32 inv[MAX_DIM] = {0};
  for (u32 i = 0; i < ndim; i++) inv[perm[i]] = i;
  for (u32 i = 0; i < ndim; i++) in_rngs[i] = out_rngs[inv[i]];
}

fn void apply_movement_op_flip(u32 ndim,
                                u32 const *in_shape,
                                u32 const *flip_mask,
                                Term const *out_rngs,
                                Term *in_rngs) {
  for (u32 i = 0; i < ndim; i++) {
    if ((flip_mask[i] & 1u) == 0) {
      in_rngs[i] = out_rngs[i];
    } else {
      // indexing.py:175: (s-1)-a
      Term sm1 = uop_const(DT_INT32, (in_shape[i] > 0) ? (in_shape[i] - 1) : 0);
      in_rngs[i] = uop_int_binary(UOP_ISUB, sm1, out_rngs[i]);
    }
  }
}

// tinygrad's EXPAND only prepends axes (indexing.py:176); here the rank is
// kept and each broadcast axis reads index 0.
fn void apply_movement_op_expand(u32 ndim,
                                  u32 const *in_shape,
                                  u32 const *out_shape,
                                  Term const *out_rngs,
                                  Term *in_rngs) {
  for (u32 i = 0; i < ndim; i++) {
    if (in_shape[i] == out_shape[i]) {
      in_rngs[i] = out_rngs[i];
    } else {
      in_rngs[i] = uop_const(DT_INT32, 0);
    }
  }
}

fn void apply_movement_op_pad(u32 ndim,
                               u32 const *in_shape,
                               u32 const *begin_end,
                               Term const *out_rngs,
                               Term *in_rngs) {
  // indexing.py:177-181: r on an unpadded axis, else
  // WHERE((r >= s) & (r < sh+s), r-s, INVALID), with r >= s as ILT(s-1, r)
  // and the lo clause dropped at s == 0.
  for (u32 i = 0; i < ndim; i++) {
    u32 ss = begin_end[2 * i];
    u32 ee = begin_end[2 * i + 1];
    if (ss == 0 && ee == 0) {
      in_rngs[i] = out_rngs[i];
      continue;
    }
    Term r        = out_rngs[i];
    Term hi_bound = uop_const(DT_INT32, in_shape[i] + ss);
    Term cond_hi  = uop_int_binary(UOP_ILT, r, hi_bound);
    Term cond;
    if (ss == 0) {
      cond = cond_hi;
    } else {
      Term s_m1   = uop_const(DT_INT32, ss - 1);
      Term cond_lo = uop_int_binary(UOP_ILT, s_m1, r);
      cond = uop_int_binary(UOP_IAND, cond_lo, cond_hi);
    }
    Term shifted;
    if (ss == 0) {
      shifted = r;
    } else {
      Term ss_const = uop_const(DT_INT32, ss);
      shifted = uop_int_binary(UOP_ISUB, r, ss_const);
    }
    in_rngs[i] = uop_iwhere(cond, shifted, uop_invalid());
  }
}

static Term reshape_const_zero(void) {
  return uop_const(DT_INT32, 0);
}

// RESHAPE takes tinygrad's PLACEHOLDER round-trip (indexing.py:182-185):
// each RANGE leaf of out_rngs becomes a fresh placeholder RANGE of the same
// extent, the decompose runs on those so the divmod-recombine folds
// (uop/index_simplify.c) fire at construction, and the real leaves are put
// back.  Placeholder axis_ids sit far above the ones RU_RANGE_IDX_COUNTER
// hands out, so the inverse substitution is exact.
#define RESHAPE_PLACEHOLDER_AXIS_BASE 0x60000000u

// Appends t's distinct RANGE leaves (axis_id, extent, axis_type) to the out_*
// arrays and returns *n, which becomes (u32)-1 past cap.  axis_type is part of
// uop_range's hash-cons key, so the rebuilt leaf must carry it.
static u32 reshape_collect_distinct_ranges(Term t, u32 *out_aids,
                                           u32 *out_ext, u32 *out_atype,
                                           u32 *n, u32 cap, u32 depth) {
  if (depth > 64) return *n;
  if (term_tag(t) != TAG_UOP) return *n;
  u8 op = term_ext(t);
  if (op == UOP_RANGE) {
    u32 aid = uop_range_axis_id(t);
    for (u32 i = 0; i < *n; i++) {
      if (out_aids[i] == aid) return *n;
    }
    if (*n >= cap) { *n = (u32)-1; return *n; }
    out_aids[*n]  = aid;
    out_ext[*n]   = uop_range_extent(t);
    out_atype[*n] = uop_range_axis_type(t);
    (*n)++;
    return *n;
  }
  if (op == UOP_CONST || op == UOP_INVALID || op == UOP_BUFFER
      || op == UOP_BUFFERIZE) {
    return *n;
  }
  u8 ar = uop_arity(op);
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar; i++) {
    reshape_collect_distinct_ranges(heap_read(loc + i), out_aids, out_ext,
                                    out_atype, n, cap, depth + 1);
    if (*n == (u32)-1) return *n;
  }
  return *n;
}

// apply_movement_op_reshape through the placeholder round-trip; also 0 when
// out_rngs hold more than 2 * MAX_DIM distinct RANGE leaves.
fn int apply_movement_op_reshape_composed(u32 out_ndim, u32 const *out_shape,
                                          u32 in_ndim,  u32 const *in_shape,
                                          Term const *out_rngs, Term *in_rngs) {
  if (out_ndim > MAX_DIM || in_ndim > MAX_DIM) return 0;

  u32 real_aids[2 * MAX_DIM];
  u32 real_ext[2 * MAX_DIM];
  u32 real_atype[2 * MAX_DIM];
  u32 n_real = 0;
  for (u32 a = 0; a < out_ndim; a++) {
    reshape_collect_distinct_ranges(out_rngs[a], real_aids, real_ext,
                                    real_atype, &n_real, 2 * MAX_DIM, 0);
    if (n_real == (u32)-1) return 0;
  }

  // real_rng[i] keeps its axis_type so it is the very Term in out_rngs: a
  // REDUCE range rebuilt as KAX_LOOP would miss the substitution and stay a
  // plain loop.
  Term real_rng[2 * MAX_DIM];
  Term placeholder[2 * MAX_DIM];
  for (u32 i = 0; i < n_real; i++) {
    real_rng[i]    = uop_range(real_aids[i], real_atype[i], real_ext[i]);
    placeholder[i] = uop_range(RESHAPE_PLACEHOLDER_AXIS_BASE + i, KAX_LOOP,
                               real_ext[i]);
  }

  // The simplifying constructors settle commutative flips early, like
  // tinygrad's UOp.sink(*rngs).simplify() (indexing.py:183).
  Term sub_out[MAX_DIM];
  for (u32 a = 0; a < out_ndim; a++) {
    Term e = out_rngs[a];
    for (u32 i = 0; i < n_real; i++) {
      e = uop_substitute(e, &real_rng[i], &placeholder[i], 1);
    }
    sub_out[a] = e;
  }

  Term sub_in[MAX_DIM];
  if (!apply_movement_op_reshape(out_ndim, out_shape, in_ndim, in_shape,
                                 sub_out, sub_in)) {
    return 0;
  }

  for (u32 a = 0; a < in_ndim; a++) {
    Term e = sub_in[a];
    for (u32 i = 0; i < n_real; i++) {
      e = uop_substitute(e, &placeholder[i], &real_rng[i], 1);
    }
    in_rngs[a] = e;
  }
  return 1;
}

// Groups input and output axes with equal cumulative products and maps each
// group's flat index across by IDIV/IMOD; size-1 input axes read CONST(0).
// Returns 0 when the shapes do not group; tinygrad's _apply_reshape
// (indexing.py:155-167) always flattens and leans on its simplifier instead.
fn int apply_movement_op_reshape(u32 out_ndim, u32 const *out_shape,
                                  u32 in_ndim,  u32 const *in_shape,
                                  Term const *out_rngs, Term *in_rngs) {
  if (out_ndim > MAX_DIM || in_ndim > MAX_DIM) return 0;
  for (u32 i = 0; i < in_ndim; i++) in_rngs[i] = reshape_const_zero();

  u32 i = 0, j = 0;
  while (i < in_ndim || j < out_ndim) {
    while (i < in_ndim && in_shape[i] == 1) { in_rngs[i] = reshape_const_zero(); i++; }
    while (j < out_ndim && out_shape[j] == 1) { j++; }
    if (i == in_ndim && j == out_ndim) break;
    if (i == in_ndim || j == out_ndim) return 0;

    // The groups [i, i_end) and [j, j_end) grow until their products match.
    u32 i_start = i, j_start = j;
    u64 in_acc  = in_shape[i];
    u64 out_acc = out_shape[j];
    u32 i_end   = i + 1;
    u32 j_end   = j + 1;
    while (in_acc != out_acc) {
      if (in_acc < out_acc) {
        if (i_end >= in_ndim) return 0;
        in_acc *= in_shape[i_end];
        i_end++;
      } else {
        if (j_end >= out_ndim) return 0;
        out_acc *= out_shape[j_end];
        j_end++;
      }
    }

    // flat = sum over k in [j_start, j_end) of
    //        out_rngs[k] * prod(out_shape[k+1 .. j_end))
    Term flat = 0;
    u64 suffix = 1;
    for (u32 k = j_end; k > j_start; k--) {
      u32 idx = k - 1;
      if (out_shape[idx] == 1) continue;
      Term term = out_rngs[idx];
      if (suffix != 1) {
        term = uop_int_binary(UOP_IMUL, term,
                              uop_const(DT_INT32, (u32)suffix));
      }
      flat = (flat == 0) ? term : uop_int_binary(UOP_IADD, flat, term);
      suffix *= out_shape[idx];
    }
    if (flat == 0) flat = reshape_const_zero();

    // in_rngs[idx] = (flat / in_suffix) % in_shape[idx]
    u64 in_suffix = 1;
    for (u32 k = i_end; k > i_start; k--) {
      u32 idx = k - 1;
      if (in_shape[idx] == 1) {
        in_rngs[idx] = reshape_const_zero();
        continue;
      }
      Term expr = flat;
      if (in_suffix != 1) {
        expr = uop_int_binary(UOP_IDIV, expr,
                              uop_const(DT_INT32, (u32)in_suffix));
      }
      // The group's outermost axis needs no IMOD: flat / in_suffix is
      // already below its extent.
      int outermost = (idx == i_start);
      if (!outermost) {
        expr = uop_int_binary(UOP_IMOD, expr,
                              uop_const(DT_INT32, in_shape[idx]));
      }
      in_rngs[idx] = expr;
      in_suffix *= in_shape[idx];
    }

    i = i_end;
    j = j_end;
  }
  return 1;
}

