// uop/binary.c - construct a 2-source UOp (ADD/MUL/CMPLT).
//
// Heap layout: [a, b] -- two cells holding the source terms.
//
// Hash-cons by (opcode, a, b) -- the same elementwise binary on
// the same children produces the same UOp Term.  Identical to
// the movement-op hash-cons but with two src args instead of one.

fn Term uop_binary(u32 opcode, Term a, Term b) {
  // Pack (a_lo, a_hi, b_lo, b_hi) into the u32-array vals so the
  // mov-cache hash discriminates u64 Term values exactly.
  u32 args[4] = { (u32)a, (u32)(a >> 32), (u32)b, (u32)(b >> 32) };
  u64 key = uop_mov_hash(opcode, 0, args, 4);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  Term t = term_new(0, TAG_UOP, opcode, loc);
  uop_mov_insert(key, t);
  return t;
}
