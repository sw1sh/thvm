// util/wspq.c -- key-priority work-stealing queue, built on WsDeque.
//
// Per-worker bank of WSPQ_BRACKETS deques keyed by reduction-priority
// bucket; lower-key buckets pop first.  A 64-bit non-empty mask per
// worker turns "find the lowest non-empty key" into a __builtin_ctzll.
// Steals can be restricted to victim buckets shallower than the
// stealer's current min so we don't drag deeper work back to a worker
// that already has shallow work.
//
// Ported from TinyHVM/HVM4/clang/data/wspq.c.  Same shape; only
// difference is the macro names and the use of fn (= static inline).
//
// Tasks are u64 values.  Caller decides the encoding -- for the WNF
// driver the task is the redex Term itself (a u64 packed Term).

#ifndef WSPQ_BRACKETS
#define WSPQ_BRACKETS 64u
#endif

#ifndef WSPQ_KEY_SHIFT
#define WSPQ_KEY_SHIFT 0u
#endif

#ifndef WSPQ_CAP_POW2
#define WSPQ_CAP_POW2 22u
#endif

#ifndef WSPQ_STEAL_ATTEMPTS
#define WSPQ_STEAL_ATTEMPTS 2u
#endif

#ifndef WSPQ_DEADLOCK_CHECK_PERIOD
#define WSPQ_DEADLOCK_CHECK_PERIOD 256u
#endif

typedef struct __attribute__((aligned(256))) {
  _Alignas(CACHE_L1) CachePaddedAtomic nonempty;
  _Alignas(CACHE_L1) WsDeque q[WSPQ_BRACKETS];
} WspqBank;

typedef struct {
  WspqBank bank[MAX_THREADS];
  u32 n;
} Wspq;

fn u32 wspq_lsb64(u64 m) { return (u32)__builtin_ctzll(m); }

fn void wspq_mask_set(Wspq *ws, u32 tid, u32 b) {
  atomic_fetch_or_explicit(&ws->bank[tid].nonempty.v,
                           (1ull << b), memory_order_relaxed);
}

fn void wspq_mask_clear_owner(Wspq *ws, u32 tid, u32 b) {
  atomic_fetch_and_explicit(&ws->bank[tid].nonempty.v,
                            ~(1ull << b), memory_order_relaxed);
}

fn u8 wspq_key_bucket(u32 key) {
  u32 bucket = key >> WSPQ_KEY_SHIFT;
  if (bucket >= WSPQ_BRACKETS) return (u8)(WSPQ_BRACKETS - 1u);
  return (u8)bucket;
}

fn _Bool wspq_init(Wspq *ws, u32 nthreads) {
  ws->n = nthreads;
  for (u32 t = 0; t < nthreads; ++t) {
    atomic_store_explicit(&ws->bank[t].nonempty.v, 0ull,
                          memory_order_relaxed);
    for (u32 b = 0; b < WSPQ_BRACKETS; ++b) {
      if (!wsq_init(&ws->bank[t].q[b], WSPQ_CAP_POW2)) {
        // Roll back any allocations we already did so init is atomic
        // from the caller's POV.
        for (u32 t2 = 0; t2 <= t; ++t2) {
          u32 bmax = (t2 == t) ? b : WSPQ_BRACKETS;
          for (u32 b2 = 0; b2 < bmax; ++b2) {
            wsq_free(&ws->bank[t2].q[b2]);
          }
        }
        return 0;
      }
    }
  }
  return 1;
}

fn void wspq_free(Wspq *ws) {
  for (u32 t = 0; t < ws->n; ++t) {
    for (u32 b = 0; b < WSPQ_BRACKETS; ++b) {
      wsq_free(&ws->bank[t].q[b]);
    }
  }
}

// True iff bucket b is full across every worker -- the only condition
// under which wspq_push's spin loop cannot make progress.
fn _Bool wspq_bucket_full_all(Wspq *ws, u8 b) {
  for (u32 t = 0; t < ws->n; ++t) {
    WsDeque *q = &ws->bank[t].q[b];
    size_t bot = atomic_load_explicit(&q->bot.v, memory_order_relaxed);
    size_t top = atomic_load_explicit(&q->top.v, memory_order_relaxed);
    if (bot - top < q->cap) return 0;
  }
  return 1;
}

