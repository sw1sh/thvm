// test_int_reduce_exact.c - an int or bool reduce is exact on every CPU route.
//
// tinygrad keeps an int reduce in its dtype: reduce_ranges_to_acc allocates
// the accumulator like the reduce (codegen/__init__.py:197-204) and starts
// it at identity_element (uop/ops.py:61), 0 for SUM and dt.min for MAX:
// -2^(bits-1) for a signed int, 0 for an unsigned int, False for bool
// (dtype.py:70-72).  A bool ADD or MAX is an OR (uop/symbolic.py:163-166).
// Its C renderer declares the accumulator in that dtype.
//
// The 32-bit values sit above 2^24, where an f32 accumulator rounds, the
// 64-bit ones above 2^53, where a double one does, and the u64 ones above
// 2^63.  For SUM and MAX over every int dtype and bool, out[i] =
// reduce_j x[i, j] runs on the CPU UOp walker and on the legacy C emit
// (the explicit-buffer entry, compiled with the JIT's C compiler) in five
// shapes:
//   plain            one accumulator;
//   upcast           the output axis UPCAST: parallel accumulators;
//   unroll           the reduce axis UNROLLed into the accumulator;
//   upcast-shared    x[i, j] + w[j], output UPCAST: the lane-invariant
//                    w[j] load is hoisted into a local;
//   epilogue         reduce + y[i]: a lane-blocked accumulator, with a
//                    hoisted w[j], when the output axis is UPCAST, a
//                    scalar one when it is not;
//   of-sum           max_j sum_k x[i, j, k]: nested accumulators.
// The linearized C renderer (a store through INDEX_E, expanded,
// devectorized and linearized) runs a scalar reduce and a MAX of a SUM,
// whose accumulator takes its dtype through the inner reduce's result; it
// declines an i64 MAX, whose identity does not fit a CONST payload.  A
// STACK-bodied reduce lowers to one accumulator per lane in the reduce's
// dtype.  The realized repros run end to end through thvm_realize, and
// each kernel again through the walker.
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

#define N_DTYPES 9

static u32 const DTYPES[N_DTYPES] = {
  DT_INT8, DT_UINT8, DT_INT16, DT_UINT16, DT_INT32, DT_UINT32, DT_INT64,
  DT_UINT64, DT_BOOL,
};

#define P53 ((u64)1 << 53)
#define P54 ((u64)1 << 54)
#define P62 ((u64)1 << 62)
#define P63 ((u64)1 << 63)

// Per int dtype: `edge` holds the extremes and odd values past 2^24
// (2^53); `mid` keeps headroom, so a sum of 2 * NJ + 1 of them stays in
// range and no signed add overflows.
typedef struct {
  u32 dt;
  u64 edge[8];
  u64 mid[8];
} DtVals;

static DtVals const VALS[8] = {
  { DT_INT8,
    { (u64)-128, 127, (u64)-1, 0, 100, (u64)-100, 1, (u64)-127 },
    { (u64)-5, 7, (u64)-3, 2, 1, (u64)-4, 6, 0 } },
  { DT_UINT8,
    { 255, 254, 0, 1, 128, 127, 200, 17 },
    { 5, 7, 3, 2, 1, 4, 6, 0 } },
  { DT_INT16,
    { (u64)-32768, 32767, (u64)-1, 0, 30000, (u64)-30000, 1, (u64)-32767 },
    { (u64)-1111, 1333, (u64)-777, 555, 333, (u64)-999, 1515, 101 } },
  { DT_UINT16,
    { 65535, 65534, 0, 1, 32768, 32767, 50000, 17 },
    { 1111, 1333, 777, 555, 333, 999, 1515, 101 } },
  { DT_INT32,
    { (u64)(i64)INT32_MIN, INT32_MAX, 16777217, (u64)-16777217, 16777219,
      (u64)-2147483647, 1, 33554433 },
    { 16777217, (u64)-16777219, 33554435, (u64)-50331651, 16777221, 67108867,
      (u64)-16777223, 1 } },
  { DT_UINT32,
    { UINT32_MAX, UINT32_MAX - 2, 16777217, 2147483649u, 0, 1, 4000000001u,
      33554433 },
    { 16777217, 16777219, 33554435, 50331651, 16777221, 67108867, 16777223,
      1 } },
  { DT_INT64,
    { (u64)INT64_MIN, INT64_MAX, P53 + 1, (u64)-(i64)(P53 + 1),
      (u64)-9000000000LL, (u64)-9000000001LL, P62 + 1, 1 },
    { P53 + 1, (u64)-(i64)(P53 + 3), P54 + 5, (u64)-(i64)(P54 + 7),
      9000000001ULL, (u64)-9000000003LL, P53 + 9, 1 } },
  { DT_UINT64,
    { UINT64_MAX, UINT64_MAX - 2, P63 + 1, P53 + 1, 0, 1, P63 - 1, P62 + 3 },
    { P53 + 1, P53 + 3, P54 + 5, P54 + 7, 9000000001ULL, 9000000003ULL,
      P53 + 9, 1 } },
};

