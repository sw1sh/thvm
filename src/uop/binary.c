// uop/binary.c - construct a 2-source UOp (ADD/MUL/CMPLT).
//
// Heap layout: [a, b] -- two cells holding the source terms.

fn Term uop_binary(u32 opcode, Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_UOP, opcode, loc);
}
