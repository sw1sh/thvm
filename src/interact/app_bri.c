// (APP (bri x.body) arg)
// ---------------------- APP-BRI (ICC type-backward flow)
// = bri x. (APP body[x <- lam $k. x] (ANN $k arg))
//
// $k, the inner lam's binder, is also the annotated value, so both are the one
// cell k_loc.  The ANN reads $k from outside the lam's body, so the lam is
// never erasable (no LAM_ERA_MASK).
fn Term interact_app_bri(Term bri, Term arg) {
  ITRS++;
  multi_emit(RULE_APP_BRI, MULTI_TERM, (u64)bri, (u64)arg, 0);
  u64  bri_loc = term_val(bri);
  Term body    = heap_read(bri_loc);

  u64 c = heap_alloc(6);
  u64 new_bri_loc = c + 0;
  u64 k_loc       = c + 1;
  u64 app_loc     = c + 2;
  u64 ann_loc     = c + 4;

  Term new_x = term_new(0, TAG_VAR, 0, new_bri_loc);
  heap_set(k_loc, new_x);
  Term inner_lam = term_new(0, TAG_LAM, 0, k_loc);

  Term k_var = term_new(0, TAG_VAR, 0, k_loc);
  heap_set(ann_loc + 0, k_var);
  heap_set(ann_loc + 1, arg);
  Term ann_term = term_new(0, TAG_ANN, 0, ann_loc);

  heap_set(app_loc + 0, body);
  heap_set(app_loc + 1, ann_term);
  Term app_term = term_new(0, TAG_APP, 0, app_loc);

  heap_set(new_bri_loc, app_term);

  heap_subst_var(bri_loc, inner_lam);
  return term_new(0, TAG_BRI, 0, new_bri_loc);
}
