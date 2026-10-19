// wnf/pool.c - the worker pool nf drains redexes on.  Each worker owns a
// Chase-Lev WsDeque (util/wsq.c) as its redex bag, popping and pushing its own
// and stealing from its peers' when it runs dry.  The drains take the fire
// function as a parameter: tests pass a stub, nf passes nf_pool_fire.

#include <pthread.h>
#include <time.h>

typedef struct WnfWorker {
  u32             id;
  u32             _pad0;
  WsDeque         bag;
  WnfThreadState  wnf_state;     // WNF_STACK / WNF_S_POS while this worker runs
  u64             itrs_delta;
  u64             fires;            // # redexes successfully fired
  u64             steals;           // # successful steals from peers
  u64             steal_attempts;   // total steal calls (success + miss)
  u64             pushes;           // # bag pushes (locally + via fire)
  u64             active_ns;        // ns spent in pop+fire (busy time)
  u64             idle_ns;          // ns spent in spin (waiting for work)
  u64             wakeups;          // # times reactivated after going idle
  u64             dropped;          // # pushes a full bag refused with no thief (T=1)
  u64             _pad1[5];         // cache-line padding
} WnfWorker;

// Declared before WnfFireFn: a struct first named in a prototype would get
// prototype scope, a different type from the struct WnfPool below.
typedef struct WnfPool WnfPool;

// Fires one popped redex and pushes any new work itself (wnf_pool_push); the
// drains ignore the return value.
typedef u64 (*WnfFireFn)(WnfPool *p, u32 tid, Term redex, void *user);

// Workers 1..n-1 persist: the first drain_parallel spawns them, and each drain
// wakes them through start_cond / done_cond.
typedef struct WnfPoolCtl {
  pthread_mutex_t mu;
  pthread_cond_t  start_cond;       // driver wakes workers
  pthread_cond_t  done_cond;        // last worker out wakes driver
  u64             drain_gen;        // bumped each drain_parallel call
  u64             worker_seen_gen[MAX_THREADS];
  u32             pending;          // workers still in this drain's loop
  WnfFireFn       cur_fire;
  void           *cur_user;
  pthread_t       pthreads[MAX_THREADS];
  _Bool           spawned;          // pthreads are running
  _Bool           shutdown;         // wnf_pool_free signals exit
} WnfPoolCtl;

typedef struct WnfPool {
  u32                n;
  u32                _pad0;
  WnfWorker          workers[MAX_THREADS];
  CachePaddedAtomic  alive;        // workers still claiming work
  CachePaddedAtomic  total_fires;  // global fire count (merged at drain end)
  u64                drain_wall_ns; // wall time of this run's drains
  u32                drain_rounds;  // how many drain calls in this nf run
  WnfPoolCtl         ctl;          // persistent-worker thread control
} WnfPool;

// The worker this thread runs, whose bag step_fresh_push fills; NULL on a
// thread the pool does not run, as in a T=1 drain.
extern _Thread_local struct WnfWorker *CURRENT_WORKER;
_Thread_local struct WnfWorker *CURRENT_WORKER = NULL;

// Monotonic ns, for the active / idle accounting.
fn u64 wnf_pool_now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
}

fn _Bool wnf_pool_init(WnfPool *p, u32 n) {
  if (n == 0 || n > MAX_THREADS) return 0;
  p->n = n;
  atomic_store_explicit(&p->alive.v, n, memory_order_relaxed);
  atomic_store_explicit(&p->total_fires.v, 0, memory_order_relaxed);
  p->drain_wall_ns = 0;
  p->drain_rounds  = 0;
  // The ctl mutex and condvars are initialised by the first drain_parallel.
  memset(&p->ctl, 0, sizeof(p->ctl));
  for (u32 i = 0; i < n; i++) {
    p->workers[i].id              = i;
    p->workers[i].wnf_state.s_pos = 0;
    p->workers[i].itrs_delta      = 0;
    p->workers[i].fires           = 0;
    p->workers[i].steals          = 0;
    p->workers[i].steal_attempts  = 0;
    p->workers[i].pushes          = 0;
    p->workers[i].active_ns       = 0;
    p->workers[i].idle_ns         = 0;
    p->workers[i].wakeups         = 0;
    p->workers[i].dropped         = 0;
    p->workers[i].wnf_state.stack = (Term *)calloc(WNF_CAP, sizeof(Term));
    // 2^18 slots (2 MB) per bag, a depth-19 balanced ADD tree's bottom-level
    // seed; nf clamps its seed to it.
    if (!p->workers[i].wnf_state.stack ||
        !wsq_init(&p->workers[i].bag, /*cap_pow2=*/18)) {
      free(p->workers[i].wnf_state.stack);
      for (u32 j = 0; j < i; j++) {
        wsq_free(&p->workers[j].bag);
        free(p->workers[j].wnf_state.stack);
      }
      return 0;
    }
  }
  return 1;
}

