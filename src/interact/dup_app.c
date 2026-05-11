// ! &L{X0, X1} = APP(f, a)
// ------------------------- DUP-APP (commute)
// ! &L{f0, f1} = f
// ! &L{a0, a1} = a
// X0 <- APP(f0, a0)
// X1 <- APP(f1, a1)
//
// Same shape as HVM4's generic wnf_dup_nod
// (TinyHVM/HVM4/clang/wnf/dup_nod.c) specialised to TAG_APP.  Arity
// is fixed at 2 (fun, arg) so the loop is unrolled.  Each child gets
// a fresh DUP body cell whose initial body is the original child;
// projections (DP0/DP1) sit at the matching slot in the two new APP
// layouts.
//
// Allocation per fire: 2 fresh APP layouts (2 cells each) + 2 shared
// dup-body cells = 6 cells total.  The two APP layouts intentionally
// live in separate 2-cell blocks so heap_take semantics stay
// well-defined when one projection is consumed before the other.

fn Term interact_dup_app(u32 lab, u64 loc, u8 side, Term app) {
  ITRS++;
  multi_emit(RULE_DUP_APP, MULTI_FORK, (u64)app, 0, lab);
  u64 a_loc = term_val(app);
  u32 a_ext = term_ext(app);
  u64 r0_loc = heap_alloc(2);
  u64 r1_loc = heap_alloc(2);
  for (u32 i = 0; i < 2; i++) {
    u64 body = heap_alloc(1);
    heap_set(body, heap_read(a_loc + i));
    heap_set(r0_loc + i, term_new(0, TAG_DP0, lab, body));
    heap_set(r1_loc + i, term_new(0, TAG_DP1, lab, body));
  }
  Term r0 = term_new(0, TAG_APP, a_ext, r0_loc);
  Term r1 = term_new(0, TAG_APP, a_ext, r1_loc);
  return heap_subst_cop(side, loc, r0, r1);
}
