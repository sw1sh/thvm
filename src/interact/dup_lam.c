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
//
// LAM_ERA_MASK fast-path (HVM4 wnf/dup_lam.c:13): when the *original*
// lambda's binder is unused, no VAR(lam_loc) exists in its body so the
// heap_subst_var(lam_loc, sup) write would feed nothing.  Skip it.
// The new lambdas inherit the original lam_ext (and thus the bit) --
// re-running lam_body_uses_var on a+0/a+1 here would give a false
// negative because the VAR(a+0)/VAR(a+1) live in the SUP cell at
// a+2/a+3, reachable only via the lam_loc -> SUP indirection, not by
// a syntactic walk from a+0/a+1's body.  HVM4 also propagates lam_ext
// verbatim (see TinyHVM/HVM4/clang/wnf/dup_lam.c:19).
fn Term interact_dup_lam(u32 lab, u64 loc, u8 side, Term lam) {
  ITRS++;
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
    // Original binder unused: skip the substitute write.  Body still
    // has no VAR(lam_loc), so the SUP cell is unreachable; leaving it
    // allocated is harmless (GC will collect).
    return heap_subst_cop(side, loc, l0, l1);
  }
  heap_subst_var(lam_loc, sup);
  return heap_subst_cop(side, loc, l0, l1);
}