static DtVals const *vals_of(u32 dt) {
  for (u32 i = 0; i < 8; i++) {
    if (VALS[i].dt == dt) {
      return &VALS[i];
    }
  }
  return NULL;
}

// v wrapped into dt: its low bits, sign-extended for a signed dtype.
static i64 canon(u32 dt, u64 v) {
  if (dt == DT_BOOL) {
    return v != 0;
  }
  u32 bits = dtype_info(dt)->bits;
  if (bits >= 64) {
    return (i64)v;
  }
  u64 mask = ((u64)1 << bits) - 1;
  v &= mask;
  if (dtype_is_signed(dt) && ((v >> (bits - 1)) & 1)) {
    v |= ~mask;
  }
  return (i64)v;
}

static int less_in(u32 dt, i64 a, i64 b) {
  return (dt == DT_UINT64) ? ((u64)a < (u64)b) : (a < b);
}

static i64 dt_min(u32 dt) {
  if (dt == DT_BOOL || !dtype_is_signed(dt)) {
    return 0;
  }
  return canon(dt, (u64)1 << (dtype_info(dt)->bits - 1));
}

// acc <op> v in dt, as tinygrad computes it in the accumulator's dtype.
static i64 combine(u32 dt, u32 kind, i64 acc, i64 v) {
  if (dt == DT_BOOL) {
    return (acc != 0) | (v != 0);
  }
  if (kind == REDUCE_SUM) {
    return canon(dt, (u64)acc + (u64)v);
  }
  return less_in(dt, acc, v) ? v : acc;
}

static void store_elem(u32 dt, void *buf, u32 k, i64 v) {
  switch (dtype_itemsize(dt)) {
    case 1: ((u8  *)buf)[k] = (u8)v;  break;
    case 2: ((u16 *)buf)[k] = (u16)v; break;
    case 4: ((u32 *)buf)[k] = (u32)v; break;
    default: ((u64 *)buf)[k] = (u64)v; break;
  }
}

