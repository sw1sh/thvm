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

  // wnf-first loop: wnf can fire kernels and unroll GRADs to plain
  // UOPs.  After wnf, if any UOPs remain that would compile to
  // kernels, materialize emits them; then wnf fires those.  Loop
  // until materialize is a no-op (no fresh kernel emitted).  This
  // separates the two phases per the architecture choice point:
  // the user can call TWnf / TMaterialize directly to do Order A
  // (forward-first lazy backward) themselves; thvm_realize wires
  // the Order B / fully-converged form.
  Term res = wnf(expr);
  for (int iter = 0; iter < 16; iter++) {
    u32 kn0 = KERNELS_NEXT;
    Term mat = thvm_materialize(res);
    if (KERNELS_NEXT == kn0) { res = mat; break; }
    kernel_compute_consumer_counts();
    res = wnf(mat);
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
