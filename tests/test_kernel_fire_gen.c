// test_kernel_fire_gen.c - kernel_fire_by_id per-generation memo.
//
// Builds a 3-kernel diamond (k1 -> k2, k1 -> k3).  The current
// runtime does not carry a persistent KernelEntry.fired bit and does
// not decref buffers at dispatch time.  Instead, kernel_fire_by_id
// uses fire_gen to ensure a shared producer dispatches once within a
// top-level kernel fire and can dispatch again in a later generation.

#include "../src/thvm.c"
#include "test.h"

static u32 scope_begin_count = 0;
static u32 scope_flush_count = 0;
static u32 scope_end_count   = 0;

static void scope_begin_cb(void) { scope_begin_count++; }
static void scope_flush_cb(void) { scope_flush_count++; }
static void scope_end_cb  (void) { scope_end_count++; }

static u32 alloc_f32_tensor(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) {
    s.dims[i] = dims[i];
  }
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

int main(void) {
  thvm_init();

  u32 dims[1] = {3};
  Term a = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(dims, 1));
  Term b = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(dims, 1));
  Term c = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(dims, 1));
  Term d = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(dims, 1));

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
  u32 k1_buf = TENS[K1->output_tid].buf_id;

  TEST_BEGIN("kernel-fire/precondition-counts-seeded");
  CHECK_EQ(K1->consumer_count, 2);
  CHECK_EQ(K2->consumer_count, 0);
  CHECK_EQ(K3->consumer_count, 0);
  CHECK(k1_buf > 0);
  CHECK_EQ(CPU_BUFS[k1_buf].freeable, 0);

  TEST_BEGIN("kernel-fire/first-generation-fires-producer-and-consumer");
  kernel_fire_gen_bump();
  u32 gen1 = KERNEL_FIRE_GEN;
  kernel_fire_by_id(k2);
  CHECK_EQ(K1->fire_gen, gen1);
  CHECK_EQ(K2->fire_gen, gen1);
  CHECK_EQ(cg_kernel_dispatch_count(k1), 1);
  CHECK_EQ(cg_kernel_dispatch_count(k2), 1);
  CHECK_EQ(K1->consumer_count, 2);
  CHECK_EQ(CPU_BUFS[k1_buf].freeable, 0);

  TEST_BEGIN("kernel-fire/same-generation-repeat-is-no-op");
  kernel_fire_by_id(k2);
  CHECK_EQ(cg_kernel_dispatch_count(k1), 1);
  CHECK_EQ(cg_kernel_dispatch_count(k2), 1);
  CHECK_EQ(K1->consumer_count, 2);

  TEST_BEGIN("kernel-fire/shared-producer-fires-once-per-generation");
  kernel_fire_by_id(k3);
  CHECK_EQ(K3->fire_gen, gen1);
  CHECK_EQ(cg_kernel_dispatch_count(k1), 1);
  CHECK_EQ(cg_kernel_dispatch_count(k3), 1);

  TEST_BEGIN("kernel-fire/new-generation-refires-kernel-dag");
  kernel_fire_gen_bump();
  u32 gen2 = KERNEL_FIRE_GEN;
  kernel_fire_by_id(k2);
  CHECK_EQ(K1->fire_gen, gen2);
  CHECK_EQ(K2->fire_gen, gen2);
  CHECK_EQ(cg_kernel_dispatch_count(k1), 2);
  CHECK_EQ(cg_kernel_dispatch_count(k2), 2);
  CHECK_EQ(K1->consumer_count, 2);
  CHECK_EQ(CPU_BUFS[k1_buf].freeable, 0);

  TEST_BEGIN("kernel-fire/benchmark-fire-dispatches-kernel-directly");
  u32 before_k1 = cg_kernel_dispatch_count(k1);
  u32 before_k2 = cg_kernel_dispatch_count(k2);
  u32 before_gen = KERNEL_FIRE_GEN;
  u32 before_k1_fire_gen = K1->fire_gen;
  u32 before_k2_fire_gen = K2->fire_gen;
  u64 bench_us = kernel_bench_us(k2, 3);
  (void)bench_us;
  CHECK_EQ(cg_kernel_dispatch_count(k1), before_k1);
  CHECK_EQ(cg_kernel_dispatch_count(k2), before_k2 + 3);
  CHECK_EQ(KERNEL_FIRE_GEN, before_gen);
  CHECK_EQ(K1->fire_gen, before_k1_fire_gen);
  CHECK_EQ(K2->fire_gen, before_k2_fire_gen);

  TEST_BEGIN("kernel-fire/leaf-input-bufs-not-marked-freeable");
  u32 a_buf = TENS[(u32)term_val(a)].buf_id;
  u32 b_buf = TENS[(u32)term_val(b)].buf_id;
  u32 c_buf = TENS[(u32)term_val(c)].buf_id;
  u32 d_buf = TENS[(u32)term_val(d)].buf_id;
  CHECK_EQ(CPU_BUFS[a_buf].freeable, 0);
  CHECK_EQ(CPU_BUFS[b_buf].freeable, 0);
  CHECK_EQ(CPU_BUFS[c_buf].freeable, 0);
  CHECK_EQ(CPU_BUFS[d_buf].freeable, 0);

  TEST_BEGIN("kernel-fire/realize-wraps-backend-dispatch-scope");
  CPU_BACKEND.dispatch_begin = scope_begin_cb;
  CPU_BACKEND.dispatch_flush = scope_flush_cb;
  CPU_BACKEND.dispatch_end   = scope_end_cb;
  scope_begin_count = scope_flush_count = scope_end_count = 0;
  Term x = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(dims, 1));
  Term y = term_new(0, TAG_TEN, DT_FP32, alloc_f32_tensor(dims, 1));
  thvm_realize(uop_binary(UOP_ADD, x, y));
  CHECK_EQ(scope_begin_count, 1);
  CHECK_EQ(scope_end_count,   1);
  CHECK_EQ(scope_flush_count, 0);
  CPU_BACKEND.dispatch_begin = NULL;
  CPU_BACKEND.dispatch_flush = NULL;
  CPU_BACKEND.dispatch_end   = NULL;

  thvm_free();
  TEST_REPORT();
}
