// &(&{}){a, b}
// ------------ DSU-ERA
// &{}
//
// HVM4's wnf_dsu_era (TinyHVM/HVM4/clang/wnf/dsu_era.c).
fn Term interact_dsu_era(void) {
  ITRS++;
  multi_emit(RULE_DSU_ERA, MULTI_PRUNE, 0, 0, 0);
  return term_new(0, TAG_ERA, 0, 0);
}
