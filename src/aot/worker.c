// src/aot/worker.c
//
// Worker pool for the Bend-style AOT runtime.
//
// Two run modes share the same `program->dispatch` / cont machinery:
//
//   aot_run_serial    -- T=1 fast path.  No threads, no atomics, no
//                        barrier.  Single LIFO task stack on the C
//                        stack; dispatch every task; resolve values
//                        up the cont tree.  Used both for ad-hoc
//                        T=1 invocations and as the n_threads<=1
//                        branch of aot_run.
//
//   aot_run_parallel  -- T>1.  Bend2's seed/work-phase pattern at
//                        TinyHVM/resources/gists/par_tree_sum_bend2_compiled.c:520-597.
//                        SEED splits tasks via dispatch until there's
//                        enough parallelism (#tasks >= N*2) so every
//                        worker has something to grab.  WORK runs the
//                        same loop without the seed-target check;
//                        compaction at the end of each round folds
//                        new tasks back to the start of the buffer.
//
// Atomics convention: `_Atomic u32` fields + `atomic_*_explicit`
// from <stdatomic.h>, matching src/wnf/pool.c.  Mixing __atomic_*
// builtins with `_Atomic` types fails to compile with our clang.

#ifndef THVM_AOT_WORKER_INCLUDED
#define THVM_AOT_WORKER_INCLUDED

#include <pthread.h>
#include <unistd.h>

#define AOT_MAX_THREADS    64
// Parallel queue cap.  Sized to hold one WORK round's max output:
// cur_cnt * AOT_INLINE_BUDGET pushes, where cur_cnt grows up to the
// total #fires across the run.  count(SUC^20) is 2^20 ~ 1M fires,
// so 4M slots * sizeof(AotTask)=40B = 160MB heap-alloc covers the
// D=20-22 bench.  Going further (D=24+) needs a different design
// (dynamic growth / per-worker queues / chunked resolve) since
// preallocating 640MB+ is wasteful for the common shallow case.
#define AOT_PARALLEL_BUF_CAP (4u * 1024u * 1024u)
// Serial path uses a LIFO stack on the C-stack.  Size is bounded
// by tree depth of the workload; 256 is enough for any practical
// recursion depth (deeper than 256 would blow the C call stack
// long before this).
#define AOT_SERIAL_STACK_CAP 256
#define AOT_SEED_ROUNDS    64    // hard cap on seed iterations

// === Term construction helpers ======================================
//
// These mirror term/new_ctr.c exactly except they route allocations
// through aot_heap_alloc (declared in halloc.h, included via
// src/aot/_.c above this).  CTR-heavy AOT programs use these to
// avoid the global heap_next contention that term_new_ctr otherwise
// pays per CTR.
// CTR-heavy AOT programs don't pay the global heap_next contention
// either).  Mirrors term/new_ctr.c exactly except for the allocator.
static inline Term aot_make_num_i32(u32 v) {
  return term_new(0, TAG_NUM, DT_INT32, v);
}

static inline Term aot_make_ctr0(u32 label) {
  u64 loc = aot_heap_alloc(1);
  heap_set(loc, term_new(0, TAG_NUM, DT_INT32, 0));
  return term_new(0, TAG_CTR, label, loc);
}

static inline Term aot_make_ctr1(u32 label, Term c0) {
  u64 loc = aot_heap_alloc(2);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, 1));
  heap_set(loc + 1, c0);
  return term_new(0, TAG_CTR, label, loc);
}

static inline Term aot_make_ctr2(u32 label, Term c0, Term c1) {
  u64 loc = aot_heap_alloc(3);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, 2));
  heap_set(loc + 1, c0);
  heap_set(loc + 2, c1);
  return term_new(0, TAG_CTR, label, loc);
}

static inline Term aot_make_ctr3(u32 label, Term c0, Term c1, Term c2) {
  u64 loc = aot_heap_alloc(4);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, 3));
  heap_set(loc + 1, c0);
  heap_set(loc + 2, c1);
  heap_set(loc + 3, c2);
  return term_new(0, TAG_CTR, label, loc);
}

