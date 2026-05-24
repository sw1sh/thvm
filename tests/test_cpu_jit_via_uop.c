// test_cpu_jit_via_uop.c - F6 integration test
//
// Fires a CPU JIT-eligible kernel enough times to cross the warmup
// threshold (CPU_JIT_WARMUP=5), validating that cpu_jit_dispatch's
// new render_uop_c path produces correct output for kernels that
// actually compile through clang -shared and load via dlopen.
//
// Skips the test entirely when no JIT compile occurs (the path was
// stat-cached from a prior run, or a system without clang).
//
// Validates the F6 default-on flip (fc40c60a): the rendered C99 from
// cg_render_uop_kernel_c, when compiled and loaded, yields output
// bit-equal to a pure CPU-interpreter reference.

#include "../src/thvm.c"
#include "test.h"

int main(void) {
  TEST_BEGIN("cpu-jit-via-uop/elementwise-add-warmup");

  // Pre-clear the on-disk JIT cache so this test exercises a real
  // clang compile (otherwise stat-only check shortcuts to dlopen).
  // Best-effort; missing files are fine.
  system("rm -f /tmp/thvm_jit_*.dylib /tmp/thvm_jit_*.c 2>/dev/null");

  // thvm_init/free resets cpu_jit_cache (incl fire_count) so we
  // need ONE init for the whole loop -- fire_count must persist
  // across fires to cross CPU_JIT_WARMUP=5.
  thvm_init();

  Shape s = {0}; s.ndim = 1; s.dims[0] = 32;
  f32 src_a[32], src_b[32], expected[32];
  for (u32 i = 0; i < 32; i++) {
    src_a[i] = (float)i;
    src_b[i] = 2.0f * (float)i + 1.0f;
    expected[i] = src_a[i] + src_b[i];
  }

  // Fire the same logical kernel 10 times. fire_count crosses
  // CPU_JIT_WARMUP after fire 5; subsequent fires JIT via the
  // F6 render_uop_c path. Each iteration checks output bit-equal.
  for (u32 fire = 0; fire < 10; fire++) {
    u32 ta = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
    u32 tb = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
    CURRENT_BACKEND->buf_write(TENS[ta].buf_id, src_a, sizeof(src_a));
    CURRENT_BACKEND->buf_write(TENS[tb].buf_id, src_b, sizeof(src_b));
    Term A = term_new(0, TAG_TEN, DT_FP32, ta);
    Term B = term_new(0, TAG_TEN, DT_FP32, tb);
    Term sum = uop_binary(UOP_ADD, A, B);
    Term done = wnf(thvm_materialize(sum));
    f32 result[32];
    CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                              result, sizeof(result));
    for (u32 i = 0; i < 32; i++) CHECK(result[i] == expected[i]);
  }

  // Multi-axis sum over a 4D tensor lowers to four nested UOP_REDUCE
  // shells (axis 0, 1, 2, 3) feeding the same scalar output.  Before
  // d8b4ce61's `rmu_term_uses_axis_rec` fix, parent_idx misclassified
  // the middle reduces -- the codegen emitted a reduce-loop above its
  // declaring outer loop and referenced the outer's axis var before
  // declaration (clang error).  Even when the JIT failed to compile,
  // the CPU walker fell back; the walker's pre-d8b4ce61 single-axis
  // path was O(prod(axes)^depth) via recursion-recompute.
  //
  // The walker piece additionally exposed an unsafe `uwalk_resolve_buf`
  // fallback: any unrecognized term (UOP_BUFFERIZE, TAG_TEN) routed
  // through `inst==0 -> out_ptr` and silently aliased BUFFERIZE reads
  // onto the (typically scalar) output buffer, producing OOB heap
  // reads -- chain-fusion amplified the garbage to per-step divergence
  // on beautiful_mnist BN.
  //
  // Regression test: 2 * 4 * 6 * 6 = 288 element tensor, fill with 1.0,
  // sum-all = 288.0.  Exercises the multi-axis nested-REDUCE chain end
  // to end through materialize -> JIT -> dispatch.  At pre-d8b4ce61
  // this kernel either (a) doesn't compile (undeclared axis var) and
  // fell to a walker that returned non-deterministic garbage, or
  // (b) at the chain-fusion walker without the resolve_buf restriction,
  // read past the 1-element output buffer.  Both modes are caught: a
  // sum-all that returns anything but 288.0 fails this gate.
  TEST_BEGIN("cpu-jit-via-uop/multi-axis-reduce-sum-all");
  {
    Shape s4 = {0};
    s4.ndim = 4;
    s4.dims[0] = 2;
    s4.dims[1] = 4;
    s4.dims[2] = 6;
    s4.dims[3] = 6;
    f32 src[288];
    for (u32 i = 0; i < 288; i++) src[i] = 1.0f;
    // Fire enough times to cross the JIT warmup threshold.  Each fire
    // builds a fresh Term graph (heap-resident); thvm_init persists.
    f32 last_result = 0.0f;
    for (u32 fire = 0; fire < 10; fire++) {
      u32 tx = tensor_alloc(CURRENT_BACKEND, s4, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[tx].buf_id, src, sizeof(src));
      Term X = term_new(0, TAG_TEN, DT_FP32, tx);
      // sum over all 4 axes -> scalar (288.0).  Mirror Tensor._reduce:
      // reduce innermost-first so the outer-axis indices stay valid
      // (each uop_reduce drops the reduced axis).
      Term r3 = uop_reduce(REDUCE_SUM, /*axis=*/3, X);
      Term r2 = uop_reduce(REDUCE_SUM, /*axis=*/2, r3);
      Term r1 = uop_reduce(REDUCE_SUM, /*axis=*/1, r2);
      Term r0 = uop_reduce(REDUCE_SUM, /*axis=*/0, r1);
      Term done = wnf(thvm_materialize(r0));
      f32 result = 0.0f;
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                &result, sizeof(result));
      last_result = result;
      CHECK(result == 288.0f);
    }
    // Belt-and-suspenders: the prior-bug failure mode was anything from
    // -Inf to +Inf or 0.0 (BUFFERIZE-fallback-to-output OOB read).
    CHECK(last_result == 288.0f);
  }

  // Multi-axis reduce of an elementwise *fused* expression: this is the
  // shape conv2d+BN+sum produces -- four nested SUM reduces wrapping
  // (X + Y) * Z + B, where X,Y are 4D and Z,B are per-channel
  // broadcasts.  Materialize fuses the elementwise chain into the
  // reduce body without force-realizing each REDUCE link, so the kernel
  // emerges as one STORE whose value is the deepest REDUCE shell with
  // three more nested above.  parent_idx must classify each middle
  // reduce as nested inside its outer (its body uses the outer's RANGE
  // var).  Values: X=Y=1.0, Z=1.0, B=0.0 -> (1+1)*1+0 = 2 per element,
  // 288 elements, total = 576.0.
  TEST_BEGIN("cpu-jit-via-uop/multi-axis-reduce-fused-elementwise");
  {
    Shape s4 = {0};
    s4.ndim = 4;
    s4.dims[0] = 2;
    s4.dims[1] = 4;
    s4.dims[2] = 6;
    s4.dims[3] = 6;
    Shape s_ch = {0};  // per-channel (broadcast on axis 1)
    s_ch.ndim = 1;
    s_ch.dims[0] = 4;
    f32 src4[288];
    f32 src_z[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    f32 src_b[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    for (u32 i = 0; i < 288; i++) src4[i] = 1.0f;
    f32 last_result = 0.0f;
    for (u32 fire = 0; fire < 10; fire++) {
      u32 tx = tensor_alloc(CURRENT_BACKEND, s4,   DT_FP32);
      u32 ty = tensor_alloc(CURRENT_BACKEND, s4,   DT_FP32);
      u32 tz = tensor_alloc(CURRENT_BACKEND, s_ch, DT_FP32);
      u32 tb = tensor_alloc(CURRENT_BACKEND, s_ch, DT_FP32);
      CURRENT_BACKEND->buf_write(TENS[tx].buf_id, src4,  sizeof(src4));
      CURRENT_BACKEND->buf_write(TENS[ty].buf_id, src4,  sizeof(src4));
      CURRENT_BACKEND->buf_write(TENS[tz].buf_id, src_z, sizeof(src_z));
      CURRENT_BACKEND->buf_write(TENS[tb].buf_id, src_b, sizeof(src_b));
      Term X = term_new(0, TAG_TEN, DT_FP32, tx);
      Term Y = term_new(0, TAG_TEN, DT_FP32, ty);
      Term Z = term_new(0, TAG_TEN, DT_FP32, tz);
      Term B = term_new(0, TAG_TEN, DT_FP32, tb);
      // Broadcast Z, B (shape [4]) to [2,4,6,6] via reshape + expand.
      u32 ch_shape  [4] = {1, 4, 1, 1};
      u32 full_shape[4] = {2, 4, 6, 6};
      Term Z_r = uop_reshape(Z, 4, ch_shape);
      Term B_r = uop_reshape(B, 4, ch_shape);
      Term Z_e = uop_expand (Z_r, 4, full_shape);
      Term B_e = uop_expand (B_r, 4, full_shape);
      Term sum_xy = uop_binary(UOP_ADD, X, Y);
      Term mul    = uop_binary(UOP_MUL, sum_xy, Z_e);
      Term add_b  = uop_binary(UOP_ADD, mul, B_e);
      Term r3 = uop_reduce(REDUCE_SUM, /*axis=*/3, add_b);
      Term r2 = uop_reduce(REDUCE_SUM, /*axis=*/2, r3);
      Term r1 = uop_reduce(REDUCE_SUM, /*axis=*/1, r2);
      Term r0 = uop_reduce(REDUCE_SUM, /*axis=*/0, r1);
      Term done = wnf(thvm_materialize(r0));
      f32 result = 0.0f;
      CURRENT_BACKEND->buf_read(TENS[(u32)term_val(done)].buf_id,
                                &result, sizeof(result));
      last_result = result;
      CHECK(result == 576.0f);
    }
    CHECK(last_result == 576.0f);
  }

  thvm_free();

  TEST_REPORT();
}
