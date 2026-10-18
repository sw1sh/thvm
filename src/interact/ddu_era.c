// ! X &(&{}) = v; b
// ----------------- DDU-ERA
// &{}
fn Term interact_ddu_era(void) {
  ITRS++;
  multi_emit(RULE_DDU_ERA, MULTI_PRUNE, 0, 0, 0);
  return term_new(0, TAG_ERA, 0, 0);
}
