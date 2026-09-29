// test_cpu_jit_f64_reduce.c - an f64 reduce keeps f64 precision on the
// CPU JIT.
//
// tinygrad allocates a reduce's accumulator like the reduce
// (reduce_ranges_to_acc, codegen/__init__.py:197-199 at 365f87a8a), so an
// f64 sum or max is computed in f64.  The legacy C emit (render_uop.c),
// which renders every CPU JIT kernel without an UPCAST or UNROLL, declared
// each accumulator, and each value it hoists next to one, as `float`: the
// sum of 1 and three terms near 1e-10 came out 1.
//
// Every value below has bits under f32's 24-bit mantissa, and every sum
// and max is exact in f64, so the reference does not depend on the
// summation order.
//
// out[i] = reduce_j x[i, j] runs in f64 through the explicit-buffer C
// entry, compiled with the JIT's C compiler, in the legacy emit's shapes:
//   plain            one accumulator;
//   upcast           the output axis UPCAST: parallel accumulators;
//   unroll           the reduce axis UNROLLed into the accumulator;
//   upcast-shared    x[i, j] + w[j], output UPCAST: the shared w[j] load
//                    is hoisted into a local;
//   epilogue         reduce_j (x[i, j] + w[j]) + y[i]: the generic store
//                    path's accumulator;
//   epilogue-upcast  the same with the output axis UPCAST: lane-blocked
//                    accumulators and a hoisted lane-invariant w[j];
//   of-sum           max_j sum_k x[i, j, k]: nested accumulators.
// Realized f64 reduces then run end to end, among them kernels with an
// infinite literal or a NEG of a negative literal, which the JIT compiles
// since those literals render as C.  Whenever the JIT may compile a
// kernel, the kernel must have dispatched through it.

#include "../src/thvm.c"
#include "test.h"
#include <dlfcn.h>

#define NI 8
#define NJ 6
#define NK 3

enum { SH_PLAIN, SH_UPCAST, SH_UNROLL, SH_UPCAST_SHARED, SH_EPILOGUE,
       SH_EPILOGUE_UPCAST, SH_MAX_OF_SUM, SH_COUNT };

static char const *const SHAPE_NAME[SH_COUNT] = {
  "plain", "upcast", "unroll", "upcast-shared", "epilogue", "epilogue-upcast",
  "of-sum",
};

static int shape_has_w(u32 shape) {
  return shape == SH_UPCAST_SHARED || shape == SH_EPILOGUE
      || shape == SH_EPILOGUE_UPCAST;
}

static int shape_has_y(u32 shape) {
  return shape == SH_EPILOGUE || shape == SH_EPILOGUE_UPCAST;
}

// 1 +- a multiple of 2^-40: f32 rounds every one of them to +-1.
static f64 x_val(u32 i, u32 j) {
  f64 v = 1.0 + (f64)((i * 5 + j * 3) % 8 + 1) * 0x1p-40;
  return ((i + j) % 3 == 0) ? -v : v;
}

static f64 w_val(u32 j) {
  return 0.5 + (f64)(j + 1) * 0x1p-45;
}

static f64 y_val(u32 i) {
  return 0.25 + (f64)(i + 1) * 0x1p-42;
}

static f64 combine(u32 kind, f64 acc, f64 v) {
  if (kind == REDUCE_SUM) {
    return acc + v;
  }
  return (acc < v) ? v : acc;
}

static Term ic(u32 v) {
  return uop_const(DT_INT32, v);
}

static Term iadd(Term a, Term b) {
  return uop_int_binary(UOP_IADD, a, b);
}

static Term imul(Term a, Term b) {
  return uop_int_binary(UOP_IMUL, a, b);
}

typedef struct {
  u32  kind;
  u32  shape;
  Term root;
  Term out;
  Term ins[3];                  // x, w, y: instances 1..3
  f64  x[NI * NJ * NK];
  f64  w[NJ];
  f64  y[NI];
  f64  want[NI];
} Kern;

