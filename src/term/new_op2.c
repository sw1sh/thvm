// Heap layout: [x, y]; strict on x then y: two NUMs fire the op, a SUP on
// either side commutes, anything else is stuck.
fn Term term_new_op2(u32 opcode, Term x, Term y) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, x);
  heap_set(loc + 1, y);
  return term_new(0, TAG_OP2, opcode, loc);
}
