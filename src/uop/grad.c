// uop/grad.c - construct a UOP_GRAD / UOP_FWD pair sharing a cell.
//
// Cell layout: heap[loc] = y.  Both UOP_FWD and UOP_GRAD terms point
// at the same loc with their opcode as discriminator.  Mirrors a
// dup-like agent with two aux ports:
//
//                      y (principal)
//                      |
//                    GRAD                           y at heap[loc]
//                   /    \                              ^
//                  fw     bw           --->       FWD --+-- GRAD
//
// The chain-rule rewrite fires when interact_grad is invoked on the
// BWD projection.  It allocates fresh sub-cells for each child of y,
// builds new fw/bw expressions referencing those children's
// projections, and heap_replaces BOTH the original UOP_FWD(loc) and
// UOP_GRAD(loc) at once.  Lazy: each level only fires when its
// projection is consumed.

fn u64 uop_grad_cell(Term y) {
  u64 loc = heap_alloc(1);
  heap_set(loc, y);
  return loc;
}

fn Term uop_grad(Term y) {
  return term_new(0, TAG_UOP, UOP_GRAD, uop_grad_cell(y));
}

fn Term uop_fwd(Term y) {
  return term_new(0, TAG_UOP, UOP_FWD, uop_grad_cell(y));
}
