// wnf/nf.c - full normal form: fire every redex, nested ones included (wnf
// reduces only the head), on the WnfPool of wnf/pool.c with THVM_THREADS
// workers (or nf_set_threads; default 1).  The root sits in a heap cell, so
// heap_replace patches it.  At T=1 a step session's fires push the redexes they
// create or promote to the bag; T>1 runs without one and re-enumerates.

// REF / ALO stay lazy: forcing a recursive def's ALOs before its branch fires
// diverges (sgd_loop = \w n. if n == 0 then w else sgd_loop ...).  A stale bag
// entry needs no check here: redex_fire's is_redex drops it.
static u8 nf_skip_tag(Term t) {
  u8 tag = term_tag(t);
  return tag == TAG_REF || tag == TAG_ALO;
}

#define NF_FIRE_CAP   (1u << 20)    // safety bound on total fires per nf call

// The root lives in a heap cell, so `user` is unused.
static u64 nf_pool_fire(WnfPool *p, u32 tid, Term r, void *user) {
  (void)p; (void)tid; (void)user;
  if (nf_skip_tag(r)) return 0;
  (void)redex_fire(r);
  return 0;
}

// nf_set_threads(n) overrides THVM_THREADS; n = 0 restores it.
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

static u32 nf_resolve_threads(void) { return nf_get_threads(); }

// One pool for the process, built by the first nf and kept so its workers
// persist; rebuilt when the thread count changes, released by thvm_free.
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
  if (pool == NULL) return root;   // OOM: no-op
  wnf_pool_reset_run(pool);
  redex_worklist_attach_pool(pool, 0);

  u64  root_loc = heap_alloc(1);
  heap_set(root_loc, root);

  // T=1 only: at T>1 the step index double-fires shared substructure.
  _Bool step_used = 0;
  if (n_threads == 1) {
    (void)redex_step_attach(&root, 1);
    step_used = STEP_ACTIVE;
  }

  // The seed is clamped to the bag, so no push spins on a full one.
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
    // Fires bump ITRS directly and nothing sets itrs_delta, so this adds 0.
    {
      u64 itrs_d = wnf_pool_merge_itrs(pool);
      if (itrs_d) ITRS += itrs_d;
    }
    u64 fired_round = ITRS - itrs_before;
    fired_total += fired_round;
    if (fired_round == 0) break;       // nothing fired: a fixed point

    // With a session the fires pushed every successor, so a drained bag is
    // the end; without one, re-enumerate.
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

  // For TPoolStats (wnf_pool_last_stats), before the next nf resets them.
  wnf_pool_stats_snapshot(pool);

  redex_worklist_detach_pool();
  return heap_read(root_loc);
}
