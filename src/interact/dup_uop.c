// ! &L{x0, x1} = UOP_op(...)
// ---------------------- DUP-UOP (atomic copy, mirror DUP-NUM)
//
// A passive (compute-recipe) UOP heap cell is a read-only DAG node:
// the materializer reads its inputs and emits a kernel; the cell
// itself is never consumed or rewritten.  Multiple consumers can
// share one heap reference safely -- the materializer's ref-count
// pass handles fanout.
//
// So DUP-on-passive-UOP is the same shape as DUP-NUM: copy the Term
// word into both projections.  Both sides get the SAME heap
// reference; the DUP cell at `loc` is consumed (substituted out);
// no new heap is allocated; no recursive DUPs land on the children.
//
// The earlier rule (replicate the UOP at each projection and push
// new DUPs into its compute slots) is the HVM4 DUP-CON pattern,
// which is correct for true CON-like constructors but wrong for
// compute redexes -- it duplicates the upstream subgraph at every
// fanout point, exploding kernel count and breaking diamond DAG
// shape preservation.
//
// ACTIVE UOPs (KERNEL/ASSIGN/STORE/AFTER) carry state or evolve via
// their own interactions (e.g. KERNEL may rewrite into an output
// TEN; ASSIGN/STORE perform in-place writes; AFTER is a sequencing
// barrier).  For those the DUP label still matters and we leave the
// DUP stuck via `return 0`; the caller's default branch restores
// the body and propagates the DP up.
//
// Full normal form mode (cnf with no remaining DUPs) is not affected
// either way -- it walks the UOP DAG once and reduces.

fn Term interact_dup_uop(u32 lab, u64 loc, u8 side, Term uop) {
  (void)lab;
  u8 op = term_ext(uop);
  switch (op) {
    case UOP_KERNEL:
    case UOP_ASSIGN:
    case UOP_STORE:
    case UOP_AFTER:
      return 0;   // active: stay stuck so DUP label is preserved
    default:
      break;
  }
  ITRS++;
  multi_emit(RULE_DUP_UOP, MULTI_PLUMB, loc, (u64)uop, 0);
  return heap_subst_cop(side, loc, uop, uop);
}
