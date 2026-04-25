// test_view_permute.c - exercise the view-only PERMUTE path
// (sub-item f3e of the kernel-fusion arc).
//
// Verifies:
//   - PERMUTE on a contiguous source returns an alias TenDesc that
//     shares buf_id with the source, has the permuted shape, and
//     strides reordered per the perm argument.
//   - Offset is unchanged.
//   - Non-identity perm marks the alias as non-contiguous.
//   - Identity perm preserves contiguity.
//   - thvm_materialize at root post-materializes the alias into a
//     fresh contig buf so flat buf_read gives the transposed values.
//   - producer_kid is inherited.
//   - A strided consumer (ADD over the PERMUTE alias) computes
//     correctly via cpu_interpret's pre-materialize path.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  // 2x3 source: {{1,2,3},{4,5,6}} -> permute {1,0} -> 3x2 transpose
  // {{1,4},{2,5},{3,6}}.
  Shape s = {0}; s.ndim = 2; s.dims[0] = 2; s.dims[1] = 3;
  u32 src_tid = tensor_alloc(CURRENT_BACKEND, s, DT_F32);
  f32 src_buf[6] = {1, 2, 3, 4, 5, 6};
  CURRENT_BACKEND->buf_write(TENS[src_tid].buf_id, src_buf, sizeof(src_buf));
  Term src = term_new(0, TAG_TEN, DT_F32, src_tid);

  u32 perm[2] = {1, 0};
  Term permuted = uop_permute(src, 2, perm);

  TEST_BEGIN("view-permute/uop-emits-alias-tenden");
  Term out = materialize_uop_in_env(permuted, /*env_id=*/0);
  CHECK_EQ(term_tag(out), TAG_TEN);
  u32 alias_tid = (u32)term_val(out);
  CHECK(alias_tid > 0);
  CHECK(alias_tid != src_tid);

  TEST_BEGIN("view-permute/alias-shares-buf");
  CHECK_EQ(TENS[alias_tid].buf_id, TENS[src_tid].buf_id);

  TEST_BEGIN("view-permute/alias-shape-permuted");
  CHECK_EQ(TENS[alias_tid].view.shape.ndim, 2);
  CHECK_EQ(TENS[alias_tid].view.shape.dims[0], 3);
  CHECK_EQ(TENS[alias_tid].view.shape.dims[1], 2);
  CHECK_EQ(TENS[alias_tid].view.numel, 6);

  TEST_BEGIN("view-permute/alias-strides-permuted");
  // Source strides: [3, 1]. Permuted by {1, 0}: [1, 3].
  CHECK_EQ(TENS[alias_tid].view.strides[0], 1);
  CHECK_EQ(TENS[alias_tid].view.strides[1], 3);

  TEST_BEGIN("view-permute/alias-offset-unchanged");
  CHECK_EQ(TENS[alias_tid].view.offset, 0);

  TEST_BEGIN("view-permute/non-identity-non-contig");
  CHECK_EQ(TENS[alias_tid].view.contiguous, 0);

  TEST_BEGIN("view-permute/producer-kid-inherited");
  CHECK_EQ(TENS[alias_tid].producer_kid, TENS[src_tid].producer_kid);

  TEST_BEGIN("view-permute/identity-perm-preserves-contig");
  u32 perm_id[2] = {0, 1};
  Term ident = uop_permute(src, 2, perm_id);
  Term ident_out = materialize_uop_in_env(ident, /*env_id=*/0);
  CHECK_EQ(term_tag(ident_out), TAG_TEN);
  u32 ident_tid = (u32)term_val(ident_out);
  CHECK_EQ(TENS[ident_tid].view.contiguous, 1);
  CHECK_EQ(TENS[ident_tid].view.strides[0], 3);
  CHECK_EQ(TENS[ident_tid].view.strides[1], 1);

  TEST_BEGIN("view-permute/root-materialize-flatten-gives-transposed");
  Term permuted2 = uop_permute(src, 2, perm);
  Term mat = thvm_materialize(permuted2);
  CHECK_EQ(term_tag(mat), TAG_TEN);
  u32 mat_tid = (u32)term_val(mat);
  CHECK(TENS[mat_tid].view.contiguous);
  f32 out_buf[6] = {0};
  CURRENT_BACKEND->buf_read(TENS[mat_tid].buf_id, out_buf, sizeof(out_buf));
  // Expected transpose: {{1,4},{2,5},{3,6}} flat = [1,4,2,5,3,6].
  CHECK(out_buf[0] == 1.0f);
  CHECK(out_buf[1] == 4.0f);
  CHECK(out_buf[2] == 2.0f);
  CHECK(out_buf[3] == 5.0f);
  CHECK(out_buf[4] == 3.0f);
  CHECK(out_buf[5] == 6.0f);

  TEST_BEGIN("view-permute/strided-consumer-add");
  // ADD(PERMUTE(src), PERMUTE(src)) -> 2 * each transposed value.
  Term pa = uop_permute(src, 2, perm);
  Term pb = uop_permute(src, 2, perm);
  Term add = uop_binary(UOP_ADD, pa, pb);
  Term done = wnf(thvm_materialize(add));
  CHECK_EQ(term_tag(done), TAG_TEN);
  u32 done_tid = (u32)term_val(done);
  f32 add_buf[6] = {0};
  CURRENT_BACKEND->buf_read(TENS[done_tid].buf_id, add_buf, sizeof(add_buf));
  CHECK(add_buf[0] == 2.0f);
  CHECK(add_buf[1] == 8.0f);
  CHECK(add_buf[2] == 4.0f);
  CHECK(add_buf[3] == 10.0f);
  CHECK(add_buf[4] == 6.0f);
  CHECK(add_buf[5] == 12.0f);

  thvm_free();
  TEST_REPORT();
}
