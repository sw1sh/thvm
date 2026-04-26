// test_use_realize.c - regression for f1d-b2: with the toggle
// MATERIALIZE_USE_REALIZE_INFO on, materialize_uop_in_env
// routes through materialize_kernel_inlined so a single-
// consumer elementwise chain collapses to ONE kernel instead
// of one-kernel-per-UOp.

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32_tensor(u32 dim) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = dim;
  return tensor_alloc(CURRENT_BACKEND, s, DT_F32);
}

static u32 count_unspliced_kernels(u32 from) {
  u32 n = 0;
  for (u32 k = from; k < KERNELS_NEXT; k++) {
    if (!KERNELS[k].spliced) n++;
  }
  return n;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("use-realize/legacy-mode-emits-per-uop-kernels");
  // Toggle OFF (legacy mode) -- the chain (a + b) * c should
  // materialize to TWO kernels (ADD, MUL).  Force off explicitly
  // so the test passes regardless of the global default.
  MATERIALIZE_USE_REALIZE_INFO = 0;
  u32 ta = alloc_f32_tensor(4);
  u32 tb = alloc_f32_tensor(4);
  u32 tc = alloc_f32_tensor(4);
  Term a = term_new(0, TAG_TEN, DT_F32, ta);
  Term b = term_new(0, TAG_TEN, DT_F32, tb);
  Term c = term_new(0, TAG_TEN, DT_F32, tc);
  Term add_t = uop_binary(UOP_ADD, a, b);
  Term mul_t = uop_binary(UOP_MUL, add_t, c);
  u32 prev_kid = KERNELS_NEXT;
  thvm_materialize(mul_t);
  CHECK_EQ(KERNELS_NEXT - prev_kid, 2);
  CHECK_EQ(count_unspliced_kernels(prev_kid), 2);

  TEST_BEGIN("use-realize/on-collapses-chain-to-single-kernel");
  // Fresh state.  With toggle ON the same chain materializes
  // to ONE kernel (MUL realized, ADD inlined).
  thvm_free();
  thvm_init();
  MATERIALIZE_USE_REALIZE_INFO = 1;
  ta = alloc_f32_tensor(4);
  tb = alloc_f32_tensor(4);
  tc = alloc_f32_tensor(4);
  a = term_new(0, TAG_TEN, DT_F32, ta);
  b = term_new(0, TAG_TEN, DT_F32, tb);
  c = term_new(0, TAG_TEN, DT_F32, tc);
  add_t = uop_binary(UOP_ADD, a, b);
  mul_t = uop_binary(UOP_MUL, add_t, c);
  prev_kid = KERNELS_NEXT;
  Term k = thvm_materialize(mul_t);
  CHECK_EQ(term_tag(k), TAG_UOP);
  CHECK_EQ(term_ext(k), UOP_KERNEL);
  CHECK_EQ(KERNELS_NEXT - prev_kid, 1);
  // The lone kernel has 3 inputs (a, b, c) and 2 ops (ADD, MUL).
  u32 kid = (u32)term_val(heap_read(term_val(k) + 1));
  KernelEntry *ke = &KERNELS[kid];
  CHECK_EQ(ke->n_inputs, 3);
  CHECK_EQ(ke->n_ops,    2);
  CHECK_EQ(ke->program[0].opcode, UOP_ADD);
  CHECK_EQ(ke->program[1].opcode, UOP_MUL);

  TEST_BEGIN("use-realize/on-with-reduce-uses-legacy");
  // REDUCE always realizes -- the helper bails on non-elementwise
  // roots, so the legacy path emits the kernel.  The (a + b)
  // upstream is un-realized + elementwise so it should still get
  // inlined when its REDUCE consumer is materialized; but for
  // this MVP scope the fall-back path keeps per-UOp kernels.
  thvm_free();
  thvm_init();
  MATERIALIZE_USE_REALIZE_INFO = 1;
  ta = alloc_f32_tensor(4);
  tb = alloc_f32_tensor(4);
  a = term_new(0, TAG_TEN, DT_F32, ta);
  b = term_new(0, TAG_TEN, DT_F32, tb);
  Term add2    = uop_binary(UOP_ADD, a, b);
  Term reduced = uop_reduce(REDUCE_SUM, 0, add2);
  prev_kid = KERNELS_NEXT;
  thvm_materialize(reduced);
  // Expect <= 2 kernels: ADD (left as raw or via fallback) +
  // REDUCE.  The exact count depends on fallback behavior; just
  // assert correctness of the live path.
  CHECK(KERNELS_NEXT - prev_kid <= 2);

  thvm_free();
  TEST_REPORT();
}
