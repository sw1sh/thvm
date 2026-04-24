// test_materialize.c - schedule + kernelize + linearize pipeline.
//
// Covers the pre-dispatch half of step 12: verify that running
// `thvm_materialize` on a raw UOp graph emits the expected tree of
// UOP_KERNEL terms with correctly populated KernelEntry side-table
// entries.  Actual firing (interact_kernel) lives in commit 4.

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) s.dims[i] = dims[i];
  return tensor_alloc(CURRENT_BACKEND, s, DT_F32);
}

int main(void) {
  thvm_init();

  TEST_BEGIN("materialize/single-elementwise-emits-one-kernel");
  u32 one[1] = {4};
  u32 ta = alloc_f32_tensor(one, 1);
  u32 tb = alloc_f32_tensor(one, 1);
  Term a = term_new(0, TAG_TEN, DT_F32, ta);
  Term b = term_new(0, TAG_TEN, DT_F32, tb);
  Term sum = uop_binary(UOP_ADD, a, b);
  Term res = thvm_materialize(sum);
  CHECK_EQ(term_tag(res), TAG_UOP);
  CHECK_EQ(term_ext(res), UOP_KERNEL);
  Term out_buf  = heap_read(term_val(res) + 0);
  Term kid_num  = heap_read(term_val(res) + 1);
  CHECK_EQ(term_tag(out_buf), TAG_TEN);
  CHECK_EQ(term_tag(kid_num), TAG_NUM);
  u32 kid = (u32)term_val(kid_num);
  KernelEntry *ke = &KERNELS[kid];
  CHECK_EQ(ke->n_inputs, 2);
  CHECK_EQ(ke->n_ops,    1);
  CHECK_EQ(ke->program[0].opcode, UOP_ADD);
  CHECK_EQ(ke->program[0].n_src,  2);
  CHECK(KSRC_IS_INPUT(ke->program[0].src[0]));
  CHECK(KSRC_IS_INPUT(ke->program[0].src[1]));
  CHECK_EQ(ke->output_numel, 4);

  TEST_BEGIN("materialize/compound-recurses-bottom-up");
  // (a + b) * c -- should produce three kernels: one per op.
  u32 tc = alloc_f32_tensor(one, 1);
  Term c = term_new(0, TAG_TEN, DT_F32, tc);
  Term add = uop_binary(UOP_ADD, a, b);
  Term mul = uop_binary(UOP_MUL, add, c);
  u32 prev_kernels = KERNELS_NEXT;
  Term topk = thvm_materialize(mul);
  CHECK_EQ(term_ext(topk), UOP_KERNEL);
  CHECK_EQ(KERNELS_NEXT - prev_kernels, 2);  // ADD + MUL (a/b/c already TENs)
  // The top kernel's inputs: one is the ADD kernel's output, other is c.
  u32 top_kid = (u32)term_val(heap_read(term_val(topk) + 1));
  KernelEntry *topke = &KERNELS[top_kid];
  CHECK_EQ(topke->program[0].opcode, UOP_MUL);
  CHECK_EQ(topke->n_inputs, 2);

  TEST_BEGIN("materialize/duplicate-input-deduplicated");
  // a + a -- one input, two src entries referencing the same slot.
  Term dup = uop_binary(UOP_ADD, a, a);
  Term dupk = thvm_materialize(dup);
  u32 dup_kid = (u32)term_val(heap_read(term_val(dupk) + 1));
  KernelEntry *dupke = &KERNELS[dup_kid];
  CHECK_EQ(dupke->n_inputs, 1);
  CHECK_EQ(dupke->program[0].src[0], dupke->program[0].src[1]);

  TEST_BEGIN("materialize/unary-single-source");
  Term neg = uop_unary(UOP_NEG, a);
  Term negk = thvm_materialize(neg);
  u32 neg_kid = (u32)term_val(heap_read(term_val(negk) + 1));
  KernelEntry *negke = &KERNELS[neg_kid];
  CHECK_EQ(negke->n_inputs, 1);
  CHECK_EQ(negke->n_ops,    1);
  CHECK_EQ(negke->program[0].opcode, UOP_NEG);
  CHECK_EQ(negke->program[0].n_src,  1);

  TEST_BEGIN("materialize/const-zero-inputs");
  f32 two_f = 2.0f;
  u32 bits; memcpy(&bits, &two_f, sizeof(bits));
  Term ck = thvm_materialize(uop_const(DT_F32, bits));
  u32 c_kid = (u32)term_val(heap_read(term_val(ck) + 1));
  KernelEntry *cke = &KERNELS[c_kid];
  CHECK_EQ(cke->n_inputs, 0);
  CHECK_EQ(cke->program[0].opcode, UOP_CONST);
  CHECK_EQ(cke->program[0].arg,    bits);

  TEST_BEGIN("materialize/unwraps-UOP_MATERIALIZE");
  Term wrapped = uop_materialize(uop_binary(UOP_ADD, a, b));
  Term result  = thvm_materialize(wrapped);
  CHECK_EQ(term_tag(result), TAG_UOP);
  CHECK_EQ(term_ext(result), UOP_KERNEL);

  TEST_BEGIN("materialize/tag_ten-passes-through");
  CHECK_EQ(thvm_materialize(a), a);

  thvm_free();
  TEST_REPORT();
}
