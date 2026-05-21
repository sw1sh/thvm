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

#define THVM_REALIZE_MAX_ITERS 64

// Forward decl: CTR-bundle entry point used by thvm_realize when a
// caller hands us a TAG_CTR of independent root Terms (e.g. all the
// per-param ASSIGNs of one Adam step).  Defined below.
fn Term thvm_realize_many(Term ctr_term);

fn Term thvm_realize(Term expr) {
  HOT_REALIZE_CALLS++;
  // TEN short-circuit: a Term that's already a TAG_TEN (typical when
  // the user holds onto a TTerm wrapper and re-realizes it) has no
  // remaining compute -- skip the wnf+materialize loop entirely so
  // we don't bump kernel_alloc on a no-op.  term_resolve handles
  // VAR-SUB / ALO chains pointing at a TEN.
  Term resolved = term_resolve(expr);
  if (term_tag(resolved) == TAG_TEN) return resolved;
  // CTR-bundle short-circuit: dispatch to the multi-root entry so
  // every child shares one materialize pass + one buffer-pool
  // boundary.  Per-param TAdam ASSIGNs land here.
  if (term_tag(resolved) == TAG_CTR) return thvm_realize_many(resolved);

  grad_memo_begin_realize();
  u32 cpu_wm   = cpu_buf_pool_begin();
  u32 metal_wm = thvm_metal_buf_pool_begin();
  kernel_fire_scope_begin();
  backend_dispatch_begin_all();

  // wnf is the only reducer -- `nf` is the inspector primitive (see
  // wnf/nf.c), NOT in this hot path.
  //   - wnf walks the head to WHNF using local SUB-bit substitution
  //     (heap_subst_var) -- O(1) per fire, no global heap_replace
  //     cascade.  TAG_REF / TAG_ALO unfold lazily.
  //   - When wnf finishes WHNF on a UOP it ALSO recursively drives
  //     every active child (DP1_GRAD / nested ASSIGN / nested KERNEL
  //     / DUP-projection inside UOP) via `uop_drive_inner_actives`
  //     (see src/interact/uop_grad.c).  Equivalent to tinygrad
  //     `.backward()`: a depth-first drive of every chain-rule
  //     projection in the live result graph, using wnf's own
  //     local-substitution semantics.
  //   - materialize compiles whatever lazy UOP compute survived.
  //     Materialize is graph -> kernel compile, NEVER fires
  //     interactions.
  // Three phases, NO loop:
  //   1. wnf drives every interaction -- including the FULL chain-rule
  //      backward -- on the tensor-level UOP graph.  No kernels exist
  //      yet, so grad never descends into a kernel's source_uop (that
  //      path is for the explicit materialize-then-grad case only).
  //      The complete forward+backward graph emerges as ONE shared
  //      sink, so an activation consumed by both the forward and a
  //      backward adjoint is a single hash-consed node.
  //   2. ONE materialize pass classifies that whole sink.
  //   3. wnf fires the emitted kernels (interact_kernel -> backend
  //      dispatch) to land the result TENs.
  Term res = expr;
  u32 kn_at_call_start = KERNELS_NEXT;
  int iters_used = 1;
  res = wnf(res);
  res = thvm_materialize(res);
  kernel_compute_consumer_counts();
  res = wnf(res);
  if (getenv("THVM_KCNT")) {
    fprintf(stderr, "DBG realize emit: %u kernels in %d iters\n",
            KERNELS_NEXT - kn_at_call_start, iters_used);
  }

  backend_dispatch_end_all();
  kernel_fire_scope_end();

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
  jit_capture_mark_preserved();

  cpu_buf_pool_rollback_with_preserve(cpu_wm);
  thvm_metal_buf_pool_rollback_with_preserve(metal_wm);
  cpu_buf_clear_preserved(cpu_wm);
  cpu_buf_clear_freeable(cpu_wm);
  thvm_metal_buf_clear_preserved(metal_wm);

  // Auto-trigger Cheney collection once the dyn heap crosses a
  // configurable fraction of from-space.  The result Term is added
  // to the root set; everything else WL holds is reached through
  // the EXTERN_PIN_HANDLES side table (extern_pin_handle_set on
  // every TTerm wrap), so the C-side pin table is authoritative
  // post-GC.  WL's ttermRaw refreshes through the handle on every
  // read, so cached raw Term integers can't go stale.
  // THVM_GC=0 disables; THVM_GC_KB overrides the trigger threshold
  // (in KB of heap; one cell = 8B).
  if (gc_enabled()) {
    static int gc_disabled_env = -1;
    static int gc_kb_env       = -1;
    if (gc_disabled_env == -1) {
      const char *e = getenv("THVM_GC");
      gc_disabled_env = (e != NULL && e[0] == '0') ? 1 : 0;
    }
    if (gc_kb_env == -1) {
      const char *e = getenv("THVM_GC_KB");
      gc_kb_env = (e != NULL) ? atoi(e) : 0;
    }
    u64 trigger_words = (gc_kb_env > 0)
                          ? (u64)gc_kb_env * 128
                          : (gc_from_end() - gc_from_start()) / 2;
    if (!gc_disabled_env && HEAP_NEXT > gc_from_start() + trigger_words) {
      Term roots[1] = { res };
      gc_collect(roots, 1);
      res = roots[0];
    }
  }

  // Kernel-arena GC: free per-kernel heap arrays + decref output
  // TenDescs for kernels no longer reachable from any pinned Term.
  // Bounds KERNELS_NEXT growth across long training loops -- without
  // this, each Adam step adds ~5K kernels on LeNet, hitting
  // KERNELS_CAP after ~50 steps.  Default-on; THVM_KGC=0 disables
  // for benchmarks that want the old "leak everything" behaviour.
  static int kgc_disabled_env = -1;
  if (kgc_disabled_env == -1) {
    const char *e = getenv("THVM_KGC");
    kgc_disabled_env = (e != NULL && e[0] == '0') ? 1 : 0;
  }
  if (!kgc_disabled_env) kernel_gc_sweep(res);

  return res;
}

