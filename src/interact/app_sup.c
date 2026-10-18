// (APP &L{f, g} arg)
// ------------------ APP-SUP
// ! &L{x0, x1} = arg
// &L{ APP(f, x0), APP(g, x1) }
//
// HVM4's wnf_app_sup (TinyHVM/HVM4/clang/wnf/app_sup.c), in fresh cells:
// [arg, f, x0, g, x1, APP(f, x0), APP(g, x1)], the SUP over the last two.
fn Term interact_app_sup(Term sup, Term arg) {
  ITRS++;
  multi_emit(RULE_APP_SUP, MULTI_SLIDE, (u64)sup, (u64)arg, term_ext(sup));
  u64  sup_loc = term_val(sup);
  u32  lab     = term_ext(sup);
  Term f       = heap_read(sup_loc + 0);
  Term g       = heap_read(sup_loc + 1);

  u64 c = heap_alloc(7);
  heap_set(c + 0, arg);

  heap_set(c + 1, f);
  heap_set(c + 2, term_new(0, TAG_DP0, lab, c + 0));

  heap_set(c + 3, g);
  heap_set(c + 4, term_new(0, TAG_DP1, lab, c + 0));

  heap_set(c + 5, term_new(0, TAG_APP, 0, c + 1));
  heap_set(c + 6, term_new(0, TAG_APP, 0, c + 3));

  return term_new(0, TAG_SUP, lab, c + 5);
}
