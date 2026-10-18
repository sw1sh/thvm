// (* arg)
// ------- APP-ERA
// *
//
// arg is not erased: its cells stay until the copying collector
// (heap/collect.c) finds them unreachable.
fn Term interact_app_era(void) {
  ITRS++;
  multi_emit(RULE_APP_ERA, MULTI_PRUNE, 0, 0, 0);
  return term_new(0, TAG_ERA, 0, 0);
}
