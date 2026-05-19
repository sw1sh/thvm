// test_metal_real.c -- dual-TU build smoke test.
//
// Compiled with -DTHVM_HAS_METAL so src/thvm.c skips its C stub
// for the Metal backend; METAL_BACKEND comes from
// build/backend_metal.o (built from src/backend/metal/_.m).  The
// real metal_init still hasn't been written -- this test just
// confirms the dual-TU build works (no duplicate symbols, no
// missing references) and the stub semantics are identical to the
// C stub the existing test_metal_stub.c covers.

// THVM_HAS_METAL is provided by the Makefile's -D flag for this
// binary; the runtime sees it and skips its C-side metal stub
// include so build/backend_metal.o owns METAL_BACKEND instead.
#include "../src/thvm.c"
#include "test.h"

// bm4c: forward-declared in src/backend/metal/_.m, lives in
// the linked backend_metal.o.
extern void thvm_metal_buf_freelist_push(u32 buf_id);
extern u64  thvm_metal_live_bytes(void);
extern u64  thvm_metal_retained_bytes(void);
extern u64  thvm_metal_deferred_bytes(void);
extern u32  thvm_metal_deferred_len(void);
extern u32  thvm_metal_freelist_len(void);
extern u64  thvm_metal_peak_live_bytes(void);
extern u64  thvm_metal_peak_retained_bytes(void);
extern u64  thvm_metal_peak_deferred_bytes(void);
extern u32  thvm_metal_buf_pool_begin(void);
extern void thvm_metal_buf_pool_rollback_with_preserve(u32 wm);
extern void thvm_metal_buf_mark_preserved(u32 buf_id);
extern void thvm_metal_buf_clear_preserved(u32 wm);
extern void thvm_metal_buf_get(u32 i, u64 *nbytes_out, u32 *refcount_out);

static int metal_available(void) {
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  int ok = 0;
  if (CURRENT_BACKEND == &METAL_BACKEND) {
    u32 bid = CURRENT_BACKEND->buf_alloc(4);
    if (bid != 0) {
      CURRENT_BACKEND->buf_free(bid);
      ok = 1;
    }
  }
  thvm_free();
  return ok;
}

static u32 build_metal_tile_add_kernel(u32 extent, u32 groups, u32 threads,
                                       int local_first) {
  u32 kid = kernel_alloc();
  KernelEntry *ke = &KERNELS[kid];
  kernel_inputs_reserve(ke, 2);
  ke->n_inputs        = 2;
  ke->input_tids[0]   = 0;
  ke->input_tids[1]   = 0;
  ke->input_dtypes[0] = DT_FP32;
  ke->input_dtypes[1] = DT_FP32;
  ke->input_numels[0] = extent;
  ke->input_numels[1] = extent;
  ke->output_dtype    = DT_FP32;
  ke->output_numel    = extent;

  u32 r0 = rangeify_emit_leaf(ke, S_RANGE, DT_INT32,
                              ((u64)S_AXIS_LOOP << 32) | extent);
  u32 pa = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 0);
  u32 pb = rangeify_emit_leaf(ke, S_DEFINE_PARAM,  DT_FP32, 1);
  u32 pc = rangeify_emit_leaf(ke, S_DEFINE_OUTPUT, DT_FP32, 0);
  u32 src_a[2] = {pa, r0};
  u32 src_b[2] = {pb, r0};
  u32 src_c[2] = {pc, r0};
  u32 ia = rangeify_emit(ke, S_INDEX, DT_FP32, 2, src_a, 1);
  u32 ib = rangeify_emit(ke, S_INDEX, DT_FP32, 2, src_b, 1);
  u32 ic = rangeify_emit(ke, S_INDEX, DT_FP32, 2, src_c, 1);
  u32 la = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ia);
  u32 lb = rangeify_emit_unary(ke, S_LOAD, DT_FP32, ib);
  u32 sum = rangeify_emit_binary(ke, S_ADD, DT_FP32, la, lb);
  u32 sto = rangeify_emit_binary(ke, S_STORE, DT_FP32, ic, sum);
  u32 root_src[2] = {sto, r0};
  rangeify_emit(ke, S_BUFFERIZE, DT_FP32, 2, root_src, 0);

  // Drive the axes through axes_default_for + kernel_apply_opt
  // instead of hand-writing axis_types[].  Final shape:
  // [GLOBAL=groups, LOCAL=threads] (or swapped).  Built from initial
  // [LOOP=extent] via KOP_LOCAL(threads) + KOP_GLOBAL(groups)
  // (+ KOP_SWAP for the local-first variant).
  ke->output_shape.ndim    = 1;
  ke->output_shape.dims[0] = extent;
  ke->schedule = &ke->_local_schedule;
  memset(ke->schedule, 0, sizeof(KpSchedule));
  KOpt local_op  = { .op = KOP_LOCAL,  .axis = 0, .arg = threads };
  KOpt global_op = { .op = KOP_GLOBAL, .axis = 0, .arg = groups  };
  CHECK(kernel_apply_opt(ke, local_op));
  CHECK(kernel_apply_opt(ke, global_op));
  if (local_first) {
    KOpt swap_op = { .op = KOP_SWAP, .axis = 0, .arg = 1 };
    CHECK(kernel_apply_opt(ke, swap_op));
  }
  return kid;
}

