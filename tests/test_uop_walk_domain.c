// test_uop_walk_domain.c - the CPU UOp walker evaluates each subterm in
// its own dtype, as the rendered C kernel does.
//
// The walker has a float evaluator (doubles) and an int one (i64), and a
// consumer hands each subterm to one of them.  The dtype of the subterm
// picks the evaluator, not the consumer's dtype and not the op's kind:
//   - a CMPLT or CMPEQ compares in its operands' dtype: an i64 compare
//     past 2^53, where a double rounds, a u64 compare past 2^63 unsigned,
//     and an index compare (RANGE, IADD, IMUL), which the float evaluator
//     cannot evaluate, as an int;
//   - an IWHERE condition is true in its own dtype, so a fractional float
//     mask is true, and a bool cast of a float comparison compares floats;
//   - a CAST converts its source's value, truncating a float toward zero
//     and wrapping an int to the destination: CAST(1.5 * 2.5, i32) is 3,
//     CAST(300, i8) is 44, and CAST(RANGE, f32) is the index;
//   - a BITCAST of an int sum takes the sum's bits;
//   - a float body in an int kernel (py's int32 true division builds one)
//     computes in float and converts, as C converts `acc + a * (1.0f/b)`
//     back into an int accumulator and `out[i] = a * (1.0f/b)` into an int
//     store.
// Unit kernels run through cpu_uop_walk.  The realized kernels run on the
// default route, the CPU JIT, and then again through the walker.
#include "../src/thvm.c"
#include "test.h"

#define P53 ((u64)1 << 53)
#define P63 ((u64)1 << 63)
#define MAXN 8

typedef struct {
  u32         dt;
  void const *data;
  u32         n;
} Buf;

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

static Term gbuf(u32 dt, u32 n, u32 inst) {
  u32 dims[1] = { n };
  return uop_buffer_inst(UOP_SCOPE_GLOBAL, dt, 1, dims, inst);
}

static Term ic(u32 dt, i32 v) {
  return uop_const(dt, (u32)v);
}

static Term fc(f32 v) {
  u32 bits;
  memcpy(&bits, &v, sizeof bits);
  return uop_const(DT_FP32, bits);
}

// Run `root`, which stores into `out` (instance 0) and reads bufs[s]
// through ins[s] (instance s + 1), through cpu_uop_walk into `res`.
static int walk(Term root, Term out, u32 out_dt, u32 out_n,
                Term const *ins, Buf const *bufs, u32 n_ins, void *res) {
  KernelEntry ke;
  memset(&ke, 0, sizeof ke);
  u32  tids[4] = { 0, 0, 0, 0 };
  u32  ids[4]  = { 0, 0, 0, 0 };
  Term lift[4] = { 0, 0, 0, 0 };
  for (u32 s = 0; s < n_ins; s++) {
    ids[s]  = cpu_buf(bufs[s].dt, bufs[s].data, bufs[s].n);
    lift[s] = ins[s];
  }
  ke.cached_lift.in_bufs    = lift;
  ke.cached_lift.in_cap     = 4;
  ke.n_inputs               = n_ins;
  ke.input_tids             = tids;
  ke.output_dtype           = out_dt;
  ke.cached_lift.store_root = root;
  ke.cached_lift.out_buf    = out;
  ke.cached_lift.n_inputs   = n_ins;
  u32 o = cpu_buf(out_dt, NULL, out_n);
  if (!cpu_uop_walk(&ke, ids, o)) {
    return 0;
  }
  CPU_BACKEND.buf_read(o, res, (u64)out_n * dtype_itemsize(out_dt));
  return 1;
}

static i64 elem(u32 dt, void const *p, u32 k) {
  switch (dt) {
    case DT_BOOL:   return ((u8  const *)p)[k];
    case DT_INT8:   return ((i8  const *)p)[k];
    case DT_INT32:  return ((i32 const *)p)[k];
    case DT_UINT32: return ((u32 const *)p)[k];
    default:        return ((i64 const *)p)[k];
  }
}