static inline Term aot_make_ctr4(u32 label, Term c0, Term c1, Term c2, Term c3) {
  u64 loc = aot_heap_alloc(5);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, 4));
  heap_set(loc + 1, c0);
  heap_set(loc + 2, c1);
  heap_set(loc + 3, c2);
  heap_set(loc + 4, c3);
  return term_new(0, TAG_CTR, label, loc);
}

// Generic N-ary CTR construction.  Loop-based; useful for emit
// fallback when aot_make_ctrN doesn't have a fixed-arity helper.
// Caller passes a Term[] of length n.
static inline Term aot_make_ctrn(u32 label, u32 n, const Term *children) {
  u64 loc = aot_heap_alloc(1 + n);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, n));
  for (u32 i = 0; i < n; i++) heap_set(loc + 1 + i, children[i]);
  return term_new(0, TAG_CTR, label, loc);
}

// === Barrier =========================================================

typedef struct {
  pthread_mutex_t mu;
  pthread_cond_t  cv;
  _Atomic u32     cnt;     // # workers arrived in current generation
  _Atomic u32     gen;     // bumped each release; workers wait on it
  u32             total;
  _Bool           spin_only;  // skip pthread_cond_* if true
} AotBarrier;

static void aot_barrier_init(AotBarrier *b, u32 n) {
  pthread_mutex_init(&b->mu, NULL);
  pthread_cond_init(&b->cv, NULL);
  atomic_store_explicit(&b->cnt, 0, memory_order_relaxed);
  atomic_store_explicit(&b->gen, 0, memory_order_relaxed);
  b->total = n;
  // Spin-only when threads <= cores (no oversubscription).  Each
  // round of the work loop is microseconds at small/medium tree
  // depths; pthread_cond_wait's syscall + scheduler round-trip
  // dominates per-round cost otherwise.  Mirrors Bend2's pattern
  // at par_tree_sum_bend2_compiled.c:619-620.
  long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
  b->spin_only = (ncpu > 0 && (long)n <= ncpu);
}

static void aot_barrier_destroy(AotBarrier *b) {
  pthread_mutex_destroy(&b->mu);
  pthread_cond_destroy(&b->cv);
}

#if defined(__aarch64__)
  #define AOT_SPIN_PAUSE() __asm__ volatile("yield")
#elif defined(__x86_64__) || defined(_M_X64)
  #define AOT_SPIN_PAUSE() __asm__ volatile("pause")
#else
  #define AOT_SPIN_PAUSE() ((void)0)
#endif

static void aot_barrier_wait(AotBarrier *b) {
  u32 g = atomic_load_explicit(&b->gen, memory_order_acquire);
  u32 prev = atomic_fetch_add_explicit(&b->cnt, 1, memory_order_acq_rel);
  if (prev + 1 == b->total) {
    // Last arriver: release the generation, optionally broadcast.
    atomic_store_explicit(&b->cnt, 0, memory_order_relaxed);
    atomic_fetch_add_explicit(&b->gen, 1, memory_order_release);
    if (!b->spin_only) {
      pthread_mutex_lock(&b->mu);
      pthread_cond_broadcast(&b->cv);
      pthread_mutex_unlock(&b->mu);
    }
    return;
  }
  if (b->spin_only) {
    while (atomic_load_explicit(&b->gen, memory_order_acquire) == g) {
      AOT_SPIN_PAUSE();
    }
    return;
  }
  // Slow path: spin briefly, then sleep on the condvar if still
  // unreleased.  Bend2 spins ~128 iters before sleeping.
  for (int i = 0; i < 128; i++) {
    if (atomic_load_explicit(&b->gen, memory_order_acquire) != g) return;
    AOT_SPIN_PAUSE();
  }
  pthread_mutex_lock(&b->mu);
  while (atomic_load_explicit(&b->gen, memory_order_acquire) == g) {
    pthread_cond_wait(&b->cv, &b->mu);
  }
  pthread_mutex_unlock(&b->mu);
}

