// test_fp8_nonfinite.c - fp8 encoding of inf and NaN follows tinygrad,
// which encodes host-side values and kernel stores differently.
//
// Host-side encoding is float_to_fp8 / fp8_to_float
// (tinygrad/dtype.py:245-282).  e4m3 has no inf, so every non-finite
// input encodes to NaN: 0x7f, or 0xff when the sign bit is set
// (dtype.py:251).  e5m2 keeps inf as 0x7c / 0xfc and encodes NaN as
// sign | 0x7f (dtype.py:252).  Finite inputs are unaffected, and finite
// overflow still saturates to maxnorm.  A float CONST payload truncates
// through the same encoding (DType.const, dtype.py:79-84 and 299), and
// the devectorized MAX accumulator starts at dt.min (identity_element,
// uop/ops.py:61), which is the finite -448 for e4m3 (dtype.py:69-77).
//
// A kernel store saturates instead, as tinygrad's emulated fp8 kernels
// do (tinygrad/codegen/decomp/dtype.py:115-134): +/-inf stores as
// +/-maxnorm, and NaN keeps its sign and the top mantissa bits of its
// f32 pattern.  The store checks realize kernels on the CPU walker: a
// CAST, a NEG of e5m2 inf, and an ADD of an inf CONST, which keeps inf
// as an e5m2 payload yet saturates when stored.  The walker's f64
// evaluation quiets a signaling NaN before the store, so the realized
// checks use quiet NaNs.  A sweep checks that the two encodings agree
// on finite inputs.  Every other expected value was computed with
// tinygrad at 365f87a8a: float_to_fp8 for the host encoder, DEV=CPU
// kernels for the _sat encoder and the stores.

#include "../src/thvm.c"
#include "test.h"

static u32 bits_of(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return b;
}

static f32 f32_of(u32 b) {
  f32 v;
  memcpy(&v, &b, sizeof v);
  return v;
}

static u32 const_payload(Term c) {
  return (u32)term_val(heap_read(term_val(c)));
}

static u32 const_dtype(Term c) {
  return (u32)term_ext(heap_read(term_val(c)));
}

// The accumulator's init STORE is the only STORE in the reduce_to_acc
// chain whose value is a CONST.
static Term find_init_const(Term t, u32 depth) {
  if (depth > 64 || term_tag(t) != TAG_UOP) {
    return 0;
  }
  u64 loc = term_val(t);
  switch (term_ext(t)) {
    case UOP_STORE: {
      Term v = heap_read(loc + 2);
      if (term_tag(v) == TAG_UOP && term_ext(v) == UOP_CONST) {
        return v;
      }
      return 0;
    }
    case UOP_AFTER: {
      Term a = find_init_const(heap_read(loc + 0), depth + 1);
      if (a != 0) {
        return a;
      }
      return find_init_const(heap_read(loc + 1), depth + 1);
    }
    default:
      return 0;
  }
}

static Term max_identity_for(u32 dtype, u32 instance) {
  u32  dims[1] = { 4 };
  u32  axes[1] = { 2 };
  Term buf     = uop_buffer_inst(UOP_SCOPE_GLOBAL, dtype, 1, dims, instance);
  Term body    = uop_load(uop_index_e(buf, uop_range(2, KAX_LOOP, 4)));
  Term red     = uop_reduce_multi(REDUCE_MAX, 1, axes, body);
  return find_init_const(uop_devectorize_graph(red), 0);
}

static Term cpu_buffer(u32 dtype, void const *data, u32 n) {
  Shape s   = {0};
  s.ndim    = 1;
  s.dims[0] = n;
  u32 tid = tensor_alloc(&CPU_BACKEND, s, dtype);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, data, dtype_storage_bytes(dtype, n));
  return term_new(0, TAG_TEN, dtype, tid);
}

