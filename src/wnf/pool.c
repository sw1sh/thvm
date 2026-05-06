// wnf/pool.c -- worker pool that will drive parallel WNF / NF.
//
// Phase 1 step 2 (docs/aot.md, docs/plans/levy_optimal.md).  This
// file lands the pool data structure and a generic drain loop; the
// actual wiring of `nf` onto the pool comes later, once the heap
// primitives are atomic and per-worker bumps exist.
//
// Each worker owns a Chase-Lev WsDeque (the redex bag) plus its own
// counters (itrs delta, fire count) so the global ITRS doesn't need
// to be incremented under contention.  Workers push locally fresh
// redexes onto their own bag and steal from peers when idle.
//
// nf doesn't have priority levels (every redex is equally eager), so
// we use raw WsDeque per worker rather than the bucketed Wspq -- the
// bucketing is collapse-specific and lands with the parallel CNF
// port (Phase 5).
//
// The drain loop is parametric in the fire function so this file
// stays decoupled from src/wnf/redex.c -- swap in a stub for tests,
// swap in `redex_fire` (with a thread-local NF_WORK_PTR shim) when
// nf lands on top.

#include <pthread.h>
#include <time.h>

typedef struct WnfWorker {
  u32             id;
  u32             _pad0;
  WsDeque         bag;
  WnfThreadState  wnf_state;     // routed by WNF_STACK / WNF_S_POS via
                                 // CURRENT_WNF_STATE when this worker
                                 // is the active thread
  u64             itrs_delta;
  u64             fires;            // # redexes successfully fired
  u64             steals;           // # successful steals from peers
  u64             steal_attempts;   // total steal calls (success + miss)
  u64             pushes;           // # bag pushes (locally + via fire)
  u64             active_ns;        // ns spent in pop+fire (busy time)
  u64             idle_ns;          // ns spent in spin (waiting for work)
  u64             wakeups;          // # times reactivated after going idle
  u64             _pad1[6];         // pad out to a cache-line boundary so
                                    // adjacent workers' counters don't share
} WnfWorker;

// Forward-declare the WnfPool typedef so WnfFireFn's signature uses
// the typedef name (matching what callers in nf.c write); without
// this newer clang flags `struct WnfPool *` vs `WnfPool *` as
// incompatible function-pointer types even though they're the same.
typedef struct WnfPool WnfPool;

// Action to perform per popped redex.  Returns the number of *new*
// redexes the action wants to enqueue; the driver re-checks the local
// bag after each fire.  Pushing children happens via wnf_pool_push
// from inside the action itself (the action receives the pool + tid).
typedef u64 (*WnfFireFn)(WnfPool *p, u32 tid, Term redex, void *user);

// Persistent-worker control plane.  Workers are spawned lazily at
// the first drain_parallel call and kept alive across subsequent
// calls; signal start/done via mutex + condvars so the per-drain
// pthread_create + pthread_join overhead disappears.  Shutdown is
// signaled at wnf_pool_free.
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
  u64                drain_wall_ns; // last drain_parallel/local wall time
  u32                drain_rounds;  // how many drain calls in this nf run
  WnfPoolCtl         ctl;          // persistent-worker thread control
} WnfPool;

// Thread-local pointer to the worker driving the current thread.  NULL
// outside a parallel section (T=1, all serial paths) so the WNF_STACK /
// WNF_S_POS macros fall back to CURRENT_CTX.  Set by wnf_pool_drain_parallel
// inside each spawned pthread; restored on return.
extern _Thread_local struct WnfWorker *CURRENT_WORKER;
_Thread_local struct WnfWorker *CURRENT_WORKER = NULL;

// Monotonic nanoseconds since some unspecified epoch.  Used to charge
// per-worker active vs idle time.  CLOCK_MONOTONIC is portable across
// macOS / Linux and survives wall-clock changes.
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
  // Control-plane: pthread mutex + condvars are lazily inited on the
  // first drain_parallel call (worker spawn point); zero them here so
  // the unspawned predicate is "ctl.spawned == 0".
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
    p->workers[i].wnf_state.stack = (Term *)calloc(WNF_CAP, sizeof(Term));
    // Bag: 256K slots = 2MB/worker.  Sized to hold the bottom-level
    // seed of a balanced ADD tree up to depth 19 (2^18 = 262144
    // initial redexes).  Larger than necessary for typical
    // workloads but cheap relative to the heap; bump WSPQ_CAP_POW2
    // (in src/util/wspq.c) for collapse-style fan-out.
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

// Forward decl: shutdown helper defined alongside the persistent
// worker spawn (further down with the parallel drain).
static void wnf_pool_shutdown_workers(WnfPool *p);

fn void wnf_pool_free(WnfPool *p) {
  // Tear down persistent pthreads first so they aren't using the
  // bags / stacks while we free them.
  wnf_pool_shutdown_workers(p);
  for (u32 i = 0; i < p->n; i++) {
    wsq_free(&p->workers[i].bag);
    free(p->workers[i].wnf_state.stack);
    p->workers[i].wnf_state.stack = NULL;
  }
}