// === Per-run state shared across workers =============================

typedef struct {
  AotProgram   *program;
  AotTask      *task_buf;          // capacity AOT_PARALLEL_BUF_CAP
  _Atomic u32   task_cnt;          // # tasks to process this round
  _Atomic u32   task_cnt_new;      // # new tasks pushed mid-round
  _Atomic u32   work_idx;          // next task index a worker grabs
  AotBarrier    barrier;
  u32           n_threads;
} AotRun;

typedef struct {
  AotRun *run;
  u32     tid;
} AotWorkerArg;

// === Serial runner ===================================================

// LIFO task stack; dispatch one at a time; resolve values up.  No
// threads.  Out-buffer count is _Atomic only because aot_resolve's
// signature requires it (the parallel path needs the atomicity);
// at T=1 there's no contention so atomic ops degrade to plain
// reads/writes.
fn Term aot_run_serial(AotProgram *program, AotTask root_task) {
  AotTask stack[AOT_SERIAL_STACK_CAP];
  u32     sp = 0;
  u32     out_n;

  // Reset the calling thread's tl heap chunk so we don't write into
  // stale chunks from a previous run that may have been reset by
  // thvm_init.  Workers spawned by aot_run_parallel start with
  // zero-initialised __thread vars, so they don't need this.
  aot_heap_tl_reset();

  atomic_store_explicit(&program->done, 0, memory_order_relaxed);
  stack[sp++] = root_task;

  while (sp > 0 &&
         !atomic_load_explicit(&program->done, memory_order_acquire)) {
    AotTask t = stack[--sp];
    AotResult r = program->dispatch(program, &t);
    if (r.tag == AOT_R_VALUE) {
      out_n = 0;
      aot_resolve(program, t.ret, r.val, stack + sp, &out_n);
      sp += out_n;
      continue;
    }
    if (r.tag == AOT_R_SPLIT) {
      // Push t1 then t0 so t0 dispatches first (LIFO).
      stack[sp++] = r.t1;
      stack[sp++] = r.t0;
      continue;
    }
    // AOT_R_CALL
    stack[sp++] = r.t0;
  }
  return program->result;
}

// === Parallel worker loop ============================================

// Inline-DFS dispatch loop.  Two modes:
//
//   inline_mode=0 (SEED phase):
//       Each grabbed task gets dispatched ONCE; both children of
//       a SPLIT go into the queue; a CALL pushes one.  This grows
//       the queue exponentially -- exactly what we need to seed
//       enough parallelism for N workers.  Without this, one
//       worker would DFS-descend the whole tree from the root and
//       no queue would ever build up for the others.
//
//   inline_mode=1 (WORK phase):
//       After grabbing a task, descend serially -- on SPLIT push
//       t1 to the queue and continue inline on t0; on CALL just
//       continue.  Bounds queue growth (tree-sum at d=18 has
//       256k leaves and would overflow our 512-task buffer in
//       milliseconds otherwise).  Budget caps how deep one inline
//       chain goes so a single worker doesn't monopolise; when
//       hit, the current head is pushed back to the queue.
//
// WORK phase descent: each grabbed task runs through to completion
// using a per-worker LIFO C-stack -- exactly the shape of
// aot_run_serial, just embedded inside the parallel worker.  This
// matches Bend2's eval_*() pattern (TinyHVM/resources/gists/par_tree_sum_bend2_compiled.c)
// where SEED spreads tasks across workers and each worker drains
// its share atomic-free.  The global queue is only touched by SEED
// (~N*2 pushes) -- WORK does ZERO global pushes regardless of tree
// size, so D=22's 4M splits cost nothing on the hot path.
//
// AOT_LOCAL_STACK_CAP bounds tree depth.  256 covers any practical
// recursion (deeper than that would have blown the C call stack
// of the dispatch chain anyway).
#define AOT_LOCAL_STACK_CAP   256u

