// opcode is ADD, MUL, CMPLT, CMPEQ or ASSIGN; heap layout [a, b].
fn Term uop_binary(u32 opcode, Term a, Term b) {
  Term r = uop_rewrite_binary(opcode, a, b);
  if (r != 0) return r;
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
