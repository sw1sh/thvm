// uop/movement_index.c - per-USE movement-chain INDEX resolver (Phase B1).
//
// `uop_resolve_movement_chain` walks a movement-op chain outside-in,
// transforming the consumer's iter context to the iter context the
// underlying buffer expects.  Mirrors tinygrad's apply_movement_op
// (indexing.py:128-145): each movement op's effect on a consumer's
// iters is a small, local rewrite.
//
// Architectural choice (per the migration plan): movement ops STAY
// in the UOp DAG.  This helper is called per-USE during lowering
// (Phase B3 wires it into rangeify) so each consumer sees its own
// resolved INDEX expression.  Eager DAG-layer rewriting would split
// buffer materializations the schedule wants to keep.
//
// Caller contract:
//   - Pass `src` = the term seen at the consumer's input slot
//     (may be a chain of UOP_PERMUTE / RESHAPE / EXPAND / PAD /
//     SHRINK / FLIP wrapping any non-movement bottom).
//   - Pass `iters[ndim]` = consumer's per-axis iter expressions
//     (typically UOP_RANGE leaves, but any UOp tree is fine).
//   - On return, `*iters[ndim_io]` is the iter context for the
//     bottom buffer; the function value is that bottom term.
//   - Returns 0 on a shape mismatch so the caller can bail.
//
// Phase B1 lands UOP_PERMUTE first; subsequent commits add the other
// five ops.  Each follows the same outside-in pattern.

// Apply UOP_PERMUTE's inverse to `iters`: PERMUTE's perm[d] tells us
// which input axis becomes output axis d.  Going from out_iters to
// in_iters: in_iters[perm[d]] = out_iters[d].  This matches
// rngs_ctx_movement_src's PERMUTE branch in rangeify.c.
static int uop_apply_permute_iters(Term src, Term *iters, u32 *ndim_io) {
  if (term_tag(src) != TAG_UOP || term_ext(src) != UOP_PERMUTE) return 0;
  u64 loc = term_val(src);
  Term ndim_cell = heap_read(loc + 1);
  if (term_tag(ndim_cell) != TAG_NUM) return 0;
  u32 ndim = (u32)term_val(ndim_cell);
  // PERMUTE preserves rank; refuse if the consumer disagrees.
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

// Recursive driver: peel one movement op at a time, transforming the
// iters context.  Returns the bottom non-movement term.  Returns 0
// on transformation failure; the caller bails.
fn Term uop_resolve_movement_chain(Term src, Term *iters, u32 *ndim_io) {
  for (u32 guard = 0; guard < (1u << 20); guard++) {
    if (term_tag(src) != TAG_UOP) return src;
    u32 op = term_ext(src);
    switch (op) {
      case UOP_PERMUTE:
        if (!uop_apply_permute_iters(src, iters, ndim_io)) return 0;
        src = heap_read(term_val(src) + 0);
        continue;
      // Phase B1.rest: UOP_RESHAPE, UOP_EXPAND, UOP_PAD, UOP_SHRINK,
      // UOP_FLIP land in subsequent commits, each as a small static
      // helper alongside uop_apply_permute_iters.
      default:
        return src;
    }
  }
  return 0;  // unreachable in practice; chain depth-limit guard.
}
