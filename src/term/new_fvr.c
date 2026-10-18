// First-order variable of the ATP encoding (docs/plans/waldmeister_ic_atp.md):
// an atom (ext = variable id); unlike TAG_VAR it has no binder. Passive in wnf.
fn Term term_new_fvr(u32 var_id) {
  return term_new(0, TAG_FVR, var_id, 0);
}
