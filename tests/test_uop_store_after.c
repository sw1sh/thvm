// test_uop_store_after.c - Phase D'2: UOP_STORE + UOP_AFTER round-trip.
//
// Validates the new store opcode (symmetric counterpart to UOP_INDEX_E)
// and the AFTER ordering annotation.  Per the TileLang correspondence:
// T.copy = STORE + AFTER, T.async_copy = STORE + AFTER + Linear bit.
// No consumer yet -- the renderer rewrite (F0) is the first reader.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  // Build a synthetic buffer + address tree to feed to STORE.
  u32 dims[1] = { 32 };
  Term buf  = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims);
  Term r    = uop_range(0, KAX_LOOP, 32);
  Term val  = uop_const(DT_FP32, 0x40490FDBu); // ~3.14159

  TEST_BEGIN("uop-store/heap-layout");
  Term st = uop_store(buf, r, val);
  CHECK(st != 0);
  CHECK_EQ(term_tag(st), TAG_UOP);
  CHECK_EQ(term_ext(st), UOP_STORE);
  CHECK_EQ(uop_store_buf  (st), buf);
  CHECK_EQ(uop_store_addr (st), r);
  CHECK_EQ(uop_store_value(st), val);

  TEST_BEGIN("uop-store/hash-cons-shares");
  // Same (buf, addr, value) -> same Term.
  Term st2 = uop_store(buf, r, val);
  CHECK_EQ(st, st2);

  TEST_BEGIN("uop-store/different-buffer-distinct");
  u32 dims_b[1] = { 64 };
  Term buf_b = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_b);
  Term st_b  = uop_store(buf_b, r, val);
  CHECK(st_b != st);
  CHECK_EQ(uop_store_buf(st_b), buf_b);

  TEST_BEGIN("uop-store/different-addr-distinct");
  Term r1 = uop_range(1, 0, 32);
  Term st_r1 = uop_store(buf, r1, val);
  CHECK(st_r1 != st);

  TEST_BEGIN("uop-store/different-value-distinct");
  Term v2 = uop_const(DT_FP32, 0x3F800000u); // 1.0f
  Term st_v2 = uop_store(buf, r, v2);
  CHECK(st_v2 != st);

  TEST_BEGIN("uop-store/non-store-term-rejects");
  CHECK_EQ(uop_store_buf  (val), 0u);
  CHECK_EQ(uop_store_addr (val), 0u);
  CHECK_EQ(uop_store_value(val), 0u);

  TEST_BEGIN("uop-after/heap-layout");
  Term st_a = uop_store(buf, r, val);
  Term st_b2 = uop_store(buf, r1, val);
  Term ord = uop_after(st_b2, st_a); // st_b2 happens after st_a
  CHECK(ord != 0);
  CHECK_EQ(term_tag(ord), TAG_UOP);
  CHECK_EQ(term_ext(ord), UOP_AFTER);
  CHECK_EQ(uop_after_node      (ord), st_b2);
  CHECK_EQ(uop_after_after_node(ord), st_a);

  TEST_BEGIN("uop-after/hash-cons");
  Term ord2 = uop_after(st_b2, st_a);
  CHECK_EQ(ord, ord2);

  TEST_BEGIN("uop-after/asymmetric");
  // AFTER(a, b) != AFTER(b, a) -- ordering matters.
  Term reverse = uop_after(st_a, st_b2);
  CHECK(reverse != ord);
  CHECK_EQ(uop_after_node      (reverse), st_a);
  CHECK_EQ(uop_after_after_node(reverse), st_b2);

  TEST_BEGIN("uop-after/non-after-term-rejects");
  CHECK_EQ(uop_after_node      (val), 0u);
  CHECK_EQ(uop_after_after_node(val), 0u);

  TEST_BEGIN("uop-after/cross-scope-barrier-shape");
  // The canonical reduce-broadcast preamble: write into LOCAL buffer,
  // AFTER between LOCAL store and the GLOBAL read it gates.  Backend
  // emits threadgroup_barrier when AFTER crosses LOCAL <-> GLOBAL.
  u32 lc_dims[1] = { 32 };
  Term lc      = uop_buffer(UOP_SCOPE_LOCAL,  DT_FP32, 1, lc_dims);
  Term gl_dims[1] = { 32 };
  Term gl      = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, gl_dims);
  Term lc_st   = uop_store(lc, r, val);
  Term gl_read = uop_index_e(gl, r);
  Term gl_st   = uop_store(lc, r, gl_read); // hypothetical
  Term barrier = uop_after(gl_st, lc_st);
  CHECK_EQ(uop_buffer_scope(uop_store_buf(lc_st)), UOP_SCOPE_LOCAL);
  CHECK_EQ(uop_buffer_scope(uop_store_buf(gl_st)), UOP_SCOPE_LOCAL);
  CHECK_EQ(uop_after_node      (barrier), gl_st);
  CHECK_EQ(uop_after_after_node(barrier), lc_st);

  thvm_free();
  TEST_REPORT();
}
