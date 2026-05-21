// schedule/indexing.c - movement-op range swizzler.
//
// Mirrors tinygrad/schedule/indexing.py.  Each movement op rewrites the
// per-axis index expression (RANGE Term or arithmetic over RANGE leaves)
// that a downstream consumer uses when indexing into the producer.
//
// SHRINK / PERMUTE / FLIP / EXPAND / PAD swizzles + the group-aligning
// RESHAPE handler live here.  Stride-trick rank-merge reshapes that
// would need tinygrad's pm_simplify_valid rewrite aren't ported yet --
// apply_movement_op_reshape returns 0 in that case and the unified
// rangeify pass declines the fold.
//
// Mirror source: tinygrad/schedule/indexing.py:129  apply_movement_op
//
// Input/output:
//   - `out_rngs[ndim_out]` are the per-axis range Terms the CONSUMER uses
//     (one per output axis of the movement op).
//   - `in_rngs[ndim_in]` are the per-axis range Terms the PRODUCER sees
//     (filled by this function).  ndim_in == ndim_out for these ops.
//
// Backing constructors all hash-cons through uop_mov_cache, so repeated
// swizzles with identical inputs deduplicate.

fn void apply_movement_op_shrink(u32 ndim,
                                  u32 const *begin_end,
                                  Term const *out_rngs,
                                  Term *in_rngs) {
  // SHRINK index transform is handled DOWNSTREAM by view_apply_shrink
  // in materialize.c, which folds the begin offset into the produced
  // TenDesc's view.offset.  Adding it here too caused double-shift
  // (project_thvm_composed_grad_bug -- May 20).  This pass leaves
  // out_rngs untouched; the kernel reads via the shifted view.
  //
  // begin_end retained in the signature for caller compatibility
  // (ru_apply_movement passes it) and so we have a hook here if a
  // UOP-DAG source ever needs the index-shift fallback (no current
  // path triggers it; the view-resolve in materialize.c bottoms out
  // at TAG_TEN for every existing test on master).
  (void)begin_end;
  for (u32 i = 0; i < ndim; i++) {
    in_rngs[i] = out_rngs[i];
  }
}

fn void apply_movement_op_permute(u32 ndim,
                                   u32 const *perm,
                                   Term const *out_rngs,
                                   Term *in_rngs) {
  // tinygrad: rngs = tuple(rngs[p] for p in argsort(arg))
  // `arg` (perm here) gives output->input axis mapping: output[i] = input[perm[i]].
  // argsort(perm)[i] = j s.t. perm[j] == i  (== perm^-1[i]).
  // For each input axis i, in_rngs[i] = out_rngs[perm^-1[i]].
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
      // (s - 1) - a
      Term sm1 = uop_const(DT_INT32, (in_shape[i] > 0) ? (in_shape[i] - 1) : 0);
      in_rngs[i] = uop_int_binary(UOP_ISUB, sm1, out_rngs[i]);
    }
  }
}

fn void apply_movement_op_expand(u32 ndim,
                                  u32 const *in_shape,
                                  u32 const *out_shape,
                                  Term const *out_rngs,
                                  Term *in_rngs) {
  for (u32 i = 0; i < ndim; i++) {
    if (in_shape[i] == out_shape[i]) {
      in_rngs[i] = out_rngs[i];
    } else {
      // broadcasted axis: collapse the consumer index to 0
      in_rngs[i] = uop_const(DT_INT32, 0);
    }
  }
}

fn void apply_movement_op_pad(u32 ndim,
                               u32 const *in_shape,
                               u32 const *begin_end,
                               Term const *out_rngs,
                               Term *in_rngs) {
  // For each axis: r if (s==0 && e==0)
  //                else WHERE((r >= s) & (r < sh+s), r-s, INVALID)
  // We model r>=s as ILT(s-1, r) (integer-only), and skip the lo clause
  // entirely when s == 0 (r >= 0 is trivially true for unsigned indices).
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
      Term cond_lo = uop_int_binary(UOP_ILT, s_m1, r);  // s-1 < r  iff  r >= s
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

// RESHAPE handler.  Aligns input and output axes into matching groups by
// scanning cumulative products; each group maps the flat range space of
// one side onto the other via IDIV/IMOD.  Handles the merging direction
// (many input axes -> one output axis, e.g. flattening) and the splitting
// direction (one input axis -> many output axes), as well as the 1:1 case.
//
// When the shapes do not decompose into matching groups (a stride-trick
// reshape that mixes split-and-merge inside a single block, the case
// tinygrad's `_apply_reshape` defers to `pm_simplify_valid`), returns 0
// so the caller can decline the fold (pm_simplify_valid not yet ported).
//
// Size-1 axes on either side contribute extent 1 and a CONST(0) range;
// they're folded into the surrounding group without disturbing the
// alignment.
static Term reshape_const_zero(void) {
  return uop_const(DT_INT32, 0);
}

fn int apply_movement_op_reshape(u32 out_ndim, u32 const *out_shape,
                                  u32 in_ndim,  u32 const *in_shape,
                                  Term const *out_rngs, Term *in_rngs) {
  if (out_ndim > MAX_DIM || in_ndim > MAX_DIM) return 0;
  // Pre-init all in_rngs to CONST(0) so size-1 axes inside groups, and
  // any trailing input axes from numel rounding, land on a safe default.
  for (u32 i = 0; i < in_ndim; i++) in_rngs[i] = reshape_const_zero();

  u32 i = 0, j = 0;
  while (i < in_ndim || j < out_ndim) {
    // Skip leading size-1 axes on either side (they contribute zero).
    while (i < in_ndim && in_shape[i] == 1) { in_rngs[i] = reshape_const_zero(); i++; }
    while (j < out_ndim && out_shape[j] == 1) { j++; }
    if (i == in_ndim && j == out_ndim) break;
    if (i == in_ndim || j == out_ndim) return 0;

    // Expand a single group on each side until the cumulative products
    // match.  `i_end`/`j_end` are one past the last axis in the group.
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

    // Build the flattened range over the output group:
    //   flat = sum_{k in [j_start, j_end)} out_rngs[k] * prod(out_shape[k+1..j_end))
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

    // Distribute the flat expression back across the input group:
    //   in_rngs[idx] = (flat / suffix_in) % in_shape[idx]
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
      // No IMOD when this is the outermost axis of the input group and
      // its extent equals the remaining product -- the upper bound of
      // `flat` is already in range.
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