static void check_ints(char const *route, u32 dt, void const *got,
                       i64 const *want, u32 n) {
  for (u32 k = 0; k < n; k++) {
    if (elem(dt, got, k) != want[k]) {
      fprintf(stderr, "  %s %s: [%u] got %lld want %lld\n", route, test_name, k,
              (long long)elem(dt, got, k), (long long)want[k]);
    }
    CHECK_EQ(elem(dt, got, k), want[k]);
  }
}

// === Unit kernels ========================================================

// out[r] = pick(a[r], b[r]) over two same-dtype inputs.
static void unit_pair(char const *name, u32 dt, u32 out_dt, void const *a,
                      void const *b, u32 n, Term (*pick)(Term, Term),
                      i64 const *want) {
  TEST_BEGIN(name);
  Term out    = gbuf(out_dt, n, 0);
  Term ins[2] = { gbuf(dt, n, 1), gbuf(dt, n, 2) };
  Buf  bufs[2] = { { dt, a, n }, { dt, b, n } };
  Term r    = uop_range(0, KAX_LOOP, n);
  Term root = uop_store(out, r, pick(uop_index_e(ins[0], r), uop_index_e(ins[1], r)));
  u8 res[8 * MAXN];
  memset(res, 0, sizeof res);
  CHECK(walk(root, out, out_dt, n, ins, bufs, 2, res));
  check_ints("walker", out_dt, res, want, n);
}

static Term pick_max(Term a, Term b) {
  return uop_iwhere(uop_binary(UOP_CMPLT, a, b), b, a);
}

static Term pick_eq(Term a, Term b) {
  return uop_iwhere(uop_binary(UOP_CMPEQ, a, b), ic(DT_INT64, 1), ic(DT_INT64, 0));
}

static Term pick_bitcast_sum(Term a, Term b) {
  return uop_bitcast(uop_binary(UOP_ADD, a, b), DT_INT32);
}

static Term pick_quotient(Term a, Term b) {
  return uop_binary(UOP_MUL, a, uop_unary(UOP_RECIP, b));
}

static void check_compares(void) {
  i64 const a[5] = { (i64)P53, 5, -(i64)(P53 + 1), (i64)P53 + 3, (i64)P53 + 7 };
  i64 const b[5] = { (i64)P53 + 1, 3, -(i64)P53, (i64)P53 + 2, (i64)P53 + 7 };
  i64 const want_max[5] = { (i64)P53 + 1, 5, -(i64)P53, (i64)P53 + 3, (i64)P53 + 7 };
  i64 const want_eq[5]  = { 0, 0, 0, 0, 1 };
  unit_pair("walker/i64-max-past-2p53", DT_INT64, DT_INT64, a, b, 5, pick_max, want_max);
  unit_pair("walker/i64-select-on-eq-past-2p53", DT_INT64, DT_INT64, a, b, 5, pick_eq, want_eq);

  u64 const ua[5] = { P63 + 1, 1, UINT64_MAX, 5, P63 };
  u64 const ub[5] = { P63 + 2, P63, UINT64_MAX - 1, 6, P63 };
  i64 const want_umax[5] = { (i64)(P63 + 2), (i64)P63, (i64)UINT64_MAX, 6, (i64)P63 };
  unit_pair("walker/u64-max-past-2p63", DT_UINT64, DT_UINT64, ua, ub, 5, pick_max, want_umax);

  u32 const sa[2] = { 0x80000000u, 5 };
  u32 const sb[2] = { 1, 7 };
  i64 const want_bits[2] = { -2147483647, 12 };
  unit_pair("walker/bitcast-of-u32-sum", DT_UINT32, DT_INT32, sa, sb, 2, pick_bitcast_sum,
            want_bits);

  i32 const qa[4] = { 7, -7, 9, 1 };
  i32 const qb[4] = { 2, 2, 4, 4 };
  i64 const want_q[4] = { 3, -3, 2, 0 };
  unit_pair("walker/i32-quotient-store", DT_INT32, DT_INT32, qa, qb, 4, pick_quotient, want_q);
}

