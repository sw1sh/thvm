// test_shrink_over_pad.c -- SHRINK begin-offset must survive rangeify.
//
// project_shrink_over_pad_offset_dropped: a SHRINK with begin > 0 that
// feeds a REDUCE (or whose source is a computed PAD) drops its begin
// offset.  The forward view-resolve fast path is correct, but the
// rangeify path (apply_movement_op_shrink in schedule/indexing.c) was
// made a pass-through in a4d6ca41 on the (now-false) assumption that
// view_apply_shrink always folds the offset downstream.  Once a REDUCE
// consumes the SHRINK, view_apply_shrink never runs and the offset
// vanishes -- tinygrad indexing.py:131 does `a+ss`.
//
// Reference (tinygrad, DEV=CPU):
//   arange(5).shrink((1,4)).sum()                   == 1+2+3 == 6
//   arange(12).reshape(3,4).shrink(((0,3),(1,4))).sum(0) == [15,18,21]
//   pad(arange(3),(1,1)).shrink((1,4)).sum()        == 1+2+3 == 6

#include "../src/thvm.c"
#include "test.h"

#define CHECK_F32(got, want) do {                                            \
  test_total++;                                                              \
  double _g = (double)(got), _w = (double)(want);                            \
  double _d = _g - _w; if (_d < 0) _d = -_d;                                 \
  if (_d > 1e-4) {                                                           \
    test_failures++;                                                         \
    fprintf(stderr, "  FAIL %s:%d  %s  expected %g, got %g\n",               \
            __FILE__, __LINE__, test_name, _w, _g);                          \
  }                                                                          \
} while (0)

static u32 alloc_f32(u32 *dims, u32 ndim, f32 *vals) {
  Shape s = {0}; s.ndim = ndim;
  u32 n = 1;
  for (u32 i = 0; i < ndim; i++) { s.dims[i] = dims[i]; n *= dims[i]; }
  u32 tid = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[tid].buf_id, vals, sizeof(f32) * n);
  return tid;
}

int main(void) {
  thvm_init();

  // CASE 1: 1-D shrink([1,4]) feeding a sum.  arange(5)[1:4] = {1,2,3}, sum=6.
  TEST_BEGIN("shrink-over-pad/1d-shrink-sum");
  {
    u32 d[1] = {5};
    f32 v[5] = {0, 1, 2, 3, 4};
    Term a = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d, 1, v));
    u32 sh[2] = {1, 4};                 // window [1:4] -> {1,2,3}
    Term s   = uop_shrink(a, 1, sh);
    Term red = uop_reduce(REDUCE_SUM, 0, s);
    Term done = wnf(thvm_realize(red));
    CHECK(term_tag(done) == TAG_TEN);
    f32 got = 0.0f;
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id, &got, sizeof(f32));
    CHECK_F32(got, 6.0f);               // 1+2+3, NOT 0+1+2==3
  }

  // CASE 2: the exact a4d6ca41 regression repro: 2-D shrink + reduce axis 0.
  TEST_BEGIN("shrink-over-pad/2d-shrink-reduce-axis0");
  {
    u32 d[2] = {3, 4};
    f32 v[12];
    for (u32 i = 0; i < 12; i++) v[i] = (f32)i;
    Term a = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d, 2, v));
    u32 sh[4] = {0, 3, 1, 4};           // rows 0:3, cols 1:4
    Term s   = uop_shrink(a, 2, sh);
    Term red = uop_reduce(REDUCE_SUM, 0, s);   // reduce axis 0 -> shape {3}
    Term done = wnf(thvm_realize(red));
    CHECK(term_tag(done) == TAG_TEN);
    f32 out[3] = {0};
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id, out, sizeof(f32) * 3);
    // cols 1,2,3 summed over rows {0,1,2}:
    //   col1 = 1+5+9 = 15, col2 = 2+6+10 = 18, col3 = 3+7+11 = 21
    CHECK_F32(out[0], 15.0f);
    CHECK_F32(out[1], 18.0f);
    CHECK_F32(out[2], 21.0f);
  }

  // CASE 3: SHRINK over a COMPUTED (PAD) source feeding a reduce.
  // pad(arange(3),(1,1)) = {0,1,2,3,0}; [1:4] = {1,2,3}; sum = 6.
  TEST_BEGIN("shrink-over-pad/shrink-of-pad-sum");
  {
    u32 d[1] = {3};
    f32 v[3] = {1, 2, 3};
    Term a = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d, 1, v));
    u32 pd[2] = {1, 1};
    Term p   = uop_pad(a, 1, pd);
    u32 sh[2] = {1, 4};
    Term s   = uop_shrink(p, 1, sh);
    Term red = uop_reduce(REDUCE_SUM, 0, s);
    Term done = wnf(thvm_realize(red));
    CHECK(term_tag(done) == TAG_TEN);
    f32 got = 0.0f;
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id, &got, sizeof(f32));
    CHECK_F32(got, 6.0f);               // 1+2+3, NOT 0+1+2==3
  }

  // CASE 4: forward window-only sanity (the view-resolve fast path, should
  // already pass): shrink over pad without a reduce.
  // pad(arange(3),(1,1))[1:4] = {1,2,3}.
  TEST_BEGIN("shrink-over-pad/shrink-of-pad-window");
  {
    u32 d[1] = {3};
    f32 v[3] = {1, 2, 3};
    Term a = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d, 1, v));
    u32 pd[2] = {1, 1};
    Term p   = uop_pad(a, 1, pd);
    u32 sh[2] = {1, 4};
    Term s   = uop_shrink(p, 1, sh);
    Term done = wnf(thvm_realize(s));
    CHECK(term_tag(done) == TAG_TEN);
    f32 out[3] = {0};
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id, out, sizeof(f32) * 3);
    CHECK_F32(out[0], 1.0f);
    CHECK_F32(out[1], 2.0f);
    CHECK_F32(out[2], 3.0f);
  }

  TEST_REPORT();
}