// Owner push (called from inside a fire action).
fn void wnf_pool_push(WnfPool *p, u32 tid, Term t) {
  if (t == 0) return;
  WsDeque *q = &p->workers[tid].bag;
  // wsq_push only fails if the deque is full; spin briefly in that
  // case (the bag's WSPQ_CAP_POW2 = 4M slots, so practical overflow
  // is a runaway interaction loop -- caller's NF_FIRE_CAP guard is
  // the real backstop).
  while (!wsq_push(q, (u64)t)) cpu_relax();
  p->workers[tid].pushes++;
}

// Owner pop -- LIFO from the bottom.  Preserves the existing
// nf order so ITRS-counting tests keep passing once nf is wired.
fn _Bool wnf_pool_pop_local(WnfPool *p, u32 tid, Term *out) {
  u64 x = 0;
  if (!wsq_pop(&p->workers[tid].bag, &x)) return 0;
  *out = (Term)x;
  return 1;
}

// Try to steal one task from any other worker.  Visit peers in a
// shifted round-robin starting at `*cursor` so workers don't all
// hammer the same victim.  Bumps `workers[tid].steals` on success
// and `steal_attempts` on every call.
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

// Reset per-run counters so a process-global pool's stats reflect
// only the latest nf invocation rather than every call since spawn.
// Doesn't touch worker bags (they're empty between runs anyway) or
// the persistent-worker control plane (mutex / cond / drain_gen).
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
  }
  p->drain_wall_ns = 0;
  p->drain_rounds  = 0;
  atomic_store_explicit(&p->total_fires.v, 0, memory_order_relaxed);
}

// True iff some worker has work (used by an idle worker before going
// to sleep / declaring termination).
fn _Bool wnf_pool_any_work(WnfPool *p, u32 tid) {
  for (u32 v = 0; v < p->n; v++) {
    if (v == tid) continue;
    if (wsq_can_steal(&p->workers[v].bag)) return 1;
  }
  return 0;
}

// T=1 drain loop.  Pop -> fire -> repeat until the bag is empty.  No
// stealing, no termination handshake.  Returns the worker's fire count.
//
// The full T>1 driver lives separately and adds steal+termination
// once heap atomics + per-worker context are in place; this helper
// is the equivalent of nf.c's inner while-loop, factored so the data
// structure can be unit-tested against a stub fire function.
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

// Sum per-worker fire counts into total_fires.  Called from
// drain_parallel after all workers have finished -- avoids a
// per-fire atomic on a hot cache line under contention.
fn void wnf_pool_aggregate_fires(WnfPool *p) {
  u64 total = 0;
  for (u32 i = 0; i < p->n; i++) total += p->workers[i].fires;
  atomic_store_explicit(&p->total_fires.v, total, memory_order_relaxed);
}

// Sum per-worker itrs_delta into the host counter and reset.  Called
// at the end of a parallel run so the rest of the runtime sees the
// accumulated interaction count via plain ITRS.
fn u64 wnf_pool_merge_itrs(WnfPool *p) {
  u64 total = 0;
  for (u32 i = 0; i < p->n; i++) {
    total += p->workers[i].itrs_delta;
    p->workers[i].itrs_delta = 0;
  }
  return total;
}

// === Pool stats snapshot ===========================================
//
// `WnfPoolStats` is a flat struct callers (especially the WL bridge)
// can copy out before the pool struct goes out of scope.  Captures
// per-worker and pool-level counters; renderers slice it however they
// want.  Populated by `wnf_pool_stats_capture(p, &stats)` and stored
// in a process-global `WNF_POOL_LAST_STATS` after each `nf` run so
// the WL bridge can introspect the most recent run without holding
// the live pool.

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
  // Zero unused worker slots so consumers iterating up to MAX_THREADS
  // see clean zeros rather than the prior run's residue.
  for (u32 i = p->n; i < WNF_POOL_STATS_MAX_WORKERS; i++) {
    memset(&out->workers[i], 0, sizeof(WnfWorkerStats));
  }
}

// Public accessor for the last captured snapshot.  The WL bridge
// reads through this pointer and slices fields by name.
fn const WnfPoolStats *wnf_pool_last_stats(void) {
  return &WNF_POOL_LAST_STATS;
}

// Convenience: capture then store globally.  Called from `nf` right
// before `wnf_pool_free` wipes the live state.
fn void wnf_pool_stats_snapshot(WnfPool *p) {
  wnf_pool_stats_capture(p, &WNF_POOL_LAST_STATS);
}

