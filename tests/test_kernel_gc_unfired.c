// test_kernel_gc_unfired.c -- the kernel GC treats a kernel's live output as
// committed only once the kernel has dispatched.  A chain materialized outside
// a realize (WL TMaterialize, then TDef) has live but unwritten outputs; if an
// unrelated realize's sweep cut their producer links, firing the chain's last
// kernel would read its inputs unwritten.
#include "../src/thvm.c"
#include "test.h"

static Term ten3(f32 x, f32 y, f32 z) {
  Shape s   = {0};
  s.ndim    = 1;
  s.dims[0] = 3;
  u32 tid   = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  f32 v[3]  = { x, y, z };
  CPU_BACKEND.buf_write(TENS[tid].buf_id, v, sizeof v);
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("kernel-gc/an-unfired-chain-keeps-its-producer-links");
  {
    Term a   = ten3(1, 2, 3);
    Term b   = ten3(10, 20, 30);
    Term c   = ten3(2, 2, 2);
    thvm_materialize(uop_binary(UOP_ADD, a, b));
    u32  k1  = KERNELS_NEXT - 1;
    Term sum = term_new(0, TAG_TEN, DT_FP32, KERNELS[k1].output_tid);
    thvm_materialize(uop_binary(UOP_MUL, sum, c));
    u32  k2  = KERNELS_NEXT - 1;
    CHECK(k2 != k1);
    Term d = ten3(1, 1, 1);
    CHECK(thvm_realize(uop_binary(UOP_ADD, d, d)) != 0);
    CHECK_EQ(TENS[KERNELS[k1].output_tid].producer_kid, k1);
    kernel_fire_gen_bump();
    kernel_fire_by_id(k2);
    u32 out    = KERNELS[k2].output_tid;
    f32 got[3] = { 0 };
    TENS[out].backend->buf_read(TENS[out].buf_id, got, sizeof got);
    CHECK(got[0] == 22.0f && got[1] == 44.0f && got[2] == 66.0f);
  }

  thvm_free();
  TEST_REPORT();
}
