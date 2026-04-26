// schedule/realize.c - one-shot materialize + wnf + per-step
// buffer pool rollback (sub-item b of the per-step buffer pool
// arc).
//
// Call from WL via thvm_wl_realize.  Equivalent to
// `wnf(thvm_materialize(expr))` plus an enclosing pool boundary
// that frees every CPU buffer alloc'd during the materialize+wnf
// EXCEPT those reachable from the result tensor's producer
// chain.
//
// Result-chain walk: starts at the result TenDesc's buf_id and
// recurses via TENS[t].producer_kid -> KERNELS[k].input_tids
// to mark every buffer the result depends on as preserved.
// The pool then rolls back skipping preserved bufs.

// Recursively walk the result tensor's producer chain, marking
// every reachable buf as preserved.  Conservative -- ALL forward
// intermediates that fed the result are kept alive across the
// rollback, so subsequent reads (TGrad chain rules, view-only
// alias chains, etc.) see consistent state.  An aggressive
// "result buf only" variant broke nn.wlt + segfaulted in
// in-flight tests; a smarter design would track which kernel
// outputs have been READ by all their consumers and free those
// (refcount-driven free, the next reuse-pass arc item).
static void mark_preserved_chain(u32 tid, u8 *visited_kids) {
  if (tid == 0 || tid >= TENS_NEXT) return;
  TenDesc *d = &TENS[tid];
  if (d->buf_id != 0) cpu_buf_mark_preserved(d->buf_id);
  u32 kid = d->producer_kid;
  if (kid == 0 || kid >= KERNELS_NEXT) return;
  if (visited_kids[kid]) return;
  visited_kids[kid] = 1;
  KernelEntry *ke = &KERNELS[kid];
  for (u32 i = 0; i < ke->n_inputs; i++) {
    mark_preserved_chain(ke->input_tids[i], visited_kids);
  }
}

fn Term thvm_realize(Term expr) {
  u32 wm = cpu_buf_pool_begin();

  // wnf+nf loop:
  //   - wnf handles head-position redexes lazily, including
  //     TAG_REF / TAG_ALO unfolding (which nf excludes from
  //     eager firing -- see src/wnf/nf.c -- so recursive named
  //     definitions don't blow the alo state stack).
  //   - nf then sweeps every remaining redex for deep cases the
  //     WHNF discipline doesn't reach (e.g. GRADs nested inside
  //     ADD(GRAD, GRAD) from interact_grad's chain rule).
  //   - materialize compiles whatever lazy compute survived.
  // Fixed point: a pass where materialize emits no fresh kernel
  // AND wnf+nf fire no interactions.  Either alone isn't enough --
  // wnf can fire kernels without growing KERNELS_NEXT, and
  // materialize can emit a kernel without producing any new
  // redexes (the kernel fires next iteration via wnf).
  // Safety cap (THVM_REALIZE_MAX_ITERS) bounds runaway loops; in
  // practice the loop converges in 2-3 iterations.
#define THVM_REALIZE_MAX_ITERS 64
  Term res = expr;
  for (int iter = 0; iter < THVM_REALIZE_MAX_ITERS; iter++) {
    u32 kn0   = KERNELS_NEXT;
    u64 itrs0 = ITRS;
    res = nf(wnf(res));
    Term mat = thvm_materialize(res);
    if (KERNELS_NEXT == kn0 && ITRS == itrs0) { res = mat; break; }
    kernel_compute_consumer_counts();
    res = mat;
  }

  // gc3: tracing-GC preserve.  Composes gc1 + gc2 into
  // mark_gc_preserve(res), which walks the live root set
  // (result + WNF_LAST_STACK + DEFS) AND defensively overlays
  // mark_heap_rooted_preserve to cover pending UOP cells
  // missed by the root-set walk (e.g., forward intermediates
  // a future TGrad realize will need).  Net effective
  // coverage matches hrp2 today -- the tracing infrastructure
  // lands cleanly, no bench delta yet; real savings unblock
  // once a WL-pinned-Terms side table lets gc_mark_term
  // find pending UOPs without the heap-rooted overlay.
  mark_gc_preserve(res);

  cpu_buf_pool_rollback_with_preserve(wm);
  cpu_buf_clear_preserved(wm);
  cpu_buf_clear_freeable(wm);
  return res;
}