// out[i] = reduce_j body(i, j) [+ y[i]] with body = x[i*NJ + j] [+ w[j]],
// or out[i] = max_j sum_k x[(i*NJ + j)*NK + k], stored through a bare
// address like a lifted kernel.
static void build_kernel(Kern *k, u32 kind, u32 shape) {
  memset(k, 0, sizeof *k);
  k->kind  = kind;
  k->shape = shape;
  u32 xd[1] = { NI * NJ * NK };
  u32 wd[1] = { NJ };
  u32 od[1] = { NI };
  k->out    = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP64, 1, od, 0);
  k->ins[0] = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP64, 1, xd, 1);
  if (shape_has_w(shape)) {
    k->ins[1] = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP64, 1, wd, 2);
  }
  if (shape_has_y(shape)) {
    k->ins[2] = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP64, 1, od, 3);
  }
  Term i    = uop_range(0, KAX_LOOP, NI);
  Term j    = uop_range(1, KAX_REDUCE, NJ);
  Term body = uop_index_e(k->ins[0], iadd(imul(i, ic(NJ)), j));
  if (shape_has_w(shape)) {
    body = uop_binary(UOP_ADD, body, uop_index_e(k->ins[1], j));
  }
  if (shape == SH_MAX_OF_SUM) {
    Term kk   = uop_range(2, KAX_REDUCE, NK);
    Term addr = iadd(imul(iadd(imul(i, ic(NJ)), j), ic(NK)), kk);
    body = uop_reduce(REDUCE_SUM, 2, uop_index_e(k->ins[0], addr));
  }
  Term value = uop_reduce(kind, 1, body);
  if (shape_has_y(shape)) {
    value = uop_binary(UOP_ADD, value, uop_index_e(k->ins[2], i));
  }
  k->root = uop_store(k->out, i, value);
  if (shape == SH_UPCAST || shape == SH_UPCAST_SHARED
      || shape == SH_EPILOGUE_UPCAST) {
    KOpt opt = { KOP_UPCAST, 0, 4 };
    k->root = uop_dag_apply_kopt(k->root, opt);
  } else if (shape == SH_UNROLL) {
    KOpt opt = { KOP_UNROLL, 1, 3 };
    k->root = uop_dag_apply_kopt(k->root, opt);
  }
  for (u32 jj = 0; jj < NJ; jj++) {
    k->w[jj] = w_val(jj);
  }
  for (u32 ii = 0; ii < NI; ii++) {
    k->y[ii] = y_val(ii);
    f64 acc = (kind == REDUCE_MAX) ? -INFINITY : 0.0;
    for (u32 jj = 0; jj < NJ; jj++) {
      f64 b = 0.0;
      if (shape == SH_MAX_OF_SUM) {
        for (u32 q = 0; q < NK; q++) {
          f64 xv = x_val(ii, jj * NK + q);
          k->x[(ii * NJ + jj) * NK + q] = xv;
          b += xv;
        }
      } else {
        b = x_val(ii, jj);
        k->x[ii * NJ + jj] = b;
      }
      if (shape_has_w(shape)) {
        b += k->w[jj];
      }
      acc = combine(kind, acc, b);
    }
    if (shape_has_y(shape)) {
      acc += k->y[ii];
    }
    k->want[ii] = acc;
  }
}

static char const *kern_name(Kern const *k) {
  static char name[96];
  snprintf(name, sizeof name, "legacy-c/f64-%s-%s",
           k->kind == REDUCE_MAX ? "max" : "sum", SHAPE_NAME[k->shape]);
  return name;
}

// The rendered kernels, concatenated into one translation unit that is
// compiled once: the preludes repeat identical includes, typedefs and
// macros, which C11 accepts.
static char  *SRC;
static size_t SRC_LEN;
static size_t SRC_CAP;

