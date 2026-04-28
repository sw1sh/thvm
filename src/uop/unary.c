// uop/unary.c - construct a 1-source UOp (NEG/RECIP/EXP2/LOG2/SQRT).
//
// Heap layout: [src] -- one cell holding the source term.
//
// Hash-cons by (opcode, src).  Sharing identical UOp terms is
// always safe (UOPs are immutable; materialize dedups by heap loc
// identity); the same upstream slim down across iterations of a
// recursive lambda's chain rule.

fn Term uop_unary(u32 opcode, Term src) {
  u32 args[2] = { (u32)src, (u32)(src >> 32) };
  u64 key = uop_mov_hash(opcode, 0, args, 2);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(1);
  heap_set(loc, src);
  Term t = term_new(0, TAG_UOP, opcode, loc);
  uop_mov_insert(key, t);
  return t;
}
