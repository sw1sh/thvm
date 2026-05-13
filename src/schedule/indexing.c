// schedule/indexing.c - movement-op range swizzler + symbolic-valid simplifier.
//
// Mirrors tinygrad/schedule/indexing.py.  Each movement op rewrites the
// per-axis index expression (RANGE Term or arithmetic over RANGE leaves)
// that a downstream consumer uses when indexing into the producer.
//
// SHRINK / PERMUTE / FLIP / EXPAND / PAD swizzles are implemented here;
// RESHAPE is handled in the unified rangeify pass via pm_simplify_valid.
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
  for (u32 i = 0; i < ndim; i++) {
    u32 ss = begin_end[2 * i];
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

// === Symbolic-valid simplifiers ===
//
// Mirror source: tinygrad/uop/symbolic.py:423 pm_simplify_valid +
//                tinygrad/uop/symbolic.py:385 pm_drop_and_clauses.
//
// Both consume a Term that may contain UOP_IAND chains gating a
// UOP_IWHERE-INVALID expression (the standard PAD/RESHAPE output shape)
// and rewrite the gate into a smaller equivalent form.
//
// Identity stubs for now. Sharpen with targeted rewrites driven by
// concrete RESHAPE-output regressions vs tinygrad parity; the call
// seam is what callers in the unified rangeify pass depend on.

fn Term pm_simplify_valid_apply(Term t) {
  return t;
}

fn Term pm_drop_and_clauses_apply(Term t) {
  return t;
}
