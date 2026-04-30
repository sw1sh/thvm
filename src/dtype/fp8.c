// dtype/fp8.c -- FP8 (e4m3, e5m2) <-> f32 bit-level conversion.
//
// Direct port of tinygrad's float_to_fp8 / fp8_to_float
// (TinyHVM/tinygrad/tinygrad/dtype.py:228-287).  Both formats use 8
// total bits with sign:1, exponent + mantissa packed differently:
//   e4m3: 1 sign / 4 exponent / 3 mantissa  (EXP_BIAS = 7)
//   e5m2: 1 sign / 5 exponent / 2 mantissa  (EXP_BIAS = 15)
// fp8 has no native ALU on any backend; arithmetic always promotes
// to f32 (cpu_op_run_via_f32) and converts at the boundary.
//
// The conversion algorithm interpolates through f64 internally for
// the rounding: pack the f32 input into f64 bits, mask + shift to
// the fp8 signand, round-to-nearest-even with a banker's tiebreak,
// and pack out as one byte.  Decoding goes through f16 bit layout
// (the fp8 -> f16 mapping is exact for normal values).

typedef struct {
    u32 exp_bias;
    u32 significand_bits;       // 4 (e4m3) or 3 (e5m2)
    u32 mantissa_mask;          // 0x7 (e4m3) or 0x3 (e5m2)
    u64 mindenorm_o2;
    u64 overflow_threshold;
    u32 maxnorm;
    u64 minnorm;
    u32 inf_value;
} Fp8Config;

static Fp8Config const FP8_E4M3 = {
    .exp_bias = 7, .significand_bits = 4, .mantissa_mask = 0x7,
    .mindenorm_o2       = 0x3F50000000000000ULL,
    .overflow_threshold = 0x407D000000000000ULL,
    .maxnorm = 0x7E, .minnorm = 0x3F90000000000000ULL, .inf_value = 0x7F
};
static Fp8Config const FP8_E5M2 = {
    .exp_bias = 15, .significand_bits = 3, .mantissa_mask = 0x3,
    .mindenorm_o2       = 0x3EE0000000000000ULL,
    .overflow_threshold = 0x40EE000000000000ULL - 1,
    .maxnorm = 0x7B, .minnorm = 0x3F10000000000000ULL, .inf_value = 0x7E
};

static u8 f32_to_fp8_with(f32 x, Fp8Config const *cfg, u32 dt) {
  // Promote to f64 for the bit-level rounding (matches tinygrad's
  // path; doing the math in f32 would lose ULP at the round step).
  f64 xd = (f64)x;
  u64 xbits;
  memcpy(&xbits, &xd, sizeof(xbits));
  u64 absx = xbits & 0x7FFFFFFFFFFFFFFFULL;
  u64 fp8_dp_half_ulp = (u64)1 << (53 - cfg->significand_bits - 1);
  u32 sign = (u32)((xbits >> 63) & 1) << 7;
  u32 exp_in = (u32)((xbits >> 52) & 0x7FFu);
  // Signed exponent computation: cfg->exp_bias - 1023 + raw_exp.
  // Use signed arithmetic; the rebiased exponent can be negative.
  i32 exp = (i32)exp_in - 1023 + (i32)cfg->exp_bias;
  u32 mantissa = (u32)((xbits >> (53 - cfg->significand_bits)) & cfg->mantissa_mask);
  u32 res;

  if (absx <= cfg->mindenorm_o2) {
    res = 0;
  } else if (absx > 0x7FF0000000000000ULL) {
    // Source was NaN; pick the per-format quiet pattern.
    res = (dt == DT_FP8E4M3) ? 0x7Fu : (0x7Eu | mantissa);
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

u8 f32_to_fp8e4m3(f32 v) { return f32_to_fp8_with(v, &FP8_E4M3, DT_FP8E4M3); }
u8 f32_to_fp8e5m2(f32 v) { return f32_to_fp8_with(v, &FP8_E5M2, DT_FP8E5M2); }

f32 fp8e5m2_to_f32(u8 x) {
  // e5m2 maps directly onto f16's 5-exp / 10-mantissa layout: the
  // u8 expands into a u16 by appending 8 zero mantissa bits.  Then
  // the existing fp16 decoder gives us f32.  Saturated NaN follows
  // tinygrad's pattern.
  u16 ur = (u16)x << 8;
  if ((ur & 0x7FFFu) > 0x7C00u) ur = 0x7FFFu;
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