// out[r] = value(r, x[r], f[r]) over an i32 x and an f32 f of six elements.
static i32 const X6[6] = { 100, 101, 102, 103, 104, 105 };

static void unit_indexed(char const *name, u32 out_dt, f32 const *f, Term value_of_r,
                         Term x, Term fb, i64 const *want) {
  TEST_BEGIN(name);
  Term out     = gbuf(out_dt, 6, 0);
  Term ins[2]  = { x, fb };
  Buf  bufs[2] = { { DT_INT32, X6, 6 }, { DT_FP32, f, 6 } };
  Term root = uop_store(out, uop_range(0, KAX_LOOP, 6), value_of_r);
  u8 res[8 * MAXN];
  memset(res, 0, sizeof res);
  CHECK(walk(root, out, out_dt, 6, ins, bufs, 2, res));
  if (!dtype_is_float(out_dt)) {
    check_ints("walker", out_dt, res, want, 6);
    return;
  }
  for (u32 k = 0; k < 6; k++) {
    double got = (out_dt == DT_FP64) ? ((f64 const *)res)[k] : ((f32 const *)res)[k];
    if (got != (double)want[k]) {
      fprintf(stderr, "  walker %s: [%u] got %g want %lld\n", test_name, k, got,
              (long long)want[k]);
    }
    CHECK(got == (double)want[k]);
  }
}

static void check_conditions(void) {
  Term x  = gbuf(DT_INT32, 6, 1);
  Term fb = gbuf(DT_FP32, 6, 2);
  Term r  = uop_range(0, KAX_LOOP, 6);
  Term xr = uop_index_e(x, r);
  Term fr = uop_index_e(fb, r);
  f32 const f_half[6]  = { 0.25f, 0.75f, 0.4f, 0.6f, -0.5f, 0.5f };
  f32 const f_small[6] = { 1.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
  f32 const f_mask[6]  = { 0.0f, 0.5f, 1.0f, -0.25f, 0.0f, 2.0f };
  f32 const f_pos[6]   = { -0.5f, 0.5f, 2.5f, 2.5f, 4.5f, 5.5f };
  f32 const f_scale[6] = { 1.5f, -1.5f, 0.3f, 3.9f, 2.0f, -0.2f };

  i64 const want_lt3[6] = { 100, 101, 102, 0, 0, 0 };
  unit_indexed("walker/range-cond-i32", DT_INT32, f_half,
               uop_iwhere(uop_binary(UOP_CMPLT, r, ic(DT_INT32, 3)), xr, ic(DT_INT32, 0)),
               x, fb, want_lt3);

  // The same select of f[r] into an f32 output: the float evaluator.
  i64 const want_f_lt3[6] = { 1, 1, 2, 0, 0, 0 };
  unit_indexed("walker/range-cond-f32", DT_FP32, f_small,
               uop_iwhere(uop_binary(UOP_CMPLT, r, ic(DT_INT32, 3)), fr, fc(0.0f)),
               x, fb, want_f_lt3);

  Term odd = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, r, ic(DT_INT32, 2)),
                            ic(DT_INT32, 1));
  i64 const want_odd[6] = { 100, 101, 102, -1, -1, -1 };
  unit_indexed("walker/index-expr-cond", DT_INT32, f_half,
               uop_iwhere(uop_binary(UOP_CMPLT, odd, ic(DT_INT32, 7)), xr, ic(DT_INT32, -1)),
               x, fb, want_odd);

  i64 const want_half[6] = { 100, 0, 102, 0, 104, 0 };
  unit_indexed("walker/f32-cond-in-int", DT_INT32, f_half,
               uop_iwhere(uop_binary(UOP_CMPLT, fr, fc(0.5f)), xr, ic(DT_INT32, 0)),
               x, fb, want_half);

  i64 const want_lt[6] = { 1, 0, 1, 0, 1, 0 };
  unit_indexed("walker/bool-cast-of-f32-compare", DT_BOOL, f_half,
               uop_cast(uop_binary(UOP_CMPLT, fr, fc(0.5f)), DT_BOOL), x, fb, want_lt);

  i64 const want_mask[6] = { 0, 101, 102, 103, 0, 105 };
  unit_indexed("walker/f32-mask-cond", DT_INT32, f_mask,
               uop_iwhere(fr, xr, ic(DT_INT32, 0)), x, fb, want_mask);

  i64 const want_pos[6] = { 1, 1, 0, 1, 0, 0 };
  unit_indexed("walker/f32-less-than-range", DT_INT32, f_pos,
               uop_iwhere(uop_binary(UOP_CMPLT, fr, r), ic(DT_INT32, 1), ic(DT_INT32, 0)),
               x, fb, want_pos);

  i64 const want_scaled[6] = { 3, -3, 0, 9, 5, 0 };
  unit_indexed("walker/cast-of-scaled-f32", DT_INT32, f_scale,
               uop_cast(uop_binary(UOP_MUL, fr, fc(2.5f)), DT_INT32), x, fb, want_scaled);

  // term_dtype_in types an index f32, so uop_cast(RANGE, f32) is the RANGE
  // itself: the float evaluator meets the index op.  A cast to f64 stays.
  i64 const want_sq[6] = { 0, 1, 4, 9, 16, 25 };
  Term r32 = uop_cast(r, DT_FP32);
  Term r64 = uop_cast(r, DT_FP64);
  unit_indexed("walker/index-in-f32-math", DT_FP32, f_small,
               uop_binary(UOP_MUL, r32, r32), x, fb, want_sq);
  unit_indexed("walker/cast-of-range-to-f64", DT_FP64, f_small,
               uop_binary(UOP_MUL, r64, r64), x, fb, want_sq);
}

