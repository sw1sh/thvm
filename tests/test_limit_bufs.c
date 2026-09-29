// test_limit_bufs.c - the kernel-buffer cap (tinygrad _limit_bufs,
// schedule/rangeify.py:176-198) on the CPU backend.
//
// A left-deep sum of N distinct leaf tensors is one elementwise kernel with
// N inputs.  tinygrad splits a kernel whose distinct buffers exceed
// MAX_BUFS - 1 (the output takes the last slot), with
// MAX_BUFS = MAX_KERNEL_BUFFERS.value or DEVICE_MAX_BUFS.get(device, 0);
// DEVICE_MAX_BUFS has no CPU entry (rangeify.py:170), so CPU is uncapped
// unless THVM_MAX_KERNEL_BUFFERS (tinygrad's MAX_KERNEL_BUFFERS) says
// otherwise.  ru_device_max_bufs reads the variable on every schedule.
//
// N stays below 48: rb_collect_accessed_buffers stops counting past depth
// 48, and a left-deep chain puts its first leaf at depth N - 1.

#include "../src/thvm.c"
#include "test.h"

#define LB_N     40
#define LB_ELEMS 16

// Leaf i holds (i + 1) + 64 * j at element j: every leaf differs, and every
// partial sum is an integer far below 2^24, so the f32 result is exact in
// any summation order.
static Term lb_leaf(u32 i) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = LB_ELEMS;
  u32 tid = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
  f32 data[LB_ELEMS];
  for (u32 j = 0; j < LB_ELEMS; j++) {
    data[j] = (f32)(i + 1) + 64.0f * (f32)j;
  }
  CURRENT_BACKEND->buf_write(TENS[tid].buf_id, data, sizeof(data));
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

// Materialize and run the sum of LB_N fresh leaves.  Returns the number of
// kernels it scheduled, the largest input count among them, and whether the
// output equals the exact sum.
typedef struct {
  u32 kernels;
  u32 max_inputs;
  int exact;
} LbRun;

static LbRun lb_run_sum(void) {
  LbRun r = {0, 0, 0};
  Term acc = lb_leaf(0);
  for (u32 i = 1; i < LB_N; i++) {
    acc = uop_binary(UOP_ADD, acc, lb_leaf(i));
  }
  u32  before = KERNELS_NEXT;
  Term out    = wnf(thvm_materialize(acc));
  r.kernels = KERNELS_NEXT - before;
  for (u32 k = before; k < KERNELS_NEXT; k++) {
    if (KERNELS[k].n_inputs > r.max_inputs) {
      r.max_inputs = KERNELS[k].n_inputs;
    }
  }
  if (term_tag(out) != TAG_TEN) {
    return r;
  }
  f32 got[LB_ELEMS];
  CURRENT_BACKEND->buf_read(TENS[(u32)term_val(out)].buf_id, got, sizeof(got));
  f32 leaf_sum = (f32)(LB_N * (LB_N + 1) / 2);
  r.exact = 1;
  for (u32 j = 0; j < LB_ELEMS; j++) {
    if (got[j] != leaf_sum + 64.0f * (f32)LB_N * (f32)j) {
      r.exact = 0;
    }
  }
  return r;
}

int main(void) {
  unsetenv("THVM_MAX_KERNEL_BUFFERS");
  thvm_init();

  TEST_BEGIN("limit-bufs/cpu-default-uncapped-one-kernel");
  LbRun a = lb_run_sum();
  CHECK_EQ(a.kernels, 1);
  CHECK_EQ(a.max_inputs, LB_N);
  CHECK(a.exact);

  TEST_BEGIN("limit-bufs/env-cap-8-splits-under-7-inputs");
  setenv("THVM_MAX_KERNEL_BUFFERS", "8", 1);
  LbRun b = lb_run_sum();
  CHECK(b.kernels > 1);
  CHECK(b.max_inputs <= 7);
  CHECK(b.exact);

  TEST_BEGIN("limit-bufs/env-cap-0-is-off");
  setenv("THVM_MAX_KERNEL_BUFFERS", "0", 1);
  LbRun c = lb_run_sum();
  CHECK_EQ(c.kernels, 1);
  CHECK_EQ(c.max_inputs, LB_N);
  CHECK(c.exact);

  unsetenv("THVM_MAX_KERNEL_BUFFERS");
  thvm_free();
  TEST_REPORT();
}
