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
// Each kernel is built by hand and run through kernel_harness.h: the
// rendered C and cpu_uop_walk must both equal a host reference byte for
// byte.
#include "../src/thvm.c"
#include "test.h"
#include "kernel_harness.h"

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
  KhKernel k;
  kh_init(&k, DT_FP32, N);
  Term i = uop_range(0, KAX_LOOP, N);
  f32 want[N];
  for (u32 r = 0; r < N; r++) {
    want[r] = (f32)r * 0.5f;
  }
  kh_check(uop_store(k.out, i, uop_binary(UOP_MUL, raw_cast(i, DT_FP32), kh_f32(0.5f))),
           &k, want, "cast-of-range");
  kh_check(uop_store(k.out, i, uop_binary(UOP_MUL, i, kh_f32(0.5f))),
           &k, want, "bare-range");
}

static void test_int_ops_in_float_context(void) {
  KhKernel k;
  kh_init(&k, DT_FP32, N);
  Term xb = kh_input(&k, DT_FP32, N, XF);
  Term i  = uop_range(0, KAX_LOOP, N);
  f32 want[N];
  for (u32 r = 0; r < N; r++) {
    want[r] = (f32)(3 * (i32)r + 1);
  }
  Term lin = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, i, kh_i32(3)), kh_i32(1));
  kh_check(uop_store(k.out, i, lin), &k, want, "iadd-stored-to-float");
  for (u32 r = 0; r < N; r++) {
    want[r] = XF[r] * (f32)(r + 1);
  }
  Term scaled = uop_binary(UOP_MUL, uop_index_e(xb, i), uop_int_binary(UOP_IADD, i, kh_i32(1)));
  kh_check(uop_store(k.out, i, scaled), &k, want, "iadd-under-float-mul");
  for (u32 r = 0; r < N; r++) {
    want[r] = (f32)((r < 3 ? 1 : 0) + (i32)r / 2);
  }
  Term mixed = uop_binary(UOP_ADD, uop_int_binary(UOP_ILT, i, kh_i32(3)),
                          uop_int_binary(UOP_IDIV, i, kh_i32(2)));
  kh_check(uop_store(k.out, i, mixed), &k, want, "ilt-plus-idiv");
}

// IAND(ILT(i, 5), CMPLT(x, y)) must compare x and y as floats: the values
// differ only in their fractional parts, so a truncating compare flips it.
static void test_float_compare_under_iand(void) {
  KhKernel k;
  kh_init(&k, DT_FP32, N);
  Term xb = kh_input(&k, DT_FP32, N, XF);
  Term yb = kh_input(&k, DT_FP32, N, YF);
  Term i  = uop_range(0, KAX_LOOP, N);
  Term x  = uop_index_e(xb, i);
  Term y  = uop_index_e(yb, i);
  Term cond = uop_int_binary(UOP_IAND, uop_int_binary(UOP_ILT, i, kh_i32(5)),
                             uop_binary(UOP_CMPLT, x, y));
  CHECK_EQ(term_ext(cond), UOP_IAND);
  Term sel = uop_iwhere(cond, x, y);
  CHECK_EQ(term_ext(sel), UOP_IWHERE);
  f32 want[N];
  for (u32 r = 0; r < N; r++) {
    want[r] = (r < 5 && XF[r] < YF[r]) ? XF[r] : YF[r];
  }
  kh_check(uop_store(k.out, i, sel), &k, want, "iand-cmplt");
  // CMPEQ: trunc(1.25) == trunc(1.75), but 1.25 != 1.75.
  Term eq = uop_iwhere(uop_int_binary(UOP_IAND, uop_int_binary(UOP_ILT, i, kh_i32(5)),
                                      uop_binary(UOP_CMPEQ, x, y)),
                       kh_f32(1.0f), kh_f32(2.0f));
  for (u32 r = 0; r < N; r++) {
    want[r] = (r < 5 && XF[r] == YF[r]) ? 1.0f : 2.0f;
  }
  kh_check(uop_store(k.out, i, eq), &k, want, "iand-cmpeq");
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
  KhKernel k;
  kh_init(&k, DT_FP32, SV);
  Term ib = kh_input(&k, DT_INT32, SE, idx);
  Term sb = kh_input(&k, DT_FP32, SE, src);
  Term v  = uop_range(0, KAX_LOOP, SV);
  Term e  = uop_range(1, KAX_LOOP, SE);
  Term ninf  = uop_const(DT_FP32, 0xFF800000u);
  Term init  = uop_store(k.out, v, ninf);
  Term iv    = uop_index_e(ib, e);
  Term valid = uop_int_binary(UOP_IAND, uop_int_binary(UOP_ILT, kh_i32(-1), iv),
                              uop_int_binary(UOP_ILT, iv, kh_i32(SV)));
  Term vsafe = uop_iwhere(valid, iv, kh_i32(0));
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
  kh_check(root, &k, want, "scatter-max");
}

