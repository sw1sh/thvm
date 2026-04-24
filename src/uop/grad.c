// uop/grad.c - construct a UOP_GRAD node.
//
// Heap layout: [y, gy_seed, target] (three cells).  Reducing under
// TWnf fires interact_grad which applies the chain rule recursively
// and short-circuits at TAG_TEN leaves: returning gy_seed at the
// target leaf, and a zero CONST elsewhere.

fn Term uop_grad(Term y, Term gy, Term target) {
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, y);
  heap_set(loc + 1, gy);
  heap_set(loc + 2, target);
  return term_new(0, TAG_UOP, UOP_GRAD, loc);
}
