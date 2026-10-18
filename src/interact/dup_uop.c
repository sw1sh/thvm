// ! &L{x0, x1} = UOP(...)
// ----------------------- DUP-UOP (shared, as DUP-NUM)
// x0 <- UOP(...)
// x1 <- UOP(...)
//
// A passive UOP is a DAG node the materializer only reads, so both sides share
// its cell; commuting the DUP into the inputs (HVM4's DUP-NOD) would copy the
// upstream graph at every fanout.  An active one must fire its own rule first.

fn Term interact_dup_uop(u32 lab, u64 loc, u8 side, Term uop) {
  (void)lab;
  u8 op = term_ext(uop);
  switch (op) {
    case UOP_KERNEL:
    case UOP_ASSIGN:
    case UOP_STORE:
    case UOP_AFTER:
      return 0;   // active: the caller leaves the DUP stuck
    default:
      break;
  }
  ITRS++;
  multi_emit(RULE_DUP_UOP, MULTI_DIST, loc, (u64)uop, 0);
  return heap_subst_cop(side, loc, uop, uop);
}
