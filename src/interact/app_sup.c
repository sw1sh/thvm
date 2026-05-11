// (APP &L{f, g} arg)
// ------------------ APP-SUP
// ! &L{x0, x1} = arg
// &L{ APP(f, x0), APP(g, x1) }
//
// Distributes an application across the two children of a SUP, with
// the argument shared between them via a DUP node so each child gets
// its own copy.  Mirrors the standard HVM4 APP-SUP rule
// (`TinyHVM/HVM4/clang/wnf/app_sup.c`).
//
// 8.1d-i: foundational interaction added so SUP-encoded CP enumeration
// (8.1d-ii) and SupGen-style search (8.10) can fan out applications
// across superposed function values.  Wired into the WNF dispatch
// next to APP-LAM / APP-BRI / APP-PRI.
//
// Allocates a 7-cell block:
//
//   c+0  shared DUP body cell holding `arg`
//   c+1  APP_0 fun slot       (gets f)
//   c+2  APP_0 arg slot       (DP0 reference)
//   c+3  APP_1 fun slot       (gets g)
//   c+4  APP_1 arg slot       (DP1 reference)
//   c+5  SUP child 0          (= APP(f, DP0(arg)))
//   c+6  SUP child 1          (= APP(g, DP1(arg)))
//
// The result is a fresh SUP term with the same label as the input,
// pointing at c+5.
fn Term interact_app_sup(Term sup, Term arg) {
  ITRS++;
  multi_emit(RULE_APP_SUP, MULTI_SLIDE, (u64)sup, (u64)arg, term_ext(sup));
  u64  sup_loc = term_val(sup);
  u32  lab     = term_ext(sup);
  Term f       = heap_read(sup_loc + 0);
  Term g       = heap_read(sup_loc + 1);

  u64 c = heap_alloc(7);
  // Shared DUP body holds `arg`; DP0 / DP1 fetch the two copies.
  heap_set(c + 0, arg);

  // APP_0 = APP(f, DP0(L, c+0))
  heap_set(c + 1, f);
  heap_set(c + 2, term_new(0, TAG_DP0, lab, c + 0));

  // APP_1 = APP(g, DP1(L, c+0))
  heap_set(c + 3, g);
  heap_set(c + 4, term_new(0, TAG_DP1, lab, c + 0));

  // SUP children point at the two APP heap locations.
  heap_set(c + 5, term_new(0, TAG_APP, 0, c + 1));
  heap_set(c + 6, term_new(0, TAG_APP, 0, c + 3));

  return term_new(0, TAG_SUP, lab, c + 5);
}