static void wnf_pool_shutdown_workers(WnfPool *p);

fn void wnf_pool_free(WnfPool *p) {
  // Join the workers before freeing the bags and stacks they use.
  wnf_pool_shutdown_workers(p);
  for (u32 i = 0; i < p->n; i++) {
    wsq_free(&p->workers[i].bag);
    free(p->workers[i].wnf_state.stack);
    p->workers[i].wnf_state.stack = NULL;
  }
}

// Owner push: a WsDeque takes pushes from one thread only.
fn void wnf_pool_push(WnfPool *p, u32 tid, Term t) {
  if (t == 0) return;
  WsDeque *q = &p->workers[tid].bag;
  // wsq_push fails only on a full bag: spin until a thief takes a task.  With
  // no thief the task is dropped and counted, and nf re-enumerates.
  while (!wsq_push(q, (u64)t)) {
    if (p->n <= 1) {
      p->workers[tid].dropped++;
      return;
    }
    cpu_relax();
  }
  p->workers[tid].pushes++;
}

// Owner pop, LIFO from the bottom.
fn _Bool wnf_pool_pop_local(WnfPool *p, u32 tid, Term *out) {
  u64 x = 0;
  if (!wsq_pop(&p->workers[tid].bag, &x)) return 0;
  *out = (Term)x;
  return 1;
}

// Steal one task, visiting the peers round-robin from *cursor so thieves
// spread over victims.
fn _Bool wnf_pool_steal(WnfPool *p, u32 tid, Term *out, u32 *cursor) {
  if (p->n <= 1) return 0;
  p->workers[tid].steal_attempts++;
  u32 start = *cursor;
  for (u32 k = 1; k < p->n; k++) {
    u32 v = (start + k) % p->n;
    if (v == tid) continue;
    u64 x = 0;
    if (wsq_steal(&p->workers[v].bag, &x)) {
      *out = (Term)x;
      *cursor = v + 1;
      p->workers[tid].steals++;
      return 1;
    }
  }
  *cursor = start + 1;
  return 0;
}

// Zero the per-run counters, so the stats cover only the latest nf run.
fn void wnf_pool_reset_run(WnfPool *p) {
  for (u32 i = 0; i < p->n; i++) {
    p->workers[i].fires          = 0;
    p->workers[i].steals         = 0;
    p->workers[i].steal_attempts = 0;
    p->workers[i].pushes         = 0;
    p->workers[i].active_ns      = 0;
    p->workers[i].idle_ns        = 0;
    p->workers[i].wakeups        = 0;
    p->workers[i].itrs_delta     = 0;
    p->workers[i].dropped        = 0;
  }
  p->drain_wall_ns = 0;
  p->drain_rounds  = 0;
  atomic_store_explicit(&p->total_fires.v, 0, memory_order_relaxed);
}

// Whether a peer has stealable work: an idle worker's wake-up test.
fn _Bool wnf_pool_any_work(WnfPool *p, u32 tid) {
  for (u32 v = 0; v < p->n; v++) {
    if (v == tid) continue;
    if (wsq_can_steal(&p->workers[v].bag)) return 1;
  }
  return 0;
}

// T=1 drain: pop and fire until the bag is empty or max_fires fired, with no
// stealing.  Returns the fire count.
fn u64 wnf_pool_drain_local(WnfPool *p, u32 tid, WnfFireFn fire,
                            void *user, u64 max_fires) {
  WnfWorker *w = &p->workers[tid];
  Term r;
  u64 fired = 0;
  u64 t0 = wnf_pool_now_ns();
  while (fired < max_fires && wnf_pool_pop_local(p, tid, &r)) {
    (void)fire(p, tid, r, user);
    w->fires++;
    fired++;
  }
  u64 t1 = wnf_pool_now_ns();
  w->active_ns += (t1 - t0);
  p->drain_wall_ns += (t1 - t0);
  p->drain_rounds++;
  atomic_fetch_add_explicit(&p->total_fires.v, fired, memory_order_relaxed);
  return fired;
}

// total_fires, summed once a parallel drain ends rather than bumped per fire.
fn void wnf_pool_aggregate_fires(WnfPool *p) {
  u64 total = 0;
  for (u32 i = 0; i < p->n; i++) total += p->workers[i].fires;
  atomic_store_explicit(&p->total_fires.v, total, memory_order_relaxed);
}

// Sum and zero the workers' itrs_delta; nf adds it to ITRS.
fn u64 wnf_pool_merge_itrs(WnfPool *p) {
  u64 total = 0;
  for (u32 i = 0; i < p->n; i++) {
    total += p->workers[i].itrs_delta;
    p->workers[i].itrs_delta = 0;
  }
  return total;
}

