// wnf/nf.c - full-NF reducer.  wnf only walks the head spine; nf
// descends into every child so redexes nested inside UOP / GRAD /
// kernel chains all get fired.
//
// Driven by a WnfPool (src/wnf/pool.c) -- one Chase-Lev WsDeque per
// worker.  Default is T=1 (single-worker drain on the calling
// thread); set THVM_THREADS=N (>1) to spawn N pthreads for parallel
// drain.  Termination uses an atomic alive counter; per-worker WNF
// spine routing keeps wnf() / cnf() called from inside redex_fire
// pinned to that worker's stack.
//
// The root term is wrapped in a fresh heap cell so heap_replace
// inside redex_fire patches it automatically when the fire result
// supersedes the root.  Avoids any racy off-heap update to a
// caller-held Term* under T>1.
//
// Pool-attached redex_fire pushes locally-fresh successors (result +
// cells it allocated) onto the active worker's bag, so per-fire cost
// is the redexes the interaction created, not O(heap).
//
// Parent-promotion (heap_replace patches a child cell, the parent
// term is now a redex):
//
//   T=1  -- attach a step session for the duration of nf so the
//           inverse index in src/wnf/redex.c surfaces promotions
//           into a fresh-buf as heap_replace runs.  After each drain
//           we drain the fresh-buf back into the worker bag; no
//           full-heap re-enumerate.
//   T>1  -- step session is single-threaded only (its globals race
//           under MT), so we keep the legacy O(HEAP_NEXT) re-
//           enumerate-after-drain.  The inverse-index lift for the
//           parallel path is a separate slice (thread-safe inverse
//           index, deferred).
//
// TAG_REF / TAG_ALO stay lazy: a recursive `sgd_loop = \w n. if
// n==0 then w else sgd_loop ...` would diverge if every reachable
// ALO were forced before the conditional fires.

// Cheap tag-only filter: REF / ALO are excluded from eager firing
// (recursive named definitions would non-terminatingly unfold).  We
// trust redex_fire's internal is_redex check to handle stale bag
// entries -- duplicating that check here would double the heap_read
// cost on the per-fire hot path.
static u8 nf_skip_tag(Term t) {
  u8 tag = term_tag(t);
  return tag == TAG_REF || tag == TAG_ALO;
}

#define NF_FIRE_CAP   (1u << 20)    // safety bound on total fires per nf call

// Per-fire callback.  Children get pushed back through nf_work_push
// -> wnf_pool_push from inside redex_fire.  Root tracking happens via
// the heap-cell wrap in nf() (no off-heap pointer update needed), so
// the callback ignores `user`.
static u64 nf_pool_fire(WnfPool *p, u32 tid, Term r, void *user) {
  (void)p; (void)tid; (void)user;
  if (nf_skip_tag(r)) return 0;
  (void)redex_fire(r);
  return 0;
}

// Worker count source.  0 = unset (fall back to env).  Set via
// `nf_set_threads(n)` from the WL bridge or any C caller.  Reset to
// 0 with `nf_set_threads(0)` to re-enable env-var precedence.
static u32 NF_THREADS_OVERRIDE = 0;

fn void nf_set_threads(u32 n) {
  if (n > MAX_THREADS) n = MAX_THREADS;
  NF_THREADS_OVERRIDE = n;
}

fn u32 nf_get_threads(void) {
  if (NF_THREADS_OVERRIDE > 0) return NF_THREADS_OVERRIDE;
  const char *env = getenv("THVM_THREADS");
  if (env == NULL || env[0] == '\0') return 1;
  int v = atoi(env);
  if (v < 1) return 1;
  if (v > (int)MAX_THREADS) return MAX_THREADS;
  return (u32)v;
}

// Backwards-compat alias used inside nf().
static u32 nf_resolve_threads(void) { return nf_get_threads(); }

// Process-global pool: spawned lazily on first nf call, reused
// across calls so persistent workers survive between TNf
// invocations.  Rebuilt only when the thread count changes
// (THVM_THREADS env var or nf_set_threads()).  Released on
// thvm_free via nf_pool_global_release().
static WnfPool *NF_POOL_GLOBAL = NULL;

fn void nf_pool_global_release(void) {
  if (NF_POOL_GLOBAL == NULL) return;
  wnf_pool_free(NF_POOL_GLOBAL);
  free(NF_POOL_GLOBAL);
  NF_POOL_GLOBAL = NULL;
}

static WnfPool *nf_pool_global_for(u32 n_threads) {
  if (NF_POOL_GLOBAL != NULL && NF_POOL_GLOBAL->n != n_threads) {
    nf_pool_global_release();
  }
  if (NF_POOL_GLOBAL == NULL) {
    NF_POOL_GLOBAL = (WnfPool *)calloc(1, sizeof(WnfPool));
    if (NF_POOL_GLOBAL == NULL) return NULL;
    if (!wnf_pool_init(NF_POOL_GLOBAL, n_threads)) {
      free(NF_POOL_GLOBAL);
      NF_POOL_GLOBAL = NULL;
      return NULL;
    }
  }
  return NF_POOL_GLOBAL;
}

