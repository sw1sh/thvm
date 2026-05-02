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

  ke->axes = &ke->_local_axes;
  memset(ke->axes, 0, sizeof(KernelAxes));
  ke->axes->n_axes = 2;
  if (local_first) {
    ke->axes->axis_types[0] = KAX_LOCAL;
    ke->axes->full_shape[0] = threads;
    ke->axes->axis_types[1] = KAX_GLOBAL;
    ke->axes->full_shape[1] = groups;
  } else {
    ke->axes->axis_types[0] = KAX_GLOBAL;
    ke->axes->full_shape[0] = groups;
    ke->axes->axis_types[1] = KAX_LOCAL;
    ke->axes->full_shape[1] = threads;
  }
  ke->axes->version++;
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
    backend_dispatch_end_all();

    src_nbytes = 0;
    src_refs = 0;
    thvm_metal_buf_get(in_bufs[0], &src_nbytes, &src_refs);
    CHECK_EQ(src_nbytes, 16);
    CHECK_EQ(src_refs, 2);
    CHECK_EQ(thvm_metal_freelist_len(), freelist_len);
  }
  thvm_free();

  unsetenv("THVM_BACKEND");
  TEST_REPORT();
}
