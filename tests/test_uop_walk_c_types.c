// test_uop_walk_c_types.c - the CPU UOp walker computes each value in the
// C type the legacy C emit gives it.
//
// The C emit types an f32 value `float` and an f64 one `double`, as it
// renders their buffers (rmu_c_type_name).  RECIP renders `1.0f/x`, a float
// unless x is a double; EXP2, LOG2 and SQRT call math.h's double functions;
// a CONST of a float dtype other than f32 is a double literal.  An op over
// mixed types converts by C's usual arithmetic conversions, so an int
// operand of a float op reaches f32 rounded once, and a float reduce
// accumulator is a `float` that each `acc = acc + v` rounds into.  The
// walker evaluated every float value in doubles, so it disagreed with the
// CPU JIT: py's int32 1312 / 41, which is 1312 * (1.0f/41), is 31.999998
// in float and truncates to 31, but 32 in doubles.
//
// Each kernel runs through the walker (cpu_uop_walk) and through the
// legacy C emit's explicit-buffer entry, compiled as the CPU JIT compiles
// it, and both must store the bytes C gives.  The expected values are
// computed here in C, one rounding per statement.  The guard kernels hold
// a double value in a double: a sqrt or a double CONST that a float op
// would round, and an f64 quotient.
#include "../src/thvm.c"
#include "test.h"
#include <dlfcn.h>

#define MAXI 3
#define NO   8
#define NJ   8
#define MAXE (NO * NJ)

typedef struct {
  char const *name;
  Term        root;
  Term        out;
  u32         out_dt;
  u32         n_out;
  u32         n_in;
  Term        ins[MAXI];
  u32         in_dt[MAXI];
  u32         in_n[MAXI];
  u8          in_data[MAXI][8 * MAXE];
  u8          want[8 * NO];
} Kern;

static Kern KERNS[32];
static u32  N_KERNS;

static Kern *kern(char const *name, u32 out_dt, u32 n_out) {
  Kern *k = &KERNS[N_KERNS++];
  memset(k, 0, sizeof *k);
  k->name   = name;
  k->out_dt = out_dt;
  k->n_out  = n_out;
  u32 d[1] = { n_out };
  k->out = uop_buffer_inst(UOP_SCOPE_GLOBAL, out_dt, 1, d, 0);
  return k;
}

// Input slot s (buffer instance s + 1): n elements of dtype dt.
static Term input(Kern *k, u32 dt, u32 n, void const *data) {
  u32 s = k->n_in++;
  u32 d[1] = { n };
  k->ins[s]   = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt, 1, d, s + 1);
  k->in_dt[s] = dt;
  k->in_n[s]  = n;
  memcpy(k->in_data[s], data, (size_t)n * dtype_itemsize(dt));
  return k->ins[s];
}

static void want(Kern *k, void const *data) {
  memcpy(k->want, data, (size_t)k->n_out * dtype_itemsize(k->out_dt));
}

static Term f32c(f32 v) {
  u32 bits;
  memcpy(&bits, &v, sizeof bits);
  return uop_const(DT_FP32, bits);
}

static Term f64c(f32 v) {
  u32 bits;
  memcpy(&bits, &v, sizeof bits);
  return uop_const(DT_FP64, bits);
}

static Term at_i(void) {
  return uop_range(0, KAX_LOOP, NO);
}

// Element [i, j] of a row-major NO x NJ input, j reduced.
static Term at_ij(void) {
  Term j = uop_range(1, KAX_REDUCE, NJ);
  return uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, at_i(), uop_const(DT_INT32, NJ)), j);
}

static Term ld(Kern *k, u32 s, Term addr) {
  return uop_index_e(k->ins[s], addr);
}

static Term quot(Term a, Term b) {
  return uop_binary(UOP_MUL, a, uop_unary(UOP_RECIP, b));
}

// === Expected values: one C rounding per statement ========================

static f32 quot_f32(f32 a, f32 b) {
  f32 r = 1.0f / b;
  f32 q = a * r;
  return q;
}

