// test_materialize_inlined.c - exercise the standalone helper
// that emits ONE kernel for a realized UOp, inlining all
// elementwise upstream un-realized UOPs.

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 dim) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = dim;
  return tensor_alloc(CURRENT_BACKEND, s, DT_F32);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("inlined/chain-emits-single-kernel");
  // (a + b) * c -- MUL is the root (realized), ADD is the
  // single-consumer un-realized intermediate.  Helper should
  // build ONE kernel with inputs [a, b, c] and program
  // [ADD, MUL] (no LOAD prefix in the MVP).
  u32 ta = alloc_f32_tensor(4);
  u32 tb = alloc_f32_tensor(4);
  u32 tc = alloc_f32_tensor(4);
  Term a = term_new(0, TAG_TEN, DT_F32, ta);
  Term b = term_new(0, TAG_TEN, DT_F32, tb);
  Term c = term_new(0, TAG_TEN, DT_F32, tc);
  Term add_t = uop_binary(UOP_ADD, a, b);
  Term mul_t = uop_binary(UOP_MUL, add_t, c);
  realize_classify(mul_t);
  CHECK_EQ(realize_is_realized(mul_t), 1);
  CHECK_EQ(realize_is_realized(add_t), 0);

  Term k = materialize_kernel_inlined(mul_t);
  CHECK(k != 0);
  CHECK_EQ(term_tag(k), TAG_UOP);
  CHECK_EQ(term_ext(k), UOP_KERNEL);

  u32 kid = (u32)term_val(heap_read(term_val(k) + 1));
  KernelEntry *ke = &KERNELS[kid];
  CHECK_EQ(ke->n_inputs, 3);
  // 3 LOAD prefix + ADD + MUL = 5 ops total.
  CHECK_EQ(ke->n_ops,    5);
  CHECK_EQ(ke->program[0].opcode, UOP_LOAD);
  CHECK_EQ(ke->program[1].opcode, UOP_LOAD);
  CHECK_EQ(ke->program[2].opcode, UOP_LOAD);
  CHECK_EQ(ke->program[3].opcode, UOP_ADD);
  CHECK_EQ(ke->program[4].opcode, UOP_MUL);
  // ADD's srcs reference input slots 0 and 1.
  CHECK(KSRC_IS_INPUT(ke->program[3].src[0]));
  CHECK(KSRC_IS_INPUT(ke->program[3].src[1]));
  // MUL's first src is the program-index of ADD (3 after the
  // LOAD-prefix shift); second src is input slot 2 (c).
  CHECK_EQ(ke->program[4].src[0], 3u);
  CHECK(KSRC_IS_INPUT(ke->program[4].src[1]));
  CHECK_EQ(KSRC_INDEX(ke->program[4].src[1]), 2u);
  // Output buffer was allocated.
  CHECK(ke->output_tid != 0);
  CHECK_EQ(TENS[ke->output_tid].producer_kid, kid);

  TEST_BEGIN("inlined/dedups-shared-leaf");
  // x * x where x is a TEN.  Only ONE input slot for x; MUL
  // references slot 0 twice.  No upstream UOPs to inline.
  u32 tx = alloc_f32_tensor(4);
  Term x = term_new(0, TAG_TEN, DT_F32, tx);
  Term sq = uop_binary(UOP_MUL, x, x);
  realize_classify(sq);
  Term k2 = materialize_kernel_inlined(sq);
  CHECK(k2 != 0);
  u32 kid2 = (u32)term_val(heap_read(term_val(k2) + 1));
  KernelEntry *ke2 = &KERNELS[kid2];
  CHECK_EQ(ke2->n_inputs, 1);
  // 1 LOAD prefix + MUL = 2 ops.
  CHECK_EQ(ke2->n_ops,    2);
  CHECK_EQ(ke2->program[0].opcode, UOP_LOAD);
  CHECK_EQ(ke2->program[1].opcode, UOP_MUL);
  CHECK_EQ(ke2->program[1].src[0], KSRC_AS_INPUT(0u));
  CHECK_EQ(ke2->program[1].src[1], KSRC_AS_INPUT(0u));

  TEST_BEGIN("inlined/dedups-shared-uop-subexpr");
  // shared = a + b; sq = shared * shared (MUL of same UOp twice).
  // shared has 2 distinct child references but they're the same
  // Term -- after dedup, ONE inlined op (ADD) feeds both slots
  // of MUL.  realize_classify counts dedup as 1 consumer so
  // shared stays un-realized.
  Term shared = uop_binary(UOP_ADD, a, b);
  Term sqsh   = uop_binary(UOP_MUL, shared, shared);
  realize_classify(sqsh);
  CHECK_EQ(realize_is_realized(shared), 0);
  Term k3 = materialize_kernel_inlined(sqsh);
  CHECK(k3 != 0);
  u32 kid3 = (u32)term_val(heap_read(term_val(k3) + 1));
  KernelEntry *ke3 = &KERNELS[kid3];
  CHECK_EQ(ke3->n_inputs, 2);   // a, b
  // 2 LOAD prefix + ADD + MUL = 4 ops.
  CHECK_EQ(ke3->n_ops,    4);
  CHECK_EQ(ke3->program[0].opcode, UOP_LOAD);
  CHECK_EQ(ke3->program[1].opcode, UOP_LOAD);
  CHECK_EQ(ke3->program[2].opcode, UOP_ADD);
  CHECK_EQ(ke3->program[3].opcode, UOP_MUL);
  // MUL's two srcs both reference the ADD program slot
  // (program-index 2 after the LOAD-prefix shift).
  CHECK_EQ(ke3->program[3].src[0], 2u);
  CHECK_EQ(ke3->program[3].src[1], 2u);

  TEST_BEGIN("inlined/reduce-as-tail-collapses");
  // REDUCE root with elementwise source.  Helper builds ONE kernel
  // that runs ADD into a register and REDUCEs into the output.
  // Tinygrad's "local reduction" pattern.
  Term reduced = uop_reduce(REDUCE_SUM, 0, add_t);
  realize_classify(reduced);
  Term k4 = materialize_kernel_inlined(reduced);
  CHECK(k4 != 0);
  CHECK_EQ(term_tag(k4), TAG_UOP);
  CHECK_EQ(term_ext(k4), UOP_KERNEL);
  u32 kid4 = (u32)term_val(heap_read(term_val(k4) + 1));
  KernelEntry *ke4 = &KERNELS[kid4];
  CHECK_EQ(ke4->n_inputs, 2);                     // a, b
  // 2 LOAD prefix + ADD + REDUCE = 4 ops total.
  CHECK_EQ(ke4->n_ops,            4);
  CHECK_EQ(ke4->program[0].opcode, UOP_LOAD);
  CHECK_EQ(ke4->program[1].opcode, UOP_LOAD);
  CHECK_EQ(ke4->program[2].opcode, UOP_ADD);
  CHECK_EQ(ke4->program[3].opcode, UOP_REDUCE);
  // REDUCE reads ADD's result (program-index 2 after the LOAD shift).
  CHECK_EQ(ke4->program[3].src[0], 2u);
  // Output shape is the source shape with axis 0 dropped: input
  // a is shape {4}, axis=0, so output is {1}.
  CHECK(ke4->output_tid != 0);
  CHECK_EQ(TENS[ke4->output_tid].view.numel, 1u);

  TEST_BEGIN("inlined/non-elementwise-non-reduce-root-bails");
  // Movement op as root -- still bails.  Helper only accepts
  // elementwise + CONST + REDUCE-as-tail roots.
  u32 perm[1] = {0};
  Term reshaped = uop_reshape(add_t, 1, perm);   // shape {4} -> {4}
  realize_classify(reshaped);
  Term k5 = materialize_kernel_inlined(reshaped);
  CHECK_EQ(k5, 0u);

  thvm_free();
  TEST_REPORT();
}
