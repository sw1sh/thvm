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
    // Slices 1+2 dual-write contract: program[] populated alongside
    // compute_root.  Phase C slice 7 (THVM_PHASE_C7_FREE_PROGRAM=1)
    // nulls program[] post-lift; in that mode `n_ops == 0` and the
    // assertion would be inverted.  DEFAULT is dual-write (OFF) since
    // the materialize-regression fix -- only =1 frees program[].
    char const *free_e = getenv("THVM_PHASE_C7_FREE_PROGRAM");
    int free_on = (free_e != NULL) && (free_e[0] == '1');
    if (!free_on) CHECK(ke->n_ops > 0);
    else          CHECK_EQ(ke->n_ops, 0u);
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

  // === slice 3: structural-mode renderer matches legacy ===
  // Verifies that cg_render_uop_kernel_root (Phase C slice 3) emits
  // bit-equal MSL when invoked with ke->compute_root directly,
  // compared with the legacy cg_render_uop_kernel(out_buf, in_bufs[])
  // entry given the cached_lift's tuple.  rmu_buf_name resolves
  // structurally via UOP_BUFFER.instance for inputs, so the renderer
  // no longer needs in_bufs[] to land on the correct names.
  TEST_BEGIN("slice3/structural-renderer-bit-equal");
  unsetenv("THVM_BACKEND");
  thvm_init();
  Shape s_s3 = {0}; s_s3.ndim = 1; s_s3.dims[0] = 4;
  f32 src_s3a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 src_s3b[4] = {10.0f, 20.0f, 30.0f, 40.0f};
  u32 ta_s3 = tensor_alloc(CURRENT_BACKEND, s_s3, DT_FP32);
  u32 tb_s3 = tensor_alloc(CURRENT_BACKEND, s_s3, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[ta_s3].buf_id, src_s3a, sizeof(src_s3a));
  CURRENT_BACKEND->buf_write(TENS[tb_s3].buf_id, src_s3b, sizeof(src_s3b));
  u32 kernels_before_s3 = KERNELS_NEXT;
  Term add_s3 = thvm_materialize(uop_binary(UOP_ADD,
      term_new(0, TAG_TEN, DT_FP32, ta_s3),
      term_new(0, TAG_TEN, DT_FP32, tb_s3)));
  (void)add_s3;
  u32 kid_s3 = first_emitted_kernel_with_lift_success(kernels_before_s3);
  CHECK(kid_s3 != 0);
  if (kid_s3 != 0) {
    KernelEntry *ke = &KERNELS[kid_s3];
    Term root = ke->compute_root;
    CHECK(root != 0);
    // Render via the legacy entry (out_buf + in_bufs[] from
    // cached_lift) and via the new structural entry (root only).
    char buf_legacy[16384];
    char buf_struct[16384];
    FILE *fp_l = fmemopen(buf_legacy, sizeof(buf_legacy), "w");
    FILE *fp_s = fmemopen(buf_struct, sizeof(buf_struct), "w");
    CHECK(fp_l != NULL);
    CHECK(fp_s != NULL);
    cg_render_uop_kernel(root, "k",
                         ke->cached_lift.out_buf,
                         ke->cached_lift.in_bufs,
                         ke->cached_lift.n_inputs, fp_l);
    cg_render_uop_kernel_root(root, "k", fp_s);
    fclose(fp_l);
    fclose(fp_s);
    // Bit-equality: every byte of the rendered MSL must match.
    CHECK(strcmp(buf_legacy, buf_struct) == 0);
    // C99 path (CPU JIT) parity too.
    char buf_legacy_c[16384];
    char buf_struct_c[16384];
    FILE *fp_lc = fmemopen(buf_legacy_c, sizeof(buf_legacy_c), "w");
    FILE *fp_sc = fmemopen(buf_struct_c, sizeof(buf_struct_c), "w");
    CHECK(fp_lc != NULL);
    CHECK(fp_sc != NULL);
    cg_render_uop_kernel_c(root, "k",
                           ke->cached_lift.out_buf,
                           ke->cached_lift.in_bufs,
                           ke->cached_lift.n_inputs, fp_lc);
    cg_render_uop_kernel_c_root(root, "k", fp_sc);
    fclose(fp_lc);
    fclose(fp_sc);
    CHECK(strcmp(buf_legacy_c, buf_struct_c) == 0);
    // Sanity: the rendered MSL references "in0" and "in1" and "out"
    // (proves the structural slot decode landed on the lifter's
    // intended names).
    CHECK(strstr(buf_struct, "in0") != NULL);
    CHECK(strstr(buf_struct, "in1") != NULL);
    CHECK(strstr(buf_struct, "out") != NULL);
  }
  thvm_free();

  // === slice 3: structural renderer survives re-lift ===
  // The pre-slice-3 renderer required the caller to pass
  // in_bufs[] / out_buf matching the Term identities embedded in
  // root by Term equality.  Slice 3 makes naming structural via
  // UOP_BUFFER.instance, so even if a caller invoked the renderer
  // with a fresh-lift root and stale in_bufs[] from an earlier
  // session, the structural entry still emits the same kernel.  We
  // simulate this by re-lifting and verifying bit-equality of the
  // structural output between the cached and freshly-lifted roots
  // (both should hash-cons to the same Term in the same session,
  // but the test makes the invariant explicit).
  TEST_BEGIN("slice3/structural-renderer-relift-stable");
  unsetenv("THVM_BACKEND");
  thvm_init();
  Shape s_rl = {0}; s_rl.ndim = 1; s_rl.dims[0] = 8;
  f32 src_rl[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
  u32 t_rl = tensor_alloc(CURRENT_BACKEND, s_rl, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[t_rl].buf_id, src_rl, sizeof(src_rl));
  u32 kernels_before_rl = KERNELS_NEXT;
  Term k_rl = thvm_materialize(uop_unary(UOP_NEG,
      term_new(0, TAG_TEN, DT_FP32, t_rl)));
  (void)k_rl;
  u32 kid_rl = first_emitted_kernel_with_lift_success(kernels_before_rl);
  CHECK(kid_rl != 0);
  if (kid_rl != 0) {
    KernelEntry *ke = &KERNELS[kid_rl];
    Term cached_root = ke->compute_root;
    KernelUopLift fresh = {0};
    int ok = kernel_lift_to_uop(ke, &fresh);
    CHECK(ok != 0);
    char buf_a[16384];
    char buf_b[16384];
    FILE *fp_a = fmemopen(buf_a, sizeof(buf_a), "w");
    FILE *fp_b = fmemopen(buf_b, sizeof(buf_b), "w");
    cg_render_uop_kernel_root(cached_root, "k", fp_a);
    cg_render_uop_kernel_root(fresh.store_root, "k", fp_b);
    fclose(fp_a);
    fclose(fp_b);
    CHECK(strcmp(buf_a, buf_b) == 0);
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

  // === slice 4: dag-scan dtype-uniform agrees with program[] ===
  // Phase C slice 4 introduces uop_dag_dtype_uniform, the DAG-side
  // mirror of metal_kernel_supported's per-op dtype check.  For a
  // homogeneous-FP32 add kernel, both the legacy KProgOp walk and
  // the new UOp DAG walk must agree (uniform), AND the DAG walk
  // must reject a probe with a different dtype.
  TEST_BEGIN("slice4/dag-dtype-uniform-fp32");
  unsetenv("THVM_BACKEND");
  thvm_init();
  Shape s_du = {0}; s_du.ndim = 1; s_du.dims[0] = 4;
  f32 src_du_a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 src_du_b[4] = {10.0f, 20.0f, 30.0f, 40.0f};
  u32 ta_du = tensor_alloc(CURRENT_BACKEND, s_du, DT_FP32);
  u32 tb_du = tensor_alloc(CURRENT_BACKEND, s_du, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[ta_du].buf_id, src_du_a, sizeof(src_du_a));
  CURRENT_BACKEND->buf_write(TENS[tb_du].buf_id, src_du_b, sizeof(src_du_b));
  u32 kernels_before_du = KERNELS_NEXT;
  Term add_du = thvm_materialize(uop_binary(UOP_ADD,
      term_new(0, TAG_TEN, DT_FP32, ta_du),
      term_new(0, TAG_TEN, DT_FP32, tb_du)));
  (void)add_du;
  u32 kid_du = first_emitted_kernel_with_lift_success(kernels_before_du);
  CHECK(kid_du != 0);
  if (kid_du != 0) {
    KernelEntry *ke = &KERNELS[kid_du];
    // Legacy: walk program[] and confirm uniform-fp32.  Phase C
    // slice 7 (THVM_PHASE_C7_FREE_PROGRAM=1) nulls program[] post-
    // lift; the legacy walk vacuously holds (n_ops == 0).  We assert
    // uniformity via the DAG walker below regardless of the knob.
    char const *free_e_du = getenv("THVM_PHASE_C7_FREE_PROGRAM");
    int free_on_du = (free_e_du != NULL) && (free_e_du[0] == '1');
    int legacy_uniform = free_on_du ? 1 : (ke->n_ops > 0);
    if (!free_on_du && legacy_uniform) {
      u32 dt = ke->program[0].dtype;
      CHECK_EQ(dt, (u64)DT_FP32);
      for (u32 i = 0; i < ke->n_ops; i++) {
        if (ke->program[i].dtype != dt) { legacy_uniform = 0; break; }
      }
    }
    CHECK(legacy_uniform);
    // DAG-side: cached_lift must be populated, and the DAG walker
    // must agree.
    CHECK(ke->cached_lift.store_root != 0);
    CHECK(uop_dag_dtype_uniform(ke->cached_lift.store_root, DT_FP32));
    // Negative probe: the same DAG should NOT be uniform under
    // DT_INT32 (every BUFFER carries DT_FP32).
    CHECK(!uop_dag_dtype_uniform(ke->cached_lift.store_root, DT_INT32));
    // Empty root is trivially uniform (caller's `root != 0` gate).
    CHECK(uop_dag_dtype_uniform(0, DT_FP32));
  }
  thvm_free();

  // === slice 4: dag-scan reduce-axis-extent agrees with program[] ===
  // Materialise a tail-REDUCE kernel and verify both
  // propose_kprog_reduce_axis_size (legacy) and
  // uop_dag_reduce_axis_extent (new) report the same extent.
  TEST_BEGIN("slice4/dag-reduce-axis-extent");
  unsetenv("THVM_BACKEND");
  thvm_init();
  Shape s_re = {0}; s_re.ndim = 1; s_re.dims[0] = 16;
  f32 src_re[16];
  for (u32 i = 0; i < 16; i++) src_re[i] = (f32)i;
  u32 t_re = tensor_alloc(CURRENT_BACKEND, s_re, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[t_re].buf_id, src_re, sizeof(src_re));
  u32 kernels_before_re = KERNELS_NEXT;
  Term sum_re = thvm_materialize(uop_reduce(
      REDUCE_SUM, 0, term_new(0, TAG_TEN, DT_FP32, t_re)));
  (void)sum_re;
  // Find a kernel with cached_lift populated AND a reduce in the
  // lifted DAG.  Reduce-tail kernels typically have the lift succeed.
  u32 kid_re = 0;
  for (u32 k = kernels_before_re; k < KERNELS_NEXT; k++) {
    if (KERNELS[k].cached_lift.store_root != 0
        && uop_dag_reduce_axis_extent(KERNELS[k].cached_lift.store_root) != 0) {
      kid_re = k; break;
    }
  }
  CHECK(kid_re != 0);
  if (kid_re != 0) {
    KernelEntry *ke = &KERNELS[kid_re];
    u32 dag_extent = uop_dag_reduce_axis_extent(ke->cached_lift.store_root);
    CHECK(dag_extent != 0);
    // The lifter exposes the reduce extent as the source-numel /
    // output-numel ratio, mirroring propose_kprog_reduce_axis_size.
    if (ke->n_ops > 0
        && ke->program[ke->n_ops - 1].opcode == UOP_REDUCE) {
      KProgOp const *rd = &ke->program[ke->n_ops - 1];
      u32 src_numel = KSRC_IS_INPUT(rd->src[0])
          ? ke->input_numels[KSRC_INDEX(rd->src[0])]
          : ke->program[KSRC_INDEX(rd->src[0])].numel;
      u32 out_numel = ke->output_numel ? ke->output_numel : 1;
      u32 kprog_extent = src_numel / out_numel;
      CHECK_EQ((u64)dag_extent, (u64)kprog_extent);
    }
  }
  thvm_free();

  // === slice 4: dag-scan reduce-unroll-kernel agrees with program[] ===
  // The lifted-add-then-reduce DAG should pass
  // uop_dag_is_reduce_unroll_kernel (mirrors propose_metal_reduce
  // _unroll_kernel's KProgOp gate); a pure-elementwise (no REDUCE)
  // DAG should fail it.
  TEST_BEGIN("slice4/dag-reduce-unroll-kernel-gate");
  unsetenv("THVM_BACKEND");
  thvm_init();
  // Reduce kernel: should pass.
  Shape s_ur = {0}; s_ur.ndim = 1; s_ur.dims[0] = 16;
  f32 src_ur[16];
  for (u32 i = 0; i < 16; i++) src_ur[i] = (f32)(i + 1);
  u32 t_ur = tensor_alloc(CURRENT_BACKEND, s_ur, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[t_ur].buf_id, src_ur, sizeof(src_ur));
  u32 kernels_before_ur = KERNELS_NEXT;
  Term sum_ur = thvm_materialize(uop_reduce(
      REDUCE_SUM, 0, term_new(0, TAG_TEN, DT_FP32, t_ur)));
  (void)sum_ur;
  u32 kid_ur = 0;
  for (u32 k = kernels_before_ur; k < KERNELS_NEXT; k++) {
    if (KERNELS[k].cached_lift.store_root != 0
        && uop_dag_is_reduce_unroll_kernel(
              KERNELS[k].cached_lift.store_root)) {
      kid_ur = k; break;
    }
  }
  CHECK(kid_ur != 0);
  // Pure-elementwise NEG kernel: should NOT pass (no REDUCE).
  Shape s_uel = {0}; s_uel.ndim = 1; s_uel.dims[0] = 4;
  f32 src_uel[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  u32 t_uel = tensor_alloc(CURRENT_BACKEND, s_uel, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[t_uel].buf_id, src_uel, sizeof(src_uel));
  u32 kernels_before_uel = KERNELS_NEXT;
  Term neg_uel = thvm_materialize(uop_unary(UOP_NEG,
      term_new(0, TAG_TEN, DT_FP32, t_uel)));
  (void)neg_uel;
  u32 kid_uel = first_emitted_kernel_with_lift_success(kernels_before_uel);
  CHECK(kid_uel != 0);
  if (kid_uel != 0) {
    KernelEntry *ke = &KERNELS[kid_uel];
    CHECK(!uop_dag_is_reduce_unroll_kernel(ke->cached_lift.store_root));
  }
  thvm_free();

  // === slice 7: single-write -- lift-eligible kernels have program == NULL ===
  // Phase C slice 7 adds THVM_PHASE_C7_FREE_PROGRAM=1 -- when set,
  // materialize.c frees ke->program[] post-lift and zeroes n_ops.
  // The dispatch ladder (cpu_jit_dispatch + cpu_uop_walk + the
  // metal DAG-side encoder) reads from ke->cached_lift.store_root,
  // so end-to-end forward passes must keep producing correct output.
  // This test exercises both halves of the contract: post-materialize
  // structural state AND end-to-end dispatch numerical correctness.
  TEST_BEGIN("slice7/free-program-nulls-program-and-dispatches");
  setenv("THVM_PHASE_C7_FREE_PROGRAM", "1", 1);
  unsetenv("THVM_BACKEND");
  thvm_init();
  Shape s_s7 = {0}; s_s7.ndim = 1; s_s7.dims[0] = 4;
  f32 src_s7a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 src_s7b[4] = {10.0f, 20.0f, 30.0f, 40.0f};
  u32 ta_s7 = tensor_alloc(CURRENT_BACKEND, s_s7, DT_FP32);
  u32 tb_s7 = tensor_alloc(CURRENT_BACKEND, s_s7, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[ta_s7].buf_id, src_s7a, sizeof(src_s7a));
  CURRENT_BACKEND->buf_write(TENS[tb_s7].buf_id, src_s7b, sizeof(src_s7b));
  u32 kernels_before_s7 = KERNELS_NEXT;
  Term add_s7 = thvm_materialize(uop_binary(UOP_ADD,
      term_new(0, TAG_TEN, DT_FP32, ta_s7),
      term_new(0, TAG_TEN, DT_FP32, tb_s7)));
  (void)add_s7;
  u32 kid_s7 = first_emitted_kernel_with_lift_success(kernels_before_s7);
  CHECK(kid_s7 != 0);
  if (kid_s7 != 0) {
    KernelEntry *ke = &KERNELS[kid_s7];
    // Single-write contract: lift succeeded => program is NULL,
    // n_ops is 0; cached_lift carries the canonical UOp DAG.
    CHECK(ke->cached_lift.store_root != 0);
    CHECK_EQ(ke->program, (KProgOp *)NULL);
    CHECK_EQ(ke->n_ops, 0u);
    // compute_root mirrors store_root.
    CHECK_EQ(ke->compute_root, ke->cached_lift.store_root);
    // Dispatch the kernel directly via the active backend.  The
    // ke->output_tid points at the destination tensor; reading its
    // buffer gives us the elementwise sum.
    u32 out_tid = ke->output_tid;
    u32 in_buf_ids[2] = { TENS[ta_s7].buf_id, TENS[tb_s7].buf_id };
    int rc = CURRENT_BACKEND->dispatch_kernel(ke, in_buf_ids,
                                              TENS[out_tid].buf_id);
    CHECK_EQ((u64)rc, 0u);
    f32 dst_s7[4];
    CURRENT_BACKEND->buf_read(TENS[out_tid].buf_id, dst_s7, sizeof(dst_s7));
    CHECK_EQ((u64)dst_s7[0], (u64)11.0f);
    CHECK_EQ((u64)dst_s7[1], (u64)22.0f);
    CHECK_EQ((u64)dst_s7[2], (u64)33.0f);
    CHECK_EQ((u64)dst_s7[3], (u64)44.0f);
  }
  thvm_free();
  unsetenv("THVM_PHASE_C7_FREE_PROGRAM");

  // === slice 7: dispatch-time consumers tolerate program == NULL ===
  // Re-dispatch the same kernel pattern under the knob to exercise
  // metal_kernel_supported / cpu_jit_hash / cg_supports null-paths
  // through the dispatcher; these consumers were updated to read
  // structural facts from cached_lift instead of program[].
  TEST_BEGIN("slice7/cpu-jit-hash-from-cached-lift");
  setenv("THVM_PHASE_C7_FREE_PROGRAM", "1", 1);
  unsetenv("THVM_BACKEND");
  thvm_init();
  Shape s_h = {0}; s_h.ndim = 1; s_h.dims[0] = 8;
  f32 src_h[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
  u32 t_h = tensor_alloc(CURRENT_BACKEND, s_h, DT_FP32);
  CURRENT_BACKEND->buf_write(TENS[t_h].buf_id, src_h, sizeof(src_h));
  u32 kernels_before_h = KERNELS_NEXT;
  Term neg_h = thvm_materialize(uop_unary(UOP_NEG,
      term_new(0, TAG_TEN, DT_FP32, t_h)));
  (void)neg_h;
  u32 kid_h = first_emitted_kernel_with_lift_success(kernels_before_h);
  CHECK(kid_h != 0);
  if (kid_h != 0) {
    KernelEntry *ke = &KERNELS[kid_h];
    CHECK_EQ(ke->program, (KProgOp *)NULL);
    CHECK_EQ(ke->n_ops, 0u);
    // cpu_jit_hash must produce a non-zero hash from cached_lift.
    u64 jit_h = cpu_jit_hash(ke);
    CHECK(jit_h != 0);
    // Dispatch it; numerical result follows.
    u32 out_tid_h = ke->output_tid;
    u32 in_buf_ids_h[1] = { TENS[t_h].buf_id };
    int rc_h = CURRENT_BACKEND->dispatch_kernel(ke, in_buf_ids_h,
                                                TENS[out_tid_h].buf_id);
    CHECK_EQ((u64)rc_h, 0u);
    f32 dst_h[8];
    CURRENT_BACKEND->buf_read(TENS[out_tid_h].buf_id, dst_h, sizeof(dst_h));
    for (u32 i = 0; i < 8; i++) {
      f32 expected = -(f32)(i + 1);
      CHECK(dst_h[i] == expected);
    }
  }
  thvm_free();
  unsetenv("THVM_PHASE_C7_FREE_PROGRAM");

  TEST_REPORT();
}
