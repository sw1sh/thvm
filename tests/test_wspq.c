// test_wspq.c -- bucket-priority work-stealing queue sanity.
//
// Validates wspq's lowest-key-first semantics on a single worker.
// Multi-worker steal behaviour will be exercised once the worker pool
// lands; here we set ws->n = 2 to flip wspq_pop into the LIFO mode
// (n > 1 uses wsq_pop, not wsq_steal) and check both code paths.

#include "../src/thvm.c"
#include "test.h"

static int test_init_free(void) {
  TEST_BEGIN("wspq_init / wspq_free");
  Wspq *ws = (Wspq *)calloc(1, sizeof(Wspq));
  CHECK(wspq_init(ws, 2) == 1);
  CHECK(ws->n == 2);
  wspq_free(ws);
  free(ws);
  return 0;
}

static int test_lowest_key_first_n1(void) {
  TEST_BEGIN("n=1 fifo: lowest-key bucket pops first");
  Wspq *ws = (Wspq *)calloc(1, sizeof(Wspq));
  wspq_init(ws, 1);

  // Push out-of-order keys; expect ascending pop order.
  wspq_push(ws, 0, /*key=*/5, 0xAA);
  wspq_push(ws, 0, /*key=*/2, 0xBB);
  wspq_push(ws, 0, /*key=*/9, 0xCC);
  wspq_push(ws, 0, /*key=*/2, 0xDD);

  u8 k; u64 t;
  CHECK(wspq_pop(ws, 0, &k, &t) == 1); CHECK_EQ(k, 2); CHECK_EQ(t, 0xBB);
  CHECK(wspq_pop(ws, 0, &k, &t) == 1); CHECK_EQ(k, 2); CHECK_EQ(t, 0xDD);
  CHECK(wspq_pop(ws, 0, &k, &t) == 1); CHECK_EQ(k, 5); CHECK_EQ(t, 0xAA);
  CHECK(wspq_pop(ws, 0, &k, &t) == 1); CHECK_EQ(k, 9); CHECK_EQ(t, 0xCC);
  CHECK(wspq_pop(ws, 0, &k, &t) == 0);   // empty

  wspq_free(ws);
  free(ws);
  return 0;
}

static int test_lifo_in_bucket_n2(void) {
  TEST_BEGIN("n>1: pop is LIFO inside a bucket");
  Wspq *ws = (Wspq *)calloc(1, sizeof(Wspq));
  wspq_init(ws, 2);

  wspq_push(ws, 0, 3, 0x11);
  wspq_push(ws, 0, 3, 0x22);
  wspq_push(ws, 0, 3, 0x33);

  u8 k; u64 t;
  CHECK(wspq_pop(ws, 0, &k, &t) == 1); CHECK_EQ(t, 0x33);
  CHECK(wspq_pop(ws, 0, &k, &t) == 1); CHECK_EQ(t, 0x22);
  CHECK(wspq_pop(ws, 0, &k, &t) == 1); CHECK_EQ(t, 0x11);
  CHECK(wspq_pop(ws, 0, &k, &t) == 0);

  wspq_free(ws);
  free(ws);
  return 0;
}

static int test_zero_drop(void) {
  TEST_BEGIN("push of task=0 is silently dropped");
  Wspq *ws = (Wspq *)calloc(1, sizeof(Wspq));
  wspq_init(ws, 1);
  wspq_push(ws, 0, 0, 0);          // dropped
  wspq_push(ws, 0, 0, 1);
  u8 k; u64 t;
  CHECK(wspq_pop(ws, 0, &k, &t) == 1); CHECK_EQ(t, 1);
  CHECK(wspq_pop(ws, 0, &k, &t) == 0);
  wspq_free(ws);
  free(ws);
  return 0;
}

static int test_can_steal_n2(void) {
  TEST_BEGIN("wspq_can_steal sees other worker's work");
  Wspq *ws = (Wspq *)calloc(1, sizeof(Wspq));
  wspq_init(ws, 2);

  CHECK(wspq_can_steal(ws, 0) == 0);
  wspq_push(ws, 1, 4, 0xFEED);
  CHECK(wspq_can_steal(ws, 0) == 1);
  CHECK(wspq_can_steal(ws, 1) == 0);   // can't steal from yourself

  wspq_free(ws);
  free(ws);
  return 0;
}

static int test_steal_some(void) {
  TEST_BEGIN("wspq_steal_some moves work between banks");
  Wspq *ws = (Wspq *)calloc(1, sizeof(Wspq));
  wspq_init(ws, 2);

  for (int i = 0; i < 4; i++) wspq_push(ws, 1, /*key=*/3, 100 + i);

  u32 cursor = 0;
  u32 got = wspq_steal_some(ws, /*me=*/0, /*max_batch=*/8,
                            /*restrict_deeper=*/0, &cursor);
  CHECK(got == 4);

  // The four tasks now live on worker 0's bucket-3.
  u8 k; u64 t;
  for (int i = 0; i < 4; i++) {
    CHECK(wspq_pop(ws, 0, &k, &t) == 1);
    CHECK_EQ(k, 3);
    // Order: thief steals from victim's top (FIFO), then pushes to its
    // own bucket-bottom; owner pop is LIFO -- net order is reversed.
    CHECK_EQ(t, 100u + (3 - i));
  }
  CHECK(wspq_pop(ws, 0, &k, &t) == 0);
  CHECK(wspq_pop(ws, 1, &k, &t) == 0);

  wspq_free(ws);
  free(ws);
  return 0;
}

int main(void) {
  test_init_free();
  test_lowest_key_first_n1();
  test_lifo_in_bucket_n2();
  test_zero_drop();
  test_can_steal_n2();
  test_steal_some();
  TEST_REPORT();
}
