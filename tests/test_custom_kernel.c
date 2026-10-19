// Raw custom-kernel injection bridge (kernel_emit_custom_program +
// kernel_custom_fire): hand-written C kernels run as first-class ops,
// eagerly AND under JIT capture/replay.
//
// The replay half is exactly the bug class tinygrad hit with raw
// Ops.PROGRAM (codegen/__init__.py do_to_program): a raw program whose
// outs the planner cannot see gets its scratch recycled and its
// dispatches liveness-skipped.  Here the capture ends WITHOUT result
// roots, so the ordinary reverse-liveness pass would skip every
// dispatch; the custom-op exemption must keep them firing, the scratch
// buffer of the 2-kernel chain must stay held, and each of the 3
// replays must see that call's fresh input bytes.
#include "../src/thvm.c"
#include "test.h"

static u32 mk_f32(u32 n, float const *data) {
  Shape s = {0};
  s.ndim = 1;
  s.dims[0] = n;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP32);
  if (data != NULL) {
    CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data, (u64)n * 4);
  } else {
    float z[64] = {0};
    CPU_BACKEND.buf_write(TENS[tid].buf_id, z, (u64)n * 4);
  }
  return tid;
}

// CPU custom-kernel ABI: void <name>(void *const *bufs,
// unsigned const *grid, unsigned const *block); grid[0] carries n.
static char const *SRC_ADDONE =
  "void addone(void *const *bufs, unsigned const *grid,\n"
  "            unsigned const *block) {\n"
  "  (void)block;\n"
  "  const float *x = (const float *)bufs[0];\n"
  "  float       *y = (float *)bufs[1];\n"
  "  for (unsigned i = 0; i < grid[0]; i++) y[i] = x[i] + 1.0f;\n"
  "}\n";

static char const *SRC_DOUBLE =
  "void dbl(void *const *bufs, unsigned const *grid,\n"
  "         unsigned const *block) {\n"
  "  (void)block;\n"
  "  const float *x = (const float *)bufs[0];\n"
  "  float       *s = (float *)bufs[1];\n"
  "  for (unsigned i = 0; i < grid[0]; i++) s[i] = x[i] * 2.0f;\n"
  "}\n";

static char const *SRC_ADD3 =
  "void add3(void *const *bufs, unsigned const *grid,\n"
  "          unsigned const *block) {\n"
  "  (void)block;\n"
  "  const float *s = (const float *)bufs[0];\n"
  "  float       *y = (float *)bufs[1];\n"
  "  for (unsigned i = 0; i < grid[0]; i++) y[i] = s[i] + 3.0f;\n"
  "}\n";

// One kernel, TWO written buffers (outs = slots 1 and 2): the capture
// records slot 1 as the primary out; slot 2 is visible only through
// the outs mask, so a liveness pass keyed on out_buf_id alone would
// mis-skip the op when only y2 is read.
static char const *SRC_TWO_OUT =
  "void twoout(void *const *bufs, unsigned const *grid,\n"
  "            unsigned const *block) {\n"
  "  (void)block;\n"
  "  const float *x  = (const float *)bufs[0];\n"
  "  float       *y1 = (float *)bufs[1];\n"
  "  float       *y2 = (float *)bufs[2];\n"
  "  for (unsigned i = 0; i < grid[0]; i++) {\n"
  "    y1[i] = x[i] + 1.0f;\n"
  "    y2[i] = x[i] * 10.0f;\n"
  "  }\n"
  "}\n";

