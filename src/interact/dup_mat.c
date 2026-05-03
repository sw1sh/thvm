// ! &L{X0, X1} = MAT_v(handler, fallback)
// --------------------------------------- DUP-MAT (commute)
// ! &L{h0, h1} = handler
// ! &L{f0, f1} = fallback
// X0 <- MAT_v(h0, f0)
// X1 <- MAT_v(h1, f1)
//
// HVM4's app_mat_sup distributes APP-MAT through SUP; the dual case
// -- distributing DUP through a MAT cell -- is the generic dup_nod
// (TinyHVM/HVM4/clang/wnf/dup_nod.c) specialised to TAG_MAT.  Arity
// is 2 (handler at val+0, fallback at val+1); ext carries the
// numeric match value.
//
// Allocation per fire: 2 fresh MAT layouts (2 cells each) + 2 shared
// dup-body cells = 6 cells total.

fn Term interact_dup_mat(u32 lab, u64 loc, u8 side, Term mat) {
  ITRS++;
  u64 a_loc = term_val(mat);
  u32 a_ext = term_ext(mat);
  u64 r0_loc = heap_alloc(2);
  u64 r1_loc = heap_alloc(2);
  for (u32 i = 0; i < 2; i++) {
    u64 body = heap_alloc(1);
    heap_set(body, heap_read(a_loc + i));
    heap_set(r0_loc + i, term_new(0, TAG_DP0, lab, body));
    heap_set(r1_loc + i, term_new(0, TAG_DP1, lab, body));
  }
  Term r0 = term_new(0, TAG_MAT, a_ext, r0_loc);
  Term r1 = term_new(0, TAG_MAT, a_ext, r1_loc);
  return heap_subst_cop(side, loc, r0, r1);
}
