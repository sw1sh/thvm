// util/wsq.c -- Chase-Lev work-stealing deque for u64 tasks.
//
// Foundation for the parallel WNF / CNF redex bag.  Each worker owns
// one WsDeque it pushes/pops at the bottom; thieves steal from the
// top.  Owner ops are wait-free, steals are lock-free and may fail
// under contention (the standard Chase-Lev / Le-Pop-Be guarantees).
//
// Ported from TinyHVM/HVM4/clang/data/wsq.c.  Single change vs HVM4:
// uses thvm's `fn` (= static inline) macro and lives under src/util/
// instead of src/data/.
//
// Capacity is fixed at init; wsq_push returns 0 when the bucket
// fills up (the wspq layer above handles the spin / spill).
// Counters are monotonic u64; wrap-around is unreachable in practice.

typedef struct __attribute__((aligned(CACHE_L1))) {
  _Alignas(CACHE_L1) CachePaddedAtomic top;
  _Alignas(CACHE_L1) CachePaddedAtomic bot;
  _Alignas(CACHE_L1) u64 *buf;
  size_t mask;
  size_t cap;
} WsDeque;

fn void *wsq_aligned_alloc(size_t alignment, size_t nbytes) {
  void *ptr = NULL;
  size_t size = ((nbytes + alignment - 1) / alignment) * alignment;
  int err = posix_memalign(&ptr, alignment, size);
  if (err) return NULL;
  return ptr;
}

fn int wsq_init(WsDeque *q, u32 capacity_pow2) {
  size_t cap = (size_t)1 << capacity_pow2;
  q->buf = (u64 *)wsq_aligned_alloc(CACHE_L1, cap * sizeof(u64));
  if (!q->buf) return 0;
  q->cap  = cap;
  q->mask = cap - 1;
  atomic_store_explicit(&q->top.v, 0, memory_order_relaxed);
  atomic_store_explicit(&q->bot.v, 0, memory_order_relaxed);
  return 1;
}

fn void wsq_free(WsDeque *q) {
  if (q && q->buf) {
    free(q->buf);
    q->buf = NULL;
  }
}

// Owner push to the bottom; returns 1 on success, 0 if full.
fn int wsq_push(WsDeque *q, u64 x) {
  u64 b = atomic_load_explicit(&q->bot.v, memory_order_relaxed);
  u64 t = atomic_load_explicit(&q->top.v, memory_order_acquire);
  if (b - t >= q->cap) return 0;
  __builtin_prefetch(&q->buf[b & q->mask], 1, 1);
  q->buf[b & q->mask] = x;
  atomic_store_explicit(&q->bot.v, b + 1, memory_order_release);
  return 1;
}

// Owner pop from the bottom; returns 1 on success, 0 if empty or
// the last element was lost to a concurrent thief.
fn int wsq_pop(WsDeque *q, u64 *out) {
  u64 b = atomic_load_explicit(&q->bot.v, memory_order_relaxed);
  if (b == 0) return 0;
  u64 b1 = b - 1;
  __builtin_prefetch(&q->buf[b1 & q->mask], 0, 1);
  atomic_store_explicit(&q->bot.v, b1, memory_order_release);
  atomic_thread_fence(memory_order_seq_cst);

  u64 t = atomic_load_explicit(&q->top.v, memory_order_acquire);
  if (t <= b1) {
    u64 x = q->buf[b1 & q->mask];
    if (t == b1) {
      // Last element race with thieves: CAS to claim.
      u64 expected = t;
      _Bool ok = atomic_compare_exchange_strong_explicit(
        &q->top.v, &expected, t + 1,
        memory_order_acq_rel, memory_order_acquire);
      if (!ok) {
        atomic_store_explicit(&q->bot.v, t + 1, memory_order_release);
        return 0;
      }
      atomic_store_explicit(&q->bot.v, t + 1, memory_order_release);
    }
    *out = x;
    return 1;
  } else {
    atomic_store_explicit(&q->bot.v, t, memory_order_release);
    return 0;
  }
}

// Thief steal from the top; returns 1 on success, 0 if empty or lost.
fn int wsq_steal(WsDeque *q, u64 *out) {
  u64 t = atomic_load_explicit(&q->top.v, memory_order_acquire);
  u64 b = atomic_load_explicit(&q->bot.v, memory_order_acquire);
  if (t >= b) return 0;
  __builtin_prefetch(&q->buf[t & q->mask], 0, 1);
  u64 x = q->buf[t & q->mask];
  u64 expected = t;
  _Bool ok = atomic_compare_exchange_strong_explicit(
    &q->top.v, &expected, t + 1,
    memory_order_acq_rel, memory_order_acquire);
  if (ok) {
    *out = x;
    return 1;
  }
  return 0;
}

fn _Bool wsq_can_steal(WsDeque *q) {
  u64 t = atomic_load_explicit(&q->top.v, memory_order_acquire);
  u64 b = atomic_load_explicit(&q->bot.v, memory_order_acquire);
  return t < b;
}
