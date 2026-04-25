// test_uop.c - raw UOp graph constructors.
//
// Commit 2 only builds graph structure -- no materialize, no
// dispatch yet.  These tests verify each constructor lays out heap
// cells the way docs/tensors.md's layout table says.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("uop/const-heap-layout");
  Term c  = uop_const(DT_F32, 0x40000000);   // 2.0f bit pattern
  CHECK_EQ(term_tag(c), TAG_UOP);
  CHECK_EQ(term_ext(c), UOP_CONST);
  Term num = heap_read(term_val(c));
  CHECK_EQ(term_tag(num), TAG_NUM);
  CHECK_EQ(term_ext(num), DT_F32);
  CHECK_EQ(term_val(num), 0x40000000);

  TEST_BEGIN("uop/unary-layout");
  Term a  = term_new(0, TAG_TEN, DT_F32, 1);
  Term n  = uop_unary(UOP_NEG, a);
  CHECK_EQ(term_tag(n),  TAG_UOP);
  CHECK_EQ(term_ext(n),  UOP_NEG);
  CHECK_EQ(heap_read(term_val(n)), a);

  TEST_BEGIN("uop/binary-layout");
  Term b = term_new(0, TAG_TEN, DT_F32, 2);
  Term sum = uop_binary(UOP_ADD, a, b);
  CHECK_EQ(term_tag(sum), TAG_UOP);
  CHECK_EQ(term_ext(sum), UOP_ADD);
  CHECK_EQ(heap_read(term_val(sum) + 0), a);
  CHECK_EQ(heap_read(term_val(sum) + 1), b);

  TEST_BEGIN("uop/reduce-stores-kind-and-axis");
  Term r = uop_reduce(REDUCE_SUM, 0, a);
  CHECK_EQ(term_tag(r), TAG_UOP);
  CHECK_EQ(term_ext(r), UOP_REDUCE);
  Term kind_cell = heap_read(term_val(r) + 1);
  Term axis_cell = heap_read(term_val(r) + 2);
  CHECK_EQ(term_tag(kind_cell), TAG_NUM);
  CHECK_EQ(term_val(kind_cell), REDUCE_SUM);
  CHECK_EQ(term_tag(axis_cell), TAG_NUM);
  CHECK_EQ(term_val(axis_cell), 0);

  TEST_BEGIN("uop/reshape-stores-dims");
  // Heap layout: [src, NUM(ndim), NUM(d0), ..., NUM(d_{ndim-1})].
  u32 dims[3] = {2, 3, 4};
  Term rs = uop_reshape(a, 3, dims);
  CHECK_EQ(term_tag(rs), TAG_UOP);
  CHECK_EQ(term_ext(rs), UOP_RESHAPE);
  Term ndim_n = heap_read(term_val(rs) + 1);
  CHECK_EQ(term_tag(ndim_n), TAG_NUM);
  CHECK_EQ(term_val(ndim_n), 3);
  for (u32 i = 0; i < 3; i++) {
    Term d = heap_read(term_val(rs) + 2 + i);
    CHECK_EQ(term_tag(d), TAG_NUM);
    CHECK_EQ(term_val(d), dims[i]);
  }

  TEST_BEGIN("uop/pad-layout-begin-end-interleaved");
  u32 be[4] = {1, 1, 2, 3};   // axis 0: pad (1,1); axis 1: pad (2,3)
  Term p = uop_pad(a, 2, be);
  CHECK_EQ(term_ext(p), UOP_PAD);
  for (u32 i = 0; i < 4; i++) {
    CHECK_EQ(term_val(heap_read(term_val(p) + 1 + i)), be[i]);
  }

  TEST_BEGIN("uop/graph-can-nest-deeply");
  // Build sum(a, neg(b)) and check each layer is walkable.
  Term inner = uop_unary (UOP_NEG, b);
  Term outer = uop_binary(UOP_ADD, a, inner);
  CHECK_EQ(term_ext(outer), UOP_ADD);
  Term lhs = heap_read(term_val(outer) + 0);
  Term rhs = heap_read(term_val(outer) + 1);
  CHECK_EQ(lhs, a);
  CHECK_EQ(term_ext(rhs), UOP_NEG);
  CHECK_EQ(heap_read(term_val(rhs)), b);

  TEST_BEGIN("uop/conv2d-heap-layout-three-cells");
  Term inp = term_new(0, TAG_TEN, DT_F32, 3);
  Term wt  = term_new(0, TAG_TEN, DT_F32, 4);
  Term bs  = term_new(0, TAG_TEN, DT_F32, 5);
  Term cv  = uop_conv2d(inp, wt, bs);
  CHECK_EQ(term_tag(cv), TAG_UOP);
  CHECK_EQ(term_ext(cv), UOP_CONV2D);
  CHECK_EQ(heap_read(term_val(cv) + 0), inp);
  CHECK_EQ(heap_read(term_val(cv) + 1), wt);
  CHECK_EQ(heap_read(term_val(cv) + 2), bs);

  thvm_free();
  TEST_REPORT();
}
