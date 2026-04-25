// test_consumer_count.c - exercise kernel_compute_consumer_counts
// (sub-item a of the refcount-driven-free arc).
//
// Builds a 3-kernel diamond by hand via the materialize path:
//   K1 = ADD(a, b)        produces tensor t1
//   K2 = MUL(t1, c)       consumes t1 (K1)
//   K3 = MUL(t1, d)       consumes t1 (K1)
//
// After kernel_compute_consumer_counts:
//   K1.consumer_count == 2
//   K2.consumer_count == 0
//   K3.consumer_count == 0
//
// Also verifies a re-run zeroes + recomputes (idempotent).

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) s.dims[i] = dims[i];
  return tensor_alloc(CURRENT_BACKEND, s, DT_F32);
}

int main(void) {
  thvm_init();

  u32 dims[1] = {3};
  Term a = term_new(0, TAG_TEN, DT_F32, alloc_f32_tensor(dims, 1));
  Term b = term_new(0, TAG_TEN, DT_F32, alloc_f32_tensor(dims, 1));
  Term c = term_new(0, TAG_TEN, DT_F32, alloc_f32_tensor(dims, 1));
  Term d = term_new(0, TAG_TEN, DT_F32, alloc_f32_tensor(dims, 1));

  u32 kernels_before = KERNELS_NEXT;

  Term add_ab = uop_binary(UOP_ADD, a, b);
  Term k1_res = thvm_materialize(add_ab);
  u32 k1 = KERNELS_NEXT - 1;

  Term mul1 = uop_binary(UOP_MUL, k1_res, c);
  thvm_materialize(mul1);
  u32 k2 = KERNELS_NEXT - 1;

  Term mul2 = uop_binary(UOP_MUL, k1_res, d);
  thvm_materialize(mul2);
  u32 k3 = KERNELS_NEXT - 1;

  TEST_BEGIN("consumer-count/three-kernels-allocated");
  CHECK_EQ(KERNELS_NEXT, kernels_before + 3);
  CHECK(k1 < k2);
  CHECK(k2 < k3);

  TEST_BEGIN("consumer-count/k1-feeds-k2-and-k3");
  // Both K2 and K3 should reference K1's output_tid.
  KernelEntry *K1 = &KERNELS[k1];
  KernelEntry *K2 = &KERNELS[k2];
  KernelEntry *K3 = &KERNELS[k3];
  u32 t1 = K1->output_tid;
  CHECK(t1 != 0);
  CHECK_EQ(TENS[t1].producer_kid, k1);
  int k2_uses_t1 = 0, k3_uses_t1 = 0;
  for (u32 i = 0; i < K2->n_inputs; i++)
    if (K2->input_tids[i] == t1) k2_uses_t1 = 1;
  for (u32 i = 0; i < K3->n_inputs; i++)
    if (K3->input_tids[i] == t1) k3_uses_t1 = 1;
  CHECK(k2_uses_t1);
  CHECK(k3_uses_t1);

  TEST_BEGIN("consumer-count/initial-counts-are-zero");
  // Pre-pass, the field is zero (calloc'd in kernel_alloc).
  CHECK_EQ(K1->consumer_count, 0);
  CHECK_EQ(K2->consumer_count, 0);
  CHECK_EQ(K3->consumer_count, 0);

  TEST_BEGIN("consumer-count/diamond-counts-correctly");
  kernel_compute_consumer_counts();
  CHECK_EQ(K1->consumer_count, 2);
  CHECK_EQ(K2->consumer_count, 0);
  CHECK_EQ(K3->consumer_count, 0);

  TEST_BEGIN("consumer-count/idempotent-rerun");
  // Second invocation must reset + recompute, not accumulate.
  kernel_compute_consumer_counts();
  CHECK_EQ(K1->consumer_count, 2);
  CHECK_EQ(K2->consumer_count, 0);
  CHECK_EQ(K3->consumer_count, 0);

  TEST_BEGIN("consumer-count/leaves-have-zero-producer");
  // Leaf tensors a/b/c/d have producer_kid=0; their slots in
  // input_tids should NOT increment any kernel's count.
  // Verified implicitly: K1 has 2 inputs (a, b), both leaves --
  // total inputs across all kernels = 2 + 2 + 2 = 6, but only 2
  // increments hit (K2->K1 and K3->K1).  K1's count being exactly
  // 2 (not 4) confirms the leaf skip.
  CHECK_EQ(K1->consumer_count, 2);

  thvm_free();
  TEST_REPORT();
}
