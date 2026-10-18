// (ANN val (lam x.body))
// ---------------------- ANN-LAM (ICC type-forward flow)
// = lam x. (ANN (APP val $k) body[x <- bri $k. x])
//
// Mirror of APP-BRI: $k, the inner bridge's binder, is also the APP's argument,
// so both are the one cell k_loc.
fn Term interact_ann_lam(Term val, Term lam) {
  ITRS++;
  multi_emit(RULE_ANN_LAM, MULTI_TERM, (u64)val, (u64)lam, 0);
  u64  lam_loc = term_val(lam);
  Term body    = heap_read(lam_loc);

  u64 c = heap_alloc(6);
  u64 new_lam_loc   = c + 0;
  u64 k_loc         = c + 1;
  u64 app_loc       = c + 2;
  u64 ann_inner_loc = c + 4;

  Term new_x = term_new(0, TAG_VAR, 0, new_lam_loc);
  heap_set(k_loc, new_x);
  Term inner_val = term_new(0, TAG_BRI, 0, k_loc);

  Term k_var = term_new(0, TAG_VAR, 0, k_loc);
  heap_set(app_loc + 0, val);
  heap_set(app_loc + 1, k_var);
  Term app_term = term_new(0, TAG_APP, 0, app_loc);

  heap_set(ann_inner_loc + 0, app_term);
  heap_set(ann_inner_loc + 1, body);
  Term ann_inner_term = term_new(0, TAG_ANN, 0, ann_inner_loc);

  heap_set(new_lam_loc, ann_inner_term);

  heap_subst_var(lam_loc, inner_val);
  return term_new(0, TAG_LAM, lam_seal_ext(new_lam_loc, 0), new_lam_loc);
}