// Per-worker output buffer.  Pushes go into `local` with a non-atomic
// counter.  When `n` reaches AOT_WORKER_FLUSH_AT we batch-copy into
// the global queue with ONE atomic_fetch_add on run->task_cnt_new.
// This collapses ~16k atomics-per-chain (one per push) down to
// ~16k/AOT_WORKER_FLUSH_AT atomics-per-chain (one per chunk),
// eliminating the cross-core cache-line contention that dominated
// the T>1 perf gap.
//
// Sizing: pushes are checked against AOT_WORKER_FLUSH_AT after each
// dispatch; aot_resolve writes at most 2 tasks per call.  Local cap
// = flush threshold + small headroom for resolve overruns.  Keeping
// this tight keeps WorkerOut stack-allocated under macOS pthread's
// 512KB default thread-stack ceiling: 1024 slots * 40B = 40KB, well
// under the limit.
#define AOT_WORKER_FLUSH_AT    512u
#define AOT_WORKER_LOCAL_CAP   1024u

typedef struct {
  AotTask local[AOT_WORKER_LOCAL_CAP];
  u32     n;
} __attribute__((aligned(64))) WorkerOut;

static inline void worker_flush(AotRun *run, WorkerOut *wo, u32 base) {
  if (wo->n == 0) return;
  u32 dst = atomic_fetch_add_explicit(&run->task_cnt_new, wo->n,
                                       memory_order_relaxed);
  for (u32 i = 0; i < wo->n; i++) {
    run->task_buf[base + dst + i] = wo->local[i];
  }
  wo->n = 0;
}

static inline void worker_push1(AotRun *run, WorkerOut *wo,
                                u32 base, AotTask t) {
  wo->local[wo->n++] = t;
  if (wo->n >= AOT_WORKER_FLUSH_AT) worker_flush(run, wo, base);
}

static inline void worker_push2(AotRun *run, WorkerOut *wo,
                                u32 base, AotTask t0, AotTask t1) {
  wo->local[wo->n + 0] = t0;
  wo->local[wo->n + 1] = t1;
  wo->n += 2;
  if (wo->n >= AOT_WORKER_FLUSH_AT) worker_flush(run, wo, base);
}

// Maximum tasks one grab claims at once.  Combined with the
// adaptive shrink below (batch = min(MAX, cur_cnt / (N * 4))) this
// reduces work_idx atomic frequency by up to 32x at the cost of a
// fairer-than-strict-FIFO distribution.  Workers that finish their
// batch quickly come back for another, so load balance stays good.
#define AOT_GRAB_BATCH_MAX  32u

