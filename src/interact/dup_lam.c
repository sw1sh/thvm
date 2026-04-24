// ! &L{F0, F1} = lam x.body
// ------------------------- DUP-LAM
// F0 <- lam x0.b0
// F1 <- lam x1.b1
// x  <- &L{x0, x1}
// ! &L{b0, b1} = body
//
// Allocates one 5-cell block:
//
//   a+0  body cell of new LAM0     (also LAM0's binder slot)
//   a+1  body cell of new LAM1     (also LAM1's binder slot)
//   a+2  SUP left  = VAR(a+0)
//   a+3  SUP right = VAR(a+1)
//   a+4  shared DUP body holding the original lambda's body
//
// The original lambda's binder cell (lam_loc) gets substituted with
// SUP(a+2, lab) so any leftover VAR -> lam_loc resolves to the new
// pair of bound vars.  The active projection's side gets back its
// fresh LAM via heap_subst_cop.
fn Term interact_dup_lam(u32 lab, u64 loc, u8 side, Term lam) {
  ITRS++;
  u64  lam_loc = term_val(lam);
  Term body    = heap_read(lam_loc);

  u64 a = heap_alloc(5);
  heap_set(a + 4, body);
  heap_set(a + 0, term_new(0, TAG_DP0, lab, a + 4));
  heap_set(a + 1, term_new(0, TAG_DP1, lab, a + 4));
  heap_set(a + 2, term_new(0, TAG_VAR, 0,   a + 0));
  heap_set(a + 3, term_new(0, TAG_VAR, 0,   a + 1));

  Term sup = term_new(0, TAG_SUP, lab, a + 2);
  Term l0  = term_new(0, TAG_LAM, 0,   a + 0);
  Term l1  = term_new(0, TAG_LAM, 0,   a + 1);

  heap_subst_var(lam_loc, sup);
  return heap_subst_cop(side, loc, l0, l1);
}
