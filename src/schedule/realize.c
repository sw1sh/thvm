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
  if (d->buf_id != 0) {
    cpu_buf_mark_preserved(d->buf_id);
    // Arena view: the parent arena CpuBuf isn't reachable through any
    // TenDesc.buf_id chain, but the view's bytes live inside it -- so
    // we also mark the parent preserved.  Without this, pool_rollback
    // would freelist-push the arena while a view still names its
    // bytes via its data pointer (dangling read).
    if (CPU_BUFS != NULL && d->buf_id < CPU_BUFS_NEXT) {
      u32 parent = CPU_BUFS[d->buf_id].parent_buf_id;
      if (parent != 0) cpu_buf_mark_preserved(parent);
    }
  }
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

// Forward-only reclaim (default ON; THVM_FWD_RECLAIM=0 disables).  The
// per-realize watermark rollback only reclaims buffers allocated DURING the
// current realize; a buffer an EARLIER realize allocated, that was `preserved`
// (WL-reachable) during that realize then dropped, is stranded below every
// later watermark and never freed.  The Qwen3-4B encoder hits this hard:
// each of 27 layers uploads ~0.37GB of fresh weights inside its own realize,
// so the device-resident set grows monotonically to ~8GB even though a
// single forward needs only the running activation + a few captured states.
// mark_gc_preserve(res) has just marked every buffer reachable from the live
// root set (result + EXTERN_PINNED_TERMS = all WL-held TTerms + DEFS), so any
// live buffer still UN-preserved is genuinely unreachable -- free them
// table-wide.  Skipped during JIT capture (recorded buffers are jit_pinned,
// already excluded, and the per-realize rollback suffices on hot training
// steps).  Mirror: tinygrad frees a Buffer when its last LazyBuffer refcount
// drops; thvm approximates that with a mark-sweep at the realize boundary
// that owns the WL root set.  Call site invariant: mark_gc_preserve(res) has
// run and thvm_metal_buf_clear_preserved has NOT yet.
fn void thvm_realize_fwd_reclaim(void) {
  if (jit_is_capturing()) return;
  // Default ON: this is the faithful realize-boundary GC (free a buffer when its
  // last WL refcount drops), correct since the heap-rooted preserve scan below
  // fixed the cross-realize free bug, free on the hot loop (the table-wide free
  // rides the GPU flush the realize already pays), and a no-op on CPU/CUDA (the
  // thvm_metal_buf_* calls stub to 0 off Metal).  THVM_FWD_RECLAIM=0 disables it
  // for A/B debugging.  Cached once on the first realize.
  static int fwd_reclaim = -1;
  if (fwd_reclaim < 0) {
    const char *e = getenv("THVM_FWD_RECLAIM");
    fwd_reclaim = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  if (!fwd_reclaim) return;
  // Overlay the heap-rooted preserve walk (the call the comment at the
  // thvm_realize preserve block promised but never wired up).  mark_gc_preserve
  // roots only at THIS realize's result + DEFS + EXTERN_PINNED, so a buffer
  // owned by a TenDesc that is referenced ONLY through a persistent,
  // hash-consed TAG_TEN heap leaf (a cross-realize materialized intermediate a
  // LATER realize will re-walk via view_resolve_inner's TAG_TEN base case) is
  // unreachable from those roots.  Without this scan the table-wide free below
  // reclaims that still-live buffer; the next realize's view_resolve_inner then
  // shares its dead buf_id (tensor_view_of re-increfs the freed slot) and binds
  // a nil MTLBuffer at a kernel input -- the FLUX Stage-3 VAE flat-image bug.
  // The scan walks [0, HEAP_NEXT) and preserves every TAG_TEN-reachable buffer,
  // a strict superset of the producer chain (see heap_rooted_preserve.c).  Runs
  // only on the (off-by-default) fwd-reclaim path, after the GPU flush the free
  // already pays, so it adds no cost to the hot training/replay loop.
  mark_heap_rooted_preserve();
  thvm_metal_buf_free_unpreserved_all();
  // Drop the preserve bits the scan set table-wide.  thvm_realize's
  // thvm_metal_buf_clear_preserved(metal_wm) only clears [metal_wm, NEXT); a
  // cross-realize buffer below the watermark would otherwise carry a stale
  // preserved=1 into the next realize's rollback and never be reclaimed.
  thvm_metal_buf_clear_preserved(1);
}

// Auto-trigger a Cheney collection at a realize boundary on EITHER heap-cell
// pressure (the historical trigger) OR device-byte pressure.  The dyn heap
// fills slowly (a few cells per realize), but each WL-held TTerm wrapper for a
// discarded prior-realize result pins a LARGE device buffer; WL's
// ManagedLibraryExpression GC is lazy in script mode, so those wrappers (and
// their buffers) accumulate -- device bytes grow ~6-8GB/denoise-step long
// before the heap-cell trigger fires.  A Cheney collection re-roots through
// EXTERN_PINNED (the live WL wrappers) and drops nothing WL still holds, but it
// compacts the heap so the next realize's fwd_reclaim free-unpreserved pass can
// reclaim buffers whose wrappers WL HAS since collected (the manager fired
// extern_pin_handle_drop).  Safe now that gc_node_size evacuates every UOP op
// correctly (the CAST/BITCAST fix).  Trigger when metal live bytes have grown
// past a bound since the last GC (default 512MB; THVM_GC_DEVICE_MB overrides,
// 0 disables).  Off-Metal thvm_metal_live_bytes() returns 0 -> never fires.
// `*res_io` rides the GC root set and is re-fetched after the space swap.
fn void thvm_realize_maybe_gc(Term *res_io) {
  if (!gc_enabled()) return;
  static int gc_disabled_env  = -1;
  static int gc_kb_env        = -1;
  static int gc_device_mb_env = -1;
  static u64 gc_device_baseline = 0;
  if (gc_disabled_env == -1) {
    const char *e = getenv("THVM_GC");
    gc_disabled_env = (e != NULL && e[0] == '0') ? 1 : 0;
  }
  if (gc_kb_env == -1) {
    const char *e = getenv("THVM_GC_KB");
    gc_kb_env = (e != NULL) ? atoi(e) : 0;
  }
  if (gc_device_mb_env == -1) {
    const char *e = getenv("THVM_GC_DEVICE_MB");
    gc_device_mb_env = (e != NULL) ? atoi(e) : 512;
  }
  if (gc_disabled_env) return;
  u64 trigger_words = (gc_kb_env > 0)
                        ? (u64)gc_kb_env * 128
                        : (gc_from_end() - gc_from_start()) / 2;
  int heap_pressure = HEAP_NEXT > gc_from_start() + trigger_words;
  int device_pressure = 0;
  if (gc_device_mb_env > 0) {
    u64 dev = thvm_metal_live_bytes();
    if (dev > gc_device_baseline + (u64)gc_device_mb_env * (1ull << 20))
      device_pressure = 1;
  }
  if (heap_pressure || device_pressure) {
    Term roots[1] = { *res_io };
    gc_collect(roots, 1);
    *res_io = roots[0];
    gc_device_baseline = thvm_metal_live_bytes();
  }
}

// === Ceiling-abort cleanup (Q2: the THVM_MAX_LIVE_BYTES retry-leak) ===
// When the Metal ceiling backstop fires mid-realize it longjmps straight out to
// the LibraryLink entry's THVM_WL_RECOVER, BYPASSING the pool-rollback at the
// normal realize exit -- so every buffer this failed realize allocated leaks,
// and live bytes grow monotonically across retries (28->45 GB over ~333 failed
// realizes, the 20-min wedge).  We stash the entry pool watermarks + an active
// flag in globals; thvm_realize_ceiling_cleanup() (called from the recovery
// branch) HARD-rolls the pools back to those watermarks, freeing the orphaned
// in-progress buffers so the next realize starts clean.  An over-budget run then
// fails in seconds instead of leaking for 20 minutes.
static u32 REALIZE_ABORT_CPU_WM   = 0;
static u32 REALIZE_ABORT_METAL_WM = 0;
#ifdef THVM_HAS_CUDA
static u32 REALIZE_ABORT_CUDA_WM  = 0;
#endif
static int REALIZE_ABORT_ACTIVE   = 0;
static u32 REALIZE_ABORT_SCOPE_DEPTH = 0;
static u32 REALIZE_ABORT_SAVED_DEVICE = 0;

// Public: invoked by the LibraryLink ceiling-recovery branch (and safe to call
// when no realize is in flight -- it no-ops).  Frees everything this realize
// allocated since its entry watermark.
fn void thvm_realize_ceiling_cleanup(void) {
  if (!REALIZE_ABORT_ACTIVE) return;
  REALIZE_ABORT_ACTIVE = 0;
  // The longjmp interrupted dispatch mid-flight; drain/flush so a freed buffer
  // is not still referenced by an in-flight GPU command, then hard-roll the
  // pools (no "preserve" -- the realize FAILED, its buffers are orphaned).
  cpu_buf_pool_rollback(REALIZE_ABORT_CPU_WM);
  thvm_metal_buf_pool_rollback_with_preserve(REALIZE_ABORT_METAL_WM);
  thvm_metal_buf_clear_preserved(REALIZE_ABORT_METAL_WM);
#ifdef THVM_HAS_CUDA
  cuda_buf_pool_rollback_with_preserve(REALIZE_ABORT_CUDA_WM);
  cuda_buf_clear_preserved(REALIZE_ABORT_CUDA_WM);
#endif
  // Unwind the materialized-loc scope the realize entered so a later realize's
  // scope depth (and its cross-realize cache) starts clean.
  while (materialized_loc_scope_depth() > REALIZE_ABORT_SCOPE_DEPTH)
    materialized_loc_scope_leave();
  materialized_loc_clear();
  // Restore the session default device the realize may have re-routed.
  if (CURRENT_CTX != NULL) CURRENT_CTX->default_device = REALIZE_ABORT_SAVED_DEVICE;
}

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

  // Device routing (tinygrad's "the device is in the graph"): realize the
  // graph on the device its leaves place it on (term_device_in), not a
  // global session backend.  A uniform-device graph -- all leaves COPY'd
  // to one device -- realizes wholly on that backend; default_device is
  // scoped for THIS call and restored at the single return below, so a
  // CPU-default session realizes a Metal-placed graph on Metal with no
  // DEV= switch.  dev < 0 (no device in the graph) keeps the session
  // default.  Mixed-device graphs report their first-found device here;
  // true per-op cross-backend routing is a later phase.
  u32 saved_default_device = CURRENT_CTX->default_device;
  // A COPY at the RESULT copies the output to a device; the COMPUTE still runs
  // on its SRC's device, and materialize_copy lowers the output transfer to a
  // fire-time cross-backend ASSIGN.  Route the realize to the COMPUTE device
  // (unwrap the output COPY), NOT the COPY's output TARGET: otherwise a CPU-src
  // graph whose result is TToDevice'd to Metal would allocate its CPU-compute
  // intermediates (e.g. const_to_tendesc scalars) on Metal, and a Metal buf id
  // read as the same-numbered CPU buffer yields wrong data.  Uniform-device
  // graphs (all leaves already on the target) are unaffected: term_device_in of
  // the unwrapped src still finds that device.
  Term route_term = resolved;
  while (term_tag(route_term) == TAG_UOP && term_ext(route_term) == UOP_COPY) {
    route_term = term_resolve(heap_read(term_val(route_term) + 0));
  }
  i32 want_device = term_device_in(route_term);
  if (want_device >= 0 && want_device != (i32)saved_default_device
      && ctx_ensure_backend(want_device) != NULL) {
    CURRENT_CTX->default_device = (u32)want_device;
  }

  double _rt0 = sched_prof_enabled() ? sched_prof_now() : 0.0;
  grad_memo_begin_realize();
  u32 cpu_wm   = cpu_buf_pool_begin();
  u32 metal_wm = thvm_metal_buf_pool_begin();
#ifdef THVM_HAS_CUDA
  u32 cuda_wm  = cuda_buf_pool_begin();
#endif
  kernel_fire_scope_begin();
  backend_dispatch_begin_all();
  // Start with a clean cross-realize loc -> tid cache so a prior
  // realize's intermediate (whose buffer may have been pool-rolled-
  // back) cannot alias a later realize's input.  Tests that call
  // bufferize_classify / thvm_materialize directly bypass realize
  // entirely and so never see a populated cache (gated by the scope
  // counter below).
  materialized_loc_clear();
  // Arm the ceiling-abort cleanup BEFORE entering the scope: if the Metal
  // ceiling backstop longjmps out before the normal exit below, the recovery
  // path (thvm_realize_ceiling_cleanup) hard-rolls these pools so the failed
  // realize's in-progress buffers don't leak across retries.
  REALIZE_ABORT_CPU_WM   = cpu_wm;
  REALIZE_ABORT_METAL_WM = metal_wm;
#ifdef THVM_HAS_CUDA
  REALIZE_ABORT_CUDA_WM  = cuda_wm;
#endif
  REALIZE_ABORT_SAVED_DEVICE = saved_default_device;
  REALIZE_ABORT_SCOPE_DEPTH  = materialized_loc_scope_depth();
  REALIZE_ABORT_ACTIVE       = 1;
  materialized_loc_scope_enter();

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
  // Bounded wnf<->materialize fixpoint loop (same shape + guard as
  // thvm_realize_many).  Iteration 0's wnf drives every interaction --
  // including the FULL chain-rule backward -- on the kernel-free tensor
  // graph, so the complete forward+backward sink emerges before any
  // kernel exists (an activation shared by forward and a backward
  // adjoint stays a single hash-consed node, and grad never descends
  // into a kernel's source_uop).  Materialize then compiles that sink
  // and the next iteration's wnf fires it.
  //
  // The loop is required for the assign->compute->assign chain: an
  // ASSIGN whose src is a compute
  // reading a PRIOR assign's mutated buffer (e.g. y := (w := w+x) * x)
  // cannot materialize until that prior assign has fired, which needs
  // its own materialize+wnf first.  One pass left the outer compute
  // stranded as a live UOP (the assign's dst never updated).  The
  // fixpoint guard -- no new kernel emitted AND no interaction fired --
  // bounds the loop and keeps a converged forward/backward graph at the
  // same two effective passes it always took.
  Term res = expr;
  u32 kn_at_call_start = KERNELS_NEXT;
  int iters_used = 0;
  for (int iter = 0; iter < THVM_REALIZE_MAX_ITERS; iter++) {
    u32 kn0   = KERNELS_NEXT;
    u64 itrs0 = ITRS;
    // Cold-start lever: parallel-compile every kernel the prior iteration's
    // materialize emitted BEFORE this wnf fires them one-at-a-time.  Without
    // this, each kernel's first dispatch blocks on its own serial clang
    // (~0.3s x N kernels = the bulk of cold-forward wall); the parallel
    // posix_spawn pool collapses that to one ncores-wide pass, and the
    // dispatch below then hits the populated dylib cache (no recompile).
    // Idempotent + cache-keyed, so calling it with the growing kernel range
    // every iteration is safe.
    SCHED_PROF_BLOCK(SCHED_PROF_PRECOMPILE,
                     cpu_jit_precompile_range(kn_at_call_start, KERNELS_NEXT));
    // Same cold-start lever on Metal: parallel-compile every tile kernel's
    // MSL->AIR + PSO before the serial wnf dispatch fires them one-at-a-time
    // (each lazy compile is ~1.5s of newLibraryWithSource: on cold start).
    // No-op stub off Metal; THVM_METAL_JIT_PARALLEL=0 disables.
    thvm_metal_precompile_range(kn_at_call_start, KERNELS_NEXT);
    SCHED_PROF_BLOCK(SCHED_PROF_WNF, res = wnf(res));
    // Count cross-subgraph consumers over the whole live graph before
    // materialize so the arena planner won't recycle a buf read by a
    // sibling subgraph.  Re-populated each iteration (the graph can grow
    // across the fixpoint loop); xpass_cc_populate resets first.
    xpass_cc_populate(res);
    Term mat = thvm_materialize(res);
    iters_used = iter + 1;
    if (KERNELS_NEXT == kn0 && ITRS == itrs0) { res = mat; break; }
    kernel_compute_consumer_counts();
    res = mat;
  }
  SCHED_PROF_BLOCK(SCHED_PROF_PRECOMPILE,
                   cpu_jit_precompile_range(kn_at_call_start, KERNELS_NEXT));
  thvm_metal_precompile_range(kn_at_call_start, KERNELS_NEXT);
  SCHED_PROF_BLOCK(SCHED_PROF_WNF, res = wnf(res));
  if (getenv("THVM_KCNT")) {
    fprintf(stderr, "DBG realize emit: %u kernels in %d iters\n",
            KERNELS_NEXT - kn_at_call_start, iters_used);
  }

  backend_dispatch_end_all();
  kernel_fire_scope_end();

  // Tracing-GC preserve: mark_gc_preserve(res) walks the live root set
  // (result + WNF_LAST_STACK + DEFS) AND overlays mark_heap_rooted_preserve
  // to cover pending UOP cells the root-set walk misses (e.g. forward
  // intermediates a future TGrad realize will need).
  SCHED_PROF_BLOCK(SCHED_PROF_GC_PRESERVE, mark_gc_preserve(res));
  jit_capture_mark_preserved();

  // Reached the normal exit: disarm the ceiling-abort cleanup (the rollback
  // below supersedes it).  A subsequent non-realize entry's ceiling longjmp
  // must not roll back into this finished realize's stale watermarks.
  REALIZE_ABORT_ACTIVE = 0;

  // Arena planner cross-step leak fix: drop the preserved bit on
  // every arena view CpuBuf (and its parent arena) so pool_rollback
  // reclaims the entire arena cohort.  By construction arena views
  // hold internal forward intermediates of THIS realize (the
  // last_use>0 + consumer_count==1 gate in
  // arena_boundary_is_plannable excludes sinks/roots), so dropping
  // them is safe: the SINK lives in a standalone (non-arena)
  // CpuBuf and keeps its preserve flag.  Without this clear, the
  // gc-mark walk over the SINK result reaches view-tids embedded in
  // KERNELS[].input_tids chains, marks the views preserved, and the
  // parent-arena link in tensor_mark_buf_preserved keeps the entire
  // 600MB arena alive across the rollback (the symptom: peak grows
  // ~1.4GB -> 2.1GB by step 4 of BS=16 MNIST).  Tinygrad mirror:
  // tinygrad/schedule/memory.py:13-16 _can_plan excludes held_bufs
  // from the arena, so planned bufs naturally die at end-of-schedule.
  cpu_buf_clear_preserved_arena_views(cpu_wm);
  cpu_buf_pool_rollback_with_preserve(cpu_wm);
  thvm_metal_buf_pool_rollback_with_preserve(metal_wm);
  thvm_realize_fwd_reclaim();
  // Free this realize's fp8->bf16 cast single-use transients (the matmul that
  // consumed each has fired).  Runs AFTER fwd_reclaim's heap-rooted preserve so
  // we unhook + decref buffers that scan would otherwise pin forever.
  thvm_free_fp8_transients(term_tag(res) == TAG_TEN ? (u32)term_val(res) : 0);
  cpu_buf_clear_preserved(cpu_wm);
  cpu_buf_clear_freeable(cpu_wm);
  thvm_metal_buf_clear_preserved(metal_wm);
#ifdef THVM_HAS_CUDA
  cuda_buf_clear_preserved_arena_views(cuda_wm);
  cuda_buf_pool_rollback_with_preserve(cuda_wm);
  cuda_buf_clear_preserved(cuda_wm);
#endif

  // Auto-trigger Cheney collection once the dyn heap crosses a
  // configurable fraction of from-space.  The result Term is added
  // to the root set; everything else WL holds is reached through
  // the EXTERN_PIN_HANDLES side table (extern_pin_handle_set on
  // every TTerm wrap), so the C-side pin table is authoritative
  // post-GC.  WL's ttermRaw refreshes through the handle on every
  // read, so cached raw Term integers can't go stale.
  // THVM_GC=0 disables; THVM_GC_KB overrides the trigger threshold
  // (in KB of heap; one cell = 8B).
  thvm_realize_maybe_gc(&res);

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
  if (!kgc_disabled_env) SCHED_PROF_BLOCK(SCHED_PROF_KGC, kernel_gc_sweep(res));

  // Cross-realize loc -> tid cache: scoped to ONE realize call so a
  // forward intermediate emitted as a kernel mid-pass stays reachable
  // for sibling children inside the same materialize loop (the
  // multi-root grad share that drove the cache addition), but does
  // NOT survive the pool-rollback -- whose freelist push would alias
  // a future allocation to the cached tid and corrupt subsequent
  // grad reads (BN-grad NaN repro).  Mirrors tinygrad's per-schedule
  // _realize_cache lifetime: the cache lives only inside one
  // run_rangeify call.
  materialized_loc_clear();
  materialized_loc_scope_leave();
  xpass_cc_reset();

  // Restore the session default device (scoped device routing, above).
  CURRENT_CTX->default_device = saved_default_device;
  if (sched_prof_enabled()) sched_prof_add(SCHED_PROF_REALIZE_TOTAL, _rt0);
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
#ifdef THVM_HAS_CUDA
  u32 cuda_wm  = cuda_buf_pool_begin();
#endif
  kernel_fire_scope_begin();
  backend_dispatch_begin_all();
  // Start with a clean cross-realize loc -> tid cache (see thvm_realize).
  materialized_loc_clear();
  materialized_loc_scope_enter();
  u32 kn_at_call_start = KERNELS_NEXT;

  Term res = ctr_term;
  int iters_used = 0;
  int kcnt_dbg = (getenv("THVM_KCNT") != NULL);
  for (int iter = 0; iter < THVM_REALIZE_MAX_ITERS; iter++) {
    u32 kn0   = KERNELS_NEXT;
    u64 itrs0 = ITRS;
    // Cold-start lever: parallel-compile the prior iteration's emitted
    // kernels before the per-child wnf below fires them serially (see
    // thvm_realize for the full rationale).  Idempotent + cache-keyed.
    cpu_jit_precompile_range(kn_at_call_start, KERNELS_NEXT);
    thvm_metal_precompile_range(kn_at_call_start, KERNELS_NEXT);
    // wnf is a no-op on a CTR (passive), so drive each child to
    // fire its kernels / ASSIGNs.  Materialize then walks the
    // entire bundle, sharing forward-kernel dedup across roots.
    u32 cn = term_ctr_n(res);
    Term children[256];
    if (cn > 256) cn = 256;
    for (u32 i = 0; i < cn; i++) children[i] = wnf(term_ctr_at(res, i));
    Term wnf_ctr = term_new_ctr(term_ext(res), children, cn);
    // Count cross-subgraph consumers over the whole bundle (after every
    // child's wnf has driven its backward) so the arena planner sees a
    // forward activation shared by sibling grad targets.  Re-populated
    // each iteration: the backward graph can grow across the fixpoint
    // loop, so a once-at-iter-0 walk would miss late-emerging shared
    // consumers (see xpass_cc_populate).
    xpass_cc_populate(wnf_ctr);
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

  // Arena planner cross-step leak fix (see thvm_realize for full
  // rationale).  Drop preserve on arena views + parent arenas so
  // pool_rollback reclaims them; sink CpuBufs are standalone (not
  // arena) and keep their preserve flag.  Tinygrad mirror:
  // tinygrad/schedule/memory.py:13-16.
  cpu_buf_clear_preserved_arena_views(cpu_wm);
  cpu_buf_pool_rollback_with_preserve(cpu_wm);
  thvm_metal_buf_pool_rollback_with_preserve(metal_wm);
  thvm_realize_fwd_reclaim();
  // Free this realize's fp8->bf16 cast single-use transients (the matmul that
  // consumed each has fired).  Runs AFTER fwd_reclaim's heap-rooted preserve so
  // we unhook + decref buffers that scan would otherwise pin forever.
  thvm_free_fp8_transients(term_tag(res) == TAG_TEN ? (u32)term_val(res) : 0);
  cpu_buf_clear_preserved(cpu_wm);
  cpu_buf_clear_freeable(cpu_wm);
  thvm_metal_buf_clear_preserved(metal_wm);
#ifdef THVM_HAS_CUDA
  cuda_buf_clear_preserved_arena_views(cuda_wm);
  cuda_buf_pool_rollback_with_preserve(cuda_wm);
  cuda_buf_clear_preserved(cuda_wm);
#endif

  thvm_realize_maybe_gc(&res);

  static int kgc_disabled_env = -1;
  if (kgc_disabled_env == -1) {
    const char *e = getenv("THVM_KGC");
    kgc_disabled_env = (e != NULL && e[0] == '0') ? 1 : 0;
  }
  if (!kgc_disabled_env) kernel_gc_sweep(res);

  // Same lifetime as thvm_realize: drop the loc -> tid cache before
  // returning so non-preserved bufs (pool-rollback-recycled) can't be
  // re-handed-out under the same cached tid in a future realize call.
  materialized_loc_clear();
  materialized_loc_scope_leave();
  xpass_cc_reset();
  return res;
}