// === Kernels ==============================================================

static void build_elementwise(void) {
  {
    i32 const a[NO] = { 1312, -1312, 1271, 7, -7, 9, 1, 100 };
    i32 const b[NO] = { 41, 41, 41, 2, 2, 4, 3, 7 };
    i32 w[NO];
    for (u32 i = 0; i < NO; i++) {
      w[i] = (i32)quot_f32((f32)a[i], (f32)b[i]);
    }
    Kern *k = kern("i32-quotient", DT_INT32, NO);
    input(k, DT_INT32, NO, a);
    input(k, DT_INT32, NO, b);
    k->root = uop_store(k->out, at_i(), quot(ld(k, 0, at_i()), ld(k, 1, at_i())));
    want(k, w);
  }
  {
    f32 const x[NO] = { 1312.0f, 1271.0f, 1.0f, 10.0f, 3.0f, -1312.0f, 0.1f, 7.0f };
    f32 const y[NO] = { 41.0f, 41.0f, 3.0f, 3.0f, 10.0f, 41.0f, 3.0f, 7.0f };
    f32 w[NO];
    for (u32 i = 0; i < NO; i++) {
      w[i] = quot_f32(x[i], y[i]);
    }
    Kern *k = kern("f32-quotient", DT_FP32, NO);
    input(k, DT_FP32, NO, x);
    input(k, DT_FP32, NO, y);
    k->root = uop_store(k->out, at_i(), quot(ld(k, 0, at_i()), ld(k, 1, at_i())));
    want(k, w);
  }
  {
    f32 const x[NO] = { 1.0f, 1.0f, 3.0f, -1.0f, 1e8f, 0.1f, 16777216.0f, 1.5f };
    f32 const y[NO] = { 0x1p-24f, 0x1p-23f, 0x1p-23f, -0x1p-25f, 3.0f, 0.2f, 1.0f, 0x1p-24f };
    f32 const z[NO] = { 0x1p-24f, 0x1p-24f, 0x1p-23f, -0x1p-25f, 3.0f, 0.3f, 1.0f, 0x1p-24f };
    f32 w[NO];
    for (u32 i = 0; i < NO; i++) {
      f32 s = x[i] + y[i];
      w[i] = s + z[i];
    }
    Kern *k = kern("f32-sum-chain", DT_FP32, NO);
    input(k, DT_FP32, NO, x);
    input(k, DT_FP32, NO, y);
    input(k, DT_FP32, NO, z);
    Term s = uop_binary(UOP_ADD, ld(k, 0, at_i()), ld(k, 1, at_i()));
    k->root = uop_store(k->out, at_i(), uop_binary(UOP_ADD, s, ld(k, 2, at_i())));
    want(k, w);
  }
  {
    f32 const x[NO] = { 1.0f + 0x1p-12f, 3.0f, 0.1f, 1.0f + 0x1p-13f, -1.5f, 1e-20f,
                        65535.0f, 1.0f + 0x1p-23f };
    f64 w[NO];
    for (u32 i = 0; i < NO; i++) {
      f32 p = x[i] * x[i];
      w[i] = (f64)p;
    }
    Kern *k = kern("cast-f32-square-to-f64", DT_FP64, NO);
    input(k, DT_FP32, NO, x);
    Term v = ld(k, 0, at_i());
    k->root = uop_store(k->out, at_i(), uop_cast(uop_binary(UOP_MUL, v, v), DT_FP64));
    want(k, w);
  }
  {
    i64 const v[NO] = { (1 << 24) + 1, (1 << 24) + 3, ((i64)1 << 53) + 1, -((1 << 24) + 1),
                        7, ((i64)1 << 40) + 1, 0, 1 };
    f32 const f[NO] = { 0.5f, 0.5f, 0.0f, -0.5f, 0.25f, 0.5f, 0.1f, 0x1p-24f };
    f32 w[NO];
    for (u32 i = 0; i < NO; i++) {
      f32 a = (f32)v[i];
      w[i] = a + f[i];
    }
    Kern *k = kern("i64-plus-f32", DT_FP32, NO);
    input(k, DT_INT64, NO, v);
    input(k, DT_FP32, NO, f);
    k->root = uop_store(k->out, at_i(), uop_binary(UOP_ADD, ld(k, 0, at_i()), ld(k, 1, at_i())));
    want(k, w);
  }
  {
    u64 const v[NO] = { ((u64)1 << 63) + ((u64)1 << 39) + 1, UINT64_MAX, (1 << 24) + 1,
                        1, ((u64)1 << 63) + 1, 12345, ((u64)1 << 40) + 1, 0 };
    f32 const f[NO] = { 0.5f, 1.0f, 0.5f, 0x1p-24f, 0.0f, 0.5f, 0.5f, -0.0f };
    f32 w[NO];
    for (u32 i = 0; i < NO; i++) {
      f32 a = (f32)v[i];
      w[i] = a + f[i];
    }
    Kern *k = kern("u64-plus-f32", DT_FP32, NO);
    input(k, DT_UINT64, NO, v);
    input(k, DT_FP32, NO, f);
    k->root = uop_store(k->out, at_i(), uop_binary(UOP_ADD, ld(k, 0, at_i()), ld(k, 1, at_i())));
    want(k, w);
  }
  {
    f32 const f[NO] = { 0.25f, 1.0f, 2.0f, 0.1f, 5.0f, -1.0f, 3.0f, 0.75f };
    i32 const y[NO] = { 1, 16777217, 16777219, 5, 2147483647, 3, -16777217, 9 };
    f64 w[NO];
    for (u32 i = 0; i < NO; i++) {
      f32 v = (f[i] < 0.5f) ? f[i] : (f32)y[i];
      w[i] = (f64)v;
    }
    Kern *k = kern("where-f32-or-i32-to-f64", DT_FP64, NO);
    input(k, DT_FP32, NO, f);
    input(k, DT_INT32, NO, y);
    Term fv  = ld(k, 0, at_i());
    Term sel = uop_iwhere(uop_binary(UOP_CMPLT, fv, f32c(0.5f)), fv, ld(k, 1, at_i()));
    k->root = uop_store(k->out, at_i(), uop_cast(sel, DT_FP64));
    want(k, w);
  }
  {
    i32 const a[NO] = { 16777217, 16777217, 5, -16777217, 2147483647, 100, 1, 0 };
    f32 const f[NO] = { 16777216.0f, 16777218.0f, 5.5f, -16777216.0f, 2147483648.0f, 99.5f,
                        1.0f, -0.0f };
    i32 wl[NO], we[NO];
    for (u32 i = 0; i < NO; i++) {
      f32 af = (f32)a[i];
      wl[i] = (f[i] < af) ? 1 : 0;
      we[i] = (af == f[i]) ? 1 : 0;
    }
    Kern *k = kern("f32-less-than-i32", DT_INT32, NO);
    input(k, DT_INT32, NO, a);
    input(k, DT_FP32, NO, f);
    Term lt = uop_binary(UOP_CMPLT, ld(k, 1, at_i()), ld(k, 0, at_i()));
    k->root = uop_store(k->out, at_i(), uop_iwhere(lt, uop_const(DT_INT32, 1),
                                                uop_const(DT_INT32, 0)));
    want(k, wl);
    k = kern("i32-equals-f32", DT_INT32, NO);
    input(k, DT_INT32, NO, a);
    input(k, DT_FP32, NO, f);
    Term eq = uop_binary(UOP_CMPEQ, ld(k, 0, at_i()), ld(k, 1, at_i()));
    k->root = uop_store(k->out, at_i(), uop_iwhere(eq, uop_const(DT_INT32, 1),
                                                uop_const(DT_INT32, 0)));
    want(k, we);
  }
}

