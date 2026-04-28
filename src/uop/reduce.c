// uop/reduce.c - construct a UOP_REDUCE node.
//
// Heap layout: [src, NUM(kind), NUM(axis)] -- args live as TAG_NUM
// cells alongside the source.  Keeps EXT free for the opcode and
// matches the layout pattern used by movement ops (RESHAPE/PERMUTE).

fn Term uop_reduce(u32 kind, u32 axis, Term src) {
  u32 args[2] = {kind, axis};
  u64 key = uop_mov_hash(UOP_REDUCE, src, args, 2);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_I32, kind));
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_I32, axis));
  Term t = term_new(0, TAG_UOP, UOP_REDUCE, loc);
  uop_mov_insert(key, t);
  return t;
}