// Multi-root realize: caller bundles N independent root Terms in a
// TAG_CTR; we run a single materialize + wnf loop over the bundle so
// shared forward kernels dedup across roots, then drive each child
// through wnf to fire any side-effecting nodes (UOP_ASSIGN, etc.).
//
// Why this matters: TAdam used to emit a separate TRealize per param,
// each one re-walking the gradient chain from scratch (8 realizes ->
// ~5K kernel slots/step on LeNet).  With one bundled realize the
// forward DAG materialize sees every per-param backward chain in the
// same pass and emits each forward kernel once.  Tinygrad analogue:
// `loss.realize(*opt.schedule_step())` fires the whole step in one
// scheduler pass.
fn Term thvm_realize_many(Term ctr_term) {
  HOT_REALIZE_CALLS++;
  if (term_tag(ctr_term) != TAG_CTR) return thvm_realize(ctr_term);
  u32 n = term_ctr_n(ctr_term);
  if (n == 0) return ctr_term;

  grad_memo_begin_realize();
  u32 cpu_wm   = cpu_buf_pool_begin();
  u32 metal_wm = thvm_metal_buf_pool_begin();
  kernel_fire_scope_begin();
  backend_dispatch_begin_all();
  u32 kn_at_call_start = KERNELS_NEXT;

  Term res = ctr_term;
  int iters_used = 0;
  int kcnt_dbg = (getenv("THVM_KCNT") != NULL);
  for (int iter = 0; iter < THVM_REALIZE_MAX_ITERS; iter++) {
    u32 kn0   = KERNELS_NEXT;
    u64 itrs0 = ITRS;
    // wnf is a no-op on a CTR (passive), so drive each child to
    // fire its kernels / ASSIGNs.  Materialize then walks the
    // entire bundle, sharing forward-kernel dedup across roots.
    u32 cn = term_ctr_n(res);
    Term children[256];
    if (cn > 256) cn = 256;
    for (u32 i = 0; i < cn; i++) children[i] = wnf(term_ctr_at(res, i));
    Term wnf_ctr = term_new_ctr(term_ext(res), children, cn);
    Term mat = thvm_materialize(wnf_ctr);
    iters_used = iter + 1;
    if (kcnt_dbg) {
      fprintf(stderr, "DBG realize_many iter %d: emitted %u kernels (ITRS+%llu)\n",
              iter, KERNELS_NEXT - kn0, (unsigned long long)(ITRS - itrs0));
    }
    if (KERNELS_NEXT == kn0 && ITRS == itrs0) { res = mat; break; }
    kernel_compute_consumer_counts();
    res = mat;
  }
  if (kcnt_dbg) {
    fprintf(stderr, "DBG realize_many emit: %u kernels in %d iters (%u children)\n",
            KERNELS_NEXT - kn_at_call_start, iters_used, n);
  }

  backend_dispatch_end_all();
  kernel_fire_scope_end();

  // Preserve mark + GC same as thvm_realize.  Walk every child of
  // the result CTR so each root's producer chain is preserved.
  u32 cn = term_ctr_n(res);
  for (u32 i = 0; i < cn && i < 256; i++) mark_gc_preserve(term_ctr_at(res, i));
  jit_capture_mark_preserved();

  cpu_buf_pool_rollback_with_preserve(cpu_wm);
  thvm_metal_buf_pool_rollback_with_preserve(metal_wm);
  cpu_buf_clear_preserved(cpu_wm);
  cpu_buf_clear_freeable(cpu_wm);
  thvm_metal_buf_clear_preserved(metal_wm);

  if (gc_enabled()) {
    static int gc_disabled_env = -1;
    static int gc_kb_env       = -1;
    if (gc_disabled_env == -1) {
      const char *e = getenv("THVM_GC");
      gc_disabled_env = (e != NULL && e[0] == '0') ? 1 : 0;
    }
    if (gc_kb_env == -1) {
      const char *e = getenv("THVM_GC_KB");
      gc_kb_env = (e != NULL) ? atoi(e) : 0;
    }
    u64 trigger_words = (gc_kb_env > 0)
                          ? (u64)gc_kb_env * 128
                          : (gc_from_end() - gc_from_start()) / 2;
    if (!gc_disabled_env && HEAP_NEXT > gc_from_start() + trigger_words) {
      Term roots[1] = { res };
      gc_collect(roots, 1);
      res = roots[0];
    }
  }

  static int kgc_disabled_env = -1;
  if (kgc_disabled_env == -1) {
    const char *e = getenv("THVM_KGC");
    kgc_disabled_env = (e != NULL && e[0] == '0') ? 1 : 0;
  }
  if (!kgc_disabled_env) kernel_gc_sweep(res);
  return res;
}
