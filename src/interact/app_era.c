// (* arg)
// ------- APP-ERA
// *
//
// Erased function consumes its argument; the result is the eraser.
// The arg term is left dangling (no GC; an interaction-net program
// that produces this pair has already accepted the leak).
fn Term interact_app_era(void) {
  ITRS++;
  multi_emit(RULE_APP_ERA, MULTI_PRUNE, 0, 0, 0);
  return term_new(0, TAG_ERA, 0, 0);
}