static void aot_worker_phase(AotRun *run, u32 cur_cnt, u32 inline_mode,
                             WorkerOut *wo) {
  // Choose a batch size that gives each worker at least 4 batches'
  // worth of work over the round.  At cur_cnt < N*4 this collapses
  // to batch=1 (no batching, equal distribution); at cur_cnt > N*128
  // it caps at AOT_GRAB_BATCH_MAX.
  u32 batch = cur_cnt / (run->n_threads * 4u);
  if (batch < 1u) batch = 1u;
  if (batch > AOT_GRAB_BATCH_MAX) batch = AOT_GRAB_BATCH_MAX;

  while (1) {
    u32 base = atomic_fetch_add_explicit(&run->work_idx, batch,
                                          memory_order_relaxed);
    if (base >= cur_cnt) break;
    u32 end = base + batch;
    if (end > cur_cnt) end = cur_cnt;

  for (u32 i = base; i < end; i++) {
    AotTask t = run->task_buf[i];

    if (!inline_mode) {
      // SEED: dispatch once, push children, repeat outer loop.
      AotResult r = run->program->dispatch(run->program, &t);
      if (r.tag == AOT_R_SPLIT) {
        worker_push2(run, wo, cur_cnt, r.t0, r.t1);
      } else if (r.tag == AOT_R_CALL) {
        worker_push1(run, wo, cur_cnt, r.t0);
      } else {
        // aot_resolve writes 0/1/2 tasks into wo->local using its
        // local n counter.  Flush if the buffer is near-full so the
        // resolve never overruns AOT_WORKER_LOCAL_CAP.
        if (wo->n + 2 >= AOT_WORKER_LOCAL_CAP) worker_flush(run, wo, cur_cnt);
        aot_resolve(run->program, t.ret, r.val,
                    wo->local, &wo->n);
        if (wo->n >= AOT_WORKER_FLUSH_AT) worker_flush(run, wo, cur_cnt);
      }
      continue;
    }

    // WORK: per-task LIFO descent.  Each grabbed task runs its
    // entire subtree to completion using a worker-local C-stack --
    // ZERO atomic pushes for the duration (cont firings still use
    // atomic_write_slot for the cross-worker last-writer race, but
    // those happen at most once per cont, not once per fire).
    AotTask lstk[AOT_LOCAL_STACK_CAP];
    u32 lsp = 0;
    lstk[lsp++] = t;
    while (lsp > 0) {
      AotTask cur = lstk[--lsp];
      AotResult r = run->program->dispatch(run->program, &cur);
      if (r.tag == AOT_R_SPLIT) {
        // LIFO: push t1 then t0 so t0 dispatches first; matches
        // serial path's order so SUC^D-style descents stay
        // depth-first instead of fanning siblings.
        lstk[lsp++] = r.t1;
        lstk[lsp++] = r.t0;
        continue;
      }
      if (r.tag == AOT_R_CALL) {
        lstk[lsp++] = r.t0;
        continue;
      }
      // AOT_R_VALUE: aot_resolve writes any newly-fired children
      // (from cont fires whose siblings were already resolved by
      // OTHER workers earlier in the run) into the local stack.
      // Resolve writes at most 2 tasks per call, so the stack
      // overflow check below is conservative.
      u32 resolve_n = 0;
      aot_resolve(run->program, cur.ret, r.val,
                  lstk + lsp, &resolve_n);
      lsp += resolve_n;
    }
    }  // end for (u32 i = base; i < end; i++)
  }  // end while (1) outer batch loop

  // End-of-phase: drain residual SEED pushes so the outer barrier
  // sees a consistent task_cnt_new before tid=0 compacts.  WORK
  // doesn't touch wo at all so this only matters during SEED.
  worker_flush(run, wo, cur_cnt);
}

