// test_uop_walk_domains.c -- the CPU UOp walker evaluates every subterm in
// the domain the compiled C kernel gives it.
//
//   - an int term (RANGE, IADD, ILT, ...) read in a float context keeps its
//     value instead of reading 0.0;
//   - a float comparison under an int op (IAND(ILT, CMPLT)) compares floats,
//     not truncated floats;
//   - a CAST to int of a float expression truncates the float result, not
//     each operand;
//   - a float select stays float under CAST and BITCAST, and a float mask
//     condition is true when nonzero;
//   - a CAST to bool is truthiness;
//   - an op the walker has no rule for declines the kernel.
//
// Each kernel is built by hand, rendered through the production C entry
// (cg_render_uop_kernel_c_root), compiled with the JIT's C compiler and run,
// and walked by cpu_uop_walk.  Both results must equal a host reference byte
// for byte.
#include "../src/thvm.c"
#include "test.h"
#include <dlfcn.h>

#define WD_MAX_IN 3
#define WD_MAX_OUT_BYTES 256

typedef struct {
  u32         dtype;
  u32         numel;
  void const *data;
} WdIn;

typedef struct {
  Term buf[WD_MAX_IN];
  WdIn in[WD_MAX_IN];
  u32  n_in;
  Term out;
  u32  out_dtype;
  u32  out_numel;
} WdKernel;

static void wd_init(WdKernel *k, u32 out_dtype, u32 out_numel) {
  memset(k, 0, sizeof *k);
  u32 dims[1] = { out_numel };
  k->out       = uop_buffer_inst(UOP_SCOPE_GLOBAL, out_dtype, 1, dims, 0);
  k->out_dtype = out_dtype;
  k->out_numel = out_numel;
}

static Term wd_input(WdKernel *k, u32 dtype, u32 numel, void const *data) {
  u32 slot = k->n_in++;
  u32 dims[1] = { numel };
  k->buf[slot] = uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, 1, dims, slot + 1);
  k->in[slot].dtype = dtype;
  k->in[slot].numel = numel;
  k->in[slot].data  = data;
  return k->buf[slot];
}

static u32 wd_out_bytes(WdKernel const *k) {
  return k->out_numel * dtype_itemsize(k->out_dtype);
}

// Render `root`, compile it with the JIT's C compiler and run it.
static int wd_run_rendered(Term root, WdKernel const *k, u8 *res) {
  static char src[65536];
  FILE *fp = fmemopen(src, sizeof src, "w");
  if (fp == NULL) {
    return 0;
  }
  cg_render_uop_kernel_c_root(root, "k_domains", fp);
  fclose(fp);
  char const *tmpdir = getenv("TMPDIR");
  char path[512];
  snprintf(path, sizeof path, "%s/thvm_walk_domains_XXXXXX.c",
           (tmpdir != NULL && tmpdir[0] != '\0') ? tmpdir : ".");
  int fd = mkstemps(path, 2);
  if (fd < 0) {
    return 0;
  }
  FILE *cf = fdopen(fd, "w");
  fputs(src, cf);
  fclose(cf);
  char so[600];
  snprintf(so, sizeof so, "%s.so", path);
  char cmd[1400];
  snprintf(cmd, sizeof cmd, "%s -O1 -shared -fPIC -o '%s' '%s' 2>/dev/null",
           cpu_cc(), so, path);
  void *h = (system(cmd) == 0) ? dlopen(so, RTLD_NOW) : NULL;
  typedef void (*kfn)(void *, const void *const *, unsigned,
                      const unsigned *, const unsigned *);
  kfn kf = (h != NULL) ? (kfn)dlsym(h, "k_domains") : NULL;
  if (kf != NULL) {
    const void *ins[WD_MAX_IN] = {0};
    unsigned    numel[WD_MAX_IN] = {0};
    for (u32 i = 0; i < k->n_in; i++) {
      ins[i]   = k->in[i].data;
      numel[i] = k->in[i].numel;
    }
    memset(res, 0xA5, wd_out_bytes(k));
    kf(res, ins, k->out_numel, numel, NULL);
  } else {
    fprintf(stderr, "  render/compile failed:\n%s\n", src);
  }
  if (h != NULL) {
    dlclose(h);
  }
  unlink(path);
  unlink(so);
  return kf != NULL;
}

static u32 wd_tensor(u32 dtype, u32 numel, void const *data) {
  Shape s = {0};
  s.ndim    = 1;
  s.dims[0] = numel;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, dtype);
  if (data != NULL) {
    CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data,
                          (u64)numel * dtype_itemsize(dtype));
  }
  return tid;
}

