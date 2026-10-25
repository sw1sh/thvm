// src/aot/worker.c -- AOT (ahead-of-time) task runners.  aot_run_serial drains
// one LIFO (last-in first-out) stack; aot_run_parallel runs Bend2's SEED phase
// (one dispatch per task until there are two per worker), then WORK (each
// worker drains its tasks on a private stack),
// external/TinyHVM/resources/gists/par_tree_sum_bend2_compiled.c:520-597.

#ifndef THVM_AOT_WORKER_INCLUDED
#define THVM_AOT_WORKER_INCLUDED

#include <pthread.h>
#include <unistd.h>

#define AOT_MAX_THREADS    64
// Global queue capacity in tasks; only SEED rounds push into it.
#define AOT_PARALLEL_BUF_CAP (4u * 1024u * 1024u)
// Each SPLIT level parks one sibling on the serial stack, so this bounds the
// recursion depth (not checked).
#define AOT_SERIAL_STACK_CAP 256
#define AOT_SEED_ROUNDS    64    // as in Bend2; ends SEED if CALLs keep it flat

// CTR builders with term_new_ctr's layout (src/term/new_ctr.c) that allocate
// through aot_heap_alloc, sparing heap_alloc's shared heap_next atomic.
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

// Arities without a fixed helper (emit.c uses it above 4).
static inline Term aot_make_ctrn(u32 label, u32 n, const Term *children) {
  u64 loc = aot_heap_alloc(1 + n);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, n));
  for (u32 i = 0; i < n; i++) heap_set(loc + 1 + i, children[i]);
  return term_new(0, TAG_CTR, label, loc);
}

// _Atomic fields take atomic_*_explicit: clang rejects __atomic_* on them.
typedef struct {
  pthread_mutex_t mu;
  pthread_cond_t  cv;
  _Atomic u32     cnt;     // # workers arrived in current generation
  _Atomic u32     gen;     // bumped each release; workers wait on it
  u32             total;
  _Bool           spin_only;  // never sleep on the condvar
} AotBarrier;

static void aot_barrier_init(AotBarrier *b, u32 n) {
  pthread_mutex_init(&b->mu, NULL);
  pthread_cond_init(&b->cv, NULL);
  atomic_store_explicit(&b->cnt, 0, memory_order_relaxed);
  atomic_store_explicit(&b->gen, 0, memory_order_relaxed);
  b->total = n;
  // Spin only when each thread has a core: a sleep would dominate rounds of
  // microseconds (par_tree_sum_bend2_compiled.c:619-620).
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
  // Oversubscribed: spin 128 times, as Bend2 does
  // (par_tree_sum_bend2_compiled.c:178), then sleep.
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

fn Term aot_run_serial(AotProgram *program, AotTask root_task) {
  AotTask stack[AOT_SERIAL_STACK_CAP];
  u32     sp = 0;
  u32     out_n;

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

// WORK's per-task stack; like AOT_SERIAL_STACK_CAP it bounds recursion depth.
#define AOT_LOCAL_STACK_CAP   256u

// SEED pushes are staged per worker and copied to the global queue with one
// fetch_add on task_cnt_new per flush.  LOCAL_CAP leaves room past FLUSH_AT for
// resolve's up to 2 writes; WorkerOut's 40 KB fits a 512 KB macOS thread stack.
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

#define AOT_GRAB_BATCH_MAX  32u

// One round over task_buf[0, cur_cnt).  SEED (inline_mode 0) dispatches each
// task once and queues its children; WORK (1) runs each task's whole subtree
// on a local stack and queues nothing.
static void aot_worker_phase(AotRun *run, u32 cur_cnt, u32 inline_mode,
                             WorkerOut *wo) {
  // Claim tasks in batches to cut work_idx atomics, sized so each worker
  // still gets about four batches per round.
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
      AotResult r = run->program->dispatch(run->program, &t);
      if (r.tag == AOT_R_SPLIT) {
        worker_push2(run, wo, cur_cnt, r.t0, r.t1);
      } else if (r.tag == AOT_R_CALL) {
        worker_push1(run, wo, cur_cnt, r.t0);
      } else {
        // Make room for the up to 2 tasks resolve writes into wo->local.
        if (wo->n + 2 >= AOT_WORKER_LOCAL_CAP) worker_flush(run, wo, cur_cnt);
        aot_resolve(run->program, t.ret, r.val,
                    wo->local, &wo->n);
        if (wo->n >= AOT_WORKER_FLUSH_AT) worker_flush(run, wo, cur_cnt);
      }
      continue;
    }

    AotTask lstk[AOT_LOCAL_STACK_CAP];
    u32 lsp = 0;
    lstk[lsp++] = t;
    while (lsp > 0) {
      AotTask cur = lstk[--lsp];
      AotResult r = run->program->dispatch(run->program, &cur);
      if (r.tag == AOT_R_SPLIT) {
        // Push t1 first so t0 runs first, as in aot_run_serial.
        lstk[lsp++] = r.t1;
        lstk[lsp++] = r.t0;
        continue;
      }
      if (r.tag == AOT_R_CALL) {
        lstk[lsp++] = r.t0;
        continue;
      }
      // Children of a cont this worker fires land on its local stack.
      u32 resolve_n = 0;
      aot_resolve(run->program, cur.ret, r.val,
                  lstk + lsp, &resolve_n);
      lsp += resolve_n;
    }
    }  // end for (u32 i = base; i < end; i++)
  }  // end while (1) outer batch loop

  // Flush SEED's remaining pushes before the barrier, so tid 0 compacts them.
  worker_flush(run, wo, cur_cnt);
}

