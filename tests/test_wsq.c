// test_wsq.c -- Chase-Lev work-stealing deque sanity (single-thread).
// Threaded behaviour is exercised by test_wspq once the worker pool
// lands; here we just validate push/pop/steal semantics in isolation.

#include "../src/thvm.c"
#include "test.h"

static int test_init_free(void) {
  TEST_BEGIN("wsq_init / wsq_free");
  WsDeque q;
  CHECK(wsq_init(&q, 4) == 1);   // 16 slots
  CHECK(q.cap  == 16);
  CHECK(q.mask == 15);
  wsq_free(&q);
  CHECK(q.buf == NULL);          // free should null the buffer
  return 0;
}

static int test_push_pop_lifo(void) {
  TEST_BEGIN("owner push/pop -- LIFO order");
  WsDeque q;
  CHECK(wsq_init(&q, 4) == 1);

  for (u64 i = 1; i <= 5; i++) CHECK(wsq_push(&q, i) == 1);

  u64 out = 0;
  CHECK(wsq_pop(&q, &out) == 1); CHECK_EQ(out, 5);
  CHECK(wsq_pop(&q, &out) == 1); CHECK_EQ(out, 4);
  CHECK(wsq_pop(&q, &out) == 1); CHECK_EQ(out, 3);
  CHECK(wsq_pop(&q, &out) == 1); CHECK_EQ(out, 2);
  CHECK(wsq_pop(&q, &out) == 1); CHECK_EQ(out, 1);
  CHECK(wsq_pop(&q, &out) == 0);   // empty

  wsq_free(&q);
  return 0;
}

static int test_steal_fifo(void) {
  TEST_BEGIN("steal -- FIFO order from the top");
  WsDeque q;
  CHECK(wsq_init(&q, 4) == 1);

  for (u64 i = 1; i <= 5; i++) wsq_push(&q, i);
  CHECK(wsq_can_steal(&q) == 1);

  u64 out = 0;
  CHECK(wsq_steal(&q, &out) == 1); CHECK_EQ(out, 1);
  CHECK(wsq_steal(&q, &out) == 1); CHECK_EQ(out, 2);
  CHECK(wsq_steal(&q, &out) == 1); CHECK_EQ(out, 3);
  CHECK(wsq_steal(&q, &out) == 1); CHECK_EQ(out, 4);
  CHECK(wsq_steal(&q, &out) == 1); CHECK_EQ(out, 5);
  CHECK(wsq_steal(&q, &out) == 0);   // empty
  CHECK(wsq_can_steal(&q) == 0);

  wsq_free(&q);
  return 0;
}

static int test_full(void) {
  TEST_BEGIN("push returns 0 when full");
  WsDeque q;
  CHECK(wsq_init(&q, 2) == 1);   // 4 slots
  CHECK(wsq_push(&q, 1) == 1);
  CHECK(wsq_push(&q, 2) == 1);
  CHECK(wsq_push(&q, 3) == 1);
  CHECK(wsq_push(&q, 4) == 1);
  CHECK(wsq_push(&q, 5) == 0);   // overflow
  wsq_free(&q);
  return 0;
}

static int test_mixed_pop_steal(void) {
  TEST_BEGIN("interleaved pop + steal -- last element CAS");
  WsDeque q;
  CHECK(wsq_init(&q, 4) == 1);

  // Single element: pop wins the CAS, steal sees empty.
  wsq_push(&q, 42);
  u64 out = 0;
  CHECK(wsq_pop(&q, &out) == 1); CHECK_EQ(out, 42);
  CHECK(wsq_steal(&q, &out) == 0);

  // Single element: steal wins the CAS, pop sees empty.
  wsq_push(&q, 7);
  CHECK(wsq_steal(&q, &out) == 1); CHECK_EQ(out, 7);
  CHECK(wsq_pop(&q, &out) == 0);

  wsq_free(&q);
  return 0;
}

int main(void) {
  test_init_free();
  test_push_pop_lifo();
  test_steal_fifo();
  test_full();
  test_mixed_pop_steal();
  TEST_REPORT();
}
