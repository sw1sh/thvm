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
//
// Caveat: TAG_REF / TAG_ALO are EXCLUDED from eager firing because
// they unfold named (potentially recursive) definitions.  In a
// recursive definition like `sgd_loop = \w n. if n==0 then w else
// sgd_loop ...`, eagerly forcing every reachable ALO descends into
// the recursive call inside the `else` branch before the
// conditional has fired -- non-terminating.  wnf handles them
// lazily (force only when their value is consumed by a head
// position); nf delegates to wnf via the surrounding TRealize
// loop's nf -> materialize -> nf cadence.

static u8 nf_is_eager_redex(Term t) {
  if (!is_redex(t)) return 0;
  u8 tag = term_tag(t);
  return tag != TAG_REF && tag != TAG_ALO;
}

#define NF_WORK_CAP   8192
#define NF_FIRE_CAP   (1u << 20)    // safety bound on total fires per nf call

fn Term nf(Term root) {
  Term work[NF_WORK_CAP];
  u32  n     = redex_enumerate(&root, 1, work, NF_WORK_CAP);
  u32  fired = 0;
  while (n > 0 && fired < NF_FIRE_CAP) {
    Term r = work[--n];
    if (!nf_is_eager_redex(r)) continue;
    u64 hb = HEAP_NEXT;
    u64 itrs0 = ITRS;
    Term result = redex_fire(r);
    if (ITRS == itrs0) continue;       // stuck (e.g. UOP_GRAD with unresolvable y)
    fired++;
    // heap_replace(old, new) inside redex_fire substitutes new for
    // old in every heap cell -- but it can't update Terms held off-
    // heap, like the caller's root.  Track that explicitly so the
    // returned root reflects the head reduction.
    if (r == root && result != 0) root = result;
    if (result != 0 && nf_is_eager_redex(result) && n < NF_WORK_CAP)
      work[n++] = result;
    // Cells the fire allocated may carry fresh redexes the
    // interaction just produced.
    for (u64 i = hb; i < HEAP_NEXT && n < NF_WORK_CAP; i++) {
      Term c = heap_read(i);
      if (nf_is_eager_redex(c)) work[n++] = c;
    }
  }
  return root;
}
