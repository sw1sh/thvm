// ! &L{F0, F1} = lam x.body
// ------------------------- DUP-LAM
// F0 <- lam x0.b0
// F1 <- lam x1.b1
// x  <- &L{x0, x1}
// ! &L{b0, b1} = body
//
// Block a: [DP0 a+4, DP1 a+4, VAR(a+0), VAR(a+1), body]; a+0 / a+1 are the
// new LAMs' body cells and binders, and the SUP at a+2 replaces x.
// LAM_ERA_MASK (HVM4 wnf/dup_lam.c:13): an unused x gets no SUP.  The clones
// keep lam_ext, as in HVM4 (dup_lam.c:19): lam_body_uses_var on a+0 / a+1 would
// miss VAR(a+0) / VAR(a+1), reachable only through the SUP that x becomes.
fn Term interact_dup_lam(u32 lab, u64 loc, u8 side, Term lam) {
  ITRS++;
  multi_emit(RULE_DUP_LAM, MULTI_FORK, loc, (u64)lam, lab);
  u32  lam_ext = term_ext(lam);
  u64  lam_loc = term_val(lam);
  Term body    = heap_read(lam_loc);

  u64 a = heap_alloc(5);
  heap_set(a + 4, body);
  heap_set(a + 0, term_new(0, TAG_DP0, lab, a + 4));
  heap_set(a + 1, term_new(0, TAG_DP1, lab, a + 4));
  heap_set(a + 2, term_new(0, TAG_VAR, 0,   a + 0));
  heap_set(a + 3, term_new(0, TAG_VAR, 0,   a + 1));

  Term sup = term_new(0, TAG_SUP, lab, a + 2);
  Term l0  = term_new(0, TAG_LAM, lam_ext, a + 0);
  Term l1  = term_new(0, TAG_LAM, lam_ext, a + 1);

  if (lam_ext & LAM_ERA_MASK) {
    return heap_subst_cop(side, loc, l0, l1);
  }
  heap_subst_var(lam_loc, sup);
  return heap_subst_cop(side, loc, l0, l1);
}
