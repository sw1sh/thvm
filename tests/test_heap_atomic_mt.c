// test_heap_atomic_mt.c -- pthread torture for the atomic heap
// primitives.  Validates that under real contention:
//
//   * heap_alloc_atomic hands out disjoint ranges (no two threads
//     get the same cell).
//   * heap_take_atomic is single-winner: across N concurrent take
//     attempts on a populated cell, exactly one returns the prior
//     value, the rest return 0.
//   * heap_cas commits exactly once when N threads race on the same
//     expected value.
//
// The pthread worker pool that consumes these primitives lands in a
// later slice; this test proves the primitives themselves are
// race-free so the pool only has to worry about scheduling.

#include "../src/thvm.c"
#include "test.h"
#include <pthread.h>

#define MT_NTHREADS    8
#define MT_ALLOCS_EACH 4096

// --- heap_alloc: each worker bumps its own count of cells, all
// ranges must be disjoint and contiguous (sum == total) -------------

typedef struct {
  u32 tid;
  u64 base[MT_ALLOCS_EACH];
} AllocArg;

static void *alloc_thread(void *arg) {
  AllocArg *a = (AllocArg *)arg;
  for (u32 i = 0; i < MT_ALLOCS_EACH; i++) {
    a->base[i] = heap_alloc(1);
  }
  return NULL;
}

static int test_alloc_atomic_mt(void) {
  TEST_BEGIN("heap_alloc: N threads get disjoint cells");
  thvm_init();
  u64 base0 = HEAP_NEXT;

  pthread_t threads[MT_NTHREADS];
  AllocArg  args[MT_NTHREADS];
  for (u32 t = 0; t < MT_NTHREADS; t++) {
    args[t].tid = t;
    pthread_create(&threads[t], NULL, alloc_thread, &args[t]);
  }
  for (u32 t = 0; t < MT_NTHREADS; t++) pthread_join(threads[t], NULL);

  // HEAP_NEXT advanced by exactly N * M cells.
  CHECK_EQ(HEAP_NEXT - base0,
           (u64)MT_NTHREADS * (u64)MT_ALLOCS_EACH);

  // Mark every claimed cell with its (tid+1) and verify uniqueness:
  // if two threads got the same cell, the writes would collide and
  // the count would be off.  Using a separate side-table avoids
  // touching HEAP cells we don't own (the bump pointer says we do,
  // but defensive).
  static u8 seen[(u64)MT_NTHREADS * MT_ALLOCS_EACH] = {0};
  u64 dups = 0;
  for (u32 t = 0; t < MT_NTHREADS; t++) {
    for (u32 i = 0; i < MT_ALLOCS_EACH; i++) {
      u64 idx = args[t].base[i] - base0;
      if (idx >= sizeof seen / sizeof seen[0]) { dups++; continue; }
      if (seen[idx]) dups++;
      seen[idx] = 1;
    }
  }
  CHECK_EQ(dups, 0u);

  thvm_free();
  return 0;
}

// --- heap_take_atomic: single-winner CAS-equivalent ----------------

typedef struct {
  u64       loc;
  Term      expected;
  _Atomic u32 winners;
} TakeArg;

static void *take_thread(void *arg) {
  TakeArg *a = (TakeArg *)arg;
  Term got = heap_take_atomic(a->loc);
  if (got == a->expected) atomic_fetch_add(&a->winners, 1);
  return NULL;
}

static int test_take_atomic_mt(void) {
  TEST_BEGIN("heap_take_atomic: exactly one winner per cell");
  thvm_init();
  // Run the experiment 200 times so any race is overwhelmingly likely
  // to surface.  Each round uses a fresh cell.
  for (u32 round = 0; round < 200; round++) {
    u64  loc = heap_alloc(1);
    Term v   = term_new(0, TAG_NUM, DT_INT32, round + 1);
    heap_set(loc, v);

    TakeArg a = { .loc = loc, .expected = v, .winners = 0 };
    pthread_t threads[MT_NTHREADS];
    for (u32 t = 0; t < MT_NTHREADS; t++) {
      pthread_create(&threads[t], NULL, take_thread, &a);
    }
    for (u32 t = 0; t < MT_NTHREADS; t++) pthread_join(threads[t], NULL);

    CHECK_EQ(atomic_load(&a.winners), 1u);
    CHECK_EQ(heap_read(loc), 0u);
  }
  thvm_free();
  return 0;
}

// --- heap_cas: single-committer race -------------------------------

typedef struct {
  u64       loc;
  Term      expected;
  Term      desired;
  _Atomic u32 committers;
} CasArg;

static void *cas_thread(void *arg) {
  CasArg *a = (CasArg *)arg;
  Term seen = a->expected;
  if (heap_cas(a->loc, &seen, a->desired)) {
    atomic_fetch_add(&a->committers, 1);
  }
  return NULL;
}

static int test_cas_mt(void) {
  TEST_BEGIN("heap_cas: exactly one commit per (loc, expected) race");
  thvm_init();
  for (u32 round = 0; round < 200; round++) {
    u64  loc = heap_alloc(1);
    Term old = term_new(0, TAG_NUM, DT_INT32, round + 100);
    Term new_ = term_new(0, TAG_NUM, DT_INT32, round + 200);
    heap_set(loc, old);

    CasArg a = { .loc = loc, .expected = old, .desired = new_, .committers = 0 };
    pthread_t threads[MT_NTHREADS];
    for (u32 t = 0; t < MT_NTHREADS; t++) {
      pthread_create(&threads[t], NULL, cas_thread, &a);
    }
    for (u32 t = 0; t < MT_NTHREADS; t++) pthread_join(threads[t], NULL);

    CHECK_EQ(atomic_load(&a.committers), 1u);
    CHECK_EQ(heap_read(loc), new_);
  }
  thvm_free();
  return 0;
}

// --- ITRS: atomic bumps survive concurrent contention --------------

#define MT_ITRS_BUMPS_EACH 100000

static void *itrs_bump_thread(void *arg) {
  (void)arg;
  for (u32 i = 0; i < MT_ITRS_BUMPS_EACH; i++) ITRS++;
  return NULL;
}

static int test_itrs_atomic_mt(void) {
  TEST_BEGIN("ITRS++ from N threads sums correctly (no torn bumps)");
  thvm_init();
  u64 base = ITRS;

  pthread_t threads[MT_NTHREADS];
  for (u32 t = 0; t < MT_NTHREADS; t++) {
    pthread_create(&threads[t], NULL, itrs_bump_thread, NULL);
  }
  for (u32 t = 0; t < MT_NTHREADS; t++) pthread_join(threads[t], NULL);

  CHECK_EQ(ITRS - base,
           (u64)MT_NTHREADS * (u64)MT_ITRS_BUMPS_EACH);

  thvm_free();
  return 0;
}

int main(void) {
  test_alloc_atomic_mt();
  test_take_atomic_mt();
  test_cas_mt();
  test_itrs_atomic_mt();
  TEST_REPORT();
}
