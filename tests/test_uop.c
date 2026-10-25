// test_uop.c - raw UOp graph constructors.
//
// Commit 2 only builds graph structure -- no materialize, no
// dispatch yet.  These tests verify each constructor lays out heap
// cells the way its opcode's #define in src/thvm.h says.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  thvm_init();

  TEST_BEGIN("uop/const-heap-layout");
  Term c  = uop_const(DT_FP32, 0x40000000);   // 2.0f bit pattern
  CHECK_EQ(term_tag(c), TAG_UOP);
  CHECK_EQ(term_ext(c), UOP_CONST);
  Term num = heap_read(term_val(c));
  CHECK_EQ(term_tag(num), TAG_NUM);
  CHECK_EQ(term_ext(num), DT_FP32);
  CHECK_EQ(term_val(num), 0x40000000);

  TEST_BEGIN("uop/unary-layout");
  Term a  = term_new(0, TAG_TEN, DT_FP32, 1);
  Term n  = uop_unary(UOP_NEG, a);
  CHECK_EQ(term_tag(n),  TAG_UOP);
  CHECK_EQ(term_ext(n),  UOP_NEG);
  CHECK_EQ(heap_read(term_val(n)), a);

  TEST_BEGIN("uop/binary-layout");
  Term b = term_new(0, TAG_TEN, DT_FP32, 2);
  Term sum = uop_binary(UOP_ADD, a, b);
  CHECK_EQ(term_tag(sum), TAG_UOP);
  CHECK_EQ(term_ext(sum), UOP_ADD);
  CHECK_EQ(heap_read(term_val(sum) + 0), a);
  CHECK_EQ(heap_read(term_val(sum) + 1), b);

  TEST_BEGIN("uop/reduce-stores-kind-and-axis");
  Term r = uop_reduce(REDUCE_SUM, 0, a);
  CHECK_EQ(term_tag(r), TAG_UOP);
  CHECK_EQ(term_ext(r), UOP_REDUCE);
  // Heap layout: [src, NUM(kind), NUM(n_axes), NUM(axis_0), ...].
  Term kind_cell  = heap_read(term_val(r) + 1);
  Term naxes_cell = heap_read(term_val(r) + 2);
  Term axis_cell  = heap_read(term_val(r) + 3);
  CHECK_EQ(term_tag(kind_cell), TAG_NUM);
  CHECK_EQ(term_val(kind_cell), REDUCE_SUM);
  CHECK_EQ(term_tag(naxes_cell), TAG_NUM);
  CHECK_EQ(term_val(naxes_cell), 1);
  CHECK_EQ(term_tag(axis_cell), TAG_NUM);
  CHECK_EQ(term_val(axis_cell), 0);
  CHECK_EQ(uop_reduce_n_axes(r), 1);
  CHECK_EQ(uop_reduce_axis(r, 0), 0);

  TEST_BEGIN("uop/reduce-axes-is-the-innermost-first-chain");
  // Any order, repeats once: the hash-consed term of reducing 3, 2, 0 one
  // axis at a time, which chain-fuses into one REDUCE.
  Term x4      = term_new(0, TAG_TEN, DT_FP32, 3);
  u32  axes[4] = {2, 0, 3, 2};
  Term chain   = uop_reduce(REDUCE_SUM, 0,
                            uop_reduce(REDUCE_SUM, 2, uop_reduce(REDUCE_SUM, 3, x4)));
  Term ra      = uop_reduce_axes(REDUCE_SUM, 4, axes, x4);
  CHECK_EQ(ra, chain);
  CHECK_EQ(uop_reduce_n_axes(ra), 3);
  CHECK_EQ(uop_reduce_src(ra), x4);
  CHECK_EQ(uop_reduce_axes(REDUCE_SUM, 0, axes, x4), x4);

  TEST_BEGIN("uop/broadcast2-right-aligns-then-expands");
  // (3, 1) with (4,): a only expands; b gains a leading 1, then expands.
  u32  s31[2] = {3, 1}, s4[1] = {4}, s14[2] = {1, 4}, s34[2] = {3, 4}, bout[MAX_DIM];
  Term ba = term_new(0, TAG_TEN, DT_FP32, 4), bb = term_new(0, TAG_TEN, DT_FP32, 5);
  Term ga = ba, gb = bb;
  CHECK_EQ(uop_broadcast2(&ga, 2, s31, &gb, 1, s4, bout), 2);
  CHECK_EQ(bout[0], 3);
  CHECK_EQ(bout[1], 4);
  CHECK_EQ(ga, uop_expand(ba, 2, s34));
  CHECK_EQ(gb, uop_expand(uop_reshape(bb, 2, s14), 2, s34));
  u32 s3[1] = {3};
  ga = ba;
  gb = bb;
  CHECK_EQ(uop_broadcast2(&ga, 1, s3, &gb, 1, s4, bout), -1);

  TEST_BEGIN("uop/self-compare-folds-keep-the-shape");
  // tinygrad symbolic.py:139: x < x is a false CONST at x's shape.  x == x
  // stays a compare: NaN == NaN is false.
  Shape s34s   = {0};
  s34s.ndim    = 2;
  s34s.dims[0] = 3;
  s34s.dims[1] = 4;
  Term  xs     = term_new(0, TAG_TEN, DT_FP32, tensor_alloc(&CPU_BACKEND, s34s, DT_FP32));
  Term  lt     = uop_binary(UOP_CMPLT, xs, xs);
  Shape ls     = {0};
  CHECK(term_shape_in(lt, 0, &ls));
  CHECK_EQ(ls.ndim, 2);
  CHECK_EQ(ls.dims[0], 3);
  CHECK_EQ(ls.dims[1], 4);
  Term eq = uop_binary(UOP_CMPEQ, xs, xs);
  CHECK_EQ(term_tag(eq), TAG_UOP);
  CHECK_EQ(term_ext(eq), UOP_CMPEQ);

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
  CHECK_EQ(term_val(heap_read(term_val(p) + 1)), 2);
  for (u32 i = 0; i < 4; i++) {
    CHECK_EQ(term_val(heap_read(term_val(p) + 2 + i)), be[i]);
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

  thvm_free();
  TEST_REPORT();
}