// Run `root` through the CPU UOp walker.  Returns cpu_uop_walk's verdict.
static int wd_run_walker(Term root, WdKernel const *k, u8 *res) {
  KernelEntry ke;
  memset(&ke, 0, sizeof ke);
  u32  tids[WD_MAX_IN] = {0};
  u32  in_buf_ids[WD_MAX_IN] = {0};
  Term lift[WD_MAX_IN] = {0};
  for (u32 i = 0; i < k->n_in; i++) {
    in_buf_ids[i] = TENS[wd_tensor(k->in[i].dtype, k->in[i].numel,
                                   k->in[i].data)].buf_id;
    lift[i] = k->buf[i];
  }
  ke.cached_lift.in_bufs    = lift;
  ke.cached_lift.in_cap     = WD_MAX_IN;
  u8 fill[WD_MAX_OUT_BYTES];
  memset(fill, 0xA5, sizeof fill);
  u32 tout = wd_tensor(k->out_dtype, k->out_numel, fill);
  ke.n_inputs               = k->n_in;
  ke.input_tids             = tids;
  ke.output_dtype           = k->out_dtype;
  ke.cached_lift.store_root = root;
  ke.cached_lift.out_buf    = k->out;
  ke.cached_lift.n_inputs   = k->n_in;
  int ok = cpu_uop_walk(&ke, in_buf_ids, TENS[tout].buf_id);
  CPU_BACKEND.buf_read(TENS[tout].buf_id, res, wd_out_bytes(k));
  return ok;
}

static void wd_report(u8 const *got, void const *want, WdKernel const *k,
                      char const *what) {
  u32 esz = dtype_itemsize(k->out_dtype);
  for (u32 i = 0; i < k->out_numel; i++) {
    if (memcmp(got + i * esz, (u8 const *)want + i * esz, esz) == 0) {
      continue;
    }
    if (k->out_dtype == DT_FP32) {
      f32 g, w;
      memcpy(&g, got + i * esz, 4);
      memcpy(&w, (u8 const *)want + i * esz, 4);
      fprintf(stderr, "  %s: out[%u] got=%.9g want=%.9g\n", what, i, g, w);
    } else if (k->out_dtype == DT_FP64) {
      f64 g, w;
      memcpy(&g, got + i * esz, 8);
      memcpy(&w, (u8 const *)want + i * esz, 8);
      fprintf(stderr, "  %s: out[%u] got=%.17g want=%.17g\n", what, i, g, w);
    } else {
      i32 g, w;
      memcpy(&g, got + i * esz, 4);
      memcpy(&w, (u8 const *)want + i * esz, 4);
      fprintf(stderr, "  %s: out[%u] got=%d (0x%08x) want=%d (0x%08x)\n",
              what, i, g, (u32)g, w, (u32)w);
    }
  }
}

// Rendered C and the walker must both equal `want` byte for byte.
static void wd_check(Term root, WdKernel const *k, void const *want,
                     char const *what) {
  u8 got[WD_MAX_OUT_BYTES];
  u32 nb = wd_out_bytes(k);
  char label[96];
  CHECK(nb <= WD_MAX_OUT_BYTES);
  CHECK(wd_run_rendered(root, k, got));
  snprintf(label, sizeof label, "%s rendered", what);
  wd_report(got, want, k, label);
  CHECK(memcmp(got, want, nb) == 0);
  int walked = wd_run_walker(root, k, got);
  CHECK(walked);
  snprintf(label, sizeof label, "%s walker", what);
  if (walked) {
    wd_report(got, want, k, label);
  }
  CHECK(walked && memcmp(got, want, nb) == 0);
}

static Term f32c(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return uop_const(DT_FP32, b);
}

static Term i32c(i32 v) {
  return uop_const(DT_INT32, (u32)v);
}

// CAST node built directly: uop_cast folds CAST(RANGE, f32) away because
// term_dtype_in reports RANGE as f32.
static Term raw_cast(Term src, u32 dst_dtype) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, dst_dtype));
  return term_new(0, TAG_UOP, UOP_CAST, loc);
}

#define N 8

static f32 const XF[N] = { 1.25f, 2.75f, -0.5f, 3.5f, 0.25f, 1.25f, 2.75f, 0.75f };
static f32 const YF[N] = { 1.75f, 2.25f, -0.25f, 3.25f, 0.75f, 1.75f, 2.25f, 0.25f };