// === Stats ===
// Pool counters copied out of the live pool: nf stores its run's in
// WNF_POOL_LAST_STATS, which the WL bridge (TPoolStats) reads.

#define WNF_POOL_STATS_MAX_WORKERS MAX_THREADS

typedef struct {
  u32 id;
  u64 fires;
  u64 steals;
  u64 steal_attempts;
  u64 pushes;
  u64 active_ns;
  u64 idle_ns;
  u64 wakeups;
  u64 itrs_delta;
} WnfWorkerStats;

typedef struct {
  u32             n_workers;
  u32             drain_rounds;
  u64             drain_wall_ns;
  u64             total_fires;
  WnfWorkerStats  workers[WNF_POOL_STATS_MAX_WORKERS];
} WnfPoolStats;

static WnfPoolStats WNF_POOL_LAST_STATS = {0};

fn void wnf_pool_stats_capture(WnfPool *p, WnfPoolStats *out) {
  out->n_workers     = p->n;
  out->drain_rounds  = p->drain_rounds;
  out->drain_wall_ns = p->drain_wall_ns;
  out->total_fires   = atomic_load_explicit(&p->total_fires.v,
                                            memory_order_relaxed);
  for (u32 i = 0; i < p->n; i++) {
    WnfWorker *w = &p->workers[i];
    WnfWorkerStats *s = &out->workers[i];
    s->id             = w->id;
    s->fires          = w->fires;
    s->steals         = w->steals;
    s->steal_attempts = w->steal_attempts;
    s->pushes         = w->pushes;
    s->active_ns      = w->active_ns;
    s->idle_ns        = w->idle_ns;
    s->wakeups        = w->wakeups;
    s->itrs_delta     = w->itrs_delta;
  }
  // Zero the unused slots: readers may iterate to MAX_THREADS.
  for (u32 i = p->n; i < WNF_POOL_STATS_MAX_WORKERS; i++) {
    memset(&out->workers[i], 0, sizeof(WnfWorkerStats));
  }
}

fn const WnfPoolStats *wnf_pool_last_stats(void) {
  return &WNF_POOL_LAST_STATS;
}

fn void wnf_pool_stats_snapshot(WnfPool *p) {
  wnf_pool_stats_capture(p, &WNF_POOL_LAST_STATS);
}

// === Parallel drain ===

// T>1: a worker pops its bag, else steals, else leaves `alive` and spins until
// a peer has work (it rejoins) or alive is 0; the last one out (prev == 1)
// ends the drain.  wnf / cnf in a fire use the worker's own stack.
static void wnf_pool_worker_loop(WnfPool *p, u32 tid, WnfFireFn fire,
                                 void *user) {
  WnfWorker *w = &p->workers[tid];
  CURRENT_WNF_STATE = &w->wnf_state;
  CURRENT_WORKER    = w;   // step_fresh_push's bag: one producer per bag
  Term r;
  u32 cursor = (tid + 1) % p->n;
  u64 active_t0 = wnf_pool_now_ns();
  while (1) {
    if (wnf_pool_pop_local(p, tid, &r)) {
      (void)fire(p, tid, r, user);
      w->fires++;   // summed into total_fires when the drain ends
      continue;
    }
    if (wnf_pool_steal(p, tid, &r, &cursor)) {
      (void)fire(p, tid, r, user);
      w->fires++;
      continue;
    }
    u64 active_t1 = wnf_pool_now_ns();
    w->active_ns += (active_t1 - active_t0);
    u32 prev = atomic_fetch_sub_explicit(&p->alive.v, 1,
                                         memory_order_acq_rel);
    if (prev == 1) break;
    u64 idle_t0 = wnf_pool_now_ns();
    while (1) {
      cpu_relax();
      if (wnf_pool_any_work(p, tid)) {
        atomic_fetch_add_explicit(&p->alive.v, 1, memory_order_acq_rel);
        u64 idle_t1 = wnf_pool_now_ns();
        w->idle_ns += (idle_t1 - idle_t0);
        w->wakeups++;
        active_t0 = idle_t1;
        goto outer;
      }
      if (atomic_load_explicit(&p->alive.v,
                               memory_order_acquire) == 0) {
        u64 idle_t1 = wnf_pool_now_ns();
        w->idle_ns += (idle_t1 - idle_t0);
        goto done;
      }
    }
outer:;
  }
done:
  return;
}

typedef struct {
  WnfPool *pool;
  u32      tid;
} PersistentWorkerArg;