fn Term nf(Term root) {
  u32 n_threads = nf_resolve_threads();
  WnfPool *pool = nf_pool_global_for(n_threads);
  if (pool == NULL) return root;   // OOM -- no-op
  wnf_pool_reset_run(pool);
  redex_worklist_attach_pool(pool, 0);

  // Wrap root in a heap cell so heap_replace tracks root reduction
  // across however many workers.  After the drain, heap_read of the
  // cell gives the final root.
  u64  root_loc = heap_alloc(1);
  heap_set(root_loc, root);

  // T=1: attach a step session so heap_replace inside redex_fire
  // pushes parent-promoted redexes directly into the worker's bag
  // (see step_fresh_push's NF_POOL branch).  Single drain then sweeps
  // the entire reduction -- no per-fire round-trip through a fresh-buf
  // and no full-heap re-enumerate.  attach() refuses (returns 0) if a
  // session is already open (e.g. the WL stepper); in that case we
  // fall back to the legacy re-enumerate path.
  // Step session at T=1 always; opt-in at T>1 via THVM_NF_PARALLEL_STEP_SESSION
  // for benchmarking the atomic-CAS path.  Default off because the
  // inverse-index has remaining ordering bugs that surface as
  // duplicate fires on some shared-substructure workloads.
  _Bool step_used = 0;
  _Bool t1_only = 1;
  const char *step_env = getenv("THVM_NF_PARALLEL_STEP_SESSION");
  if (step_env != NULL && step_env[0] == '1') t1_only = 0;
  if (n_threads == 1 || !t1_only) {
    (void)redex_step_attach(&root, 1);
    step_used = STEP_ACTIVE;
  }

  // Seed buffer sized to fit every reachable redex without truncation
  // (worst case = HEAP_NEXT, but cap somewhere reasonable to bound
  // memory).  Enumerate-then-push pattern avoids interleaving the
  // walk with bag pushes (heap_replace's parent-promotion would race
  // a partial enumerate).
  // Seed buffer sized to fit every reachable redex without truncation
  // (worst case = HEAP_NEXT).  Also clamp to the pool bag's capacity
  // so the push loop never spins on a full bag -- overflow seeds get
  // re-seeded on the next round if step_session is off, or pushed by
  // heap_replace's parent-promotion if it is.
  u32 seed_cap = HEAP_NEXT > 4096 ? (u32)HEAP_NEXT : 4096;
  if (seed_cap > (u32)pool->workers[0].bag.cap) {
    seed_cap = (u32)pool->workers[0].bag.cap;
  }
  Term *seed = (Term *)malloc((size_t)seed_cap * sizeof(Term));
  u32  seed_n = redex_enumerate(&root, 1, seed, seed_cap);
  if (seed_n > seed_cap) seed_n = seed_cap;
  for (u32 i = 0; i < seed_n; i++) wnf_pool_push(pool, 0, seed[i]);

  u64 fired_total = 0;
  while (fired_total < NF_FIRE_CAP) {
    u64 itrs_before = ITRS;
    if (n_threads > 1) {
      (void)wnf_pool_drain_parallel(pool, nf_pool_fire, NULL);
    } else {
      (void)wnf_pool_drain_local(pool, 0, nf_pool_fire, NULL,
                                 NF_FIRE_CAP - fired_total);
    }
    // Fold per-worker itrs_delta back into the global atomic so
    // fired_round picks up any worker-local bumps.  At T=1 the
    // delta is always 0 (CURRENT_WORKER is NULL on the local-drain
    // path) so this is a noop there.  Kept around because the
    // Bend-style AOT runtime (Phase 1+) will resume routing
    // per-worker counters through this same merge.
    {
      u64 itrs_d = wnf_pool_merge_itrs(pool);
      if (itrs_d) ITRS += itrs_d;
    }
    u64 fired_round = ITRS - itrs_before;
    fired_total += fired_round;
    if (fired_round == 0) break;       // nothing fired -- fixed point

    // T=1 with step session: heap_replace already pushed parent
    // promotions into the bag during drain, so this loop just terminates
    // once the bag drains for real.  T>1 (or T=1 without a session)
    // falls back to full-heap re-enumerate to catch promotions that
    // had no inverse-index hook.
    if (step_used) {
      seed_n = 0;
    } else {
      root = heap_read(root_loc);
      seed_n = redex_enumerate(&root, 1, seed, seed_cap);
      if (seed_n > seed_cap) seed_n = seed_cap;
    }
    if (seed_n == 0) break;
    for (u32 i = 0; i < seed_n; i++) wnf_pool_push(pool, 0, seed[i]);
  }
  free(seed);

  if (step_used) redex_step_detach();

  // Snapshot per-worker stats before the next nf call resets them.
  // The WL bridge / TPoolStats reads through wnf_pool_last_stats().
  wnf_pool_stats_snapshot(pool);

  redex_worklist_detach_pool();
  // Pool stays alive (process-global) -- workers wait on start_cond
  // for the next call.  Released at thvm_free via nf_pool_global_release.
  return heap_read(root_loc);
}