int main(void) {
  thvm_init();
  TEST_BEGIN("custom-kernel/eager+jit-capture-replay");
  u32 n = 8;
  u32 grid[3]  = {n, 1, 1};
  u32 block[3] = {1, 1, 1};
  float xv[8], out[8], scratchv[8];

  // === 1. eager add-one ==============================================
  for (u32 i = 0; i < n; i++) xv[i] = (float)i;
  u32 x = mk_f32(n, xv);
  u32 y = mk_f32(n, NULL);
  u32 args1[2]  = {x, y};
  u32 outs1[1]  = {1};
  u32 k_add = kernel_emit_custom_program("addone", SRC_ADDONE, grid, block,
                                         args1, 2, outs1, 1, 0);
  CHECK(k_add != 0);
  CHECK(kernel_custom_fire(k_add) == 0);
  CPU_BACKEND.buf_read(TENS[y].buf_id, out, (u64)n * 4);
  printf("  eager addone: out[3] = %.1f (want 4)\n", out[3]);
  for (u32 i = 0; i < n; i++) CHECK(out[i] == xv[i] + 1.0f);

  // Repeat fire on mutated input: an explicit custom fire always
  // executes (no fire-generation memo).
  for (u32 i = 0; i < n; i++) xv[i] = (float)(i + 100);
  CPU_BACKEND.buf_write(TENS[x].buf_id, xv, (u64)n * 4);
  CHECK(kernel_custom_fire(k_add) == 0);
  CPU_BACKEND.buf_read(TENS[y].buf_id, out, (u64)n * 4);
  for (u32 i = 0; i < n; i++) CHECK(out[i] == xv[i] + 1.0f);

  // === 2. eager 2-kernel chain through a scratch buffer =============
  u32 cx      = mk_f32(n, xv);
  u32 scratch = mk_f32(n, NULL);
  u32 cy      = mk_f32(n, NULL);
  u32 args_d[2] = {cx, scratch};
  u32 args_a[2] = {scratch, cy};
  u32 outs_s[1] = {1};
  u32 k_dbl  = kernel_emit_custom_program("dbl", SRC_DOUBLE, grid, block,
                                          args_d, 2, outs_s, 1, 0);
  u32 k_add3 = kernel_emit_custom_program("add3", SRC_ADD3, grid, block,
                                          args_a, 2, outs_s, 1, 0);
  CHECK(k_dbl != 0 && k_add3 != 0);
  CHECK(kernel_custom_fire(k_dbl) == 0);
  CHECK(kernel_custom_fire(k_add3) == 0);
  CPU_BACKEND.buf_read(TENS[cy].buf_id, out, (u64)n * 4);
  printf("  eager chain: out[3] = %.1f (want %.1f)\n",
         out[3], xv[3] * 2.0f + 3.0f);
  for (u32 i = 0; i < n; i++) CHECK(out[i] == xv[i] * 2.0f + 3.0f);

  // === 3. JIT capture of the chain + 3 replays with fresh inputs ====
  // No result roots at capture end: only the custom-op liveness
  // exemption keeps the two dispatches alive on replay.
  u32 slot = jit_capture_begin();
  CHECK(slot != 0);
  CHECK(kernel_custom_fire(k_dbl) == 0);
  CHECK(kernel_custom_fire(k_add3) == 0);
  jit_capture_end();
  CHECK(jit_capture_op_count(slot) == 2);

  for (u32 rep = 1; rep <= 3; rep++) {
    for (u32 i = 0; i < n; i++) xv[i] = (float)(10 * rep + i);
    CPU_BACKEND.buf_write(TENS[cx].buf_id, xv, (u64)n * 4);
    // Poison the previous outputs so a skipped dispatch is caught.
    float poison[8];
    for (u32 i = 0; i < n; i++) poison[i] = -999.0f;
    CPU_BACKEND.buf_write(TENS[scratch].buf_id, poison, (u64)n * 4);
    CPU_BACKEND.buf_write(TENS[cy].buf_id,      poison, (u64)n * 4);
    jit_replay(slot);
    CPU_BACKEND.buf_read(TENS[scratch].buf_id, scratchv, (u64)n * 4);
    CPU_BACKEND.buf_read(TENS[cy].buf_id,      out,      (u64)n * 4);
    printf("  replay %u: scratch[0] = %.1f (want %.1f), out[0] = %.1f "
           "(want %.1f)\n", rep, scratchv[0], xv[0] * 2.0f,
           out[0], xv[0] * 2.0f + 3.0f);
    for (u32 i = 0; i < n; i++) CHECK(scratchv[i] == xv[i] * 2.0f);
    for (u32 i = 0; i < n; i++) CHECK(out[i] == xv[i] * 2.0f + 3.0f);
  }
  jit_capture_drop(slot);

  // === 4. multi-out kernel under capture/replay ======================
  u32 mx  = mk_f32(n, xv);
  u32 my1 = mk_f32(n, NULL);
  u32 my2 = mk_f32(n, NULL);
  u32 args_m[3] = {mx, my1, my2};
  u32 outs_m[2] = {1, 2};
  u32 k_two = kernel_emit_custom_program("twoout", SRC_TWO_OUT, grid, block,
                                         args_m, 3, outs_m, 2, 0);
  CHECK(k_two != 0);
  slot = jit_capture_begin();
  CHECK(slot != 0);
  CHECK(kernel_custom_fire(k_two) == 0);
  jit_capture_end();

  for (u32 rep = 1; rep <= 3; rep++) {
    for (u32 i = 0; i < n; i++) xv[i] = (float)(7 * rep + i);
    CPU_BACKEND.buf_write(TENS[mx].buf_id, xv, (u64)n * 4);
    jit_replay(slot);
    float o1[8], o2[8];
    CPU_BACKEND.buf_read(TENS[my1].buf_id, o1, (u64)n * 4);
    CPU_BACKEND.buf_read(TENS[my2].buf_id, o2, (u64)n * 4);
    for (u32 i = 0; i < n; i++) CHECK(o1[i] == xv[i] + 1.0f);
    for (u32 i = 0; i < n; i++) CHECK(o2[i] == xv[i] * 10.0f);
  }
  jit_capture_drop(slot);

  // === 4b. outs listed out of order: each slot keeps its own buffer ===
  TEST_BEGIN("custom-kernel/outs-in-any-order");
  {
    u32 rx  = mk_f32(n, xv);
    u32 ry1 = mk_f32(n, NULL);
    u32 ry2 = mk_f32(n, NULL);
    u32 args_r[3] = {rx, ry1, ry2};
    u32 outs_r[2] = {2, 1};
    u32 k_rev = kernel_emit_custom_program("twoout", SRC_TWO_OUT, grid, block,
                                           args_r, 3, outs_r, 2, 0);
    CHECK(k_rev != 0);
    CHECK(kernel_custom_fire(k_rev) == 0);
    float o1[8], o2[8];
    CPU_BACKEND.buf_read(TENS[ry1].buf_id, o1, (u64)n * 4);
    CPU_BACKEND.buf_read(TENS[ry2].buf_id, o2, (u64)n * 4);
    for (u32 i = 0; i < n; i++) CHECK(o1[i] == xv[i] + 1.0f);
    for (u32 i = 0; i < n; i++) CHECK(o2[i] == xv[i] * 10.0f);
  }

  // === 5. emit-time validation rejects bad specs =====================
  u32 bad_out[1] = {5};
  CHECK(kernel_emit_custom_program("addone", SRC_ADDONE, grid, block,
                                   args1, 2, bad_out, 1, 0) == 0);
  CHECK(kernel_emit_custom_program("", SRC_ADDONE, grid, block,
                                   args1, 2, outs1, 1, 0) == 0);
  CHECK(kernel_custom_fire(0) == -1);

  TEST_REPORT();
}
