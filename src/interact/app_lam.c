// (lam x.body) arg
// ---------------- APP-LAM
// x <- arg
// body
//
// LAM_ERA_MASK (HVM4 wnf/app_lam.c:10) marks an unused binder: no substitution
// is written.  lam_seal_ext sets it at construction (lam/body_uses_var.c).
// A UOP body that is not yet a KERNEL is materialized here on both paths: the
// kernel reads x as an input, through the SUB at fire time, so x's shape is
// recorded first when arg has one.
fn Term interact_app_lam(Term lam, Term arg) {
  ITRS++;
  multi_emit(RULE_APP_LAM, MULTI_TERM, (u64)lam, (u64)arg, 0);
  u32  lam_ext = term_ext(lam);
  u64  loc     = term_val(lam);
  Term body    = heap_read(loc);
  if (lam_ext & LAM_ERA_MASK) {
    // Nothing reads x, so its shape is not recorded either.
    if (term_tag(body) == TAG_UOP && term_ext(body) != UOP_KERNEL) {
      body = thvm_materialize(body);
    }
    return body;
  }
  // Record x's shape whatever the body: a nested lambda's UOP body may read
  // x, as in (lam w. lam n. body) applied to w0 and then to a NUM.
  Shape s;
  if (term_shape_in(arg, 0, &s) && s.ndim > 0) {
    lam_shape_set(loc, &s);
  }
  if (term_tag(body) == TAG_UOP && term_ext(body) != UOP_KERNEL) {
    body = thvm_materialize(body);
  }
  heap_subst_var(loc, arg);
  return body;
}