static void build_reduces(void) {
  {
    f32 x[NO * NJ];
    for (u32 n = 0; n < NO * NJ; n++) {
      u32 i = n / NJ, j = n % NJ;
      x[n] = (i % 2 == 0) ? ((j == 0) ? (f32)(i + 1) : 0x1p-24f * (f32)(i + 1))
                          : 0.1f * (f32)(j + i);
    }
    f32 w[NO];
    for (u32 i = 0; i < NO; i++) {
      f32 acc = 0.0f;
      for (u32 j = 0; j < NJ; j++) {
        acc = acc + x[i * NJ + j];
      }
      w[i] = acc;
    }
    Kern *k = kern("f32-sum", DT_FP32, NO);
    input(k, DT_FP32, NO * NJ, x);
    k->root = uop_store(k->out, at_i(), uop_reduce(REDUCE_SUM, 1, ld(k, 0, at_ij())));
    want(k, w);
  }
  {
    f32 x[NO * NJ];
    for (u32 n = 0; n < NO * NJ; n++) {
      x[n] = (f32)(2 + (n % 5) + 3 * (n / NJ));
    }
    f32 w[NO];
    for (u32 i = 0; i < NO; i++) {
      f32 acc = 0.0f;
      for (u32 j = 0; j < NJ; j++) {
        f64 s = (f64)acc + sqrt((f64)x[i * NJ + j]);
        acc = (f32)s;
      }
      w[i] = acc;
    }
    Kern *k = kern("f32-sum-of-sqrt", DT_FP32, NO);
    input(k, DT_FP32, NO * NJ, x);
    k->root = uop_store(k->out, at_i(), uop_reduce(REDUCE_SUM, 1,
                                                uop_unary(UOP_SQRT, ld(k, 0, at_ij()))));
    want(k, w);
  }
  {
    i32 a[NO * NJ], b[NO * NJ];
    for (u32 n = 0; n < NO * NJ; n++) {
      a[n] = 0;
      b[n] = 1;
    }
    i32 const a0[4] = { 1312, 0, -1312, 1271 };
    i32 const a1[4] = { 1271, 41, 0, 7 };
    for (u32 j = 0; j < 4; j++) {
      a[j] = a0[j];
      b[j] = 41;
      a[NJ + j] = a1[j];
      b[NJ + j] = (j == 3) ? 2 : 41;
    }
    for (u32 n = 2 * NJ; n < NO * NJ; n++) {
      a[n] = (i32)(n * 37 % 101) - 50;
      b[n] = (i32)(n % 7) + 2;
    }
    i32 w[NO];
    for (u32 i = 0; i < NO; i++) {
      i32 acc = 0;
      for (u32 j = 0; j < NJ; j++) {
        f32 q = quot_f32((f32)a[i * NJ + j], (f32)b[i * NJ + j]);
        f32 s = (f32)acc + q;
        acc = (i32)s;
      }
      w[i] = acc;
    }
    Kern *k = kern("i32-sum-of-quotients", DT_INT32, NO);
    input(k, DT_INT32, NO * NJ, a);
    input(k, DT_INT32, NO * NJ, b);
    k->root = uop_store(k->out, at_i(), uop_reduce(REDUCE_SUM, 1,
                                                quot(ld(k, 0, at_ij()), ld(k, 1, at_ij()))));
    want(k, w);
  }
}

