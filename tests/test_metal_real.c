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

int main(void) {
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
  CURRENT_BACKEND->buf_decref(bid2);  // drops to 0; frees.
  // Now invalid; read should fail.
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
  Term cpu_kern = thvm_materialize(uop_const(DT_F32, cu.u));
  Term cpu_done = wnf(cpu_kern);
  CHECK_EQ(term_tag(cpu_done), TAG_TEN);
  u32 cpu_tid = (u32)term_val(cpu_done);
  f32 cpu_out;
  CHECK_EQ(CPU_BACKEND.buf_read(TENS[cpu_tid].buf_id, &cpu_out, sizeof(f32)), 0);
  thvm_free();

  // Same graph under Metal; output should match bit-for-bit.
  setenv("THVM_BACKEND", "metal", 1);
  thvm_init();
  Term gpu_kern = thvm_materialize(uop_const(DT_F32, cu.u));
  Term gpu_done = wnf(gpu_kern);
  CHECK_EQ(term_tag(gpu_done), TAG_TEN);
  u32 gpu_tid = (u32)term_val(gpu_done);
  f32 gpu_out;
  CHECK_EQ(METAL_BACKEND.buf_read(TENS[gpu_tid].buf_id, &gpu_out, sizeof(f32)), 0);
  CHECK(cpu_out == gpu_out);
  CHECK(gpu_out == 3.14f);
  thvm_free();

  // === Binary elementwise parity (ADD, MUL, CMPLT) ===
  // Build [1,2,3,4] + [10,20,30,40] (etc.) under both backends,
  // verify identical output buffers.

  TEST_BEGIN("metal-real/add-mul-cmplt-parity-with-cpu");
  Shape s = {0}; s.ndim = 1; s.dims[0] = 4;
  f32 src_a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  f32 src_b[4] = {10.0f, 20.0f, 30.0f, 40.0f};

  for (int op_idx = 0; op_idx < 3; op_idx++) {
    u32 op = (op_idx == 0) ? UOP_ADD : (op_idx == 1) ? UOP_MUL : UOP_CMPLT;
    f32 cpu_buf[4], gpu_buf[4];

    unsetenv("THVM_BACKEND"); thvm_init();
    {
      u32 ta = tensor_alloc(CURRENT_BACKEND, s, DT_F32);
      u32 tb = tensor_alloc(CURRENT_BACKEND, s, DT_F32);
      CURRENT_BACKEND->buf_write(TENS[ta].buf_id, src_a, sizeof(src_a));
      CURRENT_BACKEND->buf_write(TENS[tb].buf_id, src_b, sizeof(src_b));
      Term done = wnf(thvm_materialize(uop_binary(op,
          term_new(0, TAG_TEN, DT_F32, ta),
          term_new(0, TAG_TEN, DT_F32, tb))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                cpu_buf, sizeof(cpu_buf));
    }
    thvm_free();

    setenv("THVM_BACKEND", "metal", 1); thvm_init();
    {
      u32 ta = tensor_alloc(CURRENT_BACKEND, s, DT_F32);
      u32 tb = tensor_alloc(CURRENT_BACKEND, s, DT_F32);
      CURRENT_BACKEND->buf_write(TENS[ta].buf_id, src_a, sizeof(src_a));
      CURRENT_BACKEND->buf_write(TENS[tb].buf_id, src_b, sizeof(src_b));
      Term done = wnf(thvm_materialize(uop_binary(op,
          term_new(0, TAG_TEN, DT_F32, ta),
          term_new(0, TAG_TEN, DT_F32, tb))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                gpu_buf, sizeof(gpu_buf));
    }
    thvm_free();

    for (int i = 0; i < 4; i++) CHECK(cpu_buf[i] == gpu_buf[i]);
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
      u32 tid = tensor_alloc(CURRENT_BACKEND, su, DT_F32);
      CURRENT_BACKEND->buf_write(TENS[tid].buf_id, src_u, sizeof(src_u));
      Term done = wnf(thvm_materialize(uop_unary(op,
          term_new(0, TAG_TEN, DT_F32, tid))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                cpu_buf, sizeof(cpu_buf));
    }
    thvm_free();

    setenv("THVM_BACKEND", "metal", 1); thvm_init();
    {
      u32 tid = tensor_alloc(CURRENT_BACKEND, su, DT_F32);
      CURRENT_BACKEND->buf_write(TENS[tid].buf_id, src_u, sizeof(src_u));
      Term done = wnf(thvm_materialize(uop_unary(op,
          term_new(0, TAG_TEN, DT_F32, tid))));
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
      u32 tid = tensor_alloc(CURRENT_BACKEND, sr, DT_F32);
      CURRENT_BACKEND->buf_write(TENS[tid].buf_id, src_r, sizeof(src_r));
      Term done = wnf(thvm_materialize(uop_reduce(kind, 0,
          term_new(0, TAG_TEN, DT_F32, tid))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                cpu_buf, sizeof(cpu_buf));
    }
    thvm_free();

    setenv("THVM_BACKEND", "metal", 1); thvm_init();
    {
      u32 tid = tensor_alloc(CURRENT_BACKEND, sr, DT_F32);
      CURRENT_BACKEND->buf_write(TENS[tid].buf_id, src_r, sizeof(src_r));
      Term done = wnf(thvm_materialize(uop_reduce(kind, 0,
          term_new(0, TAG_TEN, DT_F32, tid))));
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                gpu_buf, sizeof(gpu_buf));
    }
    thvm_free();

    for (int k = 0; k < 4; k++) CHECK(cpu_buf[k] == gpu_buf[k]);
  }

  unsetenv("THVM_BACKEND");
  TEST_REPORT();
}