// A bool reads raw: a stored bool is exactly 0 or 1.
static i64 load_elem(u32 dt, void const *buf, u32 k) {
  if (dt == DT_BOOL) {
    return ((u8 const *)buf)[k];
  }
  switch (dtype_itemsize(dt)) {
    case 1: return canon(dt, ((u8  const *)buf)[k]);
    case 2: return canon(dt, ((u16 const *)buf)[k]);
    case 4: return canon(dt, ((u32 const *)buf)[k]);
    default: return canon(dt, ((u64 const *)buf)[k]);
  }
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

// === Kernels =============================================================

typedef struct {
  u32  dt;
  u32  kind;
  u32  shape;
  int  nested;
  Term root;
  Term out;
  Term ins[3];                  // x, w, y: instances 1..3
  u8   x[8 * NI * NJ * NK];
  u8   w[8 * NJ];
  u8   y[8 * NI];
  i64  want[NI];
} Kern;

static int shape_has_w(u32 shape) {
  return shape == SH_UPCAST_SHARED || shape == SH_EPILOGUE
      || shape == SH_EPILOGUE_UPCAST;
}

static int shape_has_y(u32 shape) {
  return shape == SH_EPILOGUE || shape == SH_EPILOGUE_UPCAST;
}

// A bool ADD renders as `+`, not as an OR, so bool keeps to the bare
// reduce.  The nested shape is a MAX of a SUM.
static int shape_applies(u32 dt, u32 kind, u32 shape) {
  if (shape == SH_MAX_OF_SUM) {
    return kind == REDUCE_MAX && dt != DT_BOOL;
  }
  return dt != DT_BOOL || !shape_has_w(shape);
}

// The edge values where an overflow is defined: MAX never adds, an
// unsigned sum wraps, and a narrow signed sum converts back to its dtype.
static int use_edge(u32 dt, u32 kind, u32 shape) {
  if (shape_has_w(shape) || shape == SH_MAX_OF_SUM) {
    return 0;
  }
  return kind == REDUCE_MAX || !dtype_is_signed(dt) || dtype_info(dt)->bits < 32;
}

static i64 x_val(u32 dt, int edge, u32 i, u32 j) {
  if (dt == DT_BOOL) {
    return j < i;
  }
  DtVals const *v = vals_of(dt);
  u64 const    *t = edge ? v->edge : v->mid;
  if (edge && i == 0) {
    return canon(dt, t[0]);
  }
  return canon(dt, t[(i * 5 + j * 3) % 8]);
}

// out[i] = reduce_j body(i, j) [+ y[i]] with body = x[i*NJ + j] [+ w[j]],
// or out[i] = max_j sum_k x[(i*NJ + j)*NK + k], stored through a bare
// address like a lifted kernel.
static void build_kernel(Kern *k, u32 dt, u32 kind, u32 shape) {
  memset(k, 0, sizeof *k);
  k->dt    = dt;
  k->kind  = kind;
  k->shape = shape;
  int edge = use_edge(dt, kind, shape);
  u32 xd[1] = { NI * NJ * NK };
  u32 wd[1] = { NJ };
  u32 od[1] = { NI };
  k->out    = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt, 1, od, 0);
  k->ins[0] = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt, 1, xd, 1);
  if (shape_has_w(shape)) {
    k->ins[1] = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt, 1, wd, 2);
  }
  if (shape_has_y(shape)) {
    k->ins[2] = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt, 1, od, 3);
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
  DtVals const *v = vals_of(dt);
  for (u32 jj = 0; jj < NJ; jj++) {
    store_elem(dt, k->w, jj, (v != NULL) ? canon(dt, v->mid[(jj * 7 + 3) % 8]) : 0);
  }
  for (u32 ii = 0; ii < NI; ii++) {
    i64 yv = (v != NULL) ? canon(dt, v->mid[(ii * 3 + 1) % 8]) : 0;
    store_elem(dt, k->y, ii, yv);
    i64 acc = (kind == REDUCE_MAX) ? dt_min(dt) : 0;
    for (u32 jj = 0; jj < NJ; jj++) {
      i64 b = 0;
      if (shape == SH_MAX_OF_SUM) {
        for (u32 q = 0; q < NK; q++) {
          i64 xv = x_val(dt, 0, ii, jj * NK + q);
          store_elem(dt, k->x, (ii * NJ + jj) * NK + q, xv);
          b = combine(dt, REDUCE_SUM, b, xv);
        }
      } else {
        i64 xv = x_val(dt, edge, ii, jj);
        store_elem(dt, k->x, ii * NJ + jj, xv);
        b = xv;
      }
      if (shape_has_w(shape)) {
        b = canon(dt, (u64)b + (u64)load_elem(dt, k->w, jj));
      }
      acc = combine(dt, kind, acc, b);
    }
    if (shape_has_y(shape)) {
      acc = canon(dt, (u64)acc + (u64)yv);
    }
    k->want[ii] = acc;
  }
}