static void *aot_worker_main(void *arg_) {
  AotWorkerArg *arg = (AotWorkerArg *)arg_;
  AotRun *run = arg->run;
  u32 tid = arg->tid;

  // Per-worker output staging.  Stack-allocated so it lives on the
  // worker's own thread stack -- no contention, naturally NUMA-local
  // to the running core.  See WorkerOut comment for sizing rationale.
  WorkerOut wo;
  wo.n = 0;

  while (1) {
    aot_barrier_wait(&run->barrier);
    if (atomic_load_explicit(&run->program->done,
                             memory_order_acquire)) break;
    if (atomic_load_explicit(&run->task_cnt,
                             memory_order_acquire) == 0) break;

    // SEED: split until enough parallelism for N workers.
    u32 seed_target = run->n_threads * 2;
    for (u32 iter = 0; iter < AOT_SEED_ROUNDS; iter++) {
      u32 cur = atomic_load_explicit(&run->task_cnt, memory_order_acquire);
      if (cur == 0 || cur >= seed_target) break;

      if (tid == 0) {
        atomic_store_explicit(&run->task_cnt_new, 0, memory_order_relaxed);
        atomic_store_explicit(&run->work_idx, 0, memory_order_relaxed);
      }
      aot_barrier_wait(&run->barrier);

      aot_worker_phase(run, cur, /*inline_mode=*/0, &wo);

      aot_barrier_wait(&run->barrier);
      if (tid == 0) {
        u32 nc = atomic_load_explicit(&run->task_cnt_new,
                                       memory_order_relaxed);
        for (u32 i = 0; i < nc; i++) {
          run->task_buf[i] = run->task_buf[cur + i];
        }
        atomic_store_explicit(&run->task_cnt, nc, memory_order_release);
      }
      aot_barrier_wait(&run->barrier);
    }

    // WORK: same shape as one SEED iteration but without the
    // seed-target check.  Once we drain this round's tasks the outer
    // loop barrier_waits and either sees more tasks (next round) or
    // done==1 / task_cnt==0 (exit).
    u32 tc = atomic_load_explicit(&run->task_cnt, memory_order_acquire);
    if (tid == 0) {
      atomic_store_explicit(&run->task_cnt_new, 0, memory_order_relaxed);
      atomic_store_explicit(&run->work_idx, 0, memory_order_relaxed);
    }
    aot_barrier_wait(&run->barrier);

    aot_worker_phase(run, tc, /*inline_mode=*/1, &wo);

    aot_barrier_wait(&run->barrier);
    if (tid == 0) {
      u32 nc = atomic_load_explicit(&run->task_cnt_new,
                                     memory_order_relaxed);
      for (u32 i = 0; i < nc; i++) {
        run->task_buf[i] = run->task_buf[tc + i];
      }
      atomic_store_explicit(&run->task_cnt, nc, memory_order_release);
    }
    aot_barrier_wait(&run->barrier);
  }
  return NULL;
}

// === Public entry points =============================================

fn Term aot_run_parallel(AotProgram *program, AotTask root_task,
                          u32 n_threads) {
  if (n_threads <= 1) return aot_run_serial(program, root_task);
  if (n_threads > AOT_MAX_THREADS) n_threads = AOT_MAX_THREADS;

  // Calling thread acts as worker 0; reset its tl chunk so we don't
  // bump into a stale chunk from a previous run.  Spawned workers
  // (1..n_threads-1) get zero-initialised __thread vars per C
  // standard, so they're naturally fresh.
  aot_heap_tl_reset();

  AotRun run;
  run.program  = program;
  run.task_buf = (AotTask *)calloc(AOT_PARALLEL_BUF_CAP, sizeof(AotTask));
  if (!run.task_buf) return 0;

  run.task_buf[0] = root_task;
  atomic_store_explicit(&run.task_cnt,     1, memory_order_relaxed);
  atomic_store_explicit(&run.task_cnt_new, 0, memory_order_relaxed);
  atomic_store_explicit(&run.work_idx,     0, memory_order_relaxed);
  atomic_store_explicit(&program->done,    0, memory_order_relaxed);
  run.n_threads = n_threads;
  aot_barrier_init(&run.barrier, n_threads);

  pthread_t      threads[AOT_MAX_THREADS];
  AotWorkerArg   args[AOT_MAX_THREADS];
  for (u32 i = 0; i < n_threads; i++) {
    args[i].run = &run;
    args[i].tid = i;
  }
  for (u32 i = 1; i < n_threads; i++) {
    pthread_create(&threads[i], NULL, aot_worker_main, &args[i]);
  }
  aot_worker_main(&args[0]);
  for (u32 i = 1; i < n_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  aot_barrier_destroy(&run.barrier);
  free(run.task_buf);
  return program->result;
}

// Convenience wrapper: read AOT_THREADS env var, default to 1.
fn Term aot_run(AotProgram *program, AotTask root_task) {
  const char *env = getenv("AOT_THREADS");
  u32 n = (env && env[0]) ? (u32)atoi(env) : 1;
  if (n < 1) n = 1;
  return aot_run_parallel(program, root_task, n);
}

#endif  // THVM_AOT_WORKER_INCLUDED
