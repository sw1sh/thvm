// opcode is NEG, RECIP, EXP2, LOG2 or SQRT; heap layout [src].
fn Term uop_unary(u32 opcode, Term src) {
  Term r = uop_rewrite_unary(opcode, src);
  if (r != 0) return r;
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
