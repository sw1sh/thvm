// ! &L{X0, X1} = OP2_op(x, y)
// --------------------------- DUP-OP2 (commute)
// ! &L{x0, x1} = x
// ! &L{y0, y1} = y
// X0 <- OP2_op(x0, y0)
// X1 <- OP2_op(x1, y1)
//
// HVM4's wnf_op2_sup distributes OP2 through SUP; the dual case --
// distributing DUP through OP2 -- is the generic dup_nod
// (TinyHVM/HVM4/clang/wnf/dup_nod.c) specialised to TAG_OP2.  Arity
// is 2 (left, right operand); ext carries the OP_* opcode.
//
// Allocation per fire: 2 fresh OP2 layouts (2 cells each) + 2 shared
// dup-body cells = 6 cells total.

fn Term interact_dup_op2(u32 lab, u64 loc, u8 side, Term op2) {
  ITRS++;
  u64 a_loc = term_val(op2);
  u32 a_ext = term_ext(op2);
  u64 r0_loc = heap_alloc(2);
  u64 r1_loc = heap_alloc(2);
  for (u32 i = 0; i < 2; i++) {
    u64 body = heap_alloc(1);
    heap_set(body, heap_read(a_loc + i));
    heap_set(r0_loc + i, term_new(0, TAG_DP0, lab, body));
    heap_set(r1_loc + i, term_new(0, TAG_DP1, lab, body));
  }
  Term r0 = term_new(0, TAG_OP2, a_ext, r0_loc);
  Term r1 = term_new(0, TAG_OP2, a_ext, r1_loc);
  return heap_subst_cop(side, loc, r0, r1);
}