static void test_range_in_float_context(void) {
  WdKernel k;
  wd_init(&k, DT_FP32, N);
  Term i = uop_range(0, KAX_LOOP, N);
  f32 want[N];
  for (u32 r = 0; r < N; r++) {
    want[r] = (f32)r * 0.5f;
  }
  wd_check(uop_store(k.out, i, uop_binary(UOP_MUL, raw_cast(i, DT_FP32), f32c(0.5f))),
           &k, want, "cast-of-range");
  wd_check(uop_store(k.out, i, uop_binary(UOP_MUL, i, f32c(0.5f))),
           &k, want, "bare-range");
}

static void test_int_ops_in_float_context(void) {
  WdKernel k;
  wd_init(&k, DT_FP32, N);
  Term xb = wd_input(&k, DT_FP32, N, XF);
  Term i  = uop_range(0, KAX_LOOP, N);
  f32 want[N];
  for (u32 r = 0; r < N; r++) {
    want[r] = (f32)(3 * (i32)r + 1);
  }
  Term lin = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, i, i32c(3)), i32c(1));
  wd_check(uop_store(k.out, i, lin), &k, want, "iadd-stored-to-float");
  for (u32 r = 0; r < N; r++) {
    want[r] = XF[r] * (f32)(r + 1);
  }
  Term scaled = uop_binary(UOP_MUL, uop_index_e(xb, i), uop_int_binary(UOP_IADD, i, i32c(1)));
  wd_check(uop_store(k.out, i, scaled), &k, want, "iadd-under-float-mul");
  for (u32 r = 0; r < N; r++) {
    want[r] = (f32)((r < 3 ? 1 : 0) + (i32)r / 2);
  }
  Term mixed = uop_binary(UOP_ADD, uop_int_binary(UOP_ILT, i, i32c(3)),
                          uop_int_binary(UOP_IDIV, i, i32c(2)));
  wd_check(uop_store(k.out, i, mixed), &k, want, "ilt-plus-idiv");
}

// IAND(ILT(i, 5), CMPLT(x, y)) must compare x and y as floats: the values
// differ only in their fractional parts, so a truncating compare flips it.
static void test_float_compare_under_iand(void) {
  WdKernel k;
  wd_init(&k, DT_FP32, N);
  Term xb = wd_input(&k, DT_FP32, N, XF);
  Term yb = wd_input(&k, DT_FP32, N, YF);
  Term i  = uop_range(0, KAX_LOOP, N);
  Term x  = uop_index_e(xb, i);
  Term y  = uop_index_e(yb, i);
  Term cond = uop_int_binary(UOP_IAND, uop_int_binary(UOP_ILT, i, i32c(5)),
                             uop_binary(UOP_CMPLT, x, y));
  CHECK_EQ(term_ext(cond), UOP_IAND);
  Term sel = uop_iwhere(cond, x, y);
  CHECK_EQ(term_ext(sel), UOP_IWHERE);
  f32 want[N];
  for (u32 r = 0; r < N; r++) {
    want[r] = (r < 5 && XF[r] < YF[r]) ? XF[r] : YF[r];
  }
  wd_check(uop_store(k.out, i, sel), &k, want, "iand-cmplt");
  // CMPEQ: trunc(1.25) == trunc(1.75), but 1.25 != 1.75.
  Term eq = uop_iwhere(uop_int_binary(UOP_IAND, uop_int_binary(UOP_ILT, i, i32c(5)),
                                      uop_binary(UOP_CMPEQ, x, y)),
                       f32c(1.0f), f32c(2.0f));
  for (u32 r = 0; r < N; r++) {
    want[r] = (r < 5 && XF[r] == YF[r]) ? 1.0f : 2.0f;
  }
  wd_check(uop_store(k.out, i, eq), &k, want, "iand-cmpeq");
}

// 1 iff the IAND tree `t` has a CMPLT conjunct.
static int iand_has_cmplt(Term t) {
  if (term_tag(t) != TAG_UOP) {
    return 0;
  }
  if (term_ext(t) == UOP_CMPLT) {
    return 1;
  }
  if (term_ext(t) != UOP_IAND) {
    return 0;
  }
  return iand_has_cmplt(heap_read(term_val(t) + 0))
      || iand_has_cmplt(heap_read(term_val(t) + 1));
}

