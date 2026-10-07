// dtype/fp8.c -- FP8 (e4m3, e5m2) <-> f32 bit-level conversion.
//
// Port of tinygrad's float_to_fp8 / fp8_to_float
// (tinygrad/dtype.py:245-282 at 365f87a8a), which follow NVIDIA's
// cuda_fp8.hpp.  Both formats use 8 total bits with sign:1, exponent +
// mantissa packed differently:
//   e4m3: 1 sign / 4 exponent / 3 mantissa  (EXP_BIAS = 7)
//   e5m2: 1 sign / 5 exponent / 2 mantissa  (EXP_BIAS = 15)
// fp8 has no native ALU on any backend: values widen to f32 for
// arithmetic and narrow back at the storage boundary.
//
// Encoding settles non-finite inputs first, in one of two ways, as
// tinygrad does.  f32_to_fp8e4m3 / f32_to_fp8e5m2 are float_to_fp8
// (dtype.py:251-252), the host-side encoding of CONST payloads, CAST /
// BITCAST constant folds and WL tensor writes: e4m3 has no inf, so inf
// and NaN both encode to NaN, while e5m2 keeps inf.  The _sat variants
// are the kernel store, which also fills a realized CONST because
// tinygrad realizes a constant in a kernel.  tinygrad's CPU and Metal
// renderers have no fp8 (renderer/cstyle.py:307-308, 398-400), so their
// kernels narrow through tinygrad/codegen/decomp/dtype.py:
// f2f_clamp(sat=True) (:127-134) sends +/-inf to +/-maxnorm, and f2f
// (:120-121) keeps a NaN's sign and the top mantissa bits of its f32
// pattern.  Unlike f2f (:119), the _sat variants keep subnormals.
// Finite inputs encode the same way in both: interpolate through f64
// for the rounding (pack the f32 input into f64 bits, mask + shift to
// the fp8 significand, round-to-nearest-even with a banker's tiebreak,
// and pack out as one byte), and saturate finite overflow to maxnorm.
// Decoding goes through the f16 bit layout (the fp8 -> f16 mapping is
// exact for normal values).

typedef struct {
    u32 exp_bias;
    u32 significand_bits;       // 4 (e4m3) or 3 (e5m2)
    u32 mantissa_mask;          // 0x7 (e4m3) or 0x3 (e5m2)
    u64 mindenorm_o2;
    u64 overflow_threshold;
    u32 maxnorm;
    u64 minnorm;
    u32 inf_value;              // e4m3 has no inf: inf encodes to NaN
    u32 nan_value;
    u32 sat_nan;                // | top mantissa bits; e4m3's only NaN is 0x7F
} Fp8Config;

static Fp8Config const FP8_E4M3 = {
    .exp_bias = 7, .significand_bits = 4, .mantissa_mask = 0x7,
    .mindenorm_o2       = 0x3F50000000000000ULL,
    .overflow_threshold = 0x407D000000000000ULL,
    .maxnorm = 0x7E, .minnorm = 0x3F90000000000000ULL,
    .inf_value = 0x7F, .nan_value = 0x7F, .sat_nan = 0x7F
};
static Fp8Config const FP8_E5M2 = {
    .exp_bias = 15, .significand_bits = 3, .mantissa_mask = 0x3,
    .mindenorm_o2       = 0x3EE0000000000000ULL,
    .overflow_threshold = 0x40EE000000000000ULL - 1,
    .maxnorm = 0x7B, .minnorm = 0x3F10000000000000ULL,
    .inf_value = 0x7C, .nan_value = 0x7F, .sat_nan = 0x7C
};

