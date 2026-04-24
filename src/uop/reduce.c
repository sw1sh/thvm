// uop/reduce.c - construct a UOP_REDUCE node.
//
// Heap layout: [src, NUM(kind), NUM(axis)] -- args live as TAG_NUM
// cells alongside the source.  Keeps EXT free for the opcode and
// matches the layout pattern used by movement ops (RESHAPE/PERMUTE).

fn Term uop_reduce(u32 kind, u32 axis, Term src) {
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_I32, kind));
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_I32, axis));
  return term_new(0, TAG_UOP, UOP_REDUCE, loc);
}
