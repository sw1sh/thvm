// wald/_.c - Waldmeister .pr spec parser (stage 6.3).
//
// 6.3a: data model + init/free.  Subsequent stages add the lexer
// (6.3b), section drivers (6.3c), term parser (6.3d), equations
// section (6.3e), top-level driver (6.3f), and end-to-end tests
// against the group example (6.3g).

fn WaldSpec *wald_init(void) {
  WaldSpec *s = (WaldSpec *)calloc(1, sizeof(WaldSpec));
  if (s == NULL) return NULL;
  // Default to PROOF mode; explicit MODE keyword overrides.
  s->mode_proof = 1;
  // CTR label 0 is the "anonymous tuple" convention; start
  // signature labels at 1 so they don't collide.
  s->next_label = 1;
  return s;
}

fn void wald_free(WaldSpec *s) {
  if (s == NULL) return;
  free(s);
}
