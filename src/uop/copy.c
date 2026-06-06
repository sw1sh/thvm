// uop/copy.c - construct a UOP_COPY node (lazy device transfer).
//
// Heap layout: [src] -- one cell holding the source term.  COPY
// carries src's shape + dtype; at materialize time it uploads src to
// the realize backend (CURRENT_BACKEND).  When the realize backend
// already matches src's backend it is an identity (no kernel, no
// copy).  Mirrors tinygrad's Ops.COPY (uop/ops.py:660 copy_to_device,
// engine/realize.py:158 exec_copy host-staged upload).  thvm realizes
// on one backend per realize, so the COPY target is implicitly
// CURRENT_BACKEND and no explicit DEVICE arg lives in the heap.
//
// Hash-cons by (UOP_COPY, src), mirroring the single-source UOp
// constructors (uop/unary.c): COPY nodes are immutable and sharing an
// identical COPY across iterations / grad targets is always safe.

fn Term uop_copy(Term src) {
  u32 args[2] = { (u32)src, (u32)(src >> 32) };
  u64 key = uop_mov_hash(UOP_COPY, 0, args, 2);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(1);
  heap_set(loc, src);
  Term t = term_new(0, TAG_UOP, UOP_COPY, loc);
  uop_mov_insert(key, t);
  return t;
}
