// ! &L{x0, x1} = NUM(v)
// ---------------------- DUP-NUM (annihilate)
// x0 <- NUM(v)
// x1 <- NUM(v)
//
// NUM is atomic: it has no heap cells, no internal sharing concern,
// so projections simply copy the Term value into both sides.
fn Term interact_dup_num(u8 side, u64 loc, Term num) {
  ITRS++;
  /* Pass `loc` (the DUP cell's heap loc holding the NUM body) as the
     carrier so wire_prov[loc] gives the producer event.  Passing the
     NUM Term word would have term_val = the integer value, which is
     not a heap loc -- the consumed lookup would be meaningless. */
  multi_emit(RULE_DUP_NUM, MULTI_PLUMB, loc, 0, 0);
  return heap_subst_cop(side, loc, num, num);
}
