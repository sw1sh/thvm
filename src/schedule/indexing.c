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

// === PLACEHOLDER round-trip (gated THVM_FUSE_CONV_BWD) ================
//
// Mirror source: tinygrad/schedule/indexing.py:140-143 (RESHAPE case of
// apply_movement_op) + indexing.py:112-125 (_apply_reshape).
//
//   sink = UOp.sink(*rngs).simplify()
//   sub_array = {r: UOp.range(r.src[0], i, PLACEHOLDER) for i,r in enumerate(sink.ranges)}
//   rngs = _apply_reshape(in_shape, arg, sink.substitute(sub_array))
//            .substitute({v:k for k,v in sub_array.items()}).src
//
// thvm's flat-decompose (apply_movement_op_reshape) builds the same
// `combined = sum(out_rng*stride)` flat then decomposes by in_shape via
// IDIV/IMOD -- but it runs directly on the CONSUMER's out_rngs.  When a
// prior reshape already swizzled those into compound IDIV/IMOD/affine
// trees (the `_pool` unfold `[...,144,144]->[...,6,24,6,24]` mints fresh
// independent ranges for the 6x24 split, discarding the packed structure
// `flat = oh + kh*25`), the downstream `(a0 + a2*24)//25` has two
// structurally-independent RANGE leaves and the divmod-recombine folds in
// index_simplify.c have no shared `flat` to fold against.
//
// Fix (tinygrad's): substitute each free RANGE leaf in the consumer
// out_rngs with a FRESH single placeholder RANGE (distinct clean axis_id,
// same extent so the bound-aware folds still fire), run the flat-
// decompose against the clean placeholders -- so the recombine fires
// symbolically at constructor time in uop_int_binary -- then substitute
// the real ranges back.  The recombine `(x//c)*c + x%c -> x`, then
// `flat//25 -> kh, flat%25 -> oh`, fires before fresh ranges commit.
//
// Placeholder axis_ids use a high base disjoint from the real axis space
// (RU_RANGE_IDX_COUNTER) so a placeholder can never collide with a real
// range and the inverse substitution is exact.  Placeholders never escape
// this function -- every one is substituted back to its real range.
#define RESHAPE_PLACEHOLDER_AXIS_BASE 0x60000000u

// Collect distinct free RANGE leaves (axis_id + extent + axis_type) over
// all the out_rngs into out_aids/out_ext/out_atype, bounded by cap.
// Returns the count, or (u32)-1 on overflow (caller declines the round-
// trip and falls back).  axis_type is collected so the forward
// placeholder substitution can rebuild the exact hash-consed Term (the
// uop_range key includes axis_type).
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

// PLACEHOLDER-composed RESHAPE swizzle.  Substitutes each consumer free
// RANGE for a clean placeholder, runs the flat-decompose (which fires the
// symbolic recombine), then maps placeholders back to the real ranges.
// Returns the same 0/1 contract as apply_movement_op_reshape.
fn int apply_movement_op_reshape_composed(u32 out_ndim, u32 const *out_shape,
                                          u32 in_ndim,  u32 const *in_shape,
                                          Term const *out_rngs, Term *in_rngs) {
  if (out_ndim > MAX_DIM || in_ndim > MAX_DIM) return 0;

  // Gather the distinct real RANGE leaves the consumer out_rngs reference.
  u32 real_aids[2 * MAX_DIM];
  u32 real_ext[2 * MAX_DIM];
  u32 real_atype[2 * MAX_DIM];
  u32 n_real = 0;
  for (u32 a = 0; a < out_ndim; a++) {
    reshape_collect_distinct_ranges(out_rngs[a], real_aids, real_ext,
                                    real_atype, &n_real, 2 * MAX_DIM, 0);
    if (n_real == (u32)-1) return 0;  // too many leaves: decline, fall back
  }

  // Build a placeholder RANGE per distinct real leaf (clean single leaf,
  // same extent, disjoint axis_id) and the real->placeholder forward map.
  // Reconstruct `real_rng[i]` with the ORIGINAL axis_type so the forward
  // substitution matches the actual hash-consed Term in out_rngs (the
  // uop_range hash-cons key includes axis_type -- rebuilding a KAX_REDUCE
  // window range as KAX_LOOP would silently miss it, leaking it past the
  // placeholder swap so the divmod-recombine can't fire and the window
  // strands as a non-reduce loop -- the conv-bwd 2nd-step 6.8e12-iter
  // hang).  Placeholders stay KAX_LOOP (a clean disjoint axis), and the
  // inverse map restores the real range identity (incl. its type).
  Term real_rng[2 * MAX_DIM];
  Term placeholder[2 * MAX_DIM];
  for (u32 i = 0; i < n_real; i++) {
    real_rng[i]    = uop_range(real_aids[i], real_atype[i], real_ext[i]);
    placeholder[i] = uop_range(RESHAPE_PLACEHOLDER_AXIS_BASE + i, KAX_LOOP,
                               real_ext[i]);
  }

  // Substitute real->placeholder in each consumer out_rng (routing through
  // the simplifying constructors so commutative flips settle early, like
  // tinygrad's `UOp.sink(*rngs).simplify()`).
  Term sub_out[MAX_DIM];
  for (u32 a = 0; a < out_ndim; a++) {
    Term e = out_rngs[a];
    for (u32 i = 0; i < n_real; i++) {
      e = uop_index_substitute(e, real_rng[i], placeholder[i], 0);
    }
    sub_out[a] = e;
  }

  // Flat-decompose on the clean placeholder ranges.  The divmod-recombine
  // folds (index_simplify.c) fire at constructor time, collapsing the
  // split+re-split back to the source iter where possible.
  Term sub_in[MAX_DIM];
  if (!apply_movement_op_reshape(out_ndim, out_shape, in_ndim, in_shape,
                                 sub_out, sub_in)) {
    return 0;
  }

  // Substitute placeholders back to the real ranges (the inverse map).
  for (u32 a = 0; a < in_ndim; a++) {
    Term e = sub_in[a];
    for (u32 i = 0; i < n_real; i++) {
      e = uop_index_substitute(e, placeholder[i], real_rng[i], 0);
    }
    in_rngs[a] = e;
  }
  return 1;
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