static void check_realized_bytes(Term expr, u32 dtype, u8 const *want, u32 n) {
  Term done = term_resolve(thvm_realize(expr));
  CHECK(term_tag(done) == TAG_TEN);
  if (term_tag(done) != TAG_TEN) {
    return;
  }
  u32 tid = (u32)term_val(done);
  CHECK_EQ(TENS[tid].dtype, dtype);
  if (TENS[tid].dtype != dtype) {
    return;
  }
  u8 got[8] = {0};
  TENS[tid].backend->buf_read(TENS[tid].buf_id, got, n);
  for (u32 i = 0; i < n; i++) {
    CHECK_EQ(got[i], want[i]);
  }
}

// The CPU JIT declines fp8 buffers (cpu_jit_narrow_float), so each of
// these kernels runs in the walker and stores through its fp8 encoder.
static void check_kernel_stores(void) {
  f32 const cast_src[6] = { INFINITY, -INFINITY, f32_of(0x7FC00000u), 1e6f, -1e6f, 1.0f };
  Term      cast_buf    = cpu_buffer(DT_FP32, cast_src, 6);

  TEST_BEGIN("store/cast-to-e4m3-saturates-inf");
  u8 const cast_e4m3[6] = { 0x7E, 0xFE, 0x7F, 0x7E, 0xFE, 0x38 };
  check_realized_bytes(uop_cast(cast_buf, DT_FP8E4M3), DT_FP8E4M3, cast_e4m3, 6);

  TEST_BEGIN("store/cast-to-e5m2-saturates-inf");
  u8 const cast_e5m2[6] = { 0x7B, 0xFB, 0x7E, 0x7B, 0xFB, 0x3C };
  check_realized_bytes(uop_cast(cast_buf, DT_FP8E5M2), DT_FP8E5M2, cast_e5m2, 6);

  TEST_BEGIN("store/neg-of-e5m2-inf-saturates");
  u8 const neg_src[4]  = { 0x7C, 0xFC, 0x3C, 0x7F };
  u8 const neg_want[4] = { 0xFB, 0x7B, 0xBC, 0xFF };
  check_realized_bytes(uop_unary(UOP_NEG, cpu_buffer(DT_FP8E5M2, neg_src, 4)),
                       DT_FP8E5M2, neg_want, 4);

  TEST_BEGIN("store/add-inf-const-saturates-e5m2");
  u32 const one[1]      = { 1 };
  u32 const four[1]     = { 4 };
  u8  const add_src[4]  = { 0x00, 0x3C, 0xC0, 0x38 };
  u8  const add_e5m2[4] = { 0x7B, 0x7B, 0x7B, 0x7B };
  Term inf_e5m2 = uop_expand(uop_reshape(uop_const(DT_FP8E5M2, bits_of(INFINITY)), 1, one), 1, four);
  check_realized_bytes(uop_binary(UOP_ADD, cpu_buffer(DT_FP8E5M2, add_src, 4), inf_e5m2),
                       DT_FP8E5M2, add_e5m2, 4);

  TEST_BEGIN("store/add-inf-const-is-nan-e4m3");
  u8 const add_e4m3[4] = { 0x7F, 0x7F, 0x7F, 0x7F };
  Term inf_e4m3 = uop_expand(uop_reshape(uop_const(DT_FP8E4M3, bits_of(INFINITY)), 1, one), 1, four);
  check_realized_bytes(uop_binary(UOP_ADD, cpu_buffer(DT_FP8E4M3, add_src, 4), inf_e4m3),
                       DT_FP8E4M3, add_e4m3, 4);
}

