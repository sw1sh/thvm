// uop/dag_has_dynamic_index.c - does a kernel address depend on loaded data?
//
// 1 iff some INDEX_E or STORE under `root` (every store of an AFTER root
// included) has an INDEX_E inside its address: a gather or scatter whose
// position is read from an index buffer.  The opt passes assume affine
// addresses, and a scatter's read-modify-write is only race-free as a
// serial loop, so emit flags such kernels no_opts.

static void dyn_index_push_srcs(UOpWalk *w, Term t) {
  u64 loc = term_val(t);
  if (term_ext(t) == UOP_AFTER) {
    uop_walk_push(w, heap_read(loc + 0));
    uop_walk_push(w, heap_read(loc + 1));
    return;
  }
  u8 ar = uop_arity((u8)term_ext(t));
  for (u8 i = 0; i < ar; i++) {
    uop_walk_push(w, heap_read(loc + i));
  }
}

fn int uop_dag_has_dynamic_index(Term root) {
  UOpWalk all  = {0};
  UOpWalk addr = {0};
  uop_walk_push(&all, root);
  for (Term t = uop_walk_pop(&all); t != 0; t = uop_walk_pop(&all)) {
    u32 op = term_ext(t);
    if (op == UOP_INDEX_E || op == UOP_STORE) {
      uop_walk_push(&addr, heap_read(term_val(t) + 1));
    }
    dyn_index_push_srcs(&all, t);
  }
  int found = 0;
  for (Term t = uop_walk_pop(&addr); t != 0; t = uop_walk_pop(&addr)) {
    if (term_ext(t) == UOP_INDEX_E) {
      found = 1;
      break;
    }
    dyn_index_push_srcs(&addr, t);
  }
  uop_walk_free(&all);
  uop_walk_free(&addr);
  return found;
}
