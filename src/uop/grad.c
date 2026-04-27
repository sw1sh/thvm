// uop/grad.c - construct a UOP_GRAD node.
//
// Two heap layouts share TAG_UOP/UOP_GRAD; interact_grad disambiguates
// by inspecting slot 1's tag.
//
//   OUTER (constructed by user-facing TGrad):
//     heap = [y, NUM(1), target]
//     No gy stored; the cotangent seed is built lazily by
//     interact_grad on first fire.  Slot 1 is a NUM marker.
//
//   INNER (emitted by interact_grad's chain rule recursion):
//     heap = [y, gy, target]
//     gy is the threaded cotangent (a UOP/TEN, never a NUM).
//
// The split keeps the user's diagram clean (no CONST(1.0) seed
// dangling off every TGrad at construction time) while still letting
// the chain rule thread cotangents through the recursion.

fn Term uop_grad(Term y, Term target) {
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, y);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_I32, 1));
  heap_set(loc + 2, target);
  return term_new(0, TAG_UOP, UOP_GRAD, loc);
}

fn Term uop_grad_inner(Term y, Term gy, Term target) {
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, y);
  heap_set(loc + 1, gy);
  heap_set(loc + 2, target);
  return term_new(0, TAG_UOP, UOP_GRAD, loc);
}

// True iff this GRAD is the OUTER (no-gy) form.  Slot 1 is TAG_NUM
// for outer (the marker), TAG_UOP/TAG_TEN for inner (the gy term).
fn u8 uop_grad_is_outer(Term grad_term) {
  if (term_tag(grad_term) != TAG_UOP)  return 0;
  if (term_ext(grad_term) != UOP_GRAD) return 0;
  Term slot1 = heap_read(term_val(grad_term) + 1);
  return term_tag(slot1) == TAG_NUM;
}

// Both layouts are unary; n is always 1.  Kept as a fn for any
// callers that previously walked a multi-GRAD.
fn u32 uop_grad_n(Term grad_term) {
  if (term_tag(grad_term) != TAG_UOP)  return 0;
  if (term_ext(grad_term) != UOP_GRAD) return 0;
  return 1;
}

fn Term uop_grad_target(Term grad_term, u32 i) {
  if (term_tag(grad_term) != TAG_UOP)  return 0;
  if (term_ext(grad_term) != UOP_GRAD) return 0;
  if (i != 0) return 0;
  return heap_read(term_val(grad_term) + 2);
}
