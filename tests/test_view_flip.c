// test_view_flip.c - exercise the view-only FLIP path
// (sub-item f3g of the kernel-fusion arc).
//
// Verifies:
//   - FLIP on a contiguous source returns an alias TenDesc that
//     shares buf_id with the source, has the same shape, negated
//     strides on flipped axes, and offset shifted to the high end.
//   - The alias's view is non-contiguous.
//   - Empty mask (no flipped axes) preserves contig + strides.
//   - Multi-axis flip composes correctly.
//   - thvm_materialize at root post-materializes the alias into a
//     fresh contig buf with reversed values.
//   - A strided consumer (ADD over the FLIP alias) computes
//     correctly via the UOp DAG walker (cpu_uop_walk), which
//     replaced cpu_interpret's pre-materialize path in F6 cleanup.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  // 4-element 1D source.  FLIP axis 0 (mask=1) reverses to
  // [4, 3, 2, 1].
  Shape s1 = {0}; s1.ndim = 1; s1.dims[0] = 4;
  u32 src_tid = tensor_alloc(CURRENT_BACKEND, s1, DT_FP32);
  f32 src_buf[4] = {1, 2, 3, 4};
  CURRENT_BACKEND->buf_write(TENS[src_tid].buf_id, src_buf, sizeof(src_buf));
  Term src = term_new(0, TAG_TEN, DT_FP32, src_tid);

  Term flipped = uop_flip(src, /*axes_bitmask=*/1);

  TEST_BEGIN("view-flip/uop-emits-alias-tenden");
  Term out = materialize_uop_in_env(flipped, /*env_id=*/0);
  CHECK_EQ(term_tag(out), TAG_TEN);
  u32 alias_tid = (u32)term_val(out);
  CHECK(alias_tid > 0);
  CHECK(alias_tid != src_tid);

  TEST_BEGIN("view-flip/alias-shares-buf");
  CHECK_EQ(TENS[alias_tid].buf_id, TENS[src_tid].buf_id);

  TEST_BEGIN("view-flip/shape-unchanged-stride-negated");
  CHECK_EQ(TENS[alias_tid].view.shape.ndim, 1);
  CHECK_EQ(TENS[alias_tid].view.shape.dims[0], 4);
  CHECK_EQ(TENS[alias_tid].view.strides[0], -1);

  TEST_BEGIN("view-flip/offset-shifted-to-high-end");
  // offset = 0 + (4 - 1) * 1 = 3.
  CHECK_EQ(TENS[alias_tid].view.offset, 3);

  TEST_BEGIN("view-flip/non-contig");
  CHECK_EQ(TENS[alias_tid].view.contiguous, 0);

  TEST_BEGIN("view-flip/producer-kid-inherited");
  CHECK_EQ(TENS[alias_tid].producer_kid, TENS[src_tid].producer_kid);

  TEST_BEGIN("view-flip/empty-mask-preserves-contig");
  Term flip0 = uop_flip(src, /*axes_bitmask=*/0);
  Term out0  = materialize_uop_in_env(flip0, /*env_id=*/0);
  CHECK_EQ(term_tag(out0), TAG_TEN);
  u32 alias0_tid = (u32)term_val(out0);
  CHECK_EQ(TENS[alias0_tid].view.contiguous, 1);
  CHECK_EQ(TENS[alias0_tid].view.offset, 0);
  CHECK_EQ(TENS[alias0_tid].view.strides[0], 1);

  TEST_BEGIN("view-flip/root-materialize-flatten-gives-reversed");
  Term flipped2 = uop_flip(src, 1);
  Term mat = thvm_materialize(flipped2);
  CHECK_EQ(term_tag(mat), TAG_TEN);
  u32 mat_tid = (u32)term_val(mat);
  CHECK(TENS[mat_tid].view.contiguous);
  f32 out_buf[4] = {0};
  CURRENT_BACKEND->buf_read(TENS[mat_tid].buf_id, out_buf, sizeof(out_buf));
  CHECK(out_buf[0] == 4.0f);
  CHECK(out_buf[1] == 3.0f);
  CHECK(out_buf[2] == 2.0f);
  CHECK(out_buf[3] == 1.0f);

  TEST_BEGIN("view-flip/2d-multi-axis-flip");
  // 2x3 source: {{1,2,3},{4,5,6}}.
  // FLIP axes 0 + 1 (mask=3) -> reversed both ways:
  //   {{6,5,4},{3,2,1}}.
  Shape s2 = {0}; s2.ndim = 2; s2.dims[0] = 2; s2.dims[1] = 3;
  u32 src2_tid = tensor_alloc(CURRENT_BACKEND, s2, DT_FP32);
  f32 src2_buf[6] = {1, 2, 3, 4, 5, 6};
  CURRENT_BACKEND->buf_write(TENS[src2_tid].buf_id, src2_buf, sizeof(src2_buf));
  Term src2 = term_new(0, TAG_TEN, DT_FP32, src2_tid);
  Term flip2 = uop_flip(src2, 3);
  Term out2  = materialize_uop_in_env(flip2, /*env_id=*/0);
  CHECK_EQ(term_tag(out2), TAG_TEN);
  u32 alias2_tid = (u32)term_val(out2);
  // strides: src=[3,1] -> negated=[-3,-1].
  CHECK_EQ(TENS[alias2_tid].view.strides[0], -3);
  CHECK_EQ(TENS[alias2_tid].view.strides[1], -1);
  // offset: 0 + (2-1)*3 + (3-1)*1 = 5.
  CHECK_EQ(TENS[alias2_tid].view.offset, 5);

  TEST_BEGIN("view-flip/2d-root-materialize-correct");
  Term flip2b = uop_flip(src2, 3);
  Term mat2 = thvm_materialize(flip2b);
  u32 mat2_tid = (u32)term_val(mat2);
  f32 out2_buf[6] = {0};
  CURRENT_BACKEND->buf_read(TENS[mat2_tid].buf_id, out2_buf, sizeof(out2_buf));
  CHECK(out2_buf[0] == 6.0f);
  CHECK(out2_buf[1] == 5.0f);
  CHECK(out2_buf[2] == 4.0f);
  CHECK(out2_buf[3] == 3.0f);
  CHECK(out2_buf[4] == 2.0f);
  CHECK(out2_buf[5] == 1.0f);

  TEST_BEGIN("view-flip/strided-consumer-add");
  // ADD(FLIP(src), FLIP(src)) -> 2 * each reversed value.
  Term fa = uop_flip(src, 1);
  Term fb = uop_flip(src, 1);
  Term add = uop_binary(UOP_ADD, fa, fb);
  Term done = wnf(thvm_materialize(add));
  CHECK_EQ(term_tag(done), TAG_TEN);
  u32 done_tid = (u32)term_val(done);
  f32 add_buf[4] = {0};
  CURRENT_BACKEND->buf_read(TENS[done_tid].buf_id, add_buf, sizeof(add_buf));
  CHECK(add_buf[0] == 8.0f);
  CHECK(add_buf[1] == 6.0f);
  CHECK(add_buf[2] == 4.0f);
  CHECK(add_buf[3] == 2.0f);

  thvm_free();
  TEST_REPORT();
}
