// 1 iff `root` holds a CAST to a 16- or 8-bit float, which C holds as a float
// and so would not round: the C renderer declines the kernel to the walker.
fn int uop_dag_has_narrow_cast(Term root) {
  UOpWalk w = {0};
  uop_walk_push(&w, root);
  int  found = 0;
  Term t;
  while (!found && (t = uop_walk_pop(&w)) != 0) {
    if (term_ext(t) == UOP_CAST) {
      u32 dt = (u32)term_val(heap_read(term_val(t) + 1));
      found = dt == DT_FP16 || dt == DT_BF16 || dt == DT_FP8E4M3 || dt == DT_FP8E5M2;
    }
    u32 ns = uop_src_count(t);
    for (u32 i = 0; i < ns; i++) uop_walk_push(&w, uop_src_at(t, i));
  }
  uop_walk_free(&w);
  return found;
}