int main(void) {
  if (!metal_available()) {
    PENDING("no Metal device available");
  }

  TEST_BEGIN("metal-real/dual-tu-build-links");
  unsetenv("THVM_BACKEND");
  thvm_init();
  CHECK(CURRENT_BACKEND == &CPU_BACKEND);
  thvm_free();

  TEST_BEGIN("metal-real/THVM_BACKEND-metal-selects-m-backend");
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  CHECK(CURRENT_BACKEND == &METAL_BACKEND);
  CHECK_EQ(CURRENT_BACKEND->id, 2);
  // Now-real metal_buf_alloc returns a non-zero buf_id.
  u32 b = CURRENT_BACKEND->buf_alloc(64);
  CHECK(b != 0);
  CURRENT_BACKEND->buf_free(b);
  thvm_free();

  TEST_BEGIN("metal-real/init-shutdown-cycle-survives");
  // metal_init opens MTLDevice + MTLCommandQueue; metal_shutdown
  // nils the references.  Verify a second cycle re-opens cleanly.
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  thvm_free();
  thvm_init();
  CHECK(CURRENT_BACKEND == &METAL_BACKEND);
  thvm_free();

  TEST_BEGIN("metal-real/buf-write-read-roundtrip");
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  // Allocate a 16-element f32 buffer on Metal, write a known
  // sequence via shared-storage memcpy, read it back, compare.
  float src[16], dst[16];
  for (int i = 0; i < 16; i++) src[i] = (float)i * 1.5f;
  u32 bid = CURRENT_BACKEND->buf_alloc(sizeof(src));
  CHECK(bid != 0);
  CHECK_EQ(CURRENT_BACKEND->buf_write(bid, src, sizeof(src)), 0);
  CHECK_EQ(CURRENT_BACKEND->buf_read (bid, dst, sizeof(dst)), 0);
  for (int i = 0; i < 16; i++) CHECK(src[i] == dst[i]);
  CURRENT_BACKEND->buf_free(bid);
  thvm_free();

  TEST_BEGIN("metal-real/buf-copy-roundtrip");
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  float copy_src[8], copy_dst[8] = {0};
  for (int i = 0; i < 8; i++) copy_src[i] = (float)(i + 1);
  u32 src_bid = CURRENT_BACKEND->buf_alloc(sizeof(copy_src));
  u32 dst_bid = CURRENT_BACKEND->buf_alloc(sizeof(copy_dst));
  CHECK(src_bid != 0);
  CHECK(dst_bid != 0);
  CHECK_EQ(CURRENT_BACKEND->buf_write(src_bid, copy_src, sizeof(copy_src)), 0);
  CHECK_EQ(CURRENT_BACKEND->buf_copy(dst_bid, src_bid, sizeof(copy_src)), 0);
  CHECK_EQ(CURRENT_BACKEND->buf_read(dst_bid, copy_dst, sizeof(copy_dst)), 0);
  for (int i = 0; i < 8; i++) CHECK(copy_src[i] == copy_dst[i]);
  CURRENT_BACKEND->buf_free(src_bid);
  CURRENT_BACKEND->buf_free(dst_bid);
  thvm_free();

  TEST_BEGIN("metal-real/buf-refcount-shared-storage");
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  u32 bid2 = CURRENT_BACKEND->buf_alloc(64);
  CHECK(bid2 != 0);
  CURRENT_BACKEND->buf_incref(bid2);
  CURRENT_BACKEND->buf_decref(bid2);
  // Refcount still 1, buffer should still be readable.
  char tmp[8];
  CHECK_EQ(CURRENT_BACKEND->buf_read(bid2, tmp, sizeof(tmp)), 0);
  CURRENT_BACKEND->buf_decref(bid2);  // drops to 0; recycles.
  // Refcount-0 freelist slots are invalid until reallocated.
  CHECK_EQ(CURRENT_BACKEND->buf_read(bid2, tmp, sizeof(tmp)), -1);
  thvm_free();

  // === Helper: build f32 TEN, write data, return its tid. =====
  // Used by the binary-elementwise parity tests below to seed
  // inputs in the current backend.

  TEST_BEGIN("metal-real/const-kernel-parity-with-cpu");
  // Run UOP_CONST(3.14f) through the CPU backend, capture output.
  union { f32 f; u32 u; } cu; cu.f = 3.14f;
  unsetenv("THVM_BACKEND");
  thvm_init();
  Term cpu_kern = thvm_materialize(uop_const(DT_FP32, cu.u));
  Term cpu_done = wnf(cpu_kern);
  CHECK_EQ(term_tag(cpu_done), TAG_TEN);
  u32 cpu_tid = (u32)term_val(cpu_done);
  f32 cpu_out;
  CHECK_EQ(CPU_BACKEND.buf_read(TENS[cpu_tid].buf_id, &cpu_out, sizeof(f32)), 0);
  thvm_free();

  // Same graph under Metal; output should match bit-for-bit.
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  Term gpu_kern = thvm_materialize(uop_const(DT_FP32, cu.u));
  Term gpu_done = wnf(gpu_kern);
  CHECK_EQ(term_tag(gpu_done), TAG_TEN);
  u32 gpu_tid = (u32)term_val(gpu_done);
  f32 gpu_out;
  CHECK_EQ(METAL_BACKEND.buf_read(TENS[gpu_tid].buf_id, &gpu_out, sizeof(f32)), 0);
  CHECK(cpu_out == gpu_out);
  CHECK(gpu_out == 3.14f);
  thvm_free();

  // === Binary elementwise parity (ADD, MUL, CMPLT, CMPEQ) ===
  // Build [1,2,3,4] + [10,20,30,40] (etc.) under both backends,
  // verify identical output buffers.

  TEST_BEGIN("metal-real/add-mul-cmplt-cmpeq-parity-with-cpu");
  Shape s = {0}; s.ndim = 1; s.dims[0] = 4;
  f32 src_a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 src_b[4] = {10.0f, 20.0f, 30.0f, 40.0f};

  for (int op_idx = 0; op_idx < 4; op_idx++) {
    u32 op = (op_idx == 0) ? UOP_ADD :
             (op_idx == 1) ? UOP_MUL :
             (op_idx == 2) ? UOP_CMPLT : UOP_CMPEQ;
    f32 cpu_buf[4], gpu_buf[4];

    unsetenv("THVM_BACKEND"); thvm_init();
    {
      u32 ta = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
      u32 tb = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[ta].buf_id, src_a, sizeof(src_a));
      CURRENT_BACKEND->buf_write(TENS[tb].buf_id, src_b, sizeof(src_b));
      Term done = wnf(thvm_materialize(uop_binary(op,
          term_new(0, TAG_TEN, DT_FP32, ta),
          term_new(0, TAG_TEN, DT_FP32, tb))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                cpu_buf, sizeof(cpu_buf));
    }
    thvm_free();

    setenv("THVM_BACKEND", "metal", 1); thvm_init();
    {
      u32 ta = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
      u32 tb = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[ta].buf_id, src_a, sizeof(src_a));
      CURRENT_BACKEND->buf_write(TENS[tb].buf_id, src_b, sizeof(src_b));
      Term done = wnf(thvm_materialize(uop_binary(op,
          term_new(0, TAG_TEN, DT_FP32, ta),
          term_new(0, TAG_TEN, DT_FP32, tb))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                gpu_buf, sizeof(gpu_buf));
    }
    thvm_free();

    for (int i = 0; i < 4; i++) CHECK(cpu_buf[i] == gpu_buf[i]);
  }

  TEST_BEGIN("metal-real/tile-local-global-dispatch-parity");
  setenv("THVM_BACKEND", "metal", 1);
  setenv("THVM_TILE", "1", 1);
  thvm_init();
  {
    f32 in0[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    f32 in1[8] = {8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f};
    for (u32 local_first = 0; local_first < 2; local_first++) {
      f32 out[8] = {0};
      u32 kid = build_metal_tile_add_kernel(8, 2, 4, (int)local_first);
      KernelEntry *ke = &KERNELS[kid];
      u32 in0_buf = METAL_BACKEND.buf_alloc(sizeof(in0));
      u32 in1_buf = METAL_BACKEND.buf_alloc(sizeof(in1));
      u32 out_buf = METAL_BACKEND.buf_alloc(sizeof(out));
      CHECK(in0_buf != 0);
      CHECK(in1_buf != 0);
      CHECK(out_buf != 0);
      CHECK_EQ(METAL_BACKEND.buf_write(in0_buf, in0, sizeof(in0)), 0);
      CHECK_EQ(METAL_BACKEND.buf_write(in1_buf, in1, sizeof(in1)), 0);
      CHECK_EQ(METAL_BACKEND.buf_write(out_buf, out, sizeof(out)), 0);
      u32 in_bufs[2] = {in0_buf, in1_buf};
      CHECK_EQ(METAL_BACKEND.dispatch_kernel(ke, in_bufs, out_buf), 0);
      CHECK_EQ(cg_kernel_dispatch_kind(kid), (u32)KDISPATCH_METAL_TILE);
      CHECK_EQ(METAL_BACKEND.buf_read(out_buf, out, sizeof(out)), 0);
      for (u32 i = 0; i < 8; i++) {
        CHECK(out[i] == in0[i] + in1[i]);
      }
    }
  }
  thvm_free();
  unsetenv("THVM_TILE");

  // === Matmul parity: M=N=K=16 (multiple of 8 -> simdgroup_matrix) ===
  // Specifically targets the F3.1+F3.4b path where the recogniser
  // wraps the matmul with OPT(_, TC, 0) and render_uop's
  // simdgroup_matrix template fires.  Without this test, matmul
  // correctness was only checked at WL level (blas_dtypes.wlt) on
  // K=3 shapes that bypass simdgroup entirely.
  TEST_BEGIN("metal-real/matmul-mnk16-tc-parity-with-cpu");
  {
    enum { MM = 16, MK = 16, MN = 16 };
    Shape sa = {0}; sa.ndim = 2; sa.dims[0] = MM; sa.dims[1] = MK;
    Shape sb = {0}; sb.ndim = 2; sb.dims[0] = MK; sb.dims[1] = MN;
    f32 mm_a[MM*MK];
    f32 mm_b[MK*MN];
    for (u32 i = 0; i < MM*MK; i++) mm_a[i] = (f32)((i * 13 + 7) % 19) - 9.0f;
    for (u32 i = 0; i < MK*MN; i++) mm_b[i] = (f32)((i * 31 + 5) % 23) - 11.0f;

    f32 mm_cpu[MM*MN] = {0};
    f32 mm_gpu[MM*MN] = {0};

    // Reference: plain triple-loop matmul on the host.
    for (u32 m = 0; m < MM; m++) {
      for (u32 n = 0; n < MN; n++) {
        f32 acc = 0.0f;
        for (u32 k = 0; k < MK; k++) {
          acc += mm_a[m*MK + k] * mm_b[k*MN + n];
        }
        mm_cpu[m*MN + n] = acc;
      }
    }

    // CPU backend through the schedule (TMatMul-equivalent).
    unsetenv("THVM_BACKEND"); thvm_init();
    cpu_blas_gemm_dispatch_counters_reset();
    {
      u32 ta = tensor_alloc(CURRENT_BACKEND, sa, DT_FP32);
      u32 tb = tensor_alloc(CURRENT_BACKEND, sb, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[ta].buf_id, mm_a, sizeof(mm_a));
      CURRENT_BACKEND->buf_write(TENS[tb].buf_id, mm_b, sizeof(mm_b));
      Term A = term_new(0, TAG_TEN, DT_FP32, ta);
      Term B = term_new(0, TAG_TEN, DT_FP32, tb);
      // Lower TMatMul: RESHAPE A->{M,K,1} EXPAND ->{M,K,N};
      // RESHAPE B->{1,K,N} EXPAND ->{M,K,N}; MUL; REDUCE_SUM axis=1.
      u32 d_mk1[3] = {MM, MK, 1};
      u32 d_1kn[3] = {1, MK, MN};
      u32 d_mkn[3] = {MM, MK, MN};
      Term Ar  = uop_reshape(A,  3, d_mk1);
      Term Ae  = uop_expand (Ar, 3, d_mkn);
      Term Br  = uop_reshape(B,  3, d_1kn);
      Term Be  = uop_expand (Br, 3, d_mkn);
      Term mul = uop_binary(UOP_MUL, Ae, Be);
      Term red = uop_reduce(REDUCE_SUM, /*axis=*/1, mul);
      Term done = wnf(thvm_materialize(red));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                mm_cpu, sizeof(mm_cpu));
    }
    // Under default THVM_PHASE_C7_FREE_PROGRAM=1 the CPU matmul
    // kernel must route through cblas_sgemm via the DAG-side
    // classifier (uop_dag_classify_matmul_shape).  If the dispatch
    // falls through to render_uop_c there is a 30-100x perf cliff,
    // so this counter doubles as a regression guard.
    CHECK(cpu_blas_gemm_dispatch_dag_count() > 0);
    thvm_free();

    // Metal backend through the same schedule.  Whichever dispatch
    // path the schedule chooses for this TMatMul-shape kernel
    // (render_uop simdgroup, metal_try_gemm, or metal_jit_encode
    // KProgOp-flat) must produce values that match the CPU reference
    // bit-for-bit modulo fma rounding.  This catches simdgroup
    // template stride bugs that compile-only fixtures can miss.
    setenv("THVM_BACKEND", "metal", 1); thvm_init();
    {
      u32 ta = tensor_alloc(CURRENT_BACKEND, sa, DT_FP32);
      u32 tb = tensor_alloc(CURRENT_BACKEND, sb, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[ta].buf_id, mm_a, sizeof(mm_a));
      CURRENT_BACKEND->buf_write(TENS[tb].buf_id, mm_b, sizeof(mm_b));
      Term A = term_new(0, TAG_TEN, DT_FP32, ta);
      Term B = term_new(0, TAG_TEN, DT_FP32, tb);
      u32 d_mk1[3] = {MM, MK, 1};
      u32 d_1kn[3] = {1, MK, MN};
      u32 d_mkn[3] = {MM, MK, MN};
      Term Ar  = uop_reshape(A,  3, d_mk1);
      Term Ae  = uop_expand (Ar, 3, d_mkn);
      Term Br  = uop_reshape(B,  3, d_1kn);
      Term Be  = uop_expand (Br, 3, d_mkn);
      Term mul = uop_binary(UOP_MUL, Ae, Be);
      Term red = uop_reduce(REDUCE_SUM, /*axis=*/1, mul);
      Term done = wnf(thvm_materialize(red));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                mm_gpu, sizeof(mm_gpu));
    }
    thvm_free();

    // Per-element parity (allow tiny fma rounding).
    for (u32 i = 0; i < MM*MN; i++) {
      f32 d = mm_cpu[i] - mm_gpu[i];
      if (d < 0) d = -d;
      CHECK(d < 1e-3f);
    }
  }

  // === DOT routes through cblas_sdot via DAG ===
  TEST_BEGIN("metal-real/dot-cpu-routes-through-cblas-via-dag");
  {
    enum { DK = 64 };
    Shape sd = {0}; sd.ndim = 1; sd.dims[0] = DK;
    f32 dv_a[DK];
    f32 dv_b[DK];
    f32 dv_ref = 0.0f;
    for (u32 i = 0; i < DK; i++) {
      dv_a[i] = (f32)((i * 7 + 3) % 11) - 5.0f;
      dv_b[i] = (f32)((i * 5 + 2) % 13) - 6.0f;
      dv_ref += dv_a[i] * dv_b[i];
    }

    unsetenv("THVM_BACKEND"); thvm_init();
    cpu_blas_gemm_dispatch_counters_reset();
    f32 dot_out = 0.0f;
    {
      u32 ta = tensor_alloc(CURRENT_BACKEND, sd, DT_FP32);
      u32 tb = tensor_alloc(CURRENT_BACKEND, sd, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[ta].buf_id, dv_a, sizeof(dv_a));
      CURRENT_BACKEND->buf_write(TENS[tb].buf_id, dv_b, sizeof(dv_b));
      Term A = term_new(0, TAG_TEN, DT_FP32, ta);
      Term B = term_new(0, TAG_TEN, DT_FP32, tb);
      Term mul = uop_binary(UOP_MUL, A, B);
      Term red = uop_reduce(REDUCE_SUM, /*axis=*/0, mul);
      Term done = wnf(thvm_materialize(red));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                &dot_out, sizeof(dot_out));
    }
    f32 dd = dot_out - dv_ref;
    if (dd < 0) dd = -dd;
    CHECK(dd < 1e-3f);
    // The DAG-side classifier must fire under default
    // THVM_PHASE_C7_FREE_PROGRAM=1 (program[] is freed at materialize).
    CHECK(cpu_blas_dot_dispatch_dag_count() > 0);
    thvm_free();
  }

  // === GEMV routes through cblas_sgemv via DAG ===
  TEST_BEGIN("metal-real/gemv-cpu-routes-through-cblas-via-dag");
  {
    enum { GM = 16, GK = 24 };
    Shape sw = {0}; sw.ndim = 2; sw.dims[0] = GM; sw.dims[1] = GK;
    Shape sx = {0}; sx.ndim = 2; sx.dims[0] = 1;  sx.dims[1] = GK;
    f32 wv [GM*GK];
    f32 xv [GK];
    f32 yref[GM] = {0};
    for (u32 i = 0; i < GM*GK; i++) wv[i] = (f32)((i * 11 + 1) % 17) - 8.0f;
    for (u32 i = 0; i < GK; i++)    xv[i] = (f32)((i * 13 + 5) % 19) - 9.0f;
    for (u32 m = 0; m < GM; m++) {
      f32 acc = 0.0f;
      for (u32 k = 0; k < GK; k++) acc += wv[m*GK + k] * xv[k];
      yref[m] = acc;
    }

    unsetenv("THVM_BACKEND"); thvm_init();
    cpu_blas_gemm_dispatch_counters_reset();
    f32 yout[GM] = {0};
    {
      u32 tw = tensor_alloc(CURRENT_BACKEND, sw, DT_FP32);
      u32 tx = tensor_alloc(CURRENT_BACKEND, sx, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[tw].buf_id, wv, sizeof(wv));
      CURRENT_BACKEND->buf_write(TENS[tx].buf_id, xv, sizeof(xv));
      Term W = term_new(0, TAG_TEN, DT_FP32, tw);
      Term X = term_new(0, TAG_TEN, DT_FP32, tx);
      // TMatVec lower: x{1,K} EXPAND to {M,K}; MUL elementwise; REDUCE
      // SUM axis=1 -> {M}.  Shape exactly matches what
      // wl/THVMLink/Kernel/NN.wl :: TMatVec produces.
      u32 d_mk[2] = {GM, GK};
      Term Xe  = uop_expand (X, 2, d_mk);
      Term mul = uop_binary(UOP_MUL, W, Xe);
      Term red = uop_reduce(REDUCE_SUM, /*axis=*/1, mul);
      Term done = wnf(thvm_materialize(red));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                yout, sizeof(yout));
    }
    for (u32 m = 0; m < GM; m++) {
      f32 d = yout[m] - yref[m];
      if (d < 0) d = -d;
      CHECK(d < 1e-3f);
    }
    CHECK(cpu_blas_gemv_dispatch_dag_count() > 0);
    thvm_free();
  }

  // === Unary elementwise parity (NEG, RECIP, SQRT, EXP2, LOG2) ===
  TEST_BEGIN("metal-real/unary-parity-with-cpu");
  Shape su = {0}; su.ndim = 1; su.dims[0] = 4;
  f32 src_u[4] = {1.0f, 2.0f, 4.0f, 8.0f};

  u32 unary_ops[5] = {UOP_NEG, UOP_RECIP, UOP_SQRT, UOP_EXP2, UOP_LOG2};
  for (int i = 0; i < 5; i++) {
    u32 op = unary_ops[i];
    f32 cpu_buf[4], gpu_buf[4];

    unsetenv("THVM_BACKEND"); thvm_init();
    {
      u32 tid = tensor_alloc(CURRENT_BACKEND, su, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[tid].buf_id, src_u, sizeof(src_u));
      Term done = wnf(thvm_materialize(uop_unary(op,
          term_new(0, TAG_TEN, DT_FP32, tid))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                cpu_buf, sizeof(cpu_buf));
    }
    thvm_free();

    setenv("THVM_BACKEND", "metal", 1); thvm_init();
    {
      u32 tid = tensor_alloc(CURRENT_BACKEND, su, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[tid].buf_id, src_u, sizeof(src_u));
      Term done = wnf(thvm_materialize(uop_unary(op,
          term_new(0, TAG_TEN, DT_FP32, tid))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                gpu_buf, sizeof(gpu_buf));
    }
    thvm_free();

    // Use a small relative tolerance for transcendental ops
    // (exp2/log2 cores differ slightly between CPU libm and the
    // Metal SIMD fast-math path).  abs diff < 1e-5 is plenty.
    for (int k = 0; k < 4; k++) {
      f32 d = cpu_buf[k] - gpu_buf[k];
      if (d < 0) d = -d;
      CHECK(d < 1e-5f);
    }
  }

  // === Reduction parity (REDUCE_SUM, REDUCE_MAX) on a non-
  //     innermost axis to exercise the inner-stride packing.
  // Input: rank-2 {3, 4} = 12 elements; reduce axis=0 -> shape {4}.
  TEST_BEGIN("metal-real/reduce-axis0-sum-max-parity");
  Shape sr = {0}; sr.ndim = 2; sr.dims[0] = 3; sr.dims[1] = 4;
  f32 src_r[12] = {
    1.0f, 2.0f, 3.0f, 4.0f,
    5.0f, 6.0f, 7.0f, 8.0f,
    9.0f, 10.0f, 11.0f, 12.0f
  };

  u32 reduce_kinds[2] = {REDUCE_SUM, REDUCE_MAX};
  for (int i = 0; i < 2; i++) {
    u32 kind = reduce_kinds[i];
    f32 cpu_buf[4], gpu_buf[4];

    unsetenv("THVM_BACKEND"); thvm_init();
    {
      u32 tid = tensor_alloc(CURRENT_BACKEND, sr, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[tid].buf_id, src_r, sizeof(src_r));
      Term done = wnf(thvm_materialize(uop_reduce(kind, 0,
          term_new(0, TAG_TEN, DT_FP32, tid))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                cpu_buf, sizeof(cpu_buf));
    }
    thvm_free();

    setenv("THVM_BACKEND", "metal", 1); thvm_init();
    {
      u32 tid = tensor_alloc(CURRENT_BACKEND, sr, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[tid].buf_id, src_r, sizeof(src_r));
      Term done = wnf(thvm_materialize(uop_reduce(kind, 0,
          term_new(0, TAG_TEN, DT_FP32, tid))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                gpu_buf, sizeof(gpu_buf));
    }
    thvm_free();

    for (int k = 0; k < 4; k++) CHECK(cpu_buf[k] == gpu_buf[k]);
  }

  // === Hand-coded-opts UPCAST parity: a multi-output-axis reduce ===
  // {4, 64, 16} REDUCE_SUM over axis 2 -> {4, 64}.  With hand-coded
  // opts ON (the default) kernel_hand_coded_opts UPCASTs the inner
  // output axis (64 % 4 == 0) -> the lifted DAG gets a KAX_LOOP outer
  // 64-axis, a KAX_UPCAST inner 4-axis, the 4-axis dim0 (KAX_LOOP),
  // and the 16-axis reduce (KAX_REDUCE).  render_uop must promote
  // dim0 + the outer 64-axis to a flat `tid` decode while the inner
  // 4-axis becomes a `#pragma unroll(4)` for-loop, and
  // cg_tile_metal_dispatch_shape must launch a grid that covers
  // exactly that decode.  This is the path the BS=512 beautiful_mnist
  // conv/pool kernels take; before the v2 renderer+dispatch fix it
  // failed to compile and fell to the per-op interpreter.
  TEST_BEGIN("metal-real/hand-coded-upcast-reduce-parity");
  {
    Shape suc = {0}; suc.ndim = 3; suc.dims[0] = 4; suc.dims[1] = 64;
    suc.dims[2] = 16;
    enum { UC_N = 4 * 64 * 16 };
    f32 uc_src[UC_N];
    for (u32 i = 0; i < UC_N; i++) {
      uc_src[i] = (f32)((i * 17 + 3) % 29) - 14.0f;
    }
    f32 uc_ref[4 * 64];
    for (u32 a = 0; a < 4; a++) {
      for (u32 b = 0; b < 64; b++) {
        f32 acc = 0.0f;
        for (u32 c = 0; c < 16; c++) acc += uc_src[(a*64 + b)*16 + c];
        uc_ref[a*64 + b] = acc;
      }
    }
    f32 uc_cpu[4 * 64] = {0};
    f32 uc_gpu[4 * 64] = {0};

    unsetenv("THVM_BACKEND"); thvm_init();
    {
      u32 tt = tensor_alloc(CURRENT_BACKEND, suc, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[tt].buf_id, uc_src, sizeof(uc_src));
      Term done = wnf(thvm_materialize(uop_reduce(REDUCE_SUM, 2,
          term_new(0, TAG_TEN, DT_FP32, tt))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                uc_cpu, sizeof(uc_cpu));
    }
    thvm_free();

    setenv("THVM_BACKEND", "metal", 1); thvm_init();
    {
      u32 tt = tensor_alloc(CURRENT_BACKEND, suc, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[tt].buf_id, uc_src, sizeof(uc_src));
      Term done = wnf(thvm_materialize(uop_reduce(REDUCE_SUM, 2,
          term_new(0, TAG_TEN, DT_FP32, tt))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                uc_gpu, sizeof(uc_gpu));
    }
    thvm_free();

    for (u32 i = 0; i < 4 * 64; i++) {
      f32 d = uc_cpu[i] - uc_ref[i];
      if (d < 0) d = -d;
      CHECK(d < 1e-3f);
      d = uc_gpu[i] - uc_ref[i];
      if (d < 0) d = -d;
      CHECK(d < 1e-3f);
    }
  }

  // === Bigger multi-output reduce parity: {8,256,32} -> {8,256} ===
  // output_loop_product 2048 >= 1024 -- exercises kernel_hand_coded_opts
  // on a multi-output-axis reduce (UPCAST + LOCAL split off the inner
  // output axis, the generic accumulator emit path).  Must stay
  // bit-parity with the CPU backend (the same correctness gate as the
  // matmul/conv parity tests).  The deep conv-style tiling (multi-UPCAST
  // + multi-LOCAL) is gated on OPT_CONV kernels -- see
  // render-uop/two-local-axes-tt-decode for the xcrun-compiled
  // multi-LOCAL renderer check.
  TEST_BEGIN("metal-real/deep-tiled-reduce-parity");
  {
    Shape sdt = {0}; sdt.ndim = 3; sdt.dims[0] = 8; sdt.dims[1] = 256;
    sdt.dims[2] = 32;
    enum { DT_M = 8, DT_NN = 256, DT_K = 32, DT_OUT = DT_M * DT_NN };
    f32 *dt_src = malloc((size_t)DT_M * DT_NN * DT_K * sizeof(f32));
    for (u32 i = 0; i < (u32)(DT_M * DT_NN * DT_K); i++) {
      dt_src[i] = (f32)((i * 13 + 7) % 31) - 15.0f;
    }
    f32 *dt_ref = malloc(DT_OUT * sizeof(f32));
    for (u32 a = 0; a < DT_M; a++) {
      for (u32 b = 0; b < DT_NN; b++) {
        f32 acc = 0.0f;
        for (u32 c = 0; c < DT_K; c++) acc += dt_src[(a*DT_NN + b)*DT_K + c];
        dt_ref[a*DT_NN + b] = acc;
      }
    }
    f32 *dt_cpu = calloc(DT_OUT, sizeof(f32));
    f32 *dt_gpu = calloc(DT_OUT, sizeof(f32));

    unsetenv("THVM_BACKEND"); thvm_init();
    {
      u32 tt = tensor_alloc(CURRENT_BACKEND, sdt, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[tt].buf_id, dt_src,
                                 (size_t)DT_M*DT_NN*DT_K*sizeof(f32));
      Term done = wnf(thvm_materialize(uop_reduce(REDUCE_SUM, 2,
          term_new(0, TAG_TEN, DT_FP32, tt))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                dt_cpu, DT_OUT*sizeof(f32));
    }
    thvm_free();

    setenv("THVM_BACKEND", "metal", 1); thvm_init();
    {
      u32 tt = tensor_alloc(CURRENT_BACKEND, sdt, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[tt].buf_id, dt_src,
                                 (size_t)DT_M*DT_NN*DT_K*sizeof(f32));
      Term done = wnf(thvm_materialize(uop_reduce(REDUCE_SUM, 2,
          term_new(0, TAG_TEN, DT_FP32, tt))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                dt_gpu, DT_OUT*sizeof(f32));
    }
    thvm_free();

    for (u32 i = 0; i < DT_OUT; i++) {
      f32 d = dt_cpu[i] - dt_ref[i]; if (d < 0) d = -d; CHECK(d < 1e-2f);
      d = dt_gpu[i] - dt_ref[i];     if (d < 0) d = -d; CHECK(d < 1e-2f);
    }
    free(dt_src); free(dt_ref); free(dt_cpu); free(dt_gpu);
  }

  // === 4D-output conv parity: BS-batched im2col-matmul conv ===
  // Mirrors TConv2DIm2ColBatchedPool's strided-view `_pool` lowering with
  // a 4-D output {cOut, B, hOut, wOut}: input {B,cIn,H,W} unfolded into a
  // strided VIEW xCol6:{cIn,kh,kw,B,hOut,wOut} -> reshape {cIn*kh*kw,B,
  // hOut,wOut}; weights {cOut,cIn,kh,kw} -> reshape {cOut, cIn*kh*kw};
  // out4d = REDUCE_SUM_K(wExp * xExp).  Shape is sized so the deep conv
  // tiling fires (K = cIn*kh*kw = 16*5*5 = 400 >= 256, wOut = 20 >= 16):
  // hand_opts UPCASTs cOut by 8 and LOCALs wOut by 20, and render_uop's
  // rmu_emit_conv emits the 8-way register-blocked accumulator.  GPU
  // output must equal the CPU reference bit-tight (f32 tolerance).
  TEST_BEGIN("metal-real/conv2-4d-output-parity");
  {
    enum { CB = 4, CCIN = 16, CCOUT = 8, CH = 24, CW = 24, CKH = 5, CKW = 5 };
    enum { CHO = CH - CKH + 1, CWO = CW - CKW + 1 };  // 20 x 20
    enum { CK = CCIN * CKH * CKW };                    // 400
    enum { CXN = CB * CCIN * CH * CW, CWN = CCOUT * CCIN * CKH * CKW };
    enum { CON = CCOUT * CB * CHO * CWO };             // 8*4*20*20 = 12800
    f32 *cx = malloc((size_t)CXN * sizeof(f32));
    f32 *cw = malloc((size_t)CWN * sizeof(f32));
    for (u32 i = 0; i < (u32)CXN; i++) cx[i] = (f32)((i * 7 + 3) % 17) * 0.13f - 1.1f;
    for (u32 i = 0; i < (u32)CWN; i++) cw[i] = (f32)((i * 5 + 1) % 13) * 0.07f - 0.4f;
    // Reference: out[co][b][oh][ow] = sum_{ci,kh,kw} w[co][ci][kh][kw] *
    //                                 x[b][ci][oh+kh][ow+kw]
    f32 *cref = calloc((size_t)CON, sizeof(f32));
    for (u32 co = 0; co < CCOUT; co++)
    for (u32 b = 0; b < CB; b++)
    for (u32 oh = 0; oh < CHO; oh++)
    for (u32 ow = 0; ow < CWO; ow++) {
      f32 acc = 0.0f;
      for (u32 ci = 0; ci < CCIN; ci++)
      for (u32 kh = 0; kh < CKH; kh++)
      for (u32 kw = 0; kw < CKW; kw++)
        acc += cw[((co*CCIN + ci)*CKH + kh)*CKW + kw]
             * cx[((b*CCIN + ci)*CH + (oh+kh))*CW + (ow+kw)];
      cref[((co*CB + b)*CHO + oh)*CWO + ow] = acc;
    }
    f32 *ccpu = calloc((size_t)CON, sizeof(f32));
    f32 *cgpu = calloc((size_t)CON, sizeof(f32));

    // Build the conv DAG and materialize it on a given backend.
    #define CONV_BUILD_AND_RUN(out_buf)                                             do {                                                                            Shape sx = {0}; sx.ndim = 4; sx.dims[0] = CB; sx.dims[1] = CCIN;              sx.dims[2] = CH; sx.dims[3] = CW;                                             Shape sw = {0}; sw.ndim = 4; sw.dims[0] = CCOUT; sw.dims[1] = CCIN;           sw.dims[2] = CKH; sw.dims[3] = CKW;                                           u32 txid = tensor_alloc(CURRENT_BACKEND, sx, DT_FP32);                        u32 twid = tensor_alloc(CURRENT_BACKEND, sw, DT_FP32);                        CURRENT_BACKEND->buf_write(TENS[txid].buf_id, cx,                                                        (size_t)CXN * sizeof(f32));                        CURRENT_BACKEND->buf_write(TENS[twid].buf_id, cw,                                                        (size_t)CWN * sizeof(f32));                        Term xin = term_new(0, TAG_TEN, DT_FP32, txid);                               Term win = term_new(0, TAG_TEN, DT_FP32, twid);                               u32 rh = (CKH * (CH + 1) + CH - 1) / CH;                                      u32 rw = (CKW * (CW + 1) + CW - 1) / CW;                                      u32 d6a[6] = { CB, CCIN, 1, CH, 1, CW };                                      u32 d6b[6] = { CB, CCIN, rh, CH, rw, CW };                                    u32 d4a[4] = { CB, CCIN, rh*CH, rw*CW };                                      Term x1 = uop_reshape(uop_expand(uop_reshape(xin, 6, d6a), 6, d6b),                                 4, d4a);                                                u32 be4[8] = { 0,CB, 0,CCIN, 0,CKH*(CH+1), 0,CKW*(CW+1) };                    Term x2 = uop_shrink(x1, 4, be4);                                             u32 d6c[6] = { CB, CCIN, CKH, CH+1, CKW, CW+1 };                              Term x3 = uop_reshape(x2, 6, d6c);                                            u32 be6[12] = { 0,CB, 0,CCIN, 0,CKH, 0,CHO, 0,CKW, 0,CWO };                   Term x4 = uop_shrink(x3, 6, be6);                                             u32 prm[6] = { 1, 2, 4, 0, 3, 5 }; /* {cIn,kh,kw,B,hOut,wOut} */              Term xc6 = uop_permute(x4, 6, prm);                                           u32 d4x[4] = { CK, CB, CHO, CWO };                                            Term xc4 = uop_reshape(xc6, 4, d4x);                                          u32 d2w[2] = { CCOUT, CK };                                                   Term wf  = uop_reshape(win, 2, d2w);                                          u32 d5w[5] = { CCOUT, CK, 1, 1, 1 };                                          u32 d5e[5] = { CCOUT, CK, CB, CHO, CWO };                                     Term wexp = uop_expand(uop_reshape(wf, 5, d5w), 5, d5e);                      u32 d5x[5] = { 1, CK, CB, CHO, CWO };                                         Term xexp = uop_expand(uop_reshape(xc4, 5, d5x), 5, d5e);                     Term out4 = uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_MUL, wexp, xexp));                   Term done = wnf(thvm_materialize(out4));                                      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,                                             (out_buf), (size_t)CON * sizeof(f32));            } while (0)

    unsetenv("THVM_BACKEND"); thvm_init();
    CONV_BUILD_AND_RUN(ccpu);
    thvm_free();

    setenv("THVM_BACKEND", "metal", 1); thvm_init();
    CONV_BUILD_AND_RUN(cgpu);
    thvm_free();
    #undef CONV_BUILD_AND_RUN

    for (u32 i = 0; i < (u32)CON; i++) {
      f32 d = ccpu[i] - cref[i]; if (d < 0) d = -d; CHECK(d < 5e-3f);
      d = cgpu[i] - cref[i];     if (d < 0) d = -d; CHECK(d < 5e-3f);
      d = cgpu[i] - ccpu[i];     if (d < 0) d = -d; CHECK(d < 5e-3f);
    }
    free(cx); free(cw); free(cref); free(ccpu); free(cgpu);
  }

  // === Dual-UPCAST 4D-output conv parity (cOut + wOut both UPCAST'd) ===
  // Sized so hand_opts fires BOTH UPCASTs (tileable gate K>=256, wOut>=16
  // both met): cOut=16 -> UPCAST=8 (outer=2), wOut=20 -> UPCAST=2 then
  // LOCAL=10 on the outer half.  Drives rmu_emit_conv's multi-axis UPCAST
  // emit (8*2 = 16 straight-line accumulators inside the reduce nest).
  // MSL must compile and GPU output must equal CPU reference bit-tight.
  TEST_BEGIN("metal-real/conv-dual-upcast-parity");
  {
    enum { DB = 2, DCIN = 16, DCOUT = 16, DH = 24, DW = 24, DKH = 5, DKW = 5 };
    enum { DHO = DH - DKH + 1, DWO = DW - DKW + 1 };  // 20 x 20
    enum { DK = DCIN * DKH * DKW };                    // 400
    enum { DXN = DB * DCIN * DH * DW, DWN = DCOUT * DCIN * DKH * DKW };
    enum { DON = DCOUT * DB * DHO * DWO };             // 32*2*20*20 = 25600
    f32 *dx = malloc((size_t)DXN * sizeof(f32));
    f32 *dw = malloc((size_t)DWN * sizeof(f32));
    for (u32 i = 0; i < (u32)DXN; i++) dx[i] = (f32)((i * 11 + 5) % 19) * 0.09f - 0.85f;
    for (u32 i = 0; i < (u32)DWN; i++) dw[i] = (f32)((i * 3 + 2) % 11) * 0.05f - 0.27f;
    f32 *dref = calloc((size_t)DON, sizeof(f32));
    for (u32 co = 0; co < DCOUT; co++)
    for (u32 b = 0; b < DB; b++)
    for (u32 oh = 0; oh < DHO; oh++)
    for (u32 ow = 0; ow < DWO; ow++) {
      f32 acc = 0.0f;
      for (u32 ci = 0; ci < DCIN; ci++)
      for (u32 kh = 0; kh < DKH; kh++)
      for (u32 kw = 0; kw < DKW; kw++)
        acc += dw[((co*DCIN + ci)*DKH + kh)*DKW + kw]
             * dx[((b*DCIN + ci)*DH + (oh+kh))*DW + (ow+kw)];
      dref[((co*DB + b)*DHO + oh)*DWO + ow] = acc;
    }
    f32 *dcpu = calloc((size_t)DON, sizeof(f32));
    f32 *dgpu = calloc((size_t)DON, sizeof(f32));

    #define DUAL_CONV_BUILD_AND_RUN(out_buf)                                        do {                                                                            Shape sx_ = {0}; sx_.ndim = 4; sx_.dims[0] = DB; sx_.dims[1] = DCIN;           sx_.dims[2] = DH; sx_.dims[3] = DW;                                           Shape sw_ = {0}; sw_.ndim = 4; sw_.dims[0] = DCOUT; sw_.dims[1] = DCIN;        sw_.dims[2] = DKH; sw_.dims[3] = DKW;                                         u32 txid = tensor_alloc(CURRENT_BACKEND, sx_, DT_FP32);                       u32 twid = tensor_alloc(CURRENT_BACKEND, sw_, DT_FP32);                       CURRENT_BACKEND->buf_write(TENS[txid].buf_id, dx,                                                        (size_t)DXN * sizeof(f32));                        CURRENT_BACKEND->buf_write(TENS[twid].buf_id, dw,                                                        (size_t)DWN * sizeof(f32));                        Term xin = term_new(0, TAG_TEN, DT_FP32, txid);                               Term win = term_new(0, TAG_TEN, DT_FP32, twid);                               u32 rh = (DKH * (DH + 1) + DH - 1) / DH;                                      u32 rw = (DKW * (DW + 1) + DW - 1) / DW;                                      u32 d6a[6] = { DB, DCIN, 1, DH, 1, DW };                                      u32 d6b[6] = { DB, DCIN, rh, DH, rw, DW };                                    u32 d4a[4] = { DB, DCIN, rh*DH, rw*DW };                                      Term x1 = uop_reshape(uop_expand(uop_reshape(xin, 6, d6a), 6, d6b),                                 4, d4a);                                                u32 be4[8] = { 0,DB, 0,DCIN, 0,DKH*(DH+1), 0,DKW*(DW+1) };                    Term x2 = uop_shrink(x1, 4, be4);                                             u32 d6c[6] = { DB, DCIN, DKH, DH+1, DKW, DW+1 };                              Term x3 = uop_reshape(x2, 6, d6c);                                            u32 be6[12] = { 0,DB, 0,DCIN, 0,DKH, 0,DHO, 0,DKW, 0,DWO };                   Term x4 = uop_shrink(x3, 6, be6);                                             u32 prm[6] = { 1, 2, 4, 0, 3, 5 }; /* {cIn,kh,kw,B,hOut,wOut} */              Term xc6 = uop_permute(x4, 6, prm);                                           u32 d4x[4] = { DK, DB, DHO, DWO };                                            Term xc4 = uop_reshape(xc6, 4, d4x);                                          u32 d2w[2] = { DCOUT, DK };                                                   Term wf  = uop_reshape(win, 2, d2w);                                          u32 d5w[5] = { DCOUT, DK, 1, 1, 1 };                                          u32 d5e[5] = { DCOUT, DK, DB, DHO, DWO };                                     Term wexp = uop_expand(uop_reshape(wf, 5, d5w), 5, d5e);                      u32 d5x[5] = { 1, DK, DB, DHO, DWO };                                         Term xexp = uop_expand(uop_reshape(xc4, 5, d5x), 5, d5e);                     Term out4 = uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_MUL, wexp, xexp));                   Term done = wnf(thvm_materialize(out4));                                      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,                                             (out_buf), (size_t)DON * sizeof(f32));            } while (0)

    unsetenv("THVM_BACKEND"); thvm_init();
    DUAL_CONV_BUILD_AND_RUN(dcpu);
    thvm_free();

    setenv("THVM_BACKEND", "metal", 1); thvm_init();
    DUAL_CONV_BUILD_AND_RUN(dgpu);
    thvm_free();
    #undef DUAL_CONV_BUILD_AND_RUN

    for (u32 i = 0; i < (u32)DON; i++) {
      f32 d = dcpu[i] - dref[i]; if (d < 0) d = -d; CHECK(d < 5e-3f);
      d = dgpu[i] - dref[i];     if (d < 0) d = -d; CHECK(d < 5e-3f);
      d = dgpu[i] - dcpu[i];     if (d < 0) d = -d; CHECK(d < 5e-3f);
    }
    free(dx); free(dw); free(dref); free(dcpu); free(dgpu);
  }

  // === EXPAND parity (scalar -> tensor broadcast) ===
  TEST_BEGIN("metal-real/expand-scalar-to-tensor-parity");
  Shape sx = {0}; sx.ndim = 1; sx.dims[0] = 1;
  f32 src_x[1] = {7.5f};
  f32 cpu_e[5], gpu_e[5];

  unsetenv("THVM_BACKEND"); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, sx, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_x, sizeof(src_x));
    u32 dims[1] = {5};
    Term done = wnf(thvm_materialize(uop_expand(
        term_new(0, TAG_TEN, DT_FP32, t), 1, dims)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              cpu_e, sizeof(cpu_e));
  }
  thvm_free();

  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, sx, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_x, sizeof(src_x));
    u32 dims[1] = {5};
    Term done = wnf(thvm_materialize(uop_expand(
        term_new(0, TAG_TEN, DT_FP32, t), 1, dims)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              gpu_e, sizeof(gpu_e));
  }
  thvm_free();

  for (int i = 0; i < 5; i++) CHECK(cpu_e[i] == gpu_e[i]);

  // === RESHAPE parity (1D 6 -> 2D 2x3) ===
  TEST_BEGIN("metal-real/reshape-1d-to-2d-parity");
  Shape sm = {0}; sm.ndim = 1; sm.dims[0] = 6;
  f32 src_m[6] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f};
  f32 cpu_m[6], gpu_m[6];

  unsetenv("THVM_BACKEND"); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, sm, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_m, sizeof(src_m));
    u32 dims[2] = {2, 3};
    Term done = wnf(thvm_materialize(uop_reshape(
        term_new(0, TAG_TEN, DT_FP32, t), 2, dims)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              cpu_m, sizeof(cpu_m));
  }
  thvm_free();

  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, sm, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_m, sizeof(src_m));
    u32 dims[2] = {2, 3};
    Term done = wnf(thvm_materialize(uop_reshape(
        term_new(0, TAG_TEN, DT_FP32, t), 2, dims)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              gpu_m, sizeof(gpu_m));
  }
  thvm_free();

  for (int i = 0; i < 6; i++) CHECK(cpu_m[i] == gpu_m[i]);

  // === EXPAND axis-aware parity: leading- vs trailing-axis ===
  // Leading-axis broadcast {2,1} -> {2,3} should yield {a,a,a,b,b,b}
  // on both backends.  Pre-fix the Metal shader cycled in_numel,
  // producing {a,b,a,b,a,b}.
  TEST_BEGIN("metal-real/expand-leading-axis-parity");
  Shape s_la = {0}; s_la.ndim = 2; s_la.dims[0] = 2; s_la.dims[1] = 1;
  f32 src_la[2] = {3.0f, 7.0f};
  f32 cpu_la[6], gpu_la[6];
  u32 to_la[2] = {2, 3};

  unsetenv("THVM_BACKEND"); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, s_la, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_la, sizeof(src_la));
    Term done = wnf(thvm_materialize(uop_expand(
        term_new(0, TAG_TEN, DT_FP32, t), 2, to_la)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              cpu_la, sizeof(cpu_la));
  }
  thvm_free();

  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, s_la, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_la, sizeof(src_la));
    Term done = wnf(thvm_materialize(uop_expand(
        term_new(0, TAG_TEN, DT_FP32, t), 2, to_la)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              gpu_la, sizeof(gpu_la));
  }
  thvm_free();

  // Both backends should now produce {3,3,3,7,7,7}.
  for (int i = 0; i < 6; i++) CHECK(cpu_la[i] == gpu_la[i]);
  CHECK(cpu_la[0] == 3.0f && cpu_la[2] == 3.0f);
  CHECK(cpu_la[3] == 7.0f && cpu_la[5] == 7.0f);

  // Trailing-axis broadcast {1,3} -> {2,3} -- each row is the source.
  TEST_BEGIN("metal-real/expand-trailing-axis-parity");
  Shape s_ta = {0}; s_ta.ndim = 2; s_ta.dims[0] = 1; s_ta.dims[1] = 3;
  f32 src_ta[3] = {1.0f, 2.0f, 3.0f};
  f32 cpu_ta[6], gpu_ta[6];
  u32 to_ta[2] = {2, 3};

  unsetenv("THVM_BACKEND"); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, s_ta, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_ta, sizeof(src_ta));
    Term done = wnf(thvm_materialize(uop_expand(
        term_new(0, TAG_TEN, DT_FP32, t), 2, to_ta)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              cpu_ta, sizeof(cpu_ta));
  }
  thvm_free();

  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, s_ta, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_ta, sizeof(src_ta));
    Term done = wnf(thvm_materialize(uop_expand(
        term_new(0, TAG_TEN, DT_FP32, t), 2, to_ta)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              gpu_ta, sizeof(gpu_ta));
  }
  thvm_free();

  for (int i = 0; i < 6; i++) CHECK(cpu_ta[i] == gpu_ta[i]);
  CHECK(cpu_ta[0] == 1.0f && cpu_ta[1] == 2.0f && cpu_ta[2] == 3.0f);
  CHECK(cpu_ta[3] == 1.0f && cpu_ta[4] == 2.0f && cpu_ta[5] == 3.0f);

  // === FLIP axis-aware parity: 2D both axes ===
  // {{1,2,3},{4,5,6}} flipped on axes 0+1 -> {{6,5,4},{3,2,1}}.
  TEST_BEGIN("metal-real/flip-2d-both-axes-parity");
  Shape s_fl = {0}; s_fl.ndim = 2; s_fl.dims[0] = 2; s_fl.dims[1] = 3;
  f32 src_fl[6] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
  f32 cpu_fl[6], gpu_fl[6];

  unsetenv("THVM_BACKEND"); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, s_fl, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_fl, sizeof(src_fl));
    Term done = wnf(thvm_materialize(uop_flip(
        term_new(0, TAG_TEN, DT_FP32, t), 0x3)));   // flip both axes
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              cpu_fl, sizeof(cpu_fl));
  }
  thvm_free();

  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, s_fl, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_fl, sizeof(src_fl));
    Term done = wnf(thvm_materialize(uop_flip(
        term_new(0, TAG_TEN, DT_FP32, t), 0x3)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              gpu_fl, sizeof(gpu_fl));
  }
  thvm_free();

  for (int i = 0; i < 6; i++) CHECK(cpu_fl[i] == gpu_fl[i]);
  // Row-major: original {1,2,3,4,5,6} -> {6,5,4,3,2,1}.
  CHECK(cpu_fl[0] == 6.0f && cpu_fl[1] == 5.0f && cpu_fl[2] == 4.0f);
  CHECK(cpu_fl[3] == 3.0f && cpu_fl[4] == 2.0f && cpu_fl[5] == 1.0f);

  // === PAD axis-aware parity: 2D zero ring around a 2x2 ===
  // Pad {1,1} on each axis -> 4x4 with the source in the middle
  // and zeros around.  Per-axis (begin, end) widths interleaved.
  TEST_BEGIN("metal-real/pad-2d-symmetric-ring-parity");
  Shape s_pad = {0}; s_pad.ndim = 2; s_pad.dims[0] = 2; s_pad.dims[1] = 2;
  f32 src_pad[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 cpu_pad[16], gpu_pad[16];
  u32 be[4] = {1, 1, 1, 1};   // axis 0: (1,1); axis 1: (1,1)

  unsetenv("THVM_BACKEND"); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, s_pad, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_pad, sizeof(src_pad));
    Term done = wnf(thvm_materialize(uop_pad(
        term_new(0, TAG_TEN, DT_FP32, t), 2, be)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              cpu_pad, sizeof(cpu_pad));
  }
  thvm_free();

  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, s_pad, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_pad, sizeof(src_pad));
    Term done = wnf(thvm_materialize(uop_pad(
        term_new(0, TAG_TEN, DT_FP32, t), 2, be)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              gpu_pad, sizeof(gpu_pad));
  }
  thvm_free();

  for (int i = 0; i < 16; i++) CHECK(cpu_pad[i] == gpu_pad[i]);
  // Center should be the original 2x2; ring should be zero.
  CHECK(cpu_pad[5] == 1.0f && cpu_pad[6] == 2.0f);   // row 1, cols 1-2
  CHECK(cpu_pad[9] == 3.0f && cpu_pad[10] == 4.0f);  // row 2, cols 1-2
  CHECK(cpu_pad[0] == 0.0f && cpu_pad[15] == 0.0f);  // corners

  // === SHRINK axis-aware parity: 2D center-crop ===
  // {{1,2,3,4},{5,6,7,8},{9,10,11,12},{13,14,15,16}} kept on
  // [{1,3},{1,3}) -> 2x2 center {{6,7},{10,11}}.
  TEST_BEGIN("metal-real/shrink-2d-center-crop-parity");
  Shape s_sh = {0}; s_sh.ndim = 2; s_sh.dims[0] = 4; s_sh.dims[1] = 4;
  f32 src_sh[16] = {1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16};
  f32 cpu_sh[4], gpu_sh[4];
  u32 sh_be[4] = {1, 3, 1, 3};

  unsetenv("THVM_BACKEND"); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, s_sh, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_sh, sizeof(src_sh));
    Term done = wnf(thvm_materialize(uop_shrink(
        term_new(0, TAG_TEN, DT_FP32, t), 2, sh_be)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              cpu_sh, sizeof(cpu_sh));
  }
  thvm_free();

  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, s_sh, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_sh, sizeof(src_sh));
    Term done = wnf(thvm_materialize(uop_shrink(
        term_new(0, TAG_TEN, DT_FP32, t), 2, sh_be)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              gpu_sh, sizeof(gpu_sh));
  }
  thvm_free();

  for (int i = 0; i < 4; i++) CHECK(cpu_sh[i] == gpu_sh[i]);
  CHECK(cpu_sh[0] == 6.0f && cpu_sh[1] == 7.0f);
  CHECK(cpu_sh[2] == 10.0f && cpu_sh[3] == 11.0f);

  // === PERMUTE axis-aware parity: 2D transpose ===
  // {{1,2,3},{4,5,6}} permuted with axes {1,0} -> 3x2 transpose
  // {{1,4},{2,5},{3,6}}.
  TEST_BEGIN("metal-real/permute-2d-transpose-parity");
  Shape s_pe = {0}; s_pe.ndim = 2; s_pe.dims[0] = 2; s_pe.dims[1] = 3;
  f32 src_pe[6] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
  f32 cpu_pe[6], gpu_pe[6];
  u32 perm[2] = {1, 0};

  unsetenv("THVM_BACKEND"); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, s_pe, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_pe, sizeof(src_pe));
    Term done = wnf(thvm_materialize(uop_permute(
        term_new(0, TAG_TEN, DT_FP32, t), 2, perm)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              cpu_pe, sizeof(cpu_pe));
  }
  thvm_free();

  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    u32 t = tensor_alloc(CURRENT_BACKEND, s_pe, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[t].buf_id, src_pe, sizeof(src_pe));
    Term done = wnf(thvm_materialize(uop_permute(
        term_new(0, TAG_TEN, DT_FP32, t), 2, perm)));
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              gpu_pe, sizeof(gpu_pe));
  }
  thvm_free();

  for (int i = 0; i < 6; i++) CHECK(cpu_pe[i] == gpu_pe[i]);
  // Row-major after transpose: {1, 4, 2, 5, 3, 6}.
  CHECK(cpu_pe[0] == 1.0f && cpu_pe[1] == 4.0f);
  CHECK(cpu_pe[2] == 2.0f && cpu_pe[3] == 5.0f);
  CHECK(cpu_pe[4] == 3.0f && cpu_pe[5] == 6.0f);

  // === bm4c: Metal freelist primitives -- alloc, push, realloc
  // recycles same buf_id (mirrors test_cpu_free_list patterns). ===
  TEST_BEGIN("metal-real/freelist-push-then-alloc-recycles-slot");
  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    u32 a = METAL_BACKEND.buf_alloc(64);
    CHECK(a > 0);
    f32 sentinel[16];
    for (int i = 0; i < 16; i++) sentinel[i] = (f32)(i + 1);
    METAL_BACKEND.buf_write(a, sentinel, sizeof(sentinel));
    thvm_metal_buf_freelist_push(a);
    // Next 64-byte alloc must reuse `a`; data zeroed by pop.
    u32 b = METAL_BACKEND.buf_alloc(64);
    CHECK_EQ(b, a);
    f32 readback[16];
    METAL_BACKEND.buf_read(b, readback, sizeof(readback));
    for (int i = 0; i < 16; i++) CHECK(readback[i] == 0.0f);
  }
  thvm_free();

  TEST_BEGIN("metal-real/freelist-size-mismatch-misses");
  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    u32 a = METAL_BACKEND.buf_alloc(64);
    CHECK(a > 0);
    thvm_metal_buf_freelist_push(a);
    // 32-byte request can't reuse a 64-byte slot.
    u32 b = METAL_BACKEND.buf_alloc(32);
    CHECK(b != a);
  }
  thvm_free();

  TEST_BEGIN("metal-real/decref-then-alloc-recycles-slot");
  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    u32 a = METAL_BACKEND.buf_alloc(128);
    CHECK(a > 0);
    METAL_BACKEND.buf_decref(a);
    CHECK_EQ(METAL_BACKEND.buf_read(a, &a, sizeof(a)), -1);
    u32 b = METAL_BACKEND.buf_alloc(128);
    CHECK_EQ(b, a);
  }
  thvm_free();

  TEST_BEGIN("metal-real/free-then-alloc-reuses-empty-table-slot");
  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    u32 a = METAL_BACKEND.buf_alloc(64);
    CHECK(a > 0);
    METAL_BACKEND.buf_free(a);
    u32 b = METAL_BACKEND.buf_alloc(32);
    CHECK_EQ(b, a);
  }
  thvm_free();

  TEST_BEGIN("metal-real/buf-summary-tracks-live-retained-freelist");
  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    CHECK_EQ(thvm_metal_live_bytes(), 0);
    CHECK_EQ(thvm_metal_retained_bytes(), 0);
    CHECK_EQ(thvm_metal_deferred_bytes(), 0);
    CHECK_EQ(thvm_metal_deferred_len(), 0);
    CHECK_EQ(thvm_metal_freelist_len(), 0);
    CHECK_EQ(thvm_metal_peak_live_bytes(), 0);
    CHECK_EQ(thvm_metal_peak_retained_bytes(), 0);
    CHECK_EQ(thvm_metal_peak_deferred_bytes(), 0);

    u32 a = METAL_BACKEND.buf_alloc(64);
    CHECK(a > 0);
    CHECK_EQ(thvm_metal_live_bytes(), 64);
    CHECK_EQ(thvm_metal_retained_bytes(), 64);
    CHECK_EQ(thvm_metal_freelist_len(), 0);
    CHECK_EQ(thvm_metal_peak_live_bytes(), 64);
    CHECK_EQ(thvm_metal_peak_retained_bytes(), 64);

    thvm_metal_buf_freelist_push(a);
    CHECK_EQ(thvm_metal_live_bytes(), 0);
    CHECK_EQ(thvm_metal_retained_bytes(), 64);
    CHECK_EQ(thvm_metal_freelist_len(), 1);

    u32 b = METAL_BACKEND.buf_alloc(64);
    CHECK_EQ(b, a);
    CHECK_EQ(thvm_metal_live_bytes(), 64);
    CHECK_EQ(thvm_metal_retained_bytes(), 64);
    CHECK_EQ(thvm_metal_freelist_len(), 0);

    METAL_BACKEND.buf_decref(b);
    CHECK_EQ(thvm_metal_live_bytes(), 0);
    CHECK_EQ(thvm_metal_retained_bytes(), 64);
    CHECK_EQ(thvm_metal_freelist_len(), 1);
  }
  thvm_free();

  TEST_BEGIN("metal-real/freelist-byte-cap-zero-drops-dead-storage");
  setenv("THVM_BACKEND", "metal", 1);
  setenv("THVM_METAL_FREELIST_BYTES", "0", 1);
  thvm_init();
  {
    u32 a = METAL_BACKEND.buf_alloc(64);
    CHECK(a > 0);
    CHECK_EQ(thvm_metal_live_bytes(), 64);
    METAL_BACKEND.buf_decref(a);
    CHECK_EQ(thvm_metal_live_bytes(), 0);
    CHECK_EQ(thvm_metal_retained_bytes(), 0);
    CHECK_EQ(thvm_metal_freelist_len(), 0);
  }
  thvm_free();
  unsetenv("THVM_METAL_FREELIST_BYTES");

  TEST_BEGIN("metal-real/pool-rollback-preserves-marked-root");
  setenv("THVM_BACKEND", "metal", 1);
  setenv("THVM_METAL_FREELIST_BYTES", "0", 1);
  thvm_init();
  {
    Shape shape = {0};
    shape.ndim = 1;
    shape.dims[0] = 4;
    u32 wm = thvm_metal_buf_pool_begin();
    u32 keep_tid = tensor_alloc(&METAL_BACKEND, shape, DT_FP32);
    u32 drop_tid = tensor_alloc(&METAL_BACKEND, shape, DT_FP32);
    CHECK(keep_tid > 0 && drop_tid > 0);
    CHECK_EQ(thvm_metal_live_bytes(), 32);

    Term keep = term_new(0, TAG_TEN, DT_FP32, keep_tid);
    mark_gc_preserve(keep);
    thvm_metal_buf_pool_rollback_with_preserve(wm);
    thvm_metal_buf_clear_preserved(wm);

    CHECK_EQ(thvm_metal_live_bytes(), 16);
    CHECK_EQ(thvm_metal_retained_bytes(), 16);
    CHECK_EQ(thvm_metal_freelist_len(), 0);
    f32 tmp[4] = {0};
    CHECK_EQ(METAL_BACKEND.buf_read(TENS[keep_tid].buf_id, tmp, sizeof(tmp)), 0);
    CHECK_EQ(METAL_BACKEND.buf_read(TENS[drop_tid].buf_id, tmp, sizeof(tmp)), -1);
  }
  thvm_free();
  unsetenv("THVM_METAL_FREELIST_BYTES");

  TEST_BEGIN("metal-real/alias-reshape-drops-unused-output-immediately");
  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    Shape shape = {0};
    shape.ndim = 1;
    shape.dims[0] = 4;
    u32 src_tid = tensor_alloc(&METAL_BACKEND, shape, DT_FP32);
    u32 out_tid = tensor_alloc(&METAL_BACKEND, shape, DT_FP32);
    CHECK(src_tid > 0 && out_tid > 0);
    f32 src[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    METAL_BACKEND.buf_write(TENS[src_tid].buf_id, src, sizeof(src));

    u32 kid = kernel_alloc();
    KernelEntry *ke = &KERNELS[kid];
    kernel_inputs_reserve(ke, 1);
    kernel_program_reserve(ke, 1);
    ke->n_inputs        = 1;
    ke->input_tids[0]   = src_tid;
    ke->input_dtypes[0] = DT_FP32;
    ke->input_numels[0] = 4;
    ke->output_tid      = out_tid;
    ke->output_dtype    = DT_FP32;
    ke->output_numel    = 4;
    ke->n_ops           = 1;
    ke->program[0].opcode = UOP_RESHAPE;
    ke->program[0].dtype  = DT_FP32;
    ke->program[0].n_src  = 1;
    ke->program[0].src[0] = KSRC_AS_INPUT(0);
    ke->program[0].numel  = 4;

    u32 in_bufs[1] = {TENS[src_tid].buf_id};
    u32 old_out_buf = TENS[out_tid].buf_id;
    CHECK(old_out_buf != 0 && old_out_buf != in_bufs[0]);

    backend_dispatch_begin_all();
    CHECK_EQ(METAL_BACKEND.dispatch_kernel(ke, in_bufs, old_out_buf), 0);
    CHECK_EQ(cg_kernel_dispatch_kind(kid), (u32)KDISPATCH_METAL_ALIAS);
    CHECK_EQ(TENS[out_tid].buf_id, in_bufs[0]);
    CHECK_EQ(thvm_metal_deferred_len(), 0);
    CHECK_EQ(thvm_metal_deferred_bytes(), 0);
    CHECK_EQ(thvm_metal_peak_deferred_bytes(), 0);
    CHECK(thvm_metal_freelist_len() >= 1);
    backend_dispatch_end_all();

    CHECK_EQ(thvm_metal_deferred_len(), 0);
    CHECK_EQ(thvm_metal_deferred_bytes(), 0);
    CHECK_EQ(thvm_metal_live_bytes(), 16);
    CHECK(thvm_metal_retained_bytes() >= 16);

    u64 src_nbytes = 0;
    u32 src_refs = 0;
    u32 freelist_len = thvm_metal_freelist_len();
    thvm_metal_buf_get(in_bufs[0], &src_nbytes, &src_refs);
    CHECK_EQ(src_nbytes, 16);
    CHECK_EQ(src_refs, 2);

    METAL_BACKEND.buf_free(old_out_buf);
    backend_dispatch_begin_all();
    CHECK_EQ(METAL_BACKEND.dispatch_kernel(ke, in_bufs, old_out_buf), 0);
    CHECK_EQ(cg_kernel_dispatch_kind(kid), (u32)KDISPATCH_METAL_ALIAS);
    backend_dispatch_end_all();

    src_nbytes = 0;
    src_refs = 0;
    thvm_metal_buf_get(in_bufs[0], &src_nbytes, &src_refs);
    CHECK_EQ(src_nbytes, 16);
    CHECK_EQ(src_refs, 2);
    CHECK_EQ(thvm_metal_freelist_len(), freelist_len);
  }
  thvm_free();

  // === Phase C slice 5: DAG-side per-op encoder dispatches multi-output ===
  // A multi-output kernel has its tile_jit_encode rejected (because
  // cg_kernel_has_extra_outputs returns 1) and falls into the DAG-side
  // per-op encoder when cached_lift.store_root != 0.  The lifter's
  // kernel_lift_from_kprog path produces a UOP_AFTER chain of UOP_STOREs
  // for the splice-fused KProgOp[] -- the new encoder walks that chain
  // and emits the same Metal dispatches as the legacy program[] loop.
  TEST_BEGIN("metal-real/slice5-multi-output-via-dag-encoder");
  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    Shape shape = {0};
    shape.ndim = 1;
    shape.dims[0] = 4;
    // Two inputs; primary output = a + b; extra output = a * b.
    u32 ta = tensor_alloc(&METAL_BACKEND, shape, DT_FP32);
    u32 tb = tensor_alloc(&METAL_BACKEND, shape, DT_FP32);
    u32 t_primary = tensor_alloc(&METAL_BACKEND, shape, DT_FP32);
    u32 t_extra   = tensor_alloc(&METAL_BACKEND, shape, DT_FP32);
    CHECK(ta && tb && t_primary && t_extra);
    f32 va[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    f32 vb[4] = {10.0f, 20.0f, 30.0f, 40.0f};
    METAL_BACKEND.buf_write(TENS[ta].buf_id, va, sizeof(va));
    METAL_BACKEND.buf_write(TENS[tb].buf_id, vb, sizeof(vb));

    // Synthesize a multi-output KernelEntry whose program is:
    //   step 0: ADD in0 in1   -> primary
    //   step 1: MUL in0 in1   (store_extra_plus_one = 1)
    u32 kid = kernel_alloc();
    KernelEntry *ke = &KERNELS[kid];
    kernel_inputs_reserve(ke, 2);
    kernel_program_reserve(ke, 2);
    ke->n_inputs        = 2;
    ke->input_tids[0]   = ta; ke->input_tids[1]   = tb;
    ke->input_dtypes[0] = DT_FP32; ke->input_dtypes[1] = DT_FP32;
    ke->input_numels[0] = 4;       ke->input_numels[1] = 4;
    ke->output_tid      = t_primary;
    ke->output_dtype    = DT_FP32;
    ke->output_shape    = shape;
    ke->output_numel    = 4;
    ke->n_ops           = 2;
    // step 0: ADD -> primary (last op writes to outBuf by convention).
    ke->program[1].opcode = UOP_ADD;
    ke->program[1].dtype  = DT_FP32;
    ke->program[1].n_src  = 2;
    ke->program[1].src[0] = KSRC_AS_INPUT(0);
    ke->program[1].src[1] = KSRC_AS_INPUT(1);
    ke->program[1].numel  = 4;
    // step 1: MUL -> extra (store_extra_plus_one = 1 = extra index 0).
    ke->program[0].opcode = UOP_MUL;
    ke->program[0].dtype  = DT_FP32;
    ke->program[0].n_src  = 2;
    ke->program[0].src[0] = KSRC_AS_INPUT(0);
    ke->program[0].src[1] = KSRC_AS_INPUT(1);
    ke->program[0].numel  = 4;
    ke->program[0].store_extra_plus_one = 1;
    // Register the extra output.
    int set_ok = kernel_entry_set_extra_output(kid, 1, t_extra,
                                               DT_FP32, &shape, 4);
    CHECK_EQ(set_ok, 1);
    // Lift the kernel into a UOp DAG so cached_lift.store_root != 0
    // -- this routes dispatch through the slice 5 DAG encoder.
    CHECK_EQ(kernel_lift_to_uop(ke, &ke->cached_lift), 1);
    CHECK(ke->cached_lift.store_root != 0);
    CHECK_EQ(ke->cached_lift.n_outputs, 2u);

    u32 in_bufs[2] = {TENS[ta].buf_id, TENS[tb].buf_id};
    u32 primary_buf = TENS[t_primary].buf_id;
    backend_dispatch_begin_all();
    int rc = METAL_BACKEND.dispatch_kernel(ke, in_bufs, primary_buf);
    CHECK_EQ(rc, 0);
    // KDISPATCH_METAL_OP confirms we exercised the per-op encoder
    // path (either the legacy program[] loop or the slice 5 DAG
    // encoder; both report the same kind so this just verifies we
    // didn't sneak through tile_jit / alias_reshape).
    CHECK_EQ(cg_kernel_dispatch_kind(kid), (u32)KDISPATCH_METAL_OP);
    backend_dispatch_end_all();

    // Read back both outputs.
    f32 primary_out[4] = {0};
    f32 extra_out  [4] = {0};
    CHECK_EQ(METAL_BACKEND.buf_read(primary_buf, primary_out,
                                     sizeof(primary_out)), 0);
    CHECK_EQ(METAL_BACKEND.buf_read(TENS[t_extra].buf_id, extra_out,
                                     sizeof(extra_out)), 0);
    f32 expect_add[4] = {11.0f, 22.0f, 33.0f, 44.0f};
    f32 expect_mul[4] = {10.0f, 40.0f, 90.0f, 160.0f};
    for (u32 i = 0; i < 4; i++) {
      CHECK_EQ(primary_out[i], expect_add[i]);
      CHECK_EQ(extra_out  [i], expect_mul[i]);
    }
  }
  thvm_free();

  TEST_BEGIN("metal-real/slice5-lift-declined-falls-back-to-legacy");
  // When the lift declines (e.g. a kernel shape kernel_lift_from_kprog
  // doesn't recognise), cached_lift.store_root stays 0 and the dispatch
  // path falls through to the legacy KProgOp encoder.  This test
  // synthesises a single-output ADD kernel without lifting it -- the
  // dispatch must still succeed via the legacy path.
  setenv("THVM_BACKEND", "metal", 1); thvm_init();
  {
    Shape shape = {0};
    shape.ndim = 1;
    shape.dims[0] = 4;
    u32 ta = tensor_alloc(&METAL_BACKEND, shape, DT_FP32);
    u32 tb = tensor_alloc(&METAL_BACKEND, shape, DT_FP32);
    u32 t_out = tensor_alloc(&METAL_BACKEND, shape, DT_FP32);
    f32 va[4] = {5.0f, 6.0f, 7.0f, 8.0f};
    f32 vb[4] = {0.5f, 0.25f, 0.125f, 0.0625f};
    METAL_BACKEND.buf_write(TENS[ta].buf_id, va, sizeof(va));
    METAL_BACKEND.buf_write(TENS[tb].buf_id, vb, sizeof(vb));
    u32 kid = kernel_alloc();
    KernelEntry *ke = &KERNELS[kid];
    kernel_inputs_reserve(ke, 2);
    kernel_program_reserve(ke, 1);
    ke->n_inputs        = 2;
    ke->input_tids[0]   = ta; ke->input_tids[1]   = tb;
    ke->input_dtypes[0] = DT_FP32; ke->input_dtypes[1] = DT_FP32;
    ke->input_numels[0] = 4;       ke->input_numels[1] = 4;
    ke->output_tid      = t_out;
    ke->output_dtype    = DT_FP32;
    ke->output_shape    = shape;
    ke->output_numel    = 4;
    ke->n_ops           = 1;
    ke->program[0].opcode = UOP_ADD;
    ke->program[0].dtype  = DT_FP32;
    ke->program[0].n_src  = 2;
    ke->program[0].src[0] = KSRC_AS_INPUT(0);
    ke->program[0].src[1] = KSRC_AS_INPUT(1);
    ke->program[0].numel  = 4;
    // Deliberately do NOT call kernel_lift_to_uop -- cached_lift stays
    // zeroed.  The dispatcher's slice 5 gate sees store_root == 0 and
    // falls through to the legacy program[] loop.
    CHECK_EQ(ke->cached_lift.store_root, (Term)0);

    u32 in_bufs[2] = {TENS[ta].buf_id, TENS[tb].buf_id};
    u32 out_buf    = TENS[t_out].buf_id;
    backend_dispatch_begin_all();
    int rc = METAL_BACKEND.dispatch_kernel(ke, in_bufs, out_buf);
    // Single-op ADD likely takes the tile_jit path (which doesn't
    // require lift -- cg_emit_via_uop runs an on-demand fresh lift
    // when cached_lift is empty).  Either way the dispatch should
    // succeed; we just want to confirm slice 5's gate doesn't break
    // the no-lift path.
    CHECK_EQ(rc, 0);
    backend_dispatch_end_all();
    f32 out[4] = {0};
    CHECK_EQ(METAL_BACKEND.buf_read(out_buf, out, sizeof(out)), 0);
    f32 expect[4] = {5.5f, 6.25f, 7.125f, 8.0625f};
    for (u32 i = 0; i < 4; i++) CHECK_EQ(out[i], expect[i]);
  }
  thvm_free();

  unsetenv("THVM_BACKEND");
  TEST_REPORT();
}
