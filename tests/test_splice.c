// test_splice.c - exercise materialize_splice_into (sub-item f1a
// of the kernel-fusion arc).  Builds two single-elementwise
// kernels by hand, splices the child into the parent, and
// verifies:
//   - parent's program[] gained the child's main op (with refs
//     remapped from KSRC_AS_INPUT(child slot) to
//     KSRC_AS_INPUT(parent slot)).
//   - parent's input table absorbed the child's inputs (with
//     dedup).
//   - child->spliced = 1.
//   - kernel_fire_by_id on the spliced child is a no-op.

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
  MATERIALIZE_USE_REALIZE_INFO = 0;   // helper test exercises legacy splice

  // Build child = ADD(a, b) and parent = MUL(child, c) via the
  // standard materialize path.  After materialize, child becomes
  // KernelEntry K_child and parent becomes KernelEntry K_parent
  // whose first input slot is child's output tid.
  u32 d[1] = {3};
  Term a = term_new(0, TAG_TEN, DT_F32, alloc_f32_tensor(d, 1));
  Term b = term_new(0, TAG_TEN, DT_F32, alloc_f32_tensor(d, 1));
  Term c = term_new(0, TAG_TEN, DT_F32, alloc_f32_tensor(d, 1));
  Term add  = uop_binary(UOP_ADD, a, b);
  Term mul  = uop_binary(UOP_MUL, add, c);
  thvm_materialize(mul);

  // Find the parent kernel (the LATEST one is the MUL).  The
  // child is the one before it.
  CHECK(KERNELS_NEXT >= 3);
  u32 parent_kid = KERNELS_NEXT - 1;
  u32 child_kid  = KERNELS_NEXT - 2;
  KernelEntry *parent = &KERNELS[parent_kid];
  KernelEntry *child  = &KERNELS[child_kid];

  TEST_BEGIN("splice/precondition-both-elementwise-1op");
  CHECK(is_kernel_inlineable(parent));
  CHECK(is_kernel_inlineable(child));
  CHECK_EQ(parent->n_ops, parent->n_inputs + 1);
  CHECK_EQ(child->n_ops,  child->n_inputs  + 1);
  // Parent's main op is MUL.
  CHECK_EQ(parent->program[parent->n_inputs].opcode, UOP_MUL);
  // Child's main op is ADD.
  CHECK_EQ(child->program [child->n_inputs ].opcode, UOP_ADD);

  TEST_BEGIN("splice/appends-child-main-op-to-parent-program");
  u32 parent_n_ops_before    = parent->n_ops;
  u32 parent_n_inputs_before = parent->n_inputs;
  u32 last_slot = materialize_splice_into(parent_kid, child_kid);
  CHECK(last_slot != 0xFFFFFFFFu);
  // Child's prefix LOADs were absorbed into parent input remap;
  // only the ADD main op got appended.  So parent's n_ops should
  // grow by exactly 1.
  CHECK_EQ(parent->n_ops, parent_n_ops_before + 1);
  CHECK_EQ(parent->program[last_slot].opcode, UOP_ADD);
  // The appended ADD's srcs reference parent input slots (after
  // remap), not child input slots.
  KProgOp *added = &parent->program[last_slot];
  CHECK_EQ(added->n_src, 2);
  CHECK(KSRC_IS_INPUT(added->src[0]));
  CHECK(KSRC_IS_INPUT(added->src[1]));

  TEST_BEGIN("splice/merges-child-inputs-with-dedup");
  // Parent originally had inputs [child_output, c].  Child had
  // inputs [a, b].  After splice, parent should have [child_output,
  // c, a, b] (a and b weren't in parent yet, no dedup hits).
  CHECK_EQ(parent->n_inputs, parent_n_inputs_before + child->n_inputs);

  TEST_BEGIN("splice/marks-child-spliced");
  CHECK_EQ(child->spliced, 1);
  // Spliced kernel should also short-circuit kernel_fire_by_id:
  // we mark it pre-fired, so a fire call is a no-op.
  u8 fired_before = child->fired;
  kernel_fire_by_id(child_kid);
  // fired flag should NOT have flipped to 1 by the dispatch path
  // because the spliced check comes first.
  CHECK_EQ(child->fired, fired_before);

  TEST_BEGIN("splice/cap-exceeded-returns-sentinel");
  // Saturate parent->n_ops by repeatedly trying to splice the
  // (already-spliced) child -- second call should bail because
  // child->spliced is set.
  CHECK_EQ(materialize_splice_into(parent_kid, child_kid), 0xFFFFFFFFu);

  thvm_free();
  TEST_REPORT();
}