// Serial scatter-max: OUT = -inf, then for each e with a valid idx[e],
// OUT[idx[e]] = max(OUT[idx[e]], src[e]).  The update value
// IWHERE(valid, IWHERE(CMPLT(old, sv), sv, old), old) built through
// uop_iwhere collapses to IWHERE(IAND(valid, CMPLT(old, sv)), sv, old).
#define SV 4
#define SE 12
static void test_scatter_max_rmw(void) {
  static i32 const idx[SE] = { 0, 2, 2, -1, 1, SV, 0, 2, 10982377, 1, -19238, 0 };
  static f32 const src[SE] = { 1.25f, 0.5f, 0.75f, 9.0f, -1.5f, 9.5f,
                               1.75f, 0.625f, 8.0f, -1.25f, 7.0f, 1.5f };
  WdKernel k;
  wd_init(&k, DT_FP32, SV);
  Term ib = wd_input(&k, DT_INT32, SE, idx);
  Term sb = wd_input(&k, DT_FP32, SE, src);
  Term v  = uop_range(0, KAX_LOOP, SV);
  Term e  = uop_range(1, KAX_LOOP, SE);
  Term ninf  = uop_const(DT_FP32, 0xFF800000u);
  Term init  = uop_store(k.out, v, ninf);
  Term iv    = uop_index_e(ib, e);
  Term valid = uop_int_binary(UOP_IAND, uop_int_binary(UOP_ILT, i32c(-1), iv),
                              uop_int_binary(UOP_ILT, iv, i32c(SV)));
  Term vsafe = uop_iwhere(valid, iv, i32c(0));
  Term old   = uop_index_e(k.out, vsafe);
  Term sv    = uop_index_e(sb, e);
  Term upd   = uop_iwhere(uop_binary(UOP_CMPLT, old, sv), sv, old);
  Term val   = uop_iwhere(valid, upd, old);
  CHECK_EQ(term_ext(val), UOP_IWHERE);
  Term vcond = heap_read(term_val(val) + 0);
  CHECK_EQ(term_ext(vcond), UOP_IAND);
  CHECK(iand_has_cmplt(vcond));
  CHECK_EQ(heap_read(term_val(val) + 1), sv);
  CHECK_EQ(heap_read(term_val(val) + 2), old);
  Term root = uop_after(uop_store(k.out, vsafe, val), init);
  f32 want[SV];
  for (u32 j = 0; j < SV; j++) {
    want[j] = -INFINITY;
  }
  for (u32 q = 0; q < SE; q++) {
    if (idx[q] >= 0 && idx[q] < SV && want[idx[q]] < src[q]) {
      want[idx[q]] = src[q];
    }
  }
  wd_check(root, &k, want, "scatter-max");
}

// CAST(ADD(x, y) -> i32) truncates the float sum: 0.625 + 0.5 -> 1, where
// truncating each operand first gives 0.
static void test_float_sum_cast_to_int(void) {
  static f32 const a[N] = { 0.625f, 1.5f, -0.75f, 2.25f, 0.5f, -1.25f, 3.75f, 0.125f };
  static f32 const b[N] = { 0.5f, 1.75f, -0.5f, 0.875f, 0.5f, -0.5f, 0.5f, 0.875f };
  WdKernel k;
  wd_init(&k, DT_INT32, N);
  Term ab = wd_input(&k, DT_FP32, N, a);
  Term bb = wd_input(&k, DT_FP32, N, b);
  Term i  = uop_range(0, KAX_LOOP, N);
  Term sum = uop_binary(UOP_ADD, uop_index_e(ab, i), uop_index_e(bb, i));
  i32 want[N];
  for (u32 r = 0; r < N; r++) {
    want[r] = (i32)(a[r] + b[r]);
  }
  wd_check(uop_store(k.out, i, uop_cast(sum, DT_INT32)), &k, want, "float-sum-cast-to-int");
  wd_check(uop_store(k.out, i, sum), &k, want, "float-sum-stored-to-int");
}

// A select between floats keeps its fraction under CAST and its float bits
// under BITCAST.
static void test_float_select_under_cast(void) {
  Term i = uop_range(0, KAX_LOOP, N);
  {
    WdKernel k;
    wd_init(&k, DT_FP64, N);
    Term x = uop_index_e(wd_input(&k, DT_FP32, N, XF), i);
    Term y = uop_index_e(wd_input(&k, DT_FP32, N, YF), i);
    Term sel = uop_iwhere(uop_binary(UOP_CMPLT, x, y), x, y);
    f64 want[N];
    for (u32 r = 0; r < N; r++) {
      want[r] = (f64)((XF[r] < YF[r]) ? XF[r] : YF[r]);
    }
    wd_check(uop_store(k.out, i, uop_cast(sel, DT_FP64)), &k, want, "select-cast-f64");
  }
  {
    WdKernel k;
    wd_init(&k, DT_INT32, N);
    Term x = uop_index_e(wd_input(&k, DT_FP32, N, XF), i);
    Term y = uop_index_e(wd_input(&k, DT_FP32, N, YF), i);
    Term sel = uop_iwhere(uop_binary(UOP_CMPLT, x, y), x, y);
    Term bits = uop_bitcast(sel, DT_INT32);
    CHECK_EQ(term_ext(bits), UOP_BITCAST);
    i32 want[N];
    for (u32 r = 0; r < N; r++) {
      f32 m = (XF[r] < YF[r]) ? XF[r] : YF[r];
      memcpy(&want[r], &m, sizeof m);
    }
    wd_check(uop_store(k.out, i, bits), &k, want, "select-bitcast-i32");
  }
}

