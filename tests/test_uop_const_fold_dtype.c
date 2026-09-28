// test_uop_const_fold_dtype.c - CAST / BITCAST of a CONST keep the
// value the payload names.
//
// A float CONST payload holds the f32 bits of the value truncated to
// the CONST's dtype (tinygrad DType.const, dtype.py:79-84).  CAST(CONST)
// rebuilds the const at the destination dtype (symbolic.py:151-155)
// and BITCAST(CONST) reads the value's storage bits back at the
// destination width (fold_bitcast, symbolic.py:23-26; the cases mirror
// test/null/test_uop_symbolic.py test_bitcast_chain).  Constructor and
// rewrite checks only: nothing dispatches.

#include "../src/thvm.c"
#include "test.h"

static u32 bits_of(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return b;
}

// A CAST / BITCAST node built without the constructor's fold, the shape
// a graph rebuild hands to uop_graph_simplify.
static Term raw_convert(u32 op, Term src, u32 dst_dtype) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, dst_dtype));
  return term_new(0, TAG_UOP, op, loc);
}

int main(void) {
  thvm_init();

  Term f32_one  = uop_const(DT_FP32, bits_of(1.0f));
  Term bf16_one = uop_const(DT_BF16, bits_of(1.0f));
  Term f16_one  = uop_const(DT_FP16, bits_of(1.0f));
  // 1 + 2^-9: exact in f32 and f16, below half a bf16 ulp at 1.0.
  u32 near_one = bits_of(1.001953125f);

  TEST_BEGIN("const/narrow-float-payload-truncates-to-dtype");
  CHECK_EQ(uop_const(DT_BF16, near_one), bf16_one);
  CHECK(uop_const(DT_FP16, near_one) != f16_one);
  CHECK(uop_const(DT_FP32, near_one) != f32_one);
  CHECK_EQ(uop_const(DT_FP8E4M3, bits_of(1.03125f)),
           uop_const(DT_FP8E4M3, bits_of(1.0f)));

  TEST_BEGIN("cast-const/f32-to-bf16-keeps-value");
  CHECK_EQ(uop_cast(f32_one, DT_BF16), bf16_one);

  TEST_BEGIN("cast-const/f32-to-f64-keeps-value");
  CHECK_EQ(uop_cast(f32_one, DT_FP64), uop_const(DT_FP64, bits_of(1.0f)));

  TEST_BEGIN("cast-const/f16-to-bf16-keeps-value");
  CHECK_EQ(uop_cast(f16_one, DT_BF16), bf16_one);

  TEST_BEGIN("cast-const/round-trip-through-bf16-rounds");
  Term near = uop_const(DT_FP32, near_one);
  CHECK_EQ(uop_cast(uop_cast(near, DT_BF16), DT_FP32), f32_one);
  CHECK_EQ(uop_cast(uop_cast(near, DT_FP16), DT_FP32), near);

  TEST_BEGIN("bitcast-const/bf16-u16-round-trip");
  CHECK_EQ(uop_bitcast(bf16_one, DT_UINT16), uop_const(DT_UINT16, 16256));
  CHECK_EQ(uop_bitcast(uop_const(DT_UINT16, 16256), DT_BF16), bf16_one);

  TEST_BEGIN("bitcast-const/f16-u16-round-trip");
  CHECK_EQ(uop_bitcast(f16_one, DT_UINT16), uop_const(DT_UINT16, 15360));
  CHECK_EQ(uop_bitcast(uop_const(DT_UINT16, 15360), DT_FP16), f16_one);

  TEST_BEGIN("bitcast-const/f16-to-i16-sign-extends");
  Term f16_neg_one = uop_const(DT_FP16, bits_of(-1.0f));
  Term i16_bits    = uop_const(DT_INT16, (u32)(i32)-17408);
  CHECK_EQ(uop_bitcast(f16_neg_one, DT_INT16), i16_bits);
  CHECK_EQ(uop_bitcast(i16_bits, DT_FP16), f16_neg_one);
  CHECK_EQ(uop_bitcast(f16_neg_one, DT_UINT16), uop_const(DT_UINT16, 48128));

  TEST_BEGIN("bitcast-const/fp8-u8-round-trip");
  Term e4m3_one = uop_const(DT_FP8E4M3, bits_of(1.0f));
  Term e5m2_one = uop_const(DT_FP8E5M2, bits_of(1.0f));
  CHECK_EQ(uop_bitcast(e4m3_one, DT_UINT8), uop_const(DT_UINT8, 56));
  CHECK_EQ(uop_bitcast(uop_const(DT_UINT8, 56), DT_FP8E4M3), e4m3_one);
  CHECK_EQ(uop_bitcast(e5m2_one, DT_UINT8), uop_const(DT_UINT8, 60));
  CHECK_EQ(uop_bitcast(uop_const(DT_UINT8, 60), DT_FP8E5M2), e5m2_one);

  TEST_BEGIN("bitcast-const/f32-i32-unchanged");
  CHECK_EQ(uop_bitcast(f32_one, DT_INT32), uop_const(DT_INT32, 0x3F800000u));
  CHECK_EQ(uop_bitcast(uop_const(DT_INT32, 0x3F800000u), DT_FP32), f32_one);
  CHECK_EQ(uop_bitcast(uop_const(DT_FP32, bits_of(-2.0f)), DT_UINT32),
           uop_const(DT_UINT32, 0xC0000000u));

  TEST_BEGIN("bitcast-const/8-byte-folds-payload-verbatim");
  CHECK_EQ(uop_bitcast(uop_const(DT_FP64, bits_of(1.0f)), DT_INT64),
           uop_const(DT_INT64, bits_of(1.0f)));

  TEST_BEGIN("bitcast-const/int4-stays-a-node");
  Term i4    = uop_const(DT_INT4, (u32)(i32)-1);
  Term i4_bc = uop_bitcast(i4, DT_UINT4);
  CHECK_EQ(term_tag(i4_bc), TAG_UOP);
  CHECK_EQ(term_ext(i4_bc), UOP_BITCAST);
  CHECK_EQ(heap_read(term_val(i4_bc)), i4);

  TEST_BEGIN("graph-simplify/declined-bitcast-const-terminates");
  Term i4_out = uop_graph_simplify(raw_convert(UOP_BITCAST, i4, DT_UINT4));
  CHECK_EQ(term_ext(i4_out), UOP_BITCAST);
  CHECK_EQ(heap_read(term_val(i4_out)), i4);

  TEST_BEGIN("graph-simplify/cast-node-over-const-folds");
  CHECK_EQ(uop_graph_simplify(raw_convert(UOP_CAST, f32_one, DT_BF16)), bf16_one);
  CHECK_EQ(uop_graph_simplify(raw_convert(UOP_CAST, f32_one, DT_FP64)),
           uop_const(DT_FP64, bits_of(1.0f)));

  TEST_BEGIN("graph-simplify/bitcast-node-over-const-folds");
  CHECK_EQ(uop_graph_simplify(raw_convert(UOP_BITCAST, bf16_one, DT_UINT16)),
           uop_const(DT_UINT16, 16256));

  thvm_free();
  TEST_REPORT();
}
