// test_cpu_jit_nonfinite.c - the CPU JIT compiles kernels whose f64
// literals are infinite, NaN, negative or -0, and keeps their values.
//
// The legacy C emit (render_uop.c) spells a float CONST of any dtype but
// f32 as a decimal literal.  It printed an infinity or NaN as `inf` /
// `nan`, which are not C tokens, and a negative literal behind a NEG as
// `(--2.5)`, so the JIT compile failed ("'inf' undeclared", "lvalue
// required as decrement operand") and the kernel ran in the UOp walker:
// the right values, orders of magnitude slower.  tinygrad spells a
// non-finite constant with its renderer's names for infinity and NaN
// (renderer/cstyle.py:25-26, 137-138 at 365f87a8a); the C target uses
// INFINITY / NAN from math.h.  A finite literal is a floating constant, as
// tinygrad's str(float) makes it (cstyle.py:37): `-0` is the int 0, which
// drops the sign of zero.
//
// Each case realizes on the CPU and checks the values and the signs of
// zeros, compiles the kernel's JIT source with the JIT's own compiler and
// flags (cpu_jit_compile_shared), and, unless THVM_CPU_JIT_WARMUP sends
// every kernel to the walker, checks that the kernel dispatched through the
// JIT.  The literal checks render f64 constants through the
// C emit.

#include "../src/thvm.c"
#include "test.h"

static u32 bits_of(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return b;
}

static Term splat4(Term c) {
  u32 const one[1]  = { 1 };
  u32 const four[1] = { 4 };
  return uop_expand(uop_reshape(c, 1, one), 1, four);
}

static Term f64_const(f32 v) {
  return splat4(uop_const(DT_FP64, bits_of(v)));
}

static Term f64_buffer(f64 const *v) {
  Shape s   = {0};
  s.ndim    = 1;
  s.dims[0] = 4;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, DT_FP64);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, v, 4 * sizeof(f64));
  return term_new(0, TAG_TEN, DT_FP64, tid);
}

static int compiles_on_jit(KernelEntry const *ke) {
  static u32 n = 0;
  char *src = cpu_jit_render_canon(ke);
  if (src == NULL) {
    return 0;
  }
  char src_path[128], dl_path[128];
  snprintf(src_path, sizeof src_path, "/tmp/thvm_nonfinite_%d_%u.c", (int)getpid(), n);
  snprintf(dl_path, sizeof dl_path, "/tmp/thvm_nonfinite_%d_%u.so", (int)getpid(), n);
  n++;
  FILE *f = fopen(src_path, "w");
  if (f == NULL) {
    free(src);
    return 0;
  }
  fputs(src, f);
  fclose(f);
  free(src);
  int ok = cpu_jit_compile_shared(src_path, dl_path, 0);
  unlink(src_path);
  unlink(dl_path);
  return ok;
}

static void check_case(char const *name, Term expr, f64 const *want) {
  TEST_BEGIN(name);
  u32  k0   = KERNELS_NEXT;
  Term done = term_resolve(thvm_realize(expr));
  u32  k1   = KERNELS_NEXT;
  CHECK(k1 > k0);
  CHECK(term_tag(done) == TAG_TEN);
  if (term_tag(done) == TAG_TEN) {
    u32 tid = (u32)term_val(done);
    CHECK_EQ(TENS[tid].dtype, DT_FP64);
    CHECK_EQ(TENS[tid].view.numel, 4);
    f64 got[4];
    TENS[tid].backend->buf_read(TENS[tid].buf_id, got, sizeof got);
    for (u32 i = 0; i < 4; i++) {
      CHECK((isnan(want[i]) && isnan(got[i])) || got[i] == want[i]);
      CHECK(isnan(want[i]) || !signbit(got[i]) == !signbit(want[i]));
    }
  }
  for (u32 kid = k0; kid < k1; kid++) {
    CHECK(compiles_on_jit(&KERNELS[kid]));
    if (!cpu_jit_walker_only()) {
      CHECK_EQ(cg_kernel_dispatch_kind(kid), KDISPATCH_JIT);
    }
  }
}

