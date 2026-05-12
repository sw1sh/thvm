// ! &L{F0, F1} = θx.body
// ----------------------- DUP-BRI  (commute, mirror of DUP-LAM)
// F0 <- θx0. b0
// F1 <- θx1. b1
// x  <- &L{x0, x1}
// ! &L{b0, b1} = body
//
// Same allocation layout as DUP-LAM.

fn Term interact_dup_bri(u32 lab, u64 loc, u8 side, Term bri) {
  ITRS++;
  multi_emit(RULE_DUP_BRI, MULTI_FORK, loc, (u64)bri, lab);
  u64  bri_loc = term_val(bri);
  Term body    = heap_read(bri_loc);

  u64 a = heap_alloc(5);
  heap_set(a + 4, body);
  heap_set(a + 0, term_new(0, TAG_DP0, lab, a + 4));
  heap_set(a + 1, term_new(0, TAG_DP1, lab, a + 4));
  heap_set(a + 2, term_new(0, TAG_VAR, 0,   a + 0));
  heap_set(a + 3, term_new(0, TAG_VAR, 0,   a + 1));

  Term sup = term_new(0, TAG_SUP, lab, a + 2);
  Term b0  = term_new(0, TAG_BRI, 0,   a + 0);
  Term b1  = term_new(0, TAG_BRI, 0,   a + 1);

  heap_subst_var(bri_loc, sup);
  return heap_subst_cop(side, loc, b0, b1);
}
