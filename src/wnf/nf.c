// wnf/nf.c - full-NF reducer.  wnf only walks the head spine; nf
// descends into every child so redexes nested inside UOP / GRAD /
// kernel chains all get fired.
//
// Worklist-driven: redex_fire pushes its locally-fresh successors
// (result + cells it allocated) so per-fire cost is the redexes
// the interaction created, not O(heap).  Re-enumerate fallback
// catches parent promotions (heap_replace turning a UOP_ADD's
// child into a value); typically 1-2 sweeps to fixed point.
//
// TAG_REF / TAG_ALO stay lazy: a recursive `sgd_loop = \w n. if
// n==0 then w else sgd_loop ...` would diverge if every reachable
// ALO were forced before the conditional fires.  wnf forces them
// only at head positions; nf delegates via the surrounding
// thvm_realize loop's nf -> materialize -> nf cadence.

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