// out[i] = reduce_j x[i*NJ + j], or with `nested` out[i] = max_j sum_k
// x[(i*NJ + j)*NK + k], stored through INDEX_E and read through LOAD: the
// shape the linearized renderer consumes.
static void build_linear_kernel(Kern *k, u32 dt, u32 kind, int nested) {
  memset(k, 0, sizeof *k);
  k->dt     = dt;
  k->kind   = kind;
  k->nested = nested;
  int edge = !nested && use_edge(dt, kind, SH_PLAIN);
  u32 xd[1] = { NI * NJ * NK };
  u32 od[1] = { NI };
  k->out    = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt, 1, od, 0);
  k->ins[0] = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt, 1, xd, 1);
  Term i     = uop_range(0, KAX_LOOP, NI);
  Term j     = uop_range(1, KAX_REDUCE, NJ);
  Term ij    = iadd(imul(i, ic(NJ)), j);
  u32  aj[1] = { 1 };
  Term value;
  if (nested) {
    Term kk    = uop_range(2, KAX_REDUCE, NK);
    Term ld    = uop_load(uop_index_e(k->ins[0], iadd(imul(ij, ic(NK)), kk)));
    u32  ak[1] = { 2 };
    value = uop_reduce_multi(REDUCE_MAX, 1, aj,
                             uop_reduce_multi(REDUCE_SUM, 1, ak, ld));
  } else {
    value = uop_reduce_multi(kind, 1, aj, uop_load(uop_index_e(k->ins[0], ij)));
  }
  k->root = uop_store(k->out, uop_index_e(k->out, i), value);
  for (u32 ii = 0; ii < NI; ii++) {
    i64 acc = (nested || kind == REDUCE_MAX) ? dt_min(dt) : 0;
    for (u32 jj = 0; jj < NJ; jj++) {
      if (!nested) {
        i64 xv = x_val(dt, edge, ii, jj);
        store_elem(dt, k->x, ii * NJ + jj, xv);
        acc = combine(dt, kind, acc, xv);
        continue;
      }
      i64 s = 0;
      for (u32 q = 0; q < NK; q++) {
        i64 xv = x_val(dt, 0, ii, jj * NK + q);
        store_elem(dt, k->x, (ii * NJ + jj) * NK + q, xv);
        s = combine(dt, REDUCE_SUM, s, xv);
      }
      acc = combine(dt, REDUCE_MAX, acc, s);
    }
    k->want[ii] = acc;
  }
}

static char const *kern_name(Kern const *k, char const *route) {
  static char name[96];
  snprintf(name, sizeof name, "%s/%s-%s-%s", route, dtype_name(k->dt),
           k->kind == REDUCE_MAX ? "max" : "sum",
           k->nested ? "of-sum" : SHAPE_NAME[k->shape]);
  return name;
}

static void check_out(Kern const *k, void const *res) {
  for (u32 i = 0; i < NI; i++) {
    i64 got = load_elem(k->dt, res, i);
    if (got != k->want[i]) {
      fprintf(stderr, "  %s: out[%u] got %lld want %lld\n", test_name, i,
              (long long)got, (long long)k->want[i]);
    }
    CHECK_EQ(got, k->want[i]);
  }
}

#define N_KERNS (N_DTYPES * 2 * SH_COUNT)

static Kern KERNS[N_KERNS];
static int  KERNS_ON[N_KERNS];

static void build_kernels(void) {
  u32 n = 0;
  for (u32 d = 0; d < N_DTYPES; d++) {
    for (u32 kind = 0; kind < 2; kind++) {
      for (u32 sh = 0; sh < SH_COUNT; sh++, n++) {
        if (shape_applies(DTYPES[d], kind, sh)) {
          build_kernel(&KERNS[n], DTYPES[d], kind, sh);
          KERNS_ON[n] = 1;
        }
      }
    }
  }
}

// === Walker ==============================================================

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

// Run the kernel through the CPU UOp walker into `res`.  Returns 1 when
// the walker ran it.
static int run_walker(Kern const *k, void *res) {
  KernelEntry ke;
  memset(&ke, 0, sizeof ke);
  const void *data[3]  = { k->x, k->w, k->y };
  u32 const   numel[3] = { NI * NJ * NK, NJ, NI };
  u32  tids[3]       = { 0, 0, 0 };        // raw buffer pointers, no view bias
  u32  in_buf_ids[3] = { 0, 0, 0 };
  Term lift_ins[3]   = { 0, 0, 0 };
  u32  n = 0;
  for (u32 s = 0; s < 3; s++) {
    if (k->ins[s] != 0) {
      in_buf_ids[s] = cpu_buf(k->dt, data[s], numel[s]);
      lift_ins[s]   = k->ins[s];
      n = s + 1;
    }
  }
  ke.cached_lift.in_bufs    = lift_ins;
  ke.cached_lift.in_cap     = 3;
  ke.n_inputs               = n;
  ke.input_tids             = tids;
  ke.output_dtype           = k->dt;
  ke.cached_lift.store_root = k->root;
  ke.cached_lift.out_buf    = k->out;
  ke.cached_lift.n_inputs   = n;
  u32 out = cpu_buf(k->dt, NULL, NI);
  if (!cpu_uop_walk(&ke, in_buf_ids, out)) {
    return 0;
  }
  CPU_BACKEND.buf_read(out, res, (u64)NI * dtype_itemsize(k->dt));
  return 1;
}