// CAST(ADD(x, y) -> i32) truncates the float sum: 0.625 + 0.5 -> 1, where
// truncating each operand first gives 0.
static void test_float_sum_cast_to_int(void) {
  static f32 const a[N] = { 0.625f, 1.5f, -0.75f, 2.25f, 0.5f, -1.25f, 3.75f, 0.125f };
  static f32 const b[N] = { 0.5f, 1.75f, -0.5f, 0.875f, 0.5f, -0.5f, 0.5f, 0.875f };
  KhKernel k;
  kh_init(&k, DT_INT32, N);
  Term ab = kh_input(&k, DT_FP32, N, a);
  Term bb = kh_input(&k, DT_FP32, N, b);
  Term i  = uop_range(0, KAX_LOOP, N);
  Term sum = uop_binary(UOP_ADD, uop_index_e(ab, i), uop_index_e(bb, i));
  i32 want[N];
  for (u32 r = 0; r < N; r++) {
    want[r] = (i32)(a[r] + b[r]);
  }
  kh_check(uop_store(k.out, i, uop_cast(sum, DT_INT32)), &k, want, "float-sum-cast-to-int");
  kh_check(uop_store(k.out, i, sum), &k, want, "float-sum-stored-to-int");
}

// A select between floats keeps its fraction under CAST and its float bits
// under BITCAST.
static void test_float_select_under_cast(void) {
  Term i = uop_range(0, KAX_LOOP, N);
  {
    KhKernel k;
    kh_init(&k, DT_FP64, N);
    Term x = uop_index_e(kh_input(&k, DT_FP32, N, XF), i);
    Term y = uop_index_e(kh_input(&k, DT_FP32, N, YF), i);
    Term sel = uop_iwhere(uop_binary(UOP_CMPLT, x, y), x, y);
    f64 want[N];
    for (u32 r = 0; r < N; r++) {
      want[r] = (f64)((XF[r] < YF[r]) ? XF[r] : YF[r]);
    }
    kh_check(uop_store(k.out, i, uop_cast(sel, DT_FP64)), &k, want, "select-cast-f64");
  }
  {
    KhKernel k;
    kh_init(&k, DT_INT32, N);
    Term x = uop_index_e(kh_input(&k, DT_FP32, N, XF), i);
    Term y = uop_index_e(kh_input(&k, DT_FP32, N, YF), i);
    Term sel = uop_iwhere(uop_binary(UOP_CMPLT, x, y), x, y);
    Term bits = uop_bitcast(sel, DT_INT32);
    CHECK_EQ(term_ext(bits), UOP_BITCAST);
    i32 want[N];
    for (u32 r = 0; r < N; r++) {
      f32 m = (XF[r] < YF[r]) ? XF[r] : YF[r];
      memcpy(&want[r], &m, sizeof m);
    }
    kh_check(uop_store(k.out, i, bits), &k, want, "select-bitcast-i32");
  }
}

// IWHERE(m, x, y) with a float mask m: 0.5 and -0.25 are true, as in C.
static void test_float_mask_condition(void) {
  static f32 const m[N] = { 0.5f, 0.0f, 1.0f, -0.25f, 0.0f, 2.0f, 0.125f, -0.0f };
  KhKernel k;
  kh_init(&k, DT_FP32, N);
  Term i  = uop_range(0, KAX_LOOP, N);
  Term mm = uop_index_e(kh_input(&k, DT_FP32, N, m), i);
  Term x  = uop_index_e(kh_input(&k, DT_FP32, N, XF), i);
  Term y  = uop_index_e(kh_input(&k, DT_FP32, N, YF), i);
  f32 want[N];
  for (u32 r = 0; r < N; r++) {
    want[r] = (m[r] != 0.0f) ? XF[r] : YF[r];
  }
  kh_check(uop_store(k.out, i, uop_iwhere(mm, x, y)), &k, want, "float-mask-cond");
}

// CAST(x -> bool) is x != 0, so it scales y by 0 or 1.
static void test_cast_to_bool_is_truthiness(void) {
  static f32 const m[N] = { 0.5f, 0.0f, -2.0f, 0.25f, 0.0f, 3.0f, -0.125f, 1.0f };
  KhKernel k;
  kh_init(&k, DT_FP32, N);
  Term i  = uop_range(0, KAX_LOOP, N);
  Term mm = uop_index_e(kh_input(&k, DT_FP32, N, m), i);
  Term y  = uop_index_e(kh_input(&k, DT_FP32, N, YF), i);
  Term mb = uop_cast(mm, DT_BOOL);
  CHECK_EQ(term_ext(mb), UOP_CAST);
  f32 want[N];
  for (u32 r = 0; r < N; r++) {
    want[r] = (m[r] != 0.0f) ? YF[r] : 0.0f * YF[r];
  }
  kh_check(uop_store(k.out, i, uop_binary(UOP_MUL, mb, y)), &k, want, "cast-to-bool");
}

// A DETACH never reaches a kernel; the walker has no rule for it and must
// decline rather than read it as 0.0.
static void test_unknown_op_declines(void) {
  KhKernel k;
  kh_init(&k, DT_FP32, N);
  Term i = uop_range(0, KAX_LOOP, N);
  Term x = uop_index_e(kh_input(&k, DT_FP32, N, XF), i);
  u64 loc = heap_alloc(1);
  heap_set(loc, x);
  Term det  = term_new(0, TAG_UOP, UOP_DETACH, loc);
  Term root = uop_store(k.out, i, uop_binary(UOP_ADD, x, det));
  u8 got[N * sizeof(f32)];
  CHECK(!kh_run_walker(root, &k, got));
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
