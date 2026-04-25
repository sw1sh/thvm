// test_view_shrink.c - exercise the view-only SHRINK path
// (sub-item f3d of the kernel-fusion arc).
//
// Verifies:
//   - SHRINK on a contiguous source returns an alias TenDesc that
//     shares buf_id with the source, has the cropped shape,
//     inherits source strides, and offset = sum(b_i * src_strides[i]).
//   - The alias's view is non-contiguous (slice of a larger buf).
//   - producer_kid is inherited so downstream kernels can chase
//     the upstream filler.
//   - thvm_materialize at root post-materializes the alias into a
//     fresh contig buf populated via view_strided_index, so flat
//     buf_read gives the cropped values [6, 7, 10, 11].
//   - A strided consumer (an ADD over the SHRINK alias) computes
//     correctly via the cpu interpreter's pre-materialize path.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  // 4x4 source filled 1..16.
  Shape s = {0}; s.ndim = 2; s.dims[0] = 4; s.dims[1] = 4;
  u32 src_tid = tensor_alloc(CURRENT_BACKEND, s, DT_F32);
  f32 src_buf[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  CURRENT_BACKEND->buf_write(TENS[src_tid].buf_id, src_buf, sizeof(src_buf));
  Term src = term_new(0, TAG_TEN, DT_F32, src_tid);

  // SHRINK to 2x2 center: keep rows 1..2, cols 1..2 -> {{6,7},{10,11}}.
  u32 be[4] = {1, 3, 1, 3};
  Term shrunk = uop_shrink(src, 2, be);

  TEST_BEGIN("view-shrink/uop-emits-alias-tenden");
  Term out = materialize_uop_in_env(shrunk, /*env_id=*/0);
  CHECK_EQ(term_tag(out), TAG_TEN);
  u32 alias_tid = (u32)term_val(out);
  CHECK(alias_tid > 0);
  CHECK(alias_tid != src_tid);

  TEST_BEGIN("view-shrink/alias-shares-buf-with-source");
  CHECK_EQ(TENS[alias_tid].buf_id, TENS[src_tid].buf_id);

  TEST_BEGIN("view-shrink/alias-shape-cropped");
  CHECK_EQ(TENS[alias_tid].view.shape.ndim, 2);
  CHECK_EQ(TENS[alias_tid].view.shape.dims[0], 2);
  CHECK_EQ(TENS[alias_tid].view.shape.dims[1], 2);
  CHECK_EQ(TENS[alias_tid].view.numel, 4);

  TEST_BEGIN("view-shrink/alias-strides-inherited");
  // Source row-major strides for 4x4: [4, 1].
  CHECK_EQ(TENS[alias_tid].view.strides[0], 4);
  CHECK_EQ(TENS[alias_tid].view.strides[1], 1);

  TEST_BEGIN("view-shrink/alias-offset-by-begin-strided");
  // begin = (1, 1); offset = 1*4 + 1*1 = 5.
  CHECK_EQ(TENS[alias_tid].view.offset, 5);

  TEST_BEGIN("view-shrink/alias-non-contig");
  CHECK_EQ(TENS[alias_tid].view.contiguous, 0);

  TEST_BEGIN("view-shrink/producer-kid-inherited");
  // src has producer_kid == 0 (leaf); alias should mirror.
  CHECK_EQ(TENS[alias_tid].producer_kid, TENS[src_tid].producer_kid);

  TEST_BEGIN("view-shrink/root-materialize-flatten-gives-cropped-values");
  // Re-build the SHRINK fresh so the materialize_root_alias path
  // runs end-to-end (post-walk, contig buf populated via
  // view_strided_index).  Read flat -> [6, 7, 10, 11].
  Term shrunk2 = uop_shrink(src, 2, be);
  Term mat = thvm_materialize(shrunk2);
  CHECK_EQ(term_tag(mat), TAG_TEN);
  u32 mat_tid = (u32)term_val(mat);
  // Root-aliased into a contig buf -- different tid AND buf from
  // the alias above.
  CHECK(TENS[mat_tid].view.contiguous);
  f32 out_buf[4] = {0};
  CURRENT_BACKEND->buf_read(TENS[mat_tid].buf_id, out_buf, sizeof(out_buf));
  CHECK(out_buf[0] == 6.0f);
  CHECK(out_buf[1] == 7.0f);
  CHECK(out_buf[2] == 10.0f);
  CHECK(out_buf[3] == 11.0f);

  TEST_BEGIN("view-shrink/strided-consumer-add");
  // ADD(SHRINK(src, center), SHRINK(src, center)) -> 2 * each.
  Term sh_a = uop_shrink(src, 2, be);
  Term sh_b = uop_shrink(src, 2, be);
  Term add  = uop_binary(UOP_ADD, sh_a, sh_b);
  Term done = wnf(thvm_materialize(add));
  CHECK_EQ(term_tag(done), TAG_TEN);
  u32 done_tid = (u32)term_val(done);
  f32 add_buf[4] = {0};
  CURRENT_BACKEND->buf_read(TENS[done_tid].buf_id, add_buf, sizeof(add_buf));
  CHECK(add_buf[0] == 12.0f);   // 2 * 6
  CHECK(add_buf[1] == 14.0f);   // 2 * 7
  CHECK(add_buf[2] == 20.0f);   // 2 * 10
  CHECK(add_buf[3] == 22.0f);   // 2 * 11

  thvm_free();
  TEST_REPORT();
}