int main(void) {
  thvm_init();

  f32 const pinf = INFINITY;
  f32 const ninf = -INFINITY;
  u32 const nans[]     = { 0x7FC00000u, 0x7F800001u, 0x7FA00000u, 0x7FFFFFFFu };
  u32 const neg_nans[] = { 0xFFC00000u, 0xFF800001u, 0xFFA00000u, 0xFFFFFFFFu };

  TEST_BEGIN("encode/e4m3-non-finite-is-nan");
  CHECK_EQ(f32_to_fp8e4m3(pinf), 0x7F);
  CHECK_EQ(f32_to_fp8e4m3(ninf), 0xFF);
  for (u32 i = 0; i < 4; i++) {
    CHECK_EQ(f32_to_fp8e4m3(f32_of(nans[i])), 0x7F);
    CHECK_EQ(f32_to_fp8e4m3(f32_of(neg_nans[i])), 0xFF);
  }

  TEST_BEGIN("encode/e5m2-inf-is-inf");
  CHECK_EQ(f32_to_fp8e5m2(pinf), 0x7C);
  CHECK_EQ(f32_to_fp8e5m2(ninf), 0xFC);

  TEST_BEGIN("encode/e5m2-nan-is-sign-or-7f");
  for (u32 i = 0; i < 4; i++) {
    CHECK_EQ(f32_to_fp8e5m2(f32_of(nans[i])), 0x7F);
    CHECK_EQ(f32_to_fp8e5m2(f32_of(neg_nans[i])), 0xFF);
  }

  TEST_BEGIN("encode-sat/inf-saturates-to-maxnorm");
  CHECK_EQ(f32_to_fp8e4m3_sat(pinf), 0x7E);
  CHECK_EQ(f32_to_fp8e4m3_sat(ninf), 0xFE);
  CHECK_EQ(f32_to_fp8e5m2_sat(pinf), 0x7B);
  CHECK_EQ(f32_to_fp8e5m2_sat(ninf), 0xFB);

  TEST_BEGIN("encode-sat/nan-keeps-sign-and-top-mantissa");
  u8 const sat_e4m3[8] = { 0x7F, 0x7F, 0x7F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF };
  u8 const sat_e5m2[8] = { 0x7E, 0x7C, 0x7D, 0x7F, 0xFE, 0xFC, 0xFD, 0xFF };
  for (u32 i = 0; i < 4; i++) {
    CHECK_EQ(f32_to_fp8e4m3_sat(f32_of(nans[i])), sat_e4m3[i]);
    CHECK_EQ(f32_to_fp8e4m3_sat(f32_of(neg_nans[i])), sat_e4m3[i + 4]);
    CHECK_EQ(f32_to_fp8e5m2_sat(f32_of(nans[i])), sat_e5m2[i]);
    CHECK_EQ(f32_to_fp8e5m2_sat(f32_of(neg_nans[i])), sat_e5m2[i + 4]);
  }

  TEST_BEGIN("encode-sat/finite-matches-host");
  CHECK_EQ(f32_to_fp8e4m3_sat(464.0f), 0x7E);
  CHECK_EQ(f32_to_fp8e4m3_sat(f32_of(0xFF7FFFFFu)), 0xFE);
  CHECK_EQ(f32_to_fp8e5m2_sat(61440.0f), 0x7B);
  CHECK_EQ(f32_to_fp8e5m2_sat(f32_of(0x7F7FFFFFu)), 0x7B);
  u32 finite_mismatches = 0;
  for (u64 b = 0; b <= 0xFFFFFFFFull; b += 65521) {
    f32 v = f32_of((u32)b);
    if (!isfinite(v)) {
      continue;
    }
    if (f32_to_fp8e4m3_sat(v) != f32_to_fp8e4m3(v)
        || f32_to_fp8e5m2_sat(v) != f32_to_fp8e5m2(v)) {
      finite_mismatches++;
    }
  }
  CHECK_EQ(finite_mismatches, 0);

  TEST_BEGIN("decode/e4m3-7f-ff-are-nan");
  CHECK(isnan(fp8e4m3_to_f32(0x7F)));
  CHECK(isnan(fp8e4m3_to_f32(0xFF)));
  CHECK(fp8e4m3_to_f32(0x7E) == 448.0f);
  CHECK(fp8e4m3_to_f32(0xFE) == -448.0f);

  TEST_BEGIN("decode/e5m2-7c-fc-are-inf-rest-nan");
  CHECK(fp8e5m2_to_f32(0x7C) == pinf);
  CHECK(fp8e5m2_to_f32(0xFC) == ninf);
  u8 const e5m2_nans[] = { 0x7D, 0x7E, 0x7F, 0xFD, 0xFE, 0xFF };
  for (u32 i = 0; i < 6; i++) {
    CHECK(isnan(fp8e5m2_to_f32(e5m2_nans[i])));
  }

  TEST_BEGIN("encode/finite-unchanged-e4m3");
  CHECK_EQ(f32_to_fp8e4m3(1.0f), 0x38);
  CHECK_EQ(f32_to_fp8e4m3(-2.5f), 0xC2);
  CHECK_EQ(f32_to_fp8e4m3(448.0f), 0x7E);
  CHECK_EQ(f32_to_fp8e4m3(500.0f), 0x7E);
  CHECK_EQ(f32_to_fp8e4m3(1e6f), 0x7E);
  CHECK_EQ(f32_to_fp8e4m3(-1e6f), 0xFE);
  CHECK_EQ(f32_to_fp8e4m3(0x1p-9f), 0x01);
  CHECK_EQ(f32_to_fp8e4m3(0x1p-10f), 0x00);
  CHECK(fp8e4m3_to_f32(f32_to_fp8e4m3(-2.5f)) == -2.5f);

  TEST_BEGIN("encode/finite-unchanged-e5m2");
  CHECK_EQ(f32_to_fp8e5m2(1.0f), 0x3C);
  CHECK_EQ(f32_to_fp8e5m2(-2.5f), 0xC1);
  CHECK_EQ(f32_to_fp8e5m2(57344.0f), 0x7B);
  CHECK_EQ(f32_to_fp8e5m2(70000.0f), 0x7B);
  CHECK_EQ(f32_to_fp8e5m2(-1e30f), 0xFB);
  CHECK_EQ(f32_to_fp8e5m2(0x1p-16f), 0x01);
  CHECK_EQ(f32_to_fp8e5m2(0x1p-17f), 0x00);
  CHECK(fp8e5m2_to_f32(f32_to_fp8e5m2(-2.5f)) == -2.5f);

  TEST_BEGIN("const/e5m2-inf-keeps-inf");
  CHECK_EQ(const_payload(uop_const(DT_FP8E5M2, bits_of(ninf))), 0xFF800000u);
  CHECK_EQ(const_payload(uop_const(DT_FP8E5M2, bits_of(pinf))), 0x7F800000u);

  TEST_BEGIN("const/e4m3-inf-truncates-to-nan");
  CHECK(isnan(f32_of(const_payload(uop_const(DT_FP8E4M3, bits_of(pinf))))));
  CHECK(isnan(f32_of(const_payload(uop_const(DT_FP8E4M3, bits_of(ninf))))));

  TEST_BEGIN("devectorize/max-identity-is-dtype-min");
  Term id_e4m3 = max_identity_for(DT_FP8E4M3, 1);
  Term id_e5m2 = max_identity_for(DT_FP8E5M2, 2);
  Term id_f32  = max_identity_for(DT_FP32, 3);
  CHECK(id_e4m3 != 0 && id_e5m2 != 0 && id_f32 != 0);
  CHECK_EQ(const_dtype(id_e4m3), DT_FP8E4M3);
  CHECK_EQ(const_payload(id_e4m3), bits_of(-448.0f));
  CHECK_EQ(const_dtype(id_e5m2), DT_FP8E5M2);
  CHECK_EQ(const_payload(id_e5m2), bits_of(ninf));
  CHECK_EQ(const_payload(id_f32), bits_of(ninf));

  TEST_BEGIN("store/runs-on-cpu");
  CHECK(CURRENT_BACKEND == &CPU_BACKEND);
  if (CURRENT_BACKEND == &CPU_BACKEND) {
    check_kernel_stores();
  }

  thvm_free();
  TEST_REPORT();
}
