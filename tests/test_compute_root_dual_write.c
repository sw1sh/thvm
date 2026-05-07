// test_compute_root_dual_write.c - Phase C slice 1.
//
// Verifies that thvm_materialize populates KernelEntry.compute_root
// alongside the legacy program[] / scalar_uops[] outputs.  This is the
// dual-write contract: program[] remains the primary source of truth
// for dispatch; compute_root is the new UOp-DAG representation that
// later slices will flip consumers onto (replacing the per-dispatch
// kernel_lift_to_uop call in cpu_jit_build / cg_emit_via_uop and
// ultimately deleting kernel_program_cache.c).
//
// What's checked here:
//   1. After materialize, at least one KernelEntry has compute_root != 0
//      AND term_tag(compute_root) == TAG_UOP AND term_ext == UOP_STORE.
//   2. compute_root structurally mirrors what kernel_lift_to_uop would
//      return on the same kernel (uop_mov_cache hash-cons makes equal
//      lifts produce equal Terms in the same session).
//   3. compute_root survives a manual gc_collect (gc_evacuate_side_tables
//      walks ke->compute_root).

#include "../src/thvm.c"
#include "test.h"

static u32 first_emitted_kernel_with_lift_success(u32 kernels_start) {
  // Walk kernels emitted since `kernels_start` and return the kid of
  // the first one whose compute_root is populated.  0 = none found.
  for (u32 k = kernels_start; k < KERNELS_NEXT; k++) {
    if (KERNELS[k].compute_root != 0) return k;
  }
  return 0;
}

int main(void) {
  // === unary NEG kernel on a TEN input ===
  // Materialize a UOP_NEG of a real backed tensor.  This forces a
  // real compute kernel through rangeify (a fold to identity isn't
  // possible because the input is a TEN value, not a const).
  TEST_BEGIN("compute-root/unary-neg-populated");
  unsetenv("THVM_BACKEND");
  thvm_init();
  Shape s1 = {0}; s1.ndim = 1; s1.dims[0] = 8;
  f32 src1[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
  u32 t1 = tensor_alloc(CURRENT_BACKEND, s1, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[t1].buf_id, src1, sizeof(src1));
  u32 kernels_before = KERNELS_NEXT;
  Term k1 = thvm_materialize(uop_unary(UOP_NEG,
      term_new(0, TAG_TEN, DT_FP32, t1)));
  (void)k1;
  // Find a kernel with compute_root populated.  At least one of the
  // kernels emitted by this materialize call must have it set.
  u32 kid_lift = first_emitted_kernel_with_lift_success(kernels_before);
  CHECK(kid_lift != 0);
  if (kid_lift != 0) {
    KernelEntry *ke = &KERNELS[kid_lift];
    Term root = ke->compute_root;
    CHECK_EQ(term_tag(root), (u64)TAG_UOP);
    CHECK_EQ(term_ext(root), (u64)UOP_STORE);
  }
  thvm_free();

  // === binary elementwise ADD kernel ===
  TEST_BEGIN("compute-root/binary-add-populated");
  unsetenv("THVM_BACKEND");
  thvm_init();
  Shape s = {0}; s.ndim = 1; s.dims[0] = 4;
  f32 src_a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 src_b[4] = {10.0f, 20.0f, 30.0f, 40.0f};
  u32 ta = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
  u32 tb = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[ta].buf_id, src_a, sizeof(src_a));
  CURRENT_BACKEND->buf_write(TENS[tb].buf_id, src_b, sizeof(src_b));
  u32 kernels_before2 = KERNELS_NEXT;
  Term add_k = thvm_materialize(uop_binary(UOP_ADD,
      term_new(0, TAG_TEN, DT_FP32, ta),
      term_new(0, TAG_TEN, DT_FP32, tb)));
  (void)add_k;
  u32 kid_add = first_emitted_kernel_with_lift_success(kernels_before2);
  CHECK(kid_add != 0);
  if (kid_add != 0) {
    KernelEntry *ke = &KERNELS[kid_add];
    Term root = ke->compute_root;
    CHECK_EQ(term_tag(root), (u64)TAG_UOP);
    CHECK_EQ(term_ext(root), (u64)UOP_STORE);
    // Dual-write contract: program[] still primary, compute_root
    // populated alongside.
    CHECK(ke->n_ops > 0);
  }
  thvm_free();

  // === parity with on-demand lift ===
  TEST_BEGIN("compute-root/matches-on-demand-lift");
  unsetenv("THVM_BACKEND");
  thvm_init();
  Shape s3 = {0}; s3.ndim = 1; s3.dims[0] = 8;
  f32 src3[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
  u32 t3 = tensor_alloc(CURRENT_BACKEND, s3, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[t3].buf_id, src3, sizeof(src3));
  u32 kernels_before3 = KERNELS_NEXT;
  Term k3 = thvm_materialize(uop_unary(UOP_NEG,
      term_new(0, TAG_TEN, DT_FP32, t3)));
  (void)k3;
  u32 kid3 = first_emitted_kernel_with_lift_success(kernels_before3);
  CHECK(kid3 != 0);
  if (kid3 != 0) {
    KernelEntry *ke = &KERNELS[kid3];
    Term stored = ke->compute_root;
    // Re-lift in the same session (uop_mov_cache survives unless
    // gc_collect ran; nothing in this test path triggers gc_collect
    // between materialize and this lift call).  The store_root from
    // the second lift should hash-cons to the same Term.
    KernelUopLift lift = {0};
    int ok = kernel_lift_to_uop(ke, &lift);
    CHECK(ok != 0);
    if (ok) {
      CHECK_EQ(stored, lift.store_root);
    }
  }
  thvm_free();

  // === gc_collect evacuates compute_root ===
  TEST_BEGIN("compute-root/survives-gc-collect");
  unsetenv("THVM_BACKEND");
  thvm_init();
  Shape s4 = {0}; s4.ndim = 1; s4.dims[0] = 8;
  f32 src4[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
  u32 t4 = tensor_alloc(CURRENT_BACKEND, s4, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[t4].buf_id, src4, sizeof(src4));
  u32 kernels_before4 = KERNELS_NEXT;
  Term k4 = thvm_materialize(uop_unary(UOP_NEG,
      term_new(0, TAG_TEN, DT_FP32, t4)));
  (void)k4;
  u32 kid4 = first_emitted_kernel_with_lift_success(kernels_before4);
  CHECK(kid4 != 0);
  if (kid4 != 0) {
    Term before_gc = KERNELS[kid4].compute_root;
    // Force a collection.  gc_evacuate_side_tables MUST evacuate
    // ke->compute_root or the field would dangle into from-space and
    // any subsequent heap_read would either return stale data or
    // index off the end of HEAP.
    Term root_arr[1] = { k4 };
    gc_collect(root_arr, 1);
    Term after_gc = KERNELS[kid4].compute_root;
    // Post-gc compute_root must still be a valid UOP_STORE.  The
    // exact heap loc may have moved (Cheney semi-space) so we don't
    // assert before_gc == after_gc; we assert structural validity.
    (void)before_gc;
    CHECK(after_gc != 0);
    CHECK_EQ(term_tag(after_gc), (u64)TAG_UOP);
    CHECK_EQ(term_ext(after_gc), (u64)UOP_STORE);
  }
  thvm_free();

  TEST_REPORT();
}
