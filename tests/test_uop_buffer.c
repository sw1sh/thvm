// test_uop_buffer.c - Phase D'1: UOP_BUFFER scope/dtype/shape round-trip.
//
// Validates the new buffer leaf opcode that replaces today's
// implicit-via-UOP_LOAD buffer references.  Per the TileLang
// correspondence: GLOBAL = T.Tensor argument, LOCAL = T.alloc_shared,
// REG = T.alloc_fragment.  No consumer yet -- D'2 wires UOP_STORE/AFTER.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("uop-buffer/global-scope-round-trip");
  u32 dims2[2] = { 4, 8 };
  Term g = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims2);
  CHECK(g != 0);
  CHECK_EQ(term_tag(g), TAG_UOP);
  CHECK_EQ(term_ext(g), UOP_BUFFER);
  CHECK_EQ(uop_buffer_scope(g), UOP_SCOPE_GLOBAL);
  CHECK_EQ(uop_buffer_dtype(g), DT_FP32);
  CHECK_EQ(uop_buffer_ndim (g), 2u);
  CHECK_EQ(uop_buffer_dim  (g, 0), 4u);
  CHECK_EQ(uop_buffer_dim  (g, 1), 8u);
  CHECK_EQ(uop_buffer_dim  (g, 2), 0u); // out-of-range returns 0

  TEST_BEGIN("uop-buffer/local-scope-shared-memory");
  u32 dims1[1] = { 256 };
  Term l = uop_buffer(UOP_SCOPE_LOCAL, DT_FP32, 1, dims1);
  CHECK_EQ(uop_buffer_scope(l), UOP_SCOPE_LOCAL);
  CHECK_EQ(uop_buffer_ndim (l), 1u);
  CHECK_EQ(uop_buffer_dim  (l, 0), 256u);

  TEST_BEGIN("uop-buffer/reg-scope-fragment");
  u32 dims_var[1] = { 1 };
  Term r = uop_buffer(UOP_SCOPE_REG, DT_FP32, 1, dims_var);
  CHECK_EQ(uop_buffer_scope(r), UOP_SCOPE_REG);
  CHECK_EQ(uop_buffer_dim  (r, 0), 1u);

  TEST_BEGIN("uop-buffer/different-scopes-distinct-terms");
  // Same shape + dtype, different scopes -> distinct heap locs.
  Term g2 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_var);
  Term l2 = uop_buffer(UOP_SCOPE_LOCAL,  DT_FP32, 1, dims_var);
  Term r2 = uop_buffer(UOP_SCOPE_REG,    DT_FP32, 1, dims_var);
  CHECK(g2 != l2);
  CHECK(l2 != r2);
  CHECK(g2 != r2);

  TEST_BEGIN("uop-buffer/different-dtypes-distinct-terms");
  Term f32 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims_var);
  Term i32 = uop_buffer(UOP_SCOPE_GLOBAL, DT_INT32, 1, dims_var);
  CHECK(f32 != i32);
  CHECK_EQ(uop_buffer_dtype(f32), DT_FP32);
  CHECK_EQ(uop_buffer_dtype(i32), DT_INT32);

  TEST_BEGIN("uop-buffer/different-shapes-distinct-terms");
  u32 dims3[3] = { 2, 3, 4 };
  Term s2 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims2);
  Term s3 = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 3, dims3);
  CHECK(s2 != s3);
  CHECK_EQ(uop_buffer_ndim(s2), 2u);
  CHECK_EQ(uop_buffer_ndim(s3), 3u);

  TEST_BEGIN("uop-buffer/hash-cons-shares-heap-loc");
  // Identical (scope, dtype, ndim, dims) round-trips to the same Term.
  Term a = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims2);
  Term b = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims2);
  CHECK_EQ(a, b);

  TEST_BEGIN("uop-buffer/dim-permutation-distinct");
  // (4, 8) and (8, 4) are different shapes.
  u32 dims_swapped[2] = { 8, 4 };
  Term ab = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims2);
  Term ba = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims_swapped);
  CHECK(ab != ba);
  CHECK_EQ(uop_buffer_dim(ab, 0), 4u);
  CHECK_EQ(uop_buffer_dim(ba, 0), 8u);

  TEST_BEGIN("uop-buffer/non-buffer-term-rejects");
  // Accessors return 0 for non-UOP_BUFFER terms.
  Term c = uop_const(DT_FP32, 0x3F800000u); // 1.0f
  CHECK_EQ(uop_buffer_scope(c), 0u);
  CHECK_EQ(uop_buffer_dtype(c), 0u);
  CHECK_EQ(uop_buffer_ndim (c), 0u);
  CHECK_EQ(uop_buffer_dim  (c, 0), 0u);

  thvm_free();
  TEST_REPORT();
}