// IWHERE(m, x, y) with a float mask m: 0.5 and -0.25 are true, as in C.
static void test_float_mask_condition(void) {
  static f32 const m[N] = { 0.5f, 0.0f, 1.0f, -0.25f, 0.0f, 2.0f, 0.125f, -0.0f };
  WdKernel k;
  wd_init(&k, DT_FP32, N);
  Term i  = uop_range(0, KAX_LOOP, N);
  Term mm = uop_index_e(wd_input(&k, DT_FP32, N, m), i);
  Term x  = uop_index_e(wd_input(&k, DT_FP32, N, XF), i);
  Term y  = uop_index_e(wd_input(&k, DT_FP32, N, YF), i);
  f32 want[N];
  for (u32 r = 0; r < N; r++) {
    want[r] = (m[r] != 0.0f) ? XF[r] : YF[r];
  }
  wd_check(uop_store(k.out, i, uop_iwhere(mm, x, y)), &k, want, "float-mask-cond");
}

// CAST(x -> bool) is x != 0, so it scales y by 0 or 1.
static void test_cast_to_bool_is_truthiness(void) {
  static f32 const m[N] = { 0.5f, 0.0f, -2.0f, 0.25f, 0.0f, 3.0f, -0.125f, 1.0f };
  WdKernel k;
  wd_init(&k, DT_FP32, N);
  Term i  = uop_range(0, KAX_LOOP, N);
  Term mm = uop_index_e(wd_input(&k, DT_FP32, N, m), i);
  Term y  = uop_index_e(wd_input(&k, DT_FP32, N, YF), i);
  Term mb = uop_cast(mm, DT_BOOL);
  CHECK_EQ(term_ext(mb), UOP_CAST);
  f32 want[N];
  for (u32 r = 0; r < N; r++) {
    want[r] = (m[r] != 0.0f) ? YF[r] : 0.0f * YF[r];
  }
  wd_check(uop_store(k.out, i, uop_binary(UOP_MUL, mb, y)), &k, want, "cast-to-bool");
}

// A DETACH never reaches a kernel; the walker has no rule for it and must
// decline rather than read it as 0.0.
static void test_unknown_op_declines(void) {
  WdKernel k;
  wd_init(&k, DT_FP32, N);
  Term i = uop_range(0, KAX_LOOP, N);
  Term x = uop_index_e(wd_input(&k, DT_FP32, N, XF), i);
  u64 loc = heap_alloc(1);
  heap_set(loc, x);
  Term det  = term_new(0, TAG_UOP, UOP_DETACH, loc);
  Term root = uop_store(k.out, i, uop_binary(UOP_ADD, x, det));
  u8 got[WD_MAX_OUT_BYTES];
  CHECK(!wd_run_walker(root, &k, got));
}

int main(void) {
  thvm_init();
  TEST_BEGIN("uop-walk-domains/range-in-float-context");
  test_range_in_float_context();
  TEST_BEGIN("uop-walk-domains/int-ops-in-float-context");
  test_int_ops_in_float_context();
  TEST_BEGIN("uop-walk-domains/float-compare-under-iand");
  test_float_compare_under_iand();
  TEST_BEGIN("uop-walk-domains/scatter-max-rmw");
  test_scatter_max_rmw();
  TEST_BEGIN("uop-walk-domains/float-sum-cast-to-int");
  test_float_sum_cast_to_int();
  TEST_BEGIN("uop-walk-domains/float-select-under-cast");
  test_float_select_under_cast();
  TEST_BEGIN("uop-walk-domains/float-mask-condition");
  test_float_mask_condition();
  TEST_BEGIN("uop-walk-domains/cast-to-bool-is-truthiness");
  test_cast_to_bool_is_truthiness();
  TEST_BEGIN("uop-walk-domains/unknown-op-declines");
  test_unknown_op_declines();
  thvm_free();
  TEST_REPORT();
}