static void check_narrowing_cast(void) {
  TEST_BEGIN("walker/cast-wraps-to-i8");
  i32 const v[4]    = { 300, -200, 127, 128 };
  i64 const want[4] = { 44, 56, 127, -128 };
  Term out    = gbuf(DT_INT32, 4, 0);
  Term ins[1] = { gbuf(DT_INT32, 4, 1) };
  Buf  bufs[1] = { { DT_INT32, v, 4 } };
  Term r    = uop_range(0, KAX_LOOP, 4);
  Term root = uop_store(out, r, uop_cast(uop_cast(uop_index_e(ins[0], r), DT_INT8), DT_INT32));
  u8 res[8 * MAXN];
  memset(res, 0, sizeof res);
  CHECK(walk(root, out, DT_INT32, 4, ins, bufs, 1, res));
  check_ints("walker", DT_INT32, res, want, 4);
}

// out[i] = reduce_j body(a[i, j], b[i, j]) over 2 x 3 int32 inputs.
static void unit_reduce(char const *name, u32 kind, i32 const *a, i32 const *b,
                        Term (*body)(Term, Term), i64 const *want) {
  TEST_BEGIN(name);
  Term out     = gbuf(DT_INT32, 2, 0);
  Term ins[2]  = { gbuf(DT_INT32, 6, 1), gbuf(DT_INT32, 6, 2) };
  Buf  bufs[2] = { { DT_INT32, a, 6 }, { DT_INT32, b, 6 } };
  Term i    = uop_range(0, KAX_LOOP, 2);
  Term j    = uop_range(1, KAX_REDUCE, 3);
  Term ij   = uop_int_binary(UOP_IADD, uop_int_binary(UOP_IMUL, i, ic(DT_INT32, 3)), j);
  Term red  = uop_reduce(kind, 1, body(uop_index_e(ins[0], ij), uop_index_e(ins[1], ij)));
  Term root = uop_store(out, i, red);
  u8 res[8 * MAXN];
  memset(res, 0, sizeof res);
  CHECK(walk(root, out, DT_INT32, 2, ins, bufs, 2, res));
  check_ints("walker", DT_INT32, res, want, 2);
}