static void build_guards(void) {
  {
    f32 const x[NO] = { 1.0f + 0x1p-12f, 3.0f, 0.1f, -2.5f, 1.0f, 7.0f, 1e-3f, 100.0f };
    f32 const y[NO] = { 0x1p-24f, 0.5f, 0.2f, 0x1p-20f, 0x1p-24f, -7.0f, 1e-10f, 0.0f };
    f32 w[NO];
    for (u32 i = 0; i < NO; i++) {
      f64 p = (f64)x[i] * (f64)(1.0f + 0x1p-12f);
      f64 s = p + (f64)y[i];
      w[i] = (f32)s;
    }
    Kern *k = kern("guard-f64-const-in-f32-math", DT_FP32, NO);
    input(k, DT_FP32, NO, x);
    input(k, DT_FP32, NO, y);
    Term p = uop_binary(UOP_MUL, ld(k, 0, at_i()), f64c(1.0f + 0x1p-12f));
    k->root = uop_store(k->out, at_i(), uop_binary(UOP_ADD, p, ld(k, 1, at_i())));
    want(k, w);
  }
  {
    f32 const x[NO] = { 2.0f, 2.0f, 3.0f, 5.0f, 0.5f, 10.0f, 2.0f, 7.0f };
    f32 const y[NO] = { 1.0f, 3.0f, 1.0f, 0.1f, 1.0f, 1e-3f, -1.0f, 1.0f };
    f32 const z[NO] = { 3.0f * 0x1p-26f, 0x1p-22f, 1e-8f, 0.0f, 1e-8f, 0.25f, 0.0f, -1e-7f };
    f32 w[NO];
    for (u32 i = 0; i < NO; i++) {
      f64 p = sqrt((f64)x[i]) * (f64)y[i];
      f64 s = p + (f64)z[i];
      w[i] = (f32)s;
    }
    Kern *k = kern("guard-sqrt-times-f32", DT_FP32, NO);
    input(k, DT_FP32, NO, x);
    input(k, DT_FP32, NO, y);
    input(k, DT_FP32, NO, z);
    Term p = uop_binary(UOP_MUL, uop_unary(UOP_SQRT, ld(k, 0, at_i())), ld(k, 1, at_i()));
    k->root = uop_store(k->out, at_i(), uop_binary(UOP_ADD, p, ld(k, 2, at_i())));
    want(k, w);
  }
  {
    f64 const x[NO] = { 1.0, 1312.0, 2.0, 1.0, -5.0, 0.1, 1e300, 3.0 };
    f64 const y[NO] = { 3.0, 41.0, 7.0, 1e-300, 3.0, 3.0, 1e-10, 3.0 };
    f64 w[NO];
    for (u32 i = 0; i < NO; i++) {
      f64 r = 1.0 / y[i];
      w[i] = x[i] * r;
    }
    Kern *k = kern("guard-f64-quotient", DT_FP64, NO);
    input(k, DT_FP64, NO, x);
    input(k, DT_FP64, NO, y);
    k->root = uop_store(k->out, at_i(), quot(ld(k, 0, at_i()), ld(k, 1, at_i())));
    want(k, w);
  }
}

