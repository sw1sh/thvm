// test_tensor.c - tensor descriptor + CPU backend lifecycle.
//
// Covers TAG_TEN bit packing, View construction from Shape, tensor
// alloc / incref / release, view aliasing (shared buffer), buf_read /
// buf_write round-trip.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("tags/TAG_TEN-distinct-from-IC-tags");
  CHECK(TAG_TEN  == 8);
  CHECK(TAG_UOP  == 9);
  CHECK(TAG_NUM  == 10);
  CHECK(TAG_REF  == 11);
  CHECK(TAG_ALO  == 12);
  CHECK(TAG_OP2  == 13);
  CHECK(TAG_MAT  == 14);
  CHECK(TAG_EQL  == 15);
  CHECK(TAG_AND  == 16);
  CHECK(TAG_OR   == 17);
  CHECK(TAG_ANY  == 18);
  CHECK(TAG_INC  == 19);
  CHECK(TAG_CTR  == 20);
  CHECK(TAG_WHEN == 21);
  CHECK(TAG_FVR  == 22);
  CHECK(TAG_BRI  == 23);
  CHECK(TAG_ANN  == 24);
  CHECK(TAG_PRI  == 25);
  CHECK(TAG_F_OP2_NUM   == 26);
  CHECK(TAG_F_EQL_R     == 27);
  CHECK(TAG_F_UOP_CHILD == 28);
  CHECK(TAG_COUNT == 29);

  TEST_BEGIN("term/TAG_TEN-roundtrip");
  Term t = term_new(0, TAG_TEN, DT_FP32, 42);
  CHECK_EQ(term_tag(t), TAG_TEN);
  CHECK_EQ(term_ext(t), DT_FP32);
  CHECK_EQ(term_val(t), 42);

  TEST_BEGIN("view/create-row-major-2D");
  Shape s = { .ndim = 2, .dims = {3, 4, 0, 0, 0, 0, 0, 0} };
  View  v = view_create(s);
  CHECK_EQ(v.numel,      12);
  CHECK_EQ(v.contiguous,  1);
  CHECK_EQ(v.offset,      0);
  CHECK_EQ(v.strides[0],  4);
  CHECK_EQ(v.strides[1],  1);

  TEST_BEGIN("tensor/alloc-basic");
  Shape s1 = { .ndim = 1, .dims = {4} };
  u32   id = tensor_alloc(CURRENT_BACKEND, s1, DT_FP32);
  CHECK(id != 0);
  CHECK_EQ(TENS[id].dtype,    DT_FP32);
  CHECK_EQ(TENS[id].refcount, 1);
  CHECK_EQ(TENS[id].view.numel, 4);
  CHECK(TENS[id].buf_id != 0);

  TEST_BEGIN("tensor/buf_write-read-roundtrip");
  f32 in[4]  = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 out[4] = {0};
  CHECK_EQ(CURRENT_BACKEND->buf_write(TENS[id].buf_id, in,  4 * sizeof(f32)), 0);
  CHECK_EQ(CURRENT_BACKEND->buf_read (TENS[id].buf_id, out, 4 * sizeof(f32)), 0);
  for (int i = 0; i < 4; i++) CHECK(in[i] == out[i]);

  TEST_BEGIN("tensor/incref-then-release-twice");
  u32 buf = TENS[id].buf_id;
  tensor_incref(id);
  CHECK_EQ(TENS[id].refcount, 2);
  tensor_release(id);
  CHECK_EQ(TENS[id].refcount, 1);
  CHECK_EQ(CPU_BUFS[buf].refcount, 1);    // buffer still alive
  tensor_release(id);
  CHECK_EQ(TENS[id].refcount,    0);
  CHECK_EQ(CPU_BUFS[buf].refcount, 0);    // buffer freed
  CHECK(CPU_BUFS[buf].data == NULL);

  TEST_BEGIN("tensor/view_of-shares-buffer");
  Shape s2 = { .ndim = 2, .dims = {2, 3} };
  u32   src = tensor_alloc(CURRENT_BACKEND, s2, DT_FP32);
  u32   src_buf = TENS[src].buf_id;
  CHECK_EQ(CPU_BUFS[src_buf].refcount, 1);
  Shape s3 = { .ndim = 1, .dims = {6} };
  u32   alias = tensor_view_of(src, view_create(s3));
  CHECK_EQ(TENS[alias].buf_id, src_buf);  // shared
  CHECK_EQ(CPU_BUFS[src_buf].refcount, 2);
  tensor_release(src);
  CHECK_EQ(CPU_BUFS[src_buf].refcount, 1);  // alias keeps it alive
  tensor_release(alias);
  CHECK_EQ(CPU_BUFS[src_buf].refcount, 0);  // both gone -> freed
  CHECK(CPU_BUFS[src_buf].data == NULL);

  thvm_free();
  TEST_REPORT();
}
