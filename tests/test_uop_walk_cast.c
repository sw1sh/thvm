// test_uop_walk_cast.c - the CPU UOp walker's CAST converts like the C
// emit.
//
// A CAST kernel, out[i] = CAST(f(x[i]), dst), runs through the walker
// (cpu_uop_walk) and through the legacy C emit's explicit-buffer entry,
// compiled with the JIT's C compiler, and both must store the bytes a C
// conversion gives: a bool is x != 0, which is tinygrad's bool(x) and the
// C emit's `(x != 0)`; a float expression truncates toward an int dtype
// as a whole; an int reaches f32 rounded once, a u64 from its unsigned
// value; an f64 cast to f32 rounds.  The walker walked every CAST source
// in the destination's domain, so a float source was truncated before a
// bool tested it and a float product cast to int multiplied truncated
// operands, and it sent an int to f32 through a double.

#include "../src/thvm.c"
#include "test.h"
#include <dlfcn.h>

#define NI 8

static u32 bits_of(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return b;
}

static Term f32c(f32 v) {
  return uop_const(DT_FP32, bits_of(v));
}

typedef struct {
  char const *name;
  u32         sdt;
  u32         ddt;
  Term        root;
  Term        out;
  Term        in;
  u8          x[8 * NI];
  u8          want[8 * NI];
} Kern;

// out[i] = CAST(f(x[i]), ddt), stored through a bare address like a lifted
// kernel.  `f` maps the loaded x[i] to the CAST source.
static void build(Kern *k, char const *name, u32 sdt, u32 ddt,
                  void const *x, void const *want, Term (*f)(Term)) {
  memset(k, 0, sizeof *k);
  k->name = name;
  k->sdt  = sdt;
  k->ddt  = ddt;
  u32 d[1] = { NI };
  k->out = uop_buffer_inst(UOP_SCOPE_GLOBAL, ddt, 1, d, 0);
  k->in  = uop_buffer_inst(UOP_SCOPE_GLOBAL, sdt, 1, d, 1);
  Term i = uop_range(0, KAX_LOOP, NI);
  Term v = uop_index_e(k->in, i);
  k->root = uop_store(k->out, i, uop_cast((f != NULL) ? f(v) : v, ddt));
  memcpy(k->x, x, (size_t)NI * dtype_itemsize(sdt));
  memcpy(k->want, want, (size_t)NI * dtype_itemsize(ddt));
}

static Term times_half(Term v) {
  return uop_binary(UOP_MUL, v, f32c(0.5f));
}

static Term times_2_5(Term v) {
  return uop_binary(UOP_MUL, v, f32c(2.5f));
}

static Term where_lt_1(Term v) {
  return uop_iwhere(uop_binary(UOP_CMPLT, v, f32c(1.0f)), v, f32c(0.0f));
}

static Term to_f32(Term v) {
  return uop_cast(v, DT_FP32);
}

#define N_KERNS 8

static Kern KERNS[N_KERNS];

static u32 build_kernels(void) {
  u32 n = 0;

  f32 const a[NI]  = { 0.5f, 2.0f, -0.0f, -3.0f, NAN, 0.0f, -0.25f, INFINITY };
  u8  const ab[NI] = { 1, 1, 0, 1, 1, 0, 1, 1 };
  build(&KERNS[n++], "f32-to-bool", DT_FP32, DT_BOOL, a, ab, NULL);

  f32 const m[NI]  = { 1.0f, 0.5f, 0.0f, -1.0f, 3.0f, -0.0f, 0.25f, 2.0f };
  u8  const mb[NI] = { 1, 1, 0, 1, 1, 0, 1, 1 };
  build(&KERNS[n++], "f32-product-to-bool", DT_FP32, DT_BOOL, m, mb, times_half);

  f32 const w[NI]  = { 0.5f, 2.0f, -0.25f, 0.0f, 1.0f, 0.75f, -3.0f, 5.0f };
  u8  const wb[NI] = { 1, 0, 1, 0, 0, 1, 1, 0 };
  build(&KERNS[n++], "f32-where-to-bool", DT_FP32, DT_BOOL, w, wb, where_lt_1);

  f32 const p[NI]  = { 1.0f, 3.0f, -1.0f, 0.5f, -3.0f, 0.2f, 7.0f, -0.5f };
  i32 const pi[NI] = { 2, 7, -2, 1, -7, 0, 17, -1 };
  build(&KERNS[n++], "f32-product-to-i32", DT_FP32, DT_INT32, p, pi, times_2_5);

  i32 const q[NI]  = { 0, 1, -1, 2, 256, 0, -256, 7 };
  u8  const qb[NI] = { 0, 1, 1, 1, 1, 0, 1, 1 };
  build(&KERNS[n++], "i32-to-bool", DT_INT32, DT_BOOL, q, qb, NULL);

  i64 const big = ((i64)1 << 60) + ((i64)1 << 36) + 1;
  i64 const s[NI] = { big, 3, -5, -big, ((i64)1 << 24) + 1, ((i64)1 << 53) + 1, -1, 0 };
  f32 sf[NI];
  for (u32 i = 0; i < NI; i++) {
    sf[i] = (f32)s[i];
  }
  build(&KERNS[n++], "i64-to-f32", DT_INT64, DT_FP32, s, sf, NULL);

  u64 const u[NI] = { ((u64)1 << 63) + ((u64)1 << 39) + 1, 1, UINT64_MAX,
                      ((u64)1 << 53) + 1, (u64)1 << 63, 0, 12345,
                      ((u64)1 << 40) + 1 };
  f32 uf[NI];
  for (u32 i = 0; i < NI; i++) {
    uf[i] = (f32)u[i];
  }
  build(&KERNS[n++], "u64-to-f32", DT_UINT64, DT_FP32, u, uf, NULL);

  f64 const g[NI] = { 1.0 + 0x1p-40, -(1.0 + 0x1p-30), 0.1, 3.0, 1e-50, -2.5,
                      65504.0, 1.0 + 0x1p-24 + 0x1p-40 };
  f64 gf[NI];
  for (u32 i = 0; i < NI; i++) {
    gf[i] = (f64)(f32)g[i];
  }
  build(&KERNS[n++], "f64-to-f32-to-f64", DT_FP64, DT_FP64, g, gf, to_f32);

  return n;
}

