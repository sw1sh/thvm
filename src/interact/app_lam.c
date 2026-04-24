// (lam x.body) arg
// ---------------- APP-LAM
// x <- arg
// body
//
// Read the lambda body, install `arg` as the substitution at the
// binder's heap cell (so a future VAR enter on that loc picks it up
// and clears the SUB flag), then continue reducing the body.
fn Term interact_app_lam(Term lam, Term arg) {
  ITRS++;
  u64  loc  = term_val(lam);
  Term body = heap_read(loc);
  heap_subst_var(loc, arg);
  return body;
}