// === T>1 parallel drain ============================================
//
// Spawns N-1 pthreads (worker 0 = the calling thread) and runs the
// classic work-stealing loop in each: pop local, else steal peer,
// else go idle.  Termination uses an atomic `alive` counter -- a
// worker that observes empty local + no stealable work decrements
// alive, then spins watching for fresh work or alive==0.  Last
// worker out (prev==1) signals end; spinners see alive==0 and exit.
//
// Each spawned pthread sets CURRENT_WNF_STATE to its own worker's
// WnfThreadState so wnf() / cnf() called from inside the fire
// callback resolve to private spine storage.

typedef struct {
  WnfPool   *pool;
  u32        tid;
  WnfFireFn  fire;
  void      *user;
} WnfPoolWorkerArg;

static void wnf_pool_worker_loop(WnfPool *p, u32 tid, WnfFireFn fire,
                                 void *user) {
  WnfWorker *w = &p->workers[tid];
  CURRENT_WNF_STATE = &w->wnf_state;
  CURRENT_WORKER    = w;   // step_fresh_push reads ->id to push to
                           // this worker's own bag (single-producer).
  Term r;
  u32 cursor = (tid + 1) % p->n;
  u64 active_t0 = wnf_pool_now_ns();
  while (1) {
    if (wnf_pool_pop_local(p, tid, &r)) {
      (void)fire(p, tid, r, user);
      w->fires++;   // per-worker; no atomic.  total_fires aggregated
                    // at drain end via wnf_pool_aggregate_fires.
      continue;
    }
    if (wnf_pool_steal(p, tid, &r, &cursor)) {
      (void)fire(p, tid, r, user);
      w->fires++;
      continue;
    }
    // Going idle: charge the active span up to here, decrement alive.
    u64 active_t1 = wnf_pool_now_ns();
    w->active_ns += (active_t1 - active_t0);
    u32 prev = atomic_fetch_sub_explicit(&p->alive.v, 1,
                                         memory_order_acq_rel);
    if (prev == 1) break;
    // Spin watching for either fresh work (reactivate) or all peers
    // gone (exit).  cpu_relax keeps the spin cheap.
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

// === Persistent worker pthread main ================================
//
// One pthread per worker (excluding worker 0 = the caller).  Each
// loops on `start_cond`: wait until ctl.drain_gen advances past our
// last seen, then run the worker_loop, then signal `done_cond` when
// pending hits 0.  ctl.shutdown breaks the loop so wnf_pool_free
// can join cleanly.

typedef struct {
  WnfPool *pool;
  u32      tid;
} PersistentWorkerArg;

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

// Spawn N-1 persistent worker pthreads on first use.  No-op if
// already spawned.  Idempotent / cheap.
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

// Signal shutdown + join.  Called from wnf_pool_free.  No-op if no
// pthreads were ever spawned.
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

// Drive the bag in parallel.  Worker 0 runs on the calling thread;
// workers 1..n-1 are persistent pthreads spawned at first call,
// signaled via condvar each subsequent call (no per-drain spawn /
// join overhead).  Returns the fire count for THIS call.
fn u64 wnf_pool_drain_parallel(WnfPool *p, WnfFireFn fire, void *user) {
  if (p->n == 1) {
    return wnf_pool_drain_local(p, 0, fire, user, ~(u64)0);
  }
  wnf_pool_ensure_workers(p);

  u64 wall_t0 = wnf_pool_now_ns();
  // Snapshot per-worker fires; aggregate the cross-worker total at
  // drain end via wnf_pool_aggregate_fires.  Avoids a per-fire
  // atomic on the hot total_fires counter.
  u64 fires_before = 0;
  for (u32 i = 0; i < p->n; i++) fires_before += p->workers[i].fires;
  atomic_store_explicit(&p->alive.v, p->n, memory_order_relaxed);

  // Set up + signal the persistent workers to start this drain.
  pthread_mutex_lock(&p->ctl.mu);
  p->ctl.cur_fire  = fire;
  p->ctl.cur_user  = user;
  p->ctl.pending   = p->n;
  p->ctl.drain_gen++;
  pthread_cond_broadcast(&p->ctl.start_cond);
  pthread_mutex_unlock(&p->ctl.mu);

  // Worker 0 = caller thread.  Save & restore both thread-locals so
  // outer code keeps its prior routing.
  WnfThreadState *saved_state  = CURRENT_WNF_STATE;
  WnfWorker      *saved_worker = CURRENT_WORKER;
  wnf_pool_worker_loop(p, 0, fire, user);
  CURRENT_WNF_STATE = saved_state;
  CURRENT_WORKER    = saved_worker;

  // Caller's drain done -- decrement pending.  If we were the last,
  // we don't need to wait.  Otherwise wait for the persistent workers.
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
  // Aggregate per-worker fires into the pool's total_fires snapshot
  // so wnf_pool_last_stats / TPoolStats see the full count.
  wnf_pool_aggregate_fires(p);
  u64 fires_after = 0;
  for (u32 i = 0; i < p->n; i++) fires_after += p->workers[i].fires;
  return fires_after - fires_before;
}
