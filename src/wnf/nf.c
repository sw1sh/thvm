// wnf/nf.c - explicit normal-form (NF) reducer driven by a local
// worklist.  wnf is WHNF: it surfaces the head and stops at plain
// UOPs (ADD, MUL, REDUCE, ...) without descending into their
// children.  That leaves redexes nested inside (e.g. the UOPs an
// interact_grad chain rule produces) unfired -- they are redexes by
// IC semantics but the WHNF reducer never visits them.
//
// nf seeds a worklist with every redex reachable from the root,
// then pops, fires via redex_fire, and pushes the locally-fresh
// redexes (the interaction's result + any cells the interaction
// allocated).  Loops until the worklist is empty -- per-fire cost
// is O(redexes-created), not O(heap), so a long chain of grad +
// elementwise interactions is linear in the number of interactions
// rather than quadratic in heap_size * number-of-sweeps.
//
// Pure IC machinery -- no opcode is privileged.  GRAD reduces
// because interact_grad is wired into redex_fire, the same way
// APP-LAM and KERNEL do.  Future combinators wired through
// is_redex / redex_fire pick up nf coverage automatically.

#define NF_WORK_CAP 8192

fn Term nf(Term root) {
  Term work[NF_WORK_CAP];
  u32  n = redex_enumerate(&root, 1, work, NF_WORK_CAP);
  while (n > 0) {
    Term r = work[--n];
    if (!is_redex(r)) continue;
    u64 hb = HEAP_NEXT;
    u64 itrs0 = ITRS;
    Term result = redex_fire(r);
    if (ITRS == itrs0) continue;       // stuck (e.g. UOP_GRAD with unresolvable y)
    // heap_replace(old, new) inside redex_fire substitutes new for
    // old in every heap cell -- but it can't update Terms held off-
    // heap, like the caller's root.  Track that explicitly so the
    // returned root reflects the head reduction.
    if (r == root && result != 0) root = result;
    if (result != 0 && is_redex(result) && n < NF_WORK_CAP)
      work[n++] = result;
    // Cells the fire allocated may carry fresh redexes the
    // interaction just produced.
    for (u64 i = hb; i < HEAP_NEXT && n < NF_WORK_CAP; i++) {
      Term c = heap_read(i);
      if (is_redex(c)) work[n++] = c;
    }
  }
  return root;
}
