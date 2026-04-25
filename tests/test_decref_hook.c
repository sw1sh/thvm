// test_decref_hook.c - exercise the decref + mark-freeable hook
// in kernel_fire_by_id (sub-item b of the refcount-driven free arc).
//
// Builds a 3-kernel diamond (k1 -> k2, k1 -> k3), seeds consumer
// counts via kernel_compute_consumer_counts, then fires k2 and k3
// and verifies:
//   - after k2 fires:  k1.consumer_count == 1, k1's buf NOT freeable
//   - after k3 fires:  k1.consumer_count == 0, k1's buf IS freeable
//   - leaf inputs (a/b/c/d producer_kid == 0) get no decrement and
//     no buf marked freeable (their bufs aren't producer-output bufs).

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

  Term add_ab = uop_binary(UOP_ADD, a, b);
  Term k1_res = thvm_materialize(add_ab);
  u32 k1 = KERNELS_NEXT - 1;

  Term mul1 = uop_binary(UOP_MUL, k1_res, c);
  thvm_materialize(mul1);
  u32 k2 = KERNELS_NEXT - 1;

  Term mul2 = uop_binary(UOP_MUL, k1_res, d);
  thvm_materialize(mul2);
  u32 k3 = KERNELS_NEXT - 1;

  kernel_compute_consumer_counts();
  KernelEntry *K1 = &KERNELS[k1];
  KernelEntry *K2 = &KERNELS[k2];
  KernelEntry *K3 = &KERNELS[k3];

  TEST_BEGIN("decref/precondition-counts-seeded");
  CHECK_EQ(K1->consumer_count, 2);
  CHECK_EQ(K2->consumer_count, 0);
  CHECK_EQ(K3->consumer_count, 0);
  u32 k1_buf = TENS[K1->output_tid].buf_id;
  CHECK(k1_buf > 0);
  CHECK_EQ(CPU_BUFS[k1_buf].freeable, 0);

  TEST_BEGIN("decref/k2-fire-decrements-k1-but-not-yet-freeable");
  kernel_fire_by_id(k2);
  CHECK_EQ(K2->fired, 1);
  CHECK_EQ(K1->consumer_count, 1);  // one consumer left (k3)
  CHECK_EQ(CPU_BUFS[k1_buf].freeable, 0);

  TEST_BEGIN("decref/k3-fire-zeros-k1-and-marks-buf-freeable");
  kernel_fire_by_id(k3);
  CHECK_EQ(K3->fired, 1);
  CHECK_EQ(K1->consumer_count, 0);
  CHECK_EQ(CPU_BUFS[k1_buf].freeable, 1);

  TEST_BEGIN("decref/leaf-input-bufs-not-marked-freeable");
  // a/b/c/d are leaves with producer_kid == 0; they should NOT
  // have been marked freeable by either fire.  (Their bufs were
  // alloc'd by tensor_alloc + sit in CPU_BUFS too.)
  u32 a_buf = TENS[(u32)term_val(a)].buf_id;
  u32 b_buf = TENS[(u32)term_val(b)].buf_id;
  u32 c_buf = TENS[(u32)term_val(c)].buf_id;
  u32 d_buf = TENS[(u32)term_val(d)].buf_id;
  CHECK_EQ(CPU_BUFS[a_buf].freeable, 0);
  CHECK_EQ(CPU_BUFS[b_buf].freeable, 0);
  CHECK_EQ(CPU_BUFS[c_buf].freeable, 0);
  CHECK_EQ(CPU_BUFS[d_buf].freeable, 0);

  TEST_BEGIN("decref/double-fire-does-not-underflow");
  // k2 already fired; calling fire again should be a no-op (the
  // !has_symbolic + ke->fired guard short-circuits).  K1's count
  // must stay at 0, NOT wrap around to UINT32_MAX.
  kernel_fire_by_id(k2);
  CHECK_EQ(K1->consumer_count, 0);

  thvm_free();
  TEST_REPORT();
}