static u8 f32_to_fp8_with(f32 x, Fp8Config const *cfg, int sat) {
  // Promote to f64 for the bit-level rounding (matches tinygrad's
  // path; doing the math in f32 would lose ULP at the round step).
  f64 xd = (f64)x;
  u64 xbits;
  memcpy(&xbits, &xd, sizeof(xbits));
  u64 absx    = xbits & 0x7FFFFFFFFFFFFFFFULL;
  u64 f64_inf = 0x7FF0000000000000ULL;
  u32 sign    = (u32)((xbits >> 63) & 1) << 7;
  if (absx == f64_inf) {
    return (u8)(sign | (sat ? cfg->maxnorm : cfg->inf_value));
  }
  if (absx > f64_inf) {
    if (!sat) {
      return (u8)(sign | cfg->nan_value);
    }
    // f2f takes these bits from the f32 pattern, so read x, not xbits:
    // the f64 promotion quiets a signaling NaN.  A signaling NaN whose
    // top two bits are clear stores as e5m2 inf, as in tinygrad.
    u32 x32;
    memcpy(&x32, &x, sizeof(x32));
    u32 top = (x32 >> (24 - cfg->significand_bits)) & cfg->mantissa_mask;
    return (u8)(sign | cfg->sat_nan | top);
  }
  u64 fp8_dp_half_ulp = (u64)1 << (53 - cfg->significand_bits - 1);
  u32 exp_in = (u32)((xbits >> 52) & 0x7FFu);
  // Signed exponent computation: cfg->exp_bias - 1023 + raw_exp.
  // Use signed arithmetic; the rebiased exponent can be negative.
  i32 exp = (i32)exp_in - 1023 + (i32)cfg->exp_bias;
  u32 mantissa = (u32)((xbits >> (53 - cfg->significand_bits)) & cfg->mantissa_mask);
  u32 res;

  if (absx <= cfg->mindenorm_o2) {
    res = 0;
  } else if (absx > cfg->overflow_threshold) {
    res = cfg->maxnorm;
  } else if (absx >= cfg->minnorm) {
    // Normal: shift exponent + mantissa, banker's round on the
    // dropped bits.
    res = ((u32)exp << (cfg->significand_bits - 1)) | mantissa;
    u64 round_bits = xbits & ((fp8_dp_half_ulp << 1) - 1);
    if (round_bits > fp8_dp_half_ulp
        || (round_bits == fp8_dp_half_ulp && (mantissa & 1))) {
      res += 1;
    }
  } else {
    // Subnormal: shift mantissa right by (1 - exp) bits.
    i32 shift = 1 - exp;
    u32 mant = mantissa | (1u << (cfg->significand_bits - 1));
    res = (u32)(mant >> shift);
    u64 mask     = ((fp8_dp_half_ulp << (shift + 1)) - 1);
    u64 round_bits = (xbits | ((u64)1 << (53 - 1))) & mask;
    u64 ulp = fp8_dp_half_ulp << shift;
    if (round_bits > ulp || (round_bits == ulp && (res & 1))) {
      res += 1;
    }
  }
  res |= sign;
  return (u8)res;
}

u8 f32_to_fp8e4m3(f32 v) {
  return f32_to_fp8_with(v, &FP8_E4M3, 0);
}

u8 f32_to_fp8e5m2(f32 v) {
  return f32_to_fp8_with(v, &FP8_E5M2, 0);
}

u8 f32_to_fp8e4m3_sat(f32 v) {
  return f32_to_fp8_with(v, &FP8_E4M3, 1);
}

u8 f32_to_fp8e5m2_sat(f32 v) {
  return f32_to_fp8_with(v, &FP8_E5M2, 1);
}

f32 fp8e5m2_to_f32(u8 x) {
  // e5m2 maps directly onto f16's 5-exp / 10-mantissa layout: the
  // u8 expands into a u16 by appending 8 zero mantissa bits.  Then
  // the existing fp16 decoder gives us f32.  NaN becomes the
  // canonical f16 NaN 0x7FFF as in cuda_fp8.hpp, which drops the sign
  // that fp8_to_float keeps (dtype.py:279).
  u16 ur = (u16)x << 8;
  if ((ur & 0x7FFFu) > 0x7C00u) {
    ur = 0x7FFFu;
  }
  return fp16_to_f32(ur);
}

f32 fp8e4m3_to_f32(u8 x) {
  // e4m3 needs explicit renormalization for subnormals; remap into
  // the f16 bit layout and let fp16_to_f32 finish.
  u16 ur = (u16)x << 8;
  u16 sign     = (u16)(ur & 0x8000u);
  u16 exponent = (u16)((((u32)ur & 0x7800u) >> 1) + 0x2000u);
  u16 mantissa = (u16)((ur & 0x0700u) >> 1);
  u32 absx = (u32)x & 0x7Fu;
  if (absx == 0x7Fu) {
    ur = 0x7FFFu;
  } else if (exponent == 0x2000u) {
    if (mantissa != 0u) {
      mantissa <<= 1;
      while ((mantissa & 0x0400u) == 0u) {
        mantissa <<= 1;
        exponent  -= 0x0400u;
      }
      mantissa &= 0x03FFu;
    } else {
      exponent = 0;
    }
    ur = (u16)(sign | exponent | mantissa);
  } else {
    ur = (u16)(sign | exponent | mantissa);
  }
  return fp16_to_f32(ur);
}