// Worker tid >= 1: for each new ctl.drain_gen, one worker loop; the last to
// finish signals done_cond.  ctl.shutdown ends the thread.
static void *wnf_pool_persistent_main(void *arg_) {
  PersistentWorkerArg *arg = (PersistentWorkerArg *)arg_;
  WnfPool *p = arg->pool;
  u32 tid = arg->tid;
  // Pin the spine routing + worker identity for this pthread's lifetime.
  CURRENT_WNF_STATE = &p->workers[tid].wnf_state;
  CURRENT_WORKER    = &p->workers[tid];
  while (1) {
    pthread_mutex_lock(&p->ctl.mu);
    while (!p->ctl.shutdown &&
           p->ctl.drain_gen == p->ctl.worker_seen_gen[tid]) {
      pthread_cond_wait(&p->ctl.start_cond, &p->ctl.mu);
    }
    if (p->ctl.shutdown) {
      pthread_mutex_unlock(&p->ctl.mu);
      free(arg);
      return NULL;
    }
    p->ctl.worker_seen_gen[tid] = p->ctl.drain_gen;
    WnfFireFn fire = p->ctl.cur_fire;
    void *user     = p->ctl.cur_user;
    pthread_mutex_unlock(&p->ctl.mu);

    wnf_pool_worker_loop(p, tid, fire, user);

    pthread_mutex_lock(&p->ctl.mu);
    if (--p->ctl.pending == 0) pthread_cond_signal(&p->ctl.done_cond);
    pthread_mutex_unlock(&p->ctl.mu);
  }
}

// Spawn workers 1..n-1, once.
static void wnf_pool_ensure_workers(WnfPool *p) {
  if (p->ctl.spawned || p->n <= 1) return;
  pthread_mutex_init(&p->ctl.mu, NULL);
  pthread_cond_init(&p->ctl.start_cond, NULL);
  pthread_cond_init(&p->ctl.done_cond, NULL);
  for (u32 t = 1; t < p->n; t++) {
    PersistentWorkerArg *arg = (PersistentWorkerArg *)malloc(sizeof *arg);
    arg->pool = p;
    arg->tid  = t;
    pthread_create(&p->ctl.pthreads[t], NULL,
                   wnf_pool_persistent_main, arg);
  }
  p->ctl.spawned = 1;
}

static void wnf_pool_shutdown_workers(WnfPool *p) {
  if (!p->ctl.spawned) return;
  pthread_mutex_lock(&p->ctl.mu);
  p->ctl.shutdown = 1;
  pthread_cond_broadcast(&p->ctl.start_cond);
  pthread_mutex_unlock(&p->ctl.mu);
  for (u32 t = 1; t < p->n; t++) pthread_join(p->ctl.pthreads[t], NULL);
  pthread_mutex_destroy(&p->ctl.mu);
  pthread_cond_destroy(&p->ctl.start_cond);
  pthread_cond_destroy(&p->ctl.done_cond);
  p->ctl.spawned = 0;
}

// Drain on all n workers, worker 0 on this thread; returns this call's fires.
fn u64 wnf_pool_drain_parallel(WnfPool *p, WnfFireFn fire, void *user) {
  if (p->n == 1) {
    return wnf_pool_drain_local(p, 0, fire, user, ~(u64)0);
  }
  wnf_pool_ensure_workers(p);

  u64 wall_t0 = wnf_pool_now_ns();
  u64 fires_before = 0;
  for (u32 i = 0; i < p->n; i++) fires_before += p->workers[i].fires;
  atomic_store_explicit(&p->alive.v, p->n, memory_order_relaxed);

  pthread_mutex_lock(&p->ctl.mu);
  p->ctl.cur_fire  = fire;
  p->ctl.cur_user  = user;
  p->ctl.pending   = p->n;
  p->ctl.drain_gen++;
  pthread_cond_broadcast(&p->ctl.start_cond);
  pthread_mutex_unlock(&p->ctl.mu);

  // Worker 0 is this thread: restore its routing afterwards.
  WnfThreadState *saved_state  = CURRENT_WNF_STATE;
  WnfWorker      *saved_worker = CURRENT_WORKER;
  wnf_pool_worker_loop(p, 0, fire, user);
  CURRENT_WNF_STATE = saved_state;
  CURRENT_WORKER    = saved_worker;

  pthread_mutex_lock(&p->ctl.mu);
  if (--p->ctl.pending > 0) {
    while (p->ctl.pending > 0) {
      pthread_cond_wait(&p->ctl.done_cond, &p->ctl.mu);
    }
  }
  pthread_mutex_unlock(&p->ctl.mu);

  u64 wall_t1 = wnf_pool_now_ns();
  p->drain_wall_ns += (wall_t1 - wall_t0);
  p->drain_rounds++;
  wnf_pool_aggregate_fires(p);
  u64 fires_after = 0;
  for (u32 i = 0; i < p->n; i++) fires_after += p->workers[i].fires;
  return fires_after - fires_before;
}