static void src_append(char const *s, size_t n) {
  if (SRC_LEN + n + 1 > SRC_CAP) {
    SRC_CAP = (SRC_LEN + n + 1) * 2;
    SRC     = (char *)realloc(SRC, SRC_CAP);
  }
  memcpy(SRC + SRC_LEN, s, n);
  SRC_LEN += n;
  SRC[SRC_LEN] = 0;
}

static void render_legacy(Kern const *k, char const *fn_name) {
  Term   ins[3];
  u32    n   = 0;
  char  *buf = NULL;
  size_t len = 0;
  for (u32 s = 0; s < 3; s++) {
    if (k->ins[s] != 0) {
      ins[n++] = k->ins[s];
    }
  }
  FILE *fp = open_memstream(&buf, &len);
  cg_render_uop_kernel_c(k->root, fn_name, k->out, ins, n, fp);
  fclose(fp);
  src_append(buf, len);
  free(buf);
}

static void *compile_src(void) {
  char const *tmp = getenv("TMPDIR");
  char path[512];
  snprintf(path, sizeof path, "%s/thvm_f64_reduce_XXXXXX.c",
           (tmp != NULL && tmp[0] != '\0') ? tmp : "/tmp");
  int fd = mkstemps(path, 2);
  if (fd < 0) {
    return NULL;
  }
  FILE *cf = fdopen(fd, "w");
  fputs(SRC, cf);
  fclose(cf);
  char so[600];
  snprintf(so, sizeof so, "%s.so", path);
  char cmd[1400];
  snprintf(cmd, sizeof cmd, "%s -O1 -shared -fPIC -o '%s' '%s'", cpu_cc(), so, path);
  void *h = (system(cmd) == 0) ? dlopen(so, RTLD_NOW) : NULL;
  if (h == NULL) {
    fprintf(stderr, "  compile failed: %s\n", cmd);
  }
  unlink(path);
  unlink(so);
  SRC_LEN = 0;
  return h;
}

typedef void (*LegacyFn)(void *, const void *const *, unsigned,
                         const unsigned *, const unsigned *);

// The explicit-buffer entry binds the inputs in order.
static void run_legacy(Kern const *k, LegacyFn f, f64 *res) {
  const void *all[3]  = { k->x, k->w, k->y };
  unsigned    alln[3] = { NI * NJ * NK, NJ, NI };
  const void *ins[3];
  unsigned    numel[3];
  u32 n = 0;
  for (u32 s = 0; s < 3; s++) {
    if (k->ins[s] != 0) {
      ins[n]   = all[s];
      numel[n] = alln[s];
      n++;
    }
  }
  f(res, ins, NI, numel, NULL);
}

static Kern KERNS[2 * SH_COUNT];

static void check_legacy_shapes(void) {
  char fn_name[32];
  u32  n = 0;
  for (u32 kind = 0; kind < 2; kind++) {
    for (u32 sh = 0; sh < SH_COUNT; sh++) {
      if (sh == SH_MAX_OF_SUM && kind != REDUCE_MAX) {
        continue;
      }
      build_kernel(&KERNS[n], kind, sh);
      snprintf(fn_name, sizeof fn_name, "k_f64_%u", n);
      render_legacy(&KERNS[n], fn_name);
      n++;
    }
  }
  void *h = compile_src();
  TEST_BEGIN("legacy-c/compiles");
  CHECK(h != NULL);
  for (u32 q = 0; q < n; q++) {
    TEST_BEGIN(kern_name(&KERNS[q]));
    snprintf(fn_name, sizeof fn_name, "k_f64_%u", q);
    LegacyFn f = (h != NULL) ? (LegacyFn)dlsym(h, fn_name) : NULL;
    CHECK(f != NULL);
    if (f == NULL) {
      continue;
    }
    f64 res[NI];
    memset(res, 0, sizeof res);
    run_legacy(&KERNS[q], f, res);
    for (u32 i = 0; i < NI; i++) {
      if (res[i] != KERNS[q].want[i]) {
        fprintf(stderr, "  %s: out[%u] got %a want %a\n", test_name, i,
                res[i], KERNS[q].want[i]);
      }
      CHECK(res[i] == KERNS[q].want[i]);
    }
  }
  if (h != NULL) {
    dlclose(h);
  }
}

