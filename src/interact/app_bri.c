// (APP (θx. body) arg)
// ---------------------- APP-BRI  (ICC type-backward-flow)
// = θx. (APP body[x ← λ$k. x] (ANN $k arg))
//
// Where $k is a fresh free variable used twice: once as the binder of
// the inner λ (inside the substitute for x), and once as the value
// being annotated against arg.  The two occurrences must reference
// the same heap location, so we share via VAR(k_loc).
//
// Allocates a 6-cell block:
//
//   c+0  new BRI body (APP term)             (= new_bri_loc)
//   c+1  inner LAM body, holds VAR(new_bri_loc)  (= k_loc)
//   c+2  APP fun cell  (= body of old BRI, with x substituted)
//   c+3  APP arg cell  (= ANN term)
//   c+4  ANN val cell  (= VAR(k_loc))
//   c+5  ANN typ cell  (= arg)
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
  // inner_lam binds $k; body is VAR(new_bri_loc), not VAR(k_loc), so
  // the binder is unused -- LAM_ERA_MASK should fire here in practice.
  Term inner_lam = term_new(0, TAG_LAM, lam_seal_ext(k_loc, 0), k_loc);

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