fn void wspq_push(Wspq *ws, u32 tid, u8 key, u64 task) {
  if (task == 0) return;
  u8 bucket = wspq_key_bucket(key);
  WsDeque *q = &ws->bank[tid].q[bucket];
  u32 spins = 1;
  while (!wsq_push(q, task)) {
    if ((spins % WSPQ_DEADLOCK_CHECK_PERIOD) == 0) {
      if (wspq_bucket_full_all(ws, bucket)) {
        fprintf(stderr,
                "wspq deadlock: bucket %u full across all %u workers\n",
                bucket, ws->n);
        exit(1);
      }
    }
    spins++;
    cpu_relax();
  }
  wspq_mask_set(ws, tid, bucket);
}

// Pop the best-key local task; returns 0 if none are available.  In
// single-threaded mode (n == 1) we steal from the top so the order is
// FIFO inside a bucket -- matches HVM4's deterministic T=1 ordering.
fn _Bool wspq_pop(Wspq *ws, u32 tid, u8 *key, u64 *task) {
  u64 m = atomic_load_explicit(&ws->bank[tid].nonempty.v,
                               memory_order_relaxed);
  _Bool fifo = (ws->n == 1);
  while (m) {
    u32 b = wspq_lsb64(m);
    u64 x = 0;
    WsDeque *q = &ws->bank[tid].q[b];
    if (fifo ? wsq_steal(q, &x) : wsq_pop(q, &x)) {
      *key  = (u8)(b << WSPQ_KEY_SHIFT);
      *task = x;
      return 1;
    }
    // Bucket drained: clear the bit and look at the next one.
    wspq_mask_clear_owner(ws, tid, b);
    m &= (m - 1ull);
  }
  return 0;
}

// Steal up to max_batch tasks from another worker, favouring shallow
// keys.  When restrict_deeper is true, only steal from buckets
// strictly shallower than my current shallowest -- prevents a thief
// already sitting on shallow work from importing deeper work.
fn u32 wspq_steal_some(Wspq *ws, u32 me, u32 max_batch,
                       _Bool restrict_deeper, u32 *cursor) {
  u32 n = ws->n;
  if (n <= 1) return 0;

  u32 b_limit = WSPQ_BRACKETS;
  if (restrict_deeper) {
    u64 my_mask = atomic_load_explicit(&ws->bank[me].nonempty.v,
                                       memory_order_relaxed);
    if (my_mask != 0ull) b_limit = wspq_lsb64(my_mask);
  }
  u64 allowed_mask = ~0ull;
  if (b_limit < WSPQ_BRACKETS) allowed_mask = (1ull << b_limit) - 1ull;

  u32 start = *cursor;
  for (u32 k = 0; k < WSPQ_STEAL_ATTEMPTS; ++k) {
    u32 v = (start + k) % n;
    if (v == me) continue;
    u64 nm = atomic_load_explicit(&ws->bank[v].nonempty.v,
                                  memory_order_relaxed);
    nm &= allowed_mask;
    if (nm == 0ull) continue;
    u32 b = wspq_lsb64(nm);
    u32 got = 0;
    for (; got < max_batch; ++got) {
      u64 task;
      if (!wsq_steal(&ws->bank[v].q[b], &task)) break;
      wspq_push(ws, me, (u8)(b << WSPQ_KEY_SHIFT), task);
    }
    if (got == 0) continue;
    *cursor = v + 1;
    return got;
  }
  *cursor = start + WSPQ_STEAL_ATTEMPTS;
  return 0;
}

fn _Bool wspq_can_steal(Wspq *ws, u32 me) {
  u32 n = ws->n;
  for (u32 k = 1; k < n; k++) {
    u32 v = (me + k) % n;
    if (atomic_load_explicit(&ws->bank[v].nonempty.v,
                             memory_order_relaxed)) return 1;
  }
  return 0;
}