// === Realized reduces ======================================================

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

// A kernel runs compiled from the fire at which its source has fired
// THVM_CPU_JIT_WARMUP times, on every JIT path; each kernel here fires
// once.
static int jit_may_compile(void) {
  return cpu_jit_warmup() <= 1;
}

static void check_realized(char const *name, Term expr, f64 want) {
  TEST_BEGIN(name);
  u32  k0   = KERNELS_NEXT;
  Term done = term_resolve(thvm_realize(expr));
  u32  k1   = KERNELS_NEXT;
  CHECK(k1 > k0);
  CHECK(term_tag(done) == TAG_TEN);
  if (term_tag(done) == TAG_TEN) {
    u32 tid = (u32)term_val(done);
    CHECK_EQ(TENS[tid].dtype, DT_FP64);
    f64 got = 0.0;
    TENS[tid].backend->buf_read(TENS[tid].buf_id, &got, sizeof got);
    if (got != want) {
      fprintf(stderr, "  %s: got %a want %a\n", name, got, want);
    }
    CHECK(got == want);
  }
  if (jit_may_compile()) {
    for (u32 kid = k0; kid < k1; kid++) {
      CHECK_EQ(cg_kernel_dispatch_kind(kid), KDISPATCH_JIT);
    }
  }
}

static void check_realized_reduces(void) {
  f64 const a[4] = { 1.0, 0x1p-33, 0x1p-34, 0x1p-35 };
  f64 const sum_a = 1.0 + 0x1p-33 + 0x1p-34 + 0x1p-35;
  f64 const b[4] = { 1.0 + 0x1p-30, 1.0, 0.5, -3.0 };
  check_realized("realized/f64-sum", uop_reduce(REDUCE_SUM, 0, f64_buffer(a)), sum_a);
  check_realized("realized/f64-max", uop_reduce(REDUCE_MAX, 0, f64_buffer(b)), b[0]);
  Term xa = f64_buffer(a);
  Term lt = uop_binary(UOP_CMPLT, xa, f64_const(INFINITY));
  check_realized("realized/f64-sum-x-times-cast-x-lt-inf",
                 uop_reduce(REDUCE_SUM, 0, uop_binary(UOP_MUL, xa, uop_cast(lt, DT_FP64))),
                 sum_a);
  check_realized("realized/f64-sum-where-x-lt-inf",
                 uop_reduce(REDUCE_SUM, 0, uop_iwhere(lt, xa, f64_const(0.0f))), sum_a);
  Term xb = f64_buffer(b);
  check_realized("realized/f64-max-where-x-lt-inf-else-neg-inf",
                 uop_reduce(REDUCE_MAX, 0,
                            uop_iwhere(uop_binary(UOP_CMPLT, xb, f64_const(INFINITY)),
                                       xb, f64_const(-INFINITY))),
                 b[0]);
  check_realized("realized/f64-sum-x-plus-neg-of-negative-const",
                 uop_reduce(REDUCE_SUM, 0,
                            uop_binary(UOP_ADD, f64_buffer(a),
                                       uop_unary(UOP_NEG, f64_const(-1.0f)))),
                 4.0 + sum_a);
  check_realized("realized/f64-sum-x-times-neg-of-negative-const",
                 uop_reduce(REDUCE_SUM, 0,
                            uop_binary(UOP_MUL, f64_buffer(a),
                                       uop_unary(UOP_NEG, f64_const(-1.0f)))),
                 sum_a);
}

int main(void) {
  thvm_init();
  check_legacy_shapes();
  check_realized_reduces();
  thvm_free();
  TEST_REPORT();
}
