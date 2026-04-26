// ! &L{x0, x1} = NUM(v)
// ---------------------- DUP-NUM (annihilate)
// x0 <- NUM(v)
// x1 <- NUM(v)
//
// NUM is atomic: it has no heap cells, no internal sharing concern,
// so projections simply copy the Term value into both sides.
fn Term interact_dup_num(u8 side, u64 loc, Term num) {
  ITRS++;
  return heap_subst_cop(side, loc, num, num);
}
