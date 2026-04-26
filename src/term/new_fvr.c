// term/new_fvr.c - construct a TAG_FVR first-order variable.
//
// Atomic: no heap cells.  EXT = variable id; same id means "the same
// variable".  Distinct from TAG_VAR (the IC bound variable that
// references a binder loc).
//
// Used by the ATP encoding (docs/plans/waldmeister_ic_atp.md) to
// encode the universally / existentially quantified variables of
// equational logic.  e.g., f(x, e) = x has FVR at the leaves where
// x appears, with the same `var_id` for each occurrence.
//
// FVR is passive in the IC reducer (no FVR-* interactions today).
// Comparison and substitution treat it as an atom.

fn Term term_new_fvr(u32 var_id) {
  return term_new(0, TAG_FVR, var_id, 0);
}