static Term body_sqrt(Term a, Term b) {
  (void)b;
  return uop_unary(UOP_SQRT, a);
}

// The rendered C adds each float body value to the int accumulator in
// float and converts the sum back: -3.5, then -3 + 2.5, then 0 + 3.5 is 3,
// where truncating each value first gives 2.
static i32 const QA[6] = { -7, 5, 7, 7, 9, 11 };
static i32 const QB[6] = { 2, 2, 2, 2, 2, 2 };
static i32 const SA[6] = { 2, 3, 5, 16, 1, 4 };

static void check_float_bodies(void) {
  i64 const want_sum[2] = { 3, 12 };
  i64 const want_max[2] = { 3, 5 };
  i64 const want_sqrt[2] = { 4, 7 };
  unit_reduce("walker/i32-sum-of-quotients", REDUCE_SUM, QA, QB, pick_quotient, want_sum);
  unit_reduce("walker/i32-max-of-quotients", REDUCE_MAX, QA, QB, pick_quotient, want_max);
  unit_reduce("walker/i32-sum-of-sqrt", REDUCE_SUM, SA, QB, body_sqrt, want_sqrt);
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
  u8 got[8 * MAXN];
  TENS[tid].backend->buf_read(TENS[tid].buf_id, got, bytes);
  check_ints("realized", dt, got, want, n);
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
    u8 const zero[8 * MAXN] = {0};
    TENS[tid].backend->buf_write(TENS[tid].buf_id, zero, bytes);
    CHECK(cpu_uop_walk(ke, ids, TENS[tid].buf_id));
    free(ids);
    TENS[tid].backend->buf_read(TENS[tid].buf_id, got, bytes);
    check_ints("walked", dt, got, want, n);
  }
  CHECK_EQ(walked, 1);
}