// The C emit spells a finite f64 CONST as a floating constant.
static void check_c_literal(f32 v, char const *want) {
  static char name[64];
  snprintf(name, sizeof name, "c-literal/%s", want);
  TEST_BEGIN(name);
  char got[64] = {0};
  FILE *fp = fmemopen(got, sizeof got - 1, "w");
  cg_target saved = RMU_TARGET;
  RMU_TARGET = CG_TARGET_C;
  rmu_emit_term(uop_const(DT_FP64, bits_of(v)), fp);
  RMU_TARGET = saved;
  fclose(fp);
  if (strcmp(got, want) != 0) {
    fprintf(stderr, "  %s: got %s\n", name, got);
  }
  CHECK(strcmp(got, want) == 0);
}

int main(void) {
  thvm_init();

  check_c_literal(-0.0f, "(-0.0)");
  check_c_literal(0.0f, "0.0");
  check_c_literal(1.0f, "1.0");
  check_c_literal(16777216.0f, "16777216.0");
  check_c_literal(-2.5f, "(-2.5)");
  check_c_literal(0.5f, "0.5");
  check_c_literal(1e20f, "1.0000000200408773e+20");

  f64 const x[4]    = { 1.0, -2.0, 3.0, 0.5 };
  f64 const pinf[4] = { INFINITY, INFINITY, INFINITY, INFINITY };
  f64 const ninf[4] = { -INFINITY, -INFINITY, -INFINITY, -INFINITY };
  f64 const nan4[4] = { NAN, NAN, NAN, NAN };
  f64 const two5[4] = { 2.5, 2.5, 2.5, 2.5 };
  f64 const x25[4]  = { 2.5, -5.0, 7.5, 1.25 };

  Term e5m2_inf = splat4(uop_const(DT_FP8E5M2, bits_of(INFINITY)));
  check_case("cpu-jit/f64-cast-of-fp8e5m2-inf", uop_cast(e5m2_inf, DT_FP64), pinf);
  check_case("cpu-jit/f64-add-inf", uop_binary(UOP_ADD, f64_buffer(x), f64_const(INFINITY)), pinf);
  check_case("cpu-jit/f64-add-neg-inf",
             uop_binary(UOP_ADD, f64_buffer(x), f64_const(-INFINITY)), ninf);
  check_case("cpu-jit/f64-mul-nan", uop_binary(UOP_MUL, f64_buffer(x), f64_const(NAN)), nan4);
  check_case("cpu-jit/f64-neg-of-neg-inf", uop_unary(UOP_NEG, f64_const(-INFINITY)), pinf);
  check_case("cpu-jit/f64-neg-of-negative-const", uop_unary(UOP_NEG, f64_const(-2.5f)), two5);
  check_case("cpu-jit/f64-mul-neg-of-negative-const",
             uop_binary(UOP_MUL, f64_buffer(x), uop_unary(UOP_NEG, f64_const(-2.5f))), x25);

  // -0.0 keeps its sign: the int 0 that `(-0)` spells would turn x * -0.0
  // and z + -0.0 into +0 where IEEE gives -0.
  f64 const z[4]   = { -0.0, -0.0, 1.0, -0.0 };
  f64 const xnz[4] = { -0.0, 0.0, -0.0, -0.0 };
  f64 const znz[4] = { -0.0, -0.0, 1.0, -0.0 };
  check_case("cpu-jit/f64-mul-neg-zero", uop_binary(UOP_MUL, f64_buffer(x), f64_const(-0.0f)), xnz);
  check_case("cpu-jit/f64-add-neg-zero", uop_binary(UOP_ADD, f64_buffer(z), f64_const(-0.0f)), znz);

  thvm_free();
  TEST_REPORT();
}