// === Walker ===============================================================

static u32 cpu_buf(u32 dt, void const *data, u32 n) {
  Shape s   = {0};
  s.ndim    = 1;
  s.dims[0] = n;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, dt);
  if (data != NULL) {
    CPU_BACKEND.buf_write(TENS[tid].buf_id, data, (u64)n * dtype_itemsize(dt));
  }
  return TENS[tid].buf_id;
}

static int run_walker(Kern const *k, void *res) {
  KernelEntry ke;
  memset(&ke, 0, sizeof ke);
  u32  tids[MAXI] = { 0 };               // raw buffer pointers, no view bias
  u32  ids[MAXI]  = { 0 };
  Term lift[MAXI] = { 0 };
  for (u32 s = 0; s < k->n_in; s++) {
    ids[s]  = cpu_buf(k->in_dt[s], k->in_data[s], k->in_n[s]);
    lift[s] = k->ins[s];
  }
  ke.cached_lift.in_bufs    = lift;
  ke.cached_lift.in_cap     = MAXI;
  ke.n_inputs               = k->n_in;
  ke.input_tids             = tids;
  ke.output_dtype           = k->out_dt;
  ke.cached_lift.store_root = k->root;
  ke.cached_lift.out_buf    = k->out;
  ke.cached_lift.n_inputs   = k->n_in;
  u32 out = cpu_buf(k->out_dt, NULL, k->n_out);
  if (!cpu_uop_walk(&ke, ids, out)) {
    return 0;
  }
  CPU_BACKEND.buf_read(out, res, (u64)k->n_out * dtype_itemsize(k->out_dt));
  return 1;
}

static double elem(u32 dt, void const *p, u32 i) {
  switch (dt) {
    case DT_FP32:  return ((f32 const *)p)[i];
    case DT_FP64:  return ((f64 const *)p)[i];
    case DT_INT32: return ((i32 const *)p)[i];
    default:       return 0.0;
  }
}