// === Walker ================================================================

static u32 cpu_buf(u32 dt, void const *data) {
  Shape s   = {0};
  s.ndim    = 1;
  s.dims[0] = NI;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, dt);
  if (data != NULL) {
    CPU_BACKEND.buf_write(TENS[tid].buf_id, data, (u64)NI * dtype_itemsize(dt));
  }
  return TENS[tid].buf_id;
}

static int run_walker(Kern const *k, void *res) {
  KernelEntry ke;
  memset(&ke, 0, sizeof ke);
  u32  tids[1]     = { 0 };               // raw buffer pointers, no view bias
  Term lift_ins[1] = { k->in };
  u32  in_ids[1]   = { cpu_buf(k->sdt, k->x) };
  ke.cached_lift.in_bufs    = lift_ins;
  ke.cached_lift.in_cap     = 1;
  ke.n_inputs               = 1;
  ke.input_tids             = tids;
  ke.output_dtype           = k->ddt;
  ke.cached_lift.store_root = k->root;
  ke.cached_lift.out_buf    = k->out;
  ke.cached_lift.n_inputs   = 1;
  u32 out = cpu_buf(k->ddt, NULL);
  if (!cpu_uop_walk(&ke, in_ids, out)) {
    return 0;
  }
  CPU_BACKEND.buf_read(out, res, (u64)NI * dtype_itemsize(k->ddt));
  return 1;
}

static void check_bytes(Kern const *k, void const *res) {
  size_t n = (size_t)NI * dtype_itemsize(k->ddt);
  if (memcmp(res, k->want, n) != 0) {
    fprintf(stderr, "  %s: got", test_name);
    for (size_t b = 0; b < n; b++) {
      fprintf(stderr, " %02x", ((u8 const *)res)[b]);
    }
    fprintf(stderr, "\n  %s: want", test_name);
    for (size_t b = 0; b < n; b++) {
      fprintf(stderr, " %02x", k->want[b]);
    }
    fprintf(stderr, "\n");
  }
  CHECK(memcmp(res, k->want, n) == 0);
}

static void check_walker(u32 n) {
  static char name[64];
  u8 res[8 * NI];
  for (u32 q = 0; q < n; q++) {
    snprintf(name, sizeof name, "walker/%s", KERNS[q].name);
    TEST_BEGIN(name);
    memset(res, 0x5A, sizeof res);
    CHECK(run_walker(&KERNS[q], res));
    check_bytes(&KERNS[q], res);
  }
}

// === Rendered C ============================================================

typedef void (*LegacyFn)(void *, const void *const *, unsigned,
                         const unsigned *, const unsigned *);

static void check_rendered(u32 n) {
  static char name[64];
  char  *src = NULL;
  size_t len = 0;
  FILE  *fp  = open_memstream(&src, &len);
  for (u32 q = 0; q < n; q++) {
    char fn_name[32];
    snprintf(fn_name, sizeof fn_name, "k_cast_%u", q);
    cg_render_uop_kernel_c(KERNS[q].root, fn_name, KERNS[q].out, &KERNS[q].in, 1, fp);
  }
  fclose(fp);
  char const *tmp = getenv("TMPDIR");
  char path[512];
  snprintf(path, sizeof path, "%s/thvm_walk_cast_XXXXXX.c",
           (tmp != NULL && tmp[0] != '\0') ? tmp : "/tmp");
  int fd = mkstemps(path, 2);
  void *h = NULL;
  if (fd >= 0) {
    FILE *cf = fdopen(fd, "w");
    fputs(src, cf);
    fclose(cf);
    char so[600];
    snprintf(so, sizeof so, "%s.so", path);
    char cmd[1400];
    snprintf(cmd, sizeof cmd, "%s -O1 -shared -fPIC -o '%s' '%s'", cpu_cc(), so, path);
    h = (system(cmd) == 0) ? dlopen(so, RTLD_NOW) : NULL;
    unlink(path);
    unlink(so);
  }
  free(src);
  TEST_BEGIN("rendered-c/compiles");
  CHECK(h != NULL);
  u8 res[8 * NI];
  for (u32 q = 0; q < n; q++) {
    snprintf(name, sizeof name, "rendered-c/%s", KERNS[q].name);
    TEST_BEGIN(name);
    char fn_name[32];
    snprintf(fn_name, sizeof fn_name, "k_cast_%u", q);
    LegacyFn f = (h != NULL) ? (LegacyFn)dlsym(h, fn_name) : NULL;
    CHECK(f != NULL);
    if (f == NULL) {
      continue;
    }
    memset(res, 0x5A, sizeof res);
    const void *ins[1]   = { KERNS[q].x };
    unsigned    numel[1] = { NI };
    f(res, ins, NI, numel, NULL);
    check_bytes(&KERNS[q], res);
  }
  if (h != NULL) {
    dlclose(h);
  }
}

int main(void) {
  thvm_init();
  u32 n = build_kernels();
  check_walker(n);
  check_rendered(n);
  thvm_free();
  TEST_REPORT();
}