static void check_end_to_end(void) {
  u32 const five[1] = { 5 };
  i64 const a[5] = { (i64)P53, 5, -(i64)(P53 + 1), (i64)P53 + 3, (i64)P53 + 7 };
  i64 const b[5] = { (i64)P53 + 1, 3, -(i64)P53, (i64)P53 + 2, (i64)P53 + 7 };

  TEST_BEGIN("e2e/i64-maximum-past-2p53");
  {
    i64 const want[5] = { (i64)P53 + 1, 5, -(i64)P53, (i64)P53 + 3, (i64)P53 + 7 };
    check_realized(pick_max(ten(DT_INT64, a, 1, five), ten(DT_INT64, b, 1, five)), DT_INT64,
                   want, 5);
  }

  TEST_BEGIN("e2e/i64-select-on-eq-past-2p53");
  {
    i64 const ones[5]  = { 1, 1, 1, 1, 1 };
    i64 const zeros[5] = { 0, 0, 0, 0, 0 };
    i64 const want[5]  = { 0, 0, 0, 0, 1 };
    Term eq = uop_binary(UOP_CMPEQ, ten(DT_INT64, a, 1, five), ten(DT_INT64, b, 1, five));
    check_realized(uop_iwhere(eq, ten(DT_INT64, ones, 1, five), ten(DT_INT64, zeros, 1, five)),
                   DT_INT64, want, 5);
  }

  TEST_BEGIN("e2e/u64-maximum-past-2p63");
  {
    u64 const ua[5] = { P63 + 1, 1, UINT64_MAX, 5, P63 };
    u64 const ub[5] = { P63 + 2, P63, UINT64_MAX - 1, 6, P63 };
    i64 const want[5] = { (i64)(P63 + 2), (i64)P63, (i64)UINT64_MAX, 6, (i64)P63 };
    check_realized(pick_max(ten(DT_UINT64, ua, 1, five), ten(DT_UINT64, ub, 1, five)),
                   DT_UINT64, want, 5);
  }

  TEST_BEGIN("e2e/i64-sum-of-maximum-past-2p53");
  {
    u32 const dims[2] = { 2, 4 };
    i64 const sa[8] = { (i64)P53, 1, 2, 3, -(i64)(P53 + 1), 4, 5, 6 };
    i64 const sb[8] = { (i64)P53 + 1, 0, 0, 0, -(i64)P53, 0, 0, 0 };
    i64 const want[2] = { (i64)P53 + 7, -(i64)P53 + 15 };
    Term mx = pick_max(ten(DT_INT64, sa, 2, dims), ten(DT_INT64, sb, 2, dims));
    check_realized(uop_reduce(REDUCE_SUM, 1, mx), DT_INT64, want, 2);
  }

  u32 const two_by_three[2] = { 2, 3 };
  TEST_BEGIN("e2e/i32-sum-of-quotients");
  {
    i64 const want[2] = { 3, 12 };
    Term q = pick_quotient(ten(DT_INT32, QA, 2, two_by_three), ten(DT_INT32, QB, 2, two_by_three));
    check_realized(uop_reduce(REDUCE_SUM, 1, q), DT_INT32, want, 2);
  }

  TEST_BEGIN("e2e/i32-sum-of-sqrt");
  {
    i64 const want[2] = { 4, 7 };
    Term s = uop_unary(UOP_SQRT, ten(DT_INT32, SA, 2, two_by_three));
    check_realized(uop_reduce(REDUCE_SUM, 1, s), DT_INT32, want, 2);
  }

  TEST_BEGIN("e2e/i32-quotient");
  {
    u32 const four[1] = { 4 };
    i32 const qa[4]   = { 7, -7, 9, 1 };
    i32 const qb[4]   = { 2, 2, 4, 4 };
    i64 const want[4] = { 3, -3, 2, 0 };
    check_realized(pick_quotient(ten(DT_INT32, qa, 1, four), ten(DT_INT32, qb, 1, four)),
                   DT_INT32, want, 4);
  }

  TEST_BEGIN("e2e/bool-cast-of-f32-compare");
  {
    u32 const six[1]  = { 6 };
    f32 const f[6]    = { 0.25f, 0.75f, -0.5f, 0.5f, 0.4f, -1.5f };
    i64 const want[6] = { 1, 0, 1, 0, 1, 1 };
    Term lt = uop_binary(UOP_CMPLT, ten(DT_FP32, f, 1, six), fc(0.5f));
    check_realized(uop_cast(lt, DT_BOOL), DT_BOOL, want, 6);
  }

  TEST_BEGIN("e2e/i32-sum-of-cast-of-scaled-f32");
  {
    f32 const f[6]    = { 1.5f, -1.5f, 0.3f, 3.9f, 2.0f, -0.2f };
    i64 const want[2] = { 0, 14 };
    Term scaled = uop_binary(UOP_MUL, ten(DT_FP32, f, 2, two_by_three), fc(2.5f));
    check_realized(uop_reduce(REDUCE_SUM, 1, uop_cast(scaled, DT_INT32)), DT_INT32, want, 2);
  }
}

int main(void) {
  thvm_init();

  check_compares();
  check_conditions();
  check_narrowing_cast();
  check_float_bodies();

  check_end_to_end();
  printf("  e2e: %u realized kernels, %u of them on the JIT, all re-run on the walker\n",
         E2E_KERNELS, E2E_JIT);

  TEST_BEGIN("e2e/no-kernel-ran-without-a-rung");
  CHECK_EQ(cpu_dispatch_no_rung_count(), 0);

  thvm_free();
  TEST_REPORT();
}