static void check_bytes(char const *route, Kern const *k, void const *res) {
  size_t n = (size_t)k->n_out * dtype_itemsize(k->out_dt);
  if (memcmp(res, k->want, n) != 0) {
    for (u32 i = 0; i < k->n_out; i++) {
      double g = elem(k->out_dt, res, i), w = elem(k->out_dt, k->want, i);
      if (memcmp((u8 const *)res + i * dtype_itemsize(k->out_dt),
                 k->want + i * dtype_itemsize(k->out_dt), dtype_itemsize(k->out_dt)) != 0) {
        fprintf(stderr, "  %s %s: [%u] got %.17g want %.17g\n", route, test_name, i, g, w);
      }
    }
  }
  CHECK(memcmp(res, k->want, n) == 0);
}

static void check_walker(void) {
  static char name[80];
  u8 res[8 * NO];
  for (u32 q = 0; q < N_KERNS; q++) {
    snprintf(name, sizeof name, "walker/%s", KERNS[q].name);
    TEST_BEGIN(name);
    memset(res, 0x5A, sizeof res);
    CHECK(run_walker(&KERNS[q], res));
    check_bytes("walker", &KERNS[q], res);
  }
}

// === Rendered C ===========================================================

typedef void (*LegacyFn)(void *, const void *const *, unsigned,
                         const unsigned *, const unsigned *);

static void check_rendered(void) {
  static char name[80];
  char  *src = NULL;
  size_t len = 0;
  FILE  *fp  = open_memstream(&src, &len);
  for (u32 q = 0; q < N_KERNS; q++) {
    char fn_name[32];
    snprintf(fn_name, sizeof fn_name, "k_ct_%u", q);
    cg_render_uop_kernel_c(KERNS[q].root, fn_name, KERNS[q].out, KERNS[q].ins,
                           KERNS[q].n_in, fp);
  }
  fclose(fp);
  char const *tmp = getenv("TMPDIR");
  char path[512];
  snprintf(path, sizeof path, "%s/thvm_walk_c_types_XXXXXX.c",
           (tmp != NULL && tmp[0] != '\0') ? tmp : "/tmp");
  int fd = mkstemps(path, 2);
  void *h = NULL;
  if (fd >= 0) {
    FILE *cf = fdopen(fd, "w");
    fputs(src, cf);
    fclose(cf);
    char so[600];
    snprintf(so, sizeof so, "%s.so", path);
    h = cpu_jit_compile_shared(path, so) ? dlopen(so, RTLD_NOW) : NULL;
    unlink(path);
    unlink(so);
  }
  free(src);
  TEST_BEGIN("rendered-c/compiles");
  CHECK(h != NULL);
  u8 res[8 * NO];
  for (u32 q = 0; q < N_KERNS; q++) {
    Kern const *k = &KERNS[q];
    snprintf(name, sizeof name, "rendered-c/%s", k->name);
    TEST_BEGIN(name);
    char fn_name[32];
    snprintf(fn_name, sizeof fn_name, "k_ct_%u", q);
    LegacyFn f   = NULL;
    void    *sym = (h != NULL) ? dlsym(h, fn_name) : NULL;
    memcpy(&f, &sym, sizeof f);
    CHECK(f != NULL);
    if (f == NULL) {
      continue;
    }
    memset(res, 0x5A, sizeof res);
    const void *ins[MAXI]   = { k->in_data[0], k->in_data[1], k->in_data[2] };
    unsigned    numel[MAXI] = { k->in_n[0], k->in_n[1], k->in_n[2] };
    f(res, ins, k->n_out, numel, NULL);
    check_bytes("rendered", k, res);
  }
  if (h != NULL) {
    dlclose(h);
  }
}

int main(void) {
  thvm_init();
  build_elementwise();
  build_reduces();
  build_guards();
  check_walker();
  check_rendered();
  thvm_free();
  TEST_REPORT();
}