static void *aot_worker_main(void *arg_) {
  AotWorkerArg *arg = (AotWorkerArg *)arg_;
  AotRun *run = arg->run;
  u32 tid = arg->tid;

  // AOT bodies call wnf / cnf, which need a CURRENT_WNF_STATE, and a spawned
  // thread's is NULL: each worker gets its own spine, as in src/wnf/pool.c
  // wnf_pool_persistent_main; tid 0, the caller, gets its own back at exit.
  WnfThreadState saved_state = { NULL, 0 };
  WnfThreadState *prev_state_ptr = CURRENT_WNF_STATE;
  Term *worker_stack = (Term *)malloc(WNF_CAP * sizeof(Term));
  if (worker_stack != NULL) {
    saved_state.stack = worker_stack;
    saved_state.s_pos = 0;
    CURRENT_WNF_STATE = &saved_state;
  }

  WorkerOut wo;
  wo.n = 0;

  while (1) {
    aot_barrier_wait(&run->barrier);
    if (atomic_load_explicit(&run->program->done,
                             memory_order_acquire)) break;
    if (atomic_load_explicit(&run->task_cnt,
                             memory_order_acquire) == 0) break;

    // SEED until there are two tasks per worker.
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

    // WORK queues nothing, so the outer loop ends after this round.
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
  CURRENT_WNF_STATE = prev_state_ptr;
  if (worker_stack != NULL) free(worker_stack);
  return NULL;
}

fn Term aot_run_parallel(AotProgram *program, AotTask root_task,
                          u32 n_threads) {
  if (n_threads <= 1) return aot_run_serial(program, root_task);
  if (n_threads > AOT_MAX_THREADS) n_threads = AOT_MAX_THREADS;

  // The calling thread runs as worker 0.
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

fn Term aot_run(AotProgram *program, AotTask root_task) {
  const char *env = getenv("AOT_THREADS");
  u32 n = (env && env[0]) ? (u32)atoi(env) : 1;
  if (n < 1) n = 1;
  return aot_run_parallel(program, root_task, n);
}

#endif  // THVM_AOT_WORKER_INCLUDED
