// uop/grad.c - construct a UOP_GRAD node.
//
// Heap layout: [y, gy_seed, NUM(n), x_1, ..., x_n].  Reducing under
// TWnf fires interact_grad, which applies the chain rule lazily and
// short-circuits at TAG_TEN leaves: returning gy_seed at any target
// leaf, and zero elsewhere.  The n>1 multi-target firing rule lands
// in k0c; until then interact_grad bails on n>1 and returns the
// grad term unchanged.

fn Term uop_grad_multi(Term y, Term gy, const Term *targets, u32 n) {
  u64 loc = heap_alloc(3 + n);
  heap_set(loc + 0, y);
  heap_set(loc + 1, gy);
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_I32, n));
  for (u32 i = 0; i < n; i++) heap_set(loc + 3 + i, targets[i]);
  return term_new(0, TAG_UOP, UOP_GRAD, loc);
}

fn Term uop_grad(Term y, Term gy, Term target) {
  return uop_grad_multi(y, gy, &target, 1);
}

fn u32 uop_grad_n(Term grad_term) {
  if (term_tag(grad_term) != TAG_UOP)        return 0;
  if (term_ext(grad_term) != UOP_GRAD)       return 0;
  Term n_cell = heap_read(term_val(grad_term) + 2);
  if (term_tag(n_cell) != TAG_NUM)           return 0;
  return (u32)term_val(n_cell);
}

fn Term uop_grad_target(Term grad_term, u32 i) {
  u32 n = uop_grad_n(grad_term);
  if (i >= n) return 0;
  return heap_read(term_val(grad_term) + 3 + i);
}
