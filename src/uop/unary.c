// uop/unary.c - construct a 1-source UOp (NEG/RECIP/EXP2/LOG2/SQRT).
//
// Heap layout: [src] -- one cell holding the source term.

fn Term uop_unary(u32 opcode, Term src) {
  u64 loc = heap_alloc(1);
  heap_set(loc, src);
  return term_new(0, TAG_UOP, opcode, loc);
}