static void check_walker_units(void) {
  u8 res[8 * NI];
  for (u32 q = 0; q < N_KERNS; q++) {
    if (!KERNS_ON[q]) {
      continue;
    }
    TEST_BEGIN(kern_name(&KERNS[q], "walker"));
    memset(res, 0, sizeof res);
    CHECK(run_walker(&KERNS[q], res));
    check_out(&KERNS[q], res);
  }
}

// === Rendered C ==========================================================

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

static void *compile_src(void) {
  char const *tmp = getenv("TMPDIR");
  char path[512];
  snprintf(path, sizeof path, "%s/thvm_int_reduce_XXXXXX.c",
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
typedef void (*LinearFn)(void *, const void *const *, unsigned,
                         const unsigned *);

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

// The explicit-buffer entry binds the inputs in order.
static void run_legacy(Kern const *k, LegacyFn f, void *res) {
  const void *all[3]   = { k->x, k->w, k->y };
  unsigned    alln[3]  = { NI * NJ * NK, NJ, NI };
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

static void check_legacy_units(void) {
  char fn_name[32];
  for (u32 q = 0; q < N_KERNS; q++) {
    if (KERNS_ON[q]) {
      snprintf(fn_name, sizeof fn_name, "k_legacy_%u", q);
      render_legacy(&KERNS[q], fn_name);
    }
  }
  void *h = compile_src();
  TEST_BEGIN("legacy-c/compiles");
  CHECK(h != NULL);
  u8 res[8 * NI];
  for (u32 q = 0; q < N_KERNS; q++) {
    if (!KERNS_ON[q]) {
      continue;
    }
    TEST_BEGIN(kern_name(&KERNS[q], "legacy-c"));
    snprintf(fn_name, sizeof fn_name, "k_legacy_%u", q);
    LegacyFn f = (h != NULL) ? (LegacyFn)dlsym(h, fn_name) : NULL;
    CHECK(f != NULL);
    if (f != NULL) {
      memset(res, 0, sizeof res);
      run_legacy(&KERNS[q], f, res);
      check_out(&KERNS[q], res);
    }
  }
  if (h != NULL) {
    dlclose(h);
  }
}

// Expand, devectorize, linearize and render as cg_render_uop_kernel_c_root's
// linearized route does.  Returns the renderer's result.
static int render_linear(Kern const *k, char const *fn_name) {
  Term r = uop_expand_graph(k->root);
  r = uop_symbolic_rewrite(r);
  r = uop_devectorize_graph(r);
  r = uop_symbolic_rewrite(r);
  r = uop_load_store_fold_graph(r);
  r = uop_symbolic_rewrite(r);
  static LinKernel lk;
  if (!uop_linearize(r, &lk)) {
    return 0;
  }
  char  *buf = NULL;
  size_t len = 0;
  FILE  *fp  = open_memstream(&buf, &len);
  int    ok  = cg_render_linearized_c(&lk, fn_name, fp);
  fclose(fp);
  if (ok) {
    src_append(buf, len);
  }
  free(buf);
  return ok;
}

#define N_LINEAR (N_DTYPES * 2 + 8)

static Kern LINEAR[N_LINEAR];
static int  LINEAR_ON[N_LINEAR];

static void check_linear_units(void) {
  char fn_name[32];
  u32  m = 0;
  TEST_BEGIN("linearized-c/renders");
  for (u32 d = 0; d < N_DTYPES; d++) {
    for (u32 kind = 0; kind < 2; kind++, m++) {
      build_linear_kernel(&LINEAR[m], DTYPES[d], kind, 0);
      snprintf(fn_name, sizeof fn_name, "k_linear_%u", m);
      int ok = render_linear(&LINEAR[m], fn_name);
      // An i64 MAX identity does not fit a CONST payload: devectorize
      // leaves the REDUCE, the linearized renderer declines it, and the
      // legacy emit renders the kernel.
      CHECK_EQ(ok, !(DTYPES[d] == DT_INT64 && kind == REDUCE_MAX));
      LINEAR_ON[m] = ok;
    }
  }
  for (u32 d = 0; d < N_DTYPES; d++) {
    if (DTYPES[d] == DT_BOOL) {
      continue;
    }
    build_linear_kernel(&LINEAR[m], DTYPES[d], REDUCE_MAX, 1);
    snprintf(fn_name, sizeof fn_name, "k_linear_%u", m);
    LINEAR_ON[m] = render_linear(&LINEAR[m], fn_name);
    CHECK_EQ(LINEAR_ON[m], DTYPES[d] != DT_INT64);
    m++;
  }
  void *h = compile_src();
  TEST_BEGIN("linearized-c/compiles");
  CHECK(h != NULL);
  u8 res[8 * NI];
  for (u32 q = 0; q < m; q++) {
    if (!LINEAR_ON[q]) {
      continue;
    }
    TEST_BEGIN(kern_name(&LINEAR[q], "linearized-c"));
    snprintf(fn_name, sizeof fn_name, "k_linear_%u", q);
    LinearFn f = (h != NULL) ? (LinearFn)dlsym(h, fn_name) : NULL;
    CHECK(f != NULL);
    if (f != NULL) {
      const void *ins[1]   = { LINEAR[q].x };
      unsigned    numel[1] = { NI * NJ * NK };
      memset(res, 0, sizeof res);
      f(res, ins, NI, numel);
      check_out(&LINEAR[q], res);
    }
  }
  if (h != NULL) {
    dlclose(h);
  }
}

// === STACK-bodied lowering ===============================================

typedef struct {
  u32 n_acc;
  u32 acc_dt[16];
  u32 n_init;
  u32 init_bits[16];
  u32 init_dt[16];
  u32 n_reduce;
} LowerScan;

static void lower_visit(Term t, void *user) {
  LowerScan *s = (LowerScan *)user;
  switch (term_ext(t)) {
    case UOP_PLACEHOLDER:
      if (s->n_acc < 16) {
        s->acc_dt[s->n_acc++] = uop_placeholder_dtype(t);
      }
      return;
    case UOP_STORE: {
      Term v = uop_store_value(t);
      if (term_tag(v) == TAG_UOP && term_ext(v) == UOP_CONST && s->n_init < 16) {
        Term num = heap_read(term_val(v));
        s->init_bits[s->n_init] = (u32)term_val(num);
        s->init_dt[s->n_init]   = term_ext(num);
        s->n_init++;
      }
      return;
    }
    case UOP_REDUCE:
      s->n_reduce++;
      return;
    default:
      return;
  }
}

// reduce_j x[(4i + l)*NJ + j] over the four lanes l as one STACK body
// lowers to four accumulators, each in the reduce's dtype and started at
// its identity.  An i64 MAX stays a REDUCE.
static void check_stack_lowering(u32 dt, u32 kind) {
  static char name[96];
  snprintf(name, sizeof name, "devectorize/stack-body-acc-dtype/%s-%s",
           dtype_name(dt), kind == REDUCE_MAX ? "max" : "sum");
  TEST_BEGIN(name);
  u32  xd[1] = { 4 * NI * NJ };
  Term x = uop_buffer_inst(UOP_SCOPE_GLOBAL, dt, 1, xd, 1);
  Term i = uop_range(0, KAX_LOOP, NI / 4);
  Term j = uop_range(1, KAX_REDUCE, NJ);
  Term lanes[4];
  for (u32 l = 0; l < 4; l++) {
    Term row = iadd(imul(i, ic(4)), ic(l));
    lanes[l] = uop_load(uop_index_e(x, iadd(imul(row, ic(NJ)), j)));
  }
  u32  axes[1] = { 1 };
  Term red     = uop_reduce_multi(kind, 1, axes, uop_stack(4, lanes));
  LowerScan s;
  memset(&s, 0, sizeof s);
  CHECK(devec_cone_walk(uop_devectorize_graph(red), lower_visit, &s));
  if (dt == DT_INT64 && kind == REDUCE_MAX) {
    CHECK_EQ(s.n_acc, 0);
    CHECK_EQ(s.n_reduce, 1);
    return;
  }
  CHECK_EQ(s.n_reduce, 0);
  CHECK_EQ(s.n_acc, 4);
  CHECK_EQ(s.n_init, 4);
  for (u32 a = 0; a < s.n_acc; a++) {
    CHECK_EQ(s.acc_dt[a], dt);
  }
  u32 want = (kind == REDUCE_MAX) ? (u32)dt_min(dt) : 0;
  for (u32 a = 0; a < s.n_init; a++) {
    CHECK_EQ(s.init_dt[a], dt);
    CHECK_EQ(s.init_bits[a], want);
  }
}

// === End to end ==========================================================

static Term ten(u32 dt, void const *data, u32 ndim, u32 const *dims) {
  Shape s = {0};
  s.ndim  = ndim;
  u32 n   = 1;
  for (u32 d = 0; d < ndim; d++) {
    s.dims[d] = dims[d];
    n *= dims[d];
  }
  u32 tid = tensor_alloc(&CPU_BACKEND, s, dt);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, data, (u64)n * dtype_itemsize(dt));
  return term_new(0, TAG_TEN, dt, tid);
}

static u32 E2E_KERNELS;
static u32 E2E_JIT;

static void check_elems(u32 dt, u8 const *got, i64 const *want, u32 n,
                        char const *route) {
  for (u32 i = 0; i < n; i++) {
    if (load_elem(dt, got, i) != want[i]) {
      fprintf(stderr, "  %s %s: [%u] got %lld want %lld\n", route, test_name, i,
              (long long)load_elem(dt, got, i), (long long)want[i]);
    }
    CHECK_EQ(load_elem(dt, got, i), want[i]);
  }
}

// Realize `expr` and compare its n elements with `want`, then run its
// kernel again through the walker.
static void check_realized(Term expr, u32 dt, i64 const *want, u32 n) {
  u32  k0   = KERNELS_NEXT;
  Term done = term_resolve(thvm_realize(expr));
  CHECK(term_tag(done) == TAG_TEN);
  if (term_tag(done) != TAG_TEN) {
    return;
  }
  u32 tid   = (u32)term_val(done);
  u32 bytes = n * dtype_itemsize(dt);
  CHECK_EQ(TENS[tid].dtype, dt);
  CHECK_EQ(TENS[tid].view.numel, n);
  CHECK(TENS[tid].view.contiguous);
  u8 got[8 * 64];
  TENS[tid].backend->buf_read(TENS[tid].buf_id, got, bytes);
  check_elems(dt, got, want, n, "realized");
  u32 walked = 0;
  for (u32 kid = k0; kid < KERNELS_NEXT; kid++) {
    KernelEntry *ke = &KERNELS[kid];
    if (ke->output_tid != tid) {
      continue;
    }
    walked++;
    E2E_KERNELS++;
    if (KID_JIT_FN != NULL && kid < KERNELS_CAP && KID_JIT_FN[kid] != NULL) {
      E2E_JIT++;
    }
    u32 *ids = (u32 *)calloc(ke->n_inputs ? ke->n_inputs : 1, sizeof(u32));
    for (u32 i = 0; i < ke->n_inputs; i++) {
      ids[i] = TENS[ke->input_tids[i]].buf_id;
    }
    u8 const zero[8 * 64] = {0};
    TENS[tid].backend->buf_write(TENS[tid].buf_id, zero, bytes);
    CHECK(cpu_uop_walk(ke, ids, TENS[tid].buf_id));
    free(ids);
    TENS[tid].backend->buf_read(TENS[tid].buf_id, got, bytes);
    check_elems(dt, got, want, n, "walked");
  }
  CHECK_EQ(walked, 1);
}

static void check_end_to_end(void) {
  u32 const four[1] = { 4 };

  TEST_BEGIN("e2e/i32-max-past-2p24");
  {
    i32 const v[4]    = { 16777217, 1, 2, 3 };
    i64 const want[1] = { 16777217 };
    check_realized(uop_reduce(REDUCE_MAX, 0, ten(DT_INT32, v, 1, four)), DT_INT32, want, 1);
  }

  TEST_BEGIN("e2e/i32-sum-past-2p24");
  {
    i32 const v[4]    = { 16777217, 1, 2, 3 };
    i64 const want[1] = { 16777223 };
    check_realized(uop_reduce(REDUCE_SUM, 0, ten(DT_INT32, v, 1, four)), DT_INT32, want, 1);
  }

  TEST_BEGIN("e2e/i64-max-below-minus-9e9");
  {
    i64 const v[4]    = { -9000000000LL, -9000000001LL, -9000000003LL, -9000000005LL };
    i64 const want[1] = { -9000000000LL };
    check_realized(uop_reduce(REDUCE_MAX, 0, ten(DT_INT64, v, 1, four)), DT_INT64, want, 1);
  }

  TEST_BEGIN("e2e/i64-sum-past-2p53");
  {
    i64 const v[4]    = { (i64)P53 + 1, 2, 4, 8 };
    i64 const want[1] = { (i64)P53 + 15 };
    check_realized(uop_reduce(REDUCE_SUM, 0, ten(DT_INT64, v, 1, four)), DT_INT64, want, 1);
  }

  TEST_BEGIN("e2e/u64-max-past-2p63");
  {
    u64 const v[4]    = { 1, UINT64_MAX - 4, P63 + 1, 7 };
    i64 const want[1] = { (i64)(UINT64_MAX - 4) };
    check_realized(uop_reduce(REDUCE_MAX, 0, ten(DT_UINT64, v, 1, four)), DT_UINT64, want, 1);
  }

  TEST_BEGIN("e2e/u32-sum-wraps");
  {
    u32 const v[4]    = { UINT32_MAX, 16777217, 2, UINT32_MAX - 6 };
    i64 const want[1] = { (i64)(u32)(16777217u + 2u - 1u - 7u) };
    check_realized(uop_reduce(REDUCE_SUM, 0, ten(DT_UINT32, v, 1, four)), DT_UINT32, want, 1);
  }

  TEST_BEGIN("e2e/i32-4x4-max-axis1");
  {
    u32 const dims[2] = { 4, 4 };
    i32 v[16];
    i64 want[4];
    for (u32 r = 0; r < 4; r++) {
      for (u32 c = 0; c < 4; c++) {
        v[r * 4 + c] = -100000001 - (i32)(16 * r) - (i32)(3 * c);
      }
      want[r] = -100000001 - (i64)(16 * r);
    }
    check_realized(uop_reduce(REDUCE_MAX, 1, ten(DT_INT32, v, 2, dims)), DT_INT32, want, 4);
  }

  // A 37-wide reduce axis is not unrolled, so hand_opts UPCASTs the
  // 4-wide output axis: parallel accumulators on the JIT.
  u32 const wide[2] = { 4, 37 };
  static i32 xw[4 * 37];
  i64 want_max[4];
  i64 want_sum[4];
  for (u32 r = 0; r < 4; r++) {
    want_max[r] = INT64_MIN;
    want_sum[r] = 0;
    for (u32 c = 0; c < 37; c++) {
      i32 x = (16777217 + 2 * (i32)((r * 37 + c * 11) % 37)) * ((c % 3 == 0) ? -1 : 1);
      xw[r * 37 + c] = x;
      want_max[r] = (x > want_max[r]) ? x : want_max[r];
      want_sum[r] += x;
    }
  }
  TEST_BEGIN("e2e/i32-4x37-max-axis1-upcast");
  check_realized(uop_reduce(REDUCE_MAX, 1, ten(DT_INT32, xw, 2, wide)), DT_INT32, want_max, 4);
  TEST_BEGIN("e2e/i32-4x37-sum-axis1-upcast");
  check_realized(uop_reduce(REDUCE_SUM, 1, ten(DT_INT32, xw, 2, wide)), DT_INT32, want_sum, 4);

  TEST_BEGIN("e2e/bool-max-and-sum-are-or");
  {
    u32 const dims[2] = { 3, 4 };
    u8  const v[12]   = { 0, 0, 0, 0,  0, 1, 0, 0,  1, 1, 1, 0 };
    i64 const want[3] = { 0, 1, 1 };
    check_realized(uop_reduce(REDUCE_MAX, 1, ten(DT_BOOL, v, 2, dims)), DT_BOOL, want, 3);
    check_realized(uop_reduce(REDUCE_SUM, 1, ten(DT_BOOL, v, 2, dims)), DT_BOOL, want, 3);
  }
}

int main(void) {
  thvm_init();

  build_kernels();
  check_walker_units();
  check_legacy_units();
  check_linear_units();

  u32 const stack_dts[6] = { DT_INT8, DT_UINT16, DT_INT32, DT_INT64, DT_UINT64, DT_BOOL };
  for (u32 d = 0; d < 6; d++) {
    check_stack_lowering(stack_dts[d], REDUCE_SUM);
    check_stack_lowering(stack_dts[d], REDUCE_MAX);
  }

  check_end_to_end();
  printf("  e2e: %u realized kernels, %u of them on the JIT, all re-run on the walker\n",
         E2E_KERNELS, E2E_JIT);

  TEST_BEGIN("e2e/no-kernel-ran-without-a-rung");
  CHECK_EQ(cpu_dispatch_no_rung_count(), 0);

  free(SRC);
  thvm_free();
  TEST_REPORT();
}
