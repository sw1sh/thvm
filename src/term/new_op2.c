// term/new_op2.c - construct a TAG_OP2.
//
// Heap layout: [x, y].  Reduction is strict on x then y; both must
// reduce to TAG_NUM for the op to fire.

fn Term term_new_op2(u32 opcode, Term x, Term y) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, x);
  heap_set(loc + 1, y);
  return term_new(0, TAG_OP2, opcode, loc);
}
