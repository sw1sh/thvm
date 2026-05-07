// test_compute_root_dual_write.c - Phase C slices 1 + 2.
//
// Verifies that thvm_materialize populates KernelEntry.compute_root
// AND KernelEntry.cached_lift alongside the legacy program[] /
// scalar_uops[] outputs.  This is the dual-write contract: program[]
// remains the primary source of truth for dispatch; compute_root and
// cached_lift are the new UOp-DAG representation that later slices
// will flip consumers onto (replacing the per-dispatch
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
//   4. (Slice 2) cached_lift.store_root == compute_root, and
//      cached_lift.{out_buf, in_bufs[], n_inputs} match a fresh
//      kernel_lift_to_uop call by Term equality.  Both must survive
//      gc_collect: the renderer's identity-based buffer-name resolution
//      (rmu_buf_names_set) compares Terms across the in_bufs[] handle
//      so a stale cached value would mis-name buffers post-collection.

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

  // === slice 2: cached_lift mirrors a fresh lift ===
  // Verifies that the cached KernelUopLift on KernelEntry has the
  // same store_root / out_buf / in_bufs[] / n_inputs as a fresh
  // kernel_lift_to_uop call would produce.  Consumers
  // (cpu_jit_build, cg_emit_via_uop, cpu_uop_walk) read cached_lift
  // directly so this equality is the dispatch-time contract.
  TEST_BEGIN("cached-lift/matches-fresh-lift");
  unsetenv("THVM_BACKEND");
  thvm_init();
  Shape s_cl = {0}; s_cl.ndim = 1; s_cl.dims[0] = 4;
  f32 src_cl_a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 src_cl_b[4] = {10.0f, 20.0f, 30.0f, 40.0f};
  u32 ta_cl = tensor_alloc(CURRENT_BACKEND, s_cl, DT_FP32);
  u32 tb_cl = tensor_alloc(CURRENT_BACKEND, s_cl, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[ta_cl].buf_id, src_cl_a, sizeof(src_cl_a));
  CURRENT_BACKEND->buf_write(TENS[tb_cl].buf_id, src_cl_b, sizeof(src_cl_b));
  u32 kernels_before_cl = KERNELS_NEXT;
  Term add_cl = thvm_materialize(uop_binary(UOP_ADD,
      term_new(0, TAG_TEN, DT_FP32, ta_cl),
      term_new(0, TAG_TEN, DT_FP32, tb_cl)));
  (void)add_cl;
  u32 kid_cl = first_emitted_kernel_with_lift_success(kernels_before_cl);
  CHECK(kid_cl != 0);
  if (kid_cl != 0) {
    KernelEntry *ke = &KERNELS[kid_cl];
    // compute_root is a redundant view of cached_lift.store_root.
    CHECK_EQ(ke->compute_root, ke->cached_lift.store_root);
    // Fresh lift to compare against.  Hash-cons ensures structural
    // equality of every Term-typed field.
    KernelUopLift fresh = {0};
    int ok = kernel_lift_to_uop(ke, &fresh);
    CHECK(ok != 0);
    if (ok) {
      CHECK_EQ(ke->cached_lift.store_root, fresh.store_root);
      CHECK_EQ(ke->cached_lift.out_buf,    fresh.out_buf);
      CHECK_EQ(ke->cached_lift.n_inputs,   fresh.n_inputs);
      for (u32 i = 0; i < fresh.n_inputs; i++) {
        CHECK_EQ(ke->cached_lift.in_bufs[i], fresh.in_bufs[i]);
      }
      // n_inputs is at least 1 for the binary ADD (each TEN input
      // becomes a UOP_BUFFER leaf the lifter de-dups by tid).
      CHECK(fresh.n_inputs >= 1);
    }
  }
  thvm_free();

  // === slice 2: cached_lift survives gc_collect ===
  TEST_BEGIN("cached-lift/survives-gc-collect");
  unsetenv("THVM_BACKEND");
  thvm_init();
  Shape s_clg = {0}; s_clg.ndim = 1; s_clg.dims[0] = 4;
  f32 src_clg_a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 src_clg_b[4] = {10.0f, 20.0f, 30.0f, 40.0f};
  u32 ta_clg = tensor_alloc(CURRENT_BACKEND, s_clg, DT_FP32);
  u32 tb_clg = tensor_alloc(CURRENT_BACKEND, s_clg, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[ta_clg].buf_id, src_clg_a, sizeof(src_clg_a));
  CURRENT_BACKEND->buf_write(TENS[tb_clg].buf_id, src_clg_b, sizeof(src_clg_b));
  u32 kernels_before_clg = KERNELS_NEXT;
  Term add_clg = thvm_materialize(uop_binary(UOP_ADD,
      term_new(0, TAG_TEN, DT_FP32, ta_clg),
      term_new(0, TAG_TEN, DT_FP32, tb_clg)));
  u32 kid_clg = first_emitted_kernel_with_lift_success(kernels_before_clg);
  CHECK(kid_clg != 0);
  if (kid_clg != 0) {
    u32 n_in_before = KERNELS[kid_clg].cached_lift.n_inputs;
    Term root_arr[1] = { add_clg };
    gc_collect(root_arr, 1);
    KernelEntry *ke = &KERNELS[kid_clg];
    // n_inputs is plain u32, not evacuated -- it must stay equal.
    CHECK_EQ(ke->cached_lift.n_inputs, n_in_before);
    // store_root / out_buf / each in_bufs[i] must be valid Terms post-gc.
    // (heap loc may have moved; we don't assert pre==post.)
    CHECK(ke->cached_lift.store_root != 0);
    CHECK_EQ(term_tag(ke->cached_lift.store_root), (u64)TAG_UOP);
    CHECK_EQ(term_ext(ke->cached_lift.store_root), (u64)UOP_STORE);
    CHECK(ke->cached_lift.out_buf != 0);
    CHECK_EQ(term_tag(ke->cached_lift.out_buf), (u64)TAG_UOP);
    CHECK_EQ(term_ext(ke->cached_lift.out_buf), (u64)UOP_BUFFER);
    for (u32 i = 0; i < ke->cached_lift.n_inputs; i++) {
      CHECK(ke->cached_lift.in_bufs[i] != 0);
      // Each in_bufs[i] is either a UOP_BUFFER (TEN input) or a
      // UOP_NUM (const input lifted to a literal); both have valid
      // tags after evacuation.
      u64 tag = term_tag(ke->cached_lift.in_bufs[i]);
      CHECK(tag == (u64)TAG_UOP || tag == (u64)TAG_NUM);
    }
    // After gc, a fresh lift on the same kernel should still match
    // (uop_mov_cache may or may not survive depending on its own gc
    // story; the relevant invariant is that cached_lift's Terms are
    // structurally valid after evacuation).
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
