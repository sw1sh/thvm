// wnf/nf.c - explicit normal-form (NF) reducer driven by an
// incremental worklist with a re-enumerate fallback.  wnf is WHNF:
// it surfaces the head and stops at plain UOPs (ADD, MUL, REDUCE,
// ...) without descending into their children.  That leaves
// redexes nested inside (e.g. the UOPs an interact_grad chain
// rule produces) unfired -- they are redexes by IC semantics but
// the WHNF reducer never visits them.
//
// Hot path is incremental: nf seeds the worklist via
// redex_enumerate then attaches the worklist to redex.c.  Every
// redex_fire then pushes its own locally-fresh successors (result
// + cells the interaction allocated) directly into the worklist
// before returning.  Per-fire cost is O(allocated-cells), not
// O(heap_size), so a long chain of grad + elementwise
// interactions is linear in the number of interactions.  Pushes
// happen at the firing site, so a per-thread worklist would slot
// in directly when nf moves to multi-threaded firing.
//
// Cold path: when the worklist drains we re-enumerate.  Required
// for correctness because heap_replace inside redex_fire can
// promote a parent term to a redex (e.g. a UOP_ADD whose child
// cell was patched from a stuck DUP to a CTR -- the ADD itself
// wasn't on the worklist, no allocation happened during the fire,
// so the incremental push misses it).  Catching parent
// promotions incrementally would need explicit parent pointers,
// which the flat heap doesn't carry.  In practice the loop
// converges in 1-2 re-enumerates -- the inner incremental loop
// drains the bulk of the work, and the re-enumerate just sweeps
// a few promoted cells before reaching the fixed point.
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
  // Attach the worklist so redex_fire can push locally-fresh
  // redexes from inside each interaction.
  redex_worklist_attach(work, &n, NF_WORK_CAP);
  while (n > 0 && fired < NF_FIRE_CAP) {
    u32 fired_this_round = 0;
    while (n > 0 && fired < NF_FIRE_CAP) {
      Term r = work[--n];
      if (!nf_is_eager_redex(r)) continue;
      u64 itrs0 = ITRS;
      Term result = redex_fire(r);
      if (ITRS == itrs0) continue;       // stuck
      fired++;
      fired_this_round++;
      // heap_replace inside redex_fire substitutes new for old in
      // every heap cell -- but it can't update Terms held off-heap,
      // like the caller's root.  Track that explicitly so the
      // returned root reflects the head reduction.
      if (r == root && result != 0) root = result;
    }
    // Worklist drained; re-enumerate to catch parent-promotion
    // redexes (a term whose child cell was patched by heap_replace
    // and is now eligible).  Terminate when nothing fired in a
    // full pass -- re-enumerating the same stuck redexes would
    // loop forever.
    if (fired_this_round == 0) break;
    n = redex_enumerate(&root, 1, work, NF_WORK_CAP);
  }
  redex_worklist_detach();
  return root;
}
