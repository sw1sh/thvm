// dtype/fp_convert.c -- bit-level conversions between IEEE-754 half /
// bfloat16 and the native f32 / f64 ALUs.  All routines round-to-
// nearest-even and handle the IEEE special values (zero, denormals,
// infinity, NaN) per the standard.
//
// Reference: tinygrad's float_to_bf16 (dtype.py:221-225) for the
// banker's-rounding bf16 path; tinygrad relies on Python `struct`
// pack/unpack for fp16, so we either use clang's `_Float16`
// builtin (when available -- arm64 and recent Intel + -mf16c) or a
// manual bit-pack fallback.
//
// On macOS arm64 (the primary dev platform) `_Float16` is always
// available; the fallback path keeps us portable to Linux x86 builds
// without the -mf16c flag.

#if defined(__SIZEOF_FLOAT16__) || defined(__ARM_FP16_FORMAT_IEEE) || defined(__FLT16_DIG__)
  #define THVM_HAVE_FLOAT16_BUILTIN 1
#else
  #define THVM_HAVE_FLOAT16_BUILTIN 0
#endif

// === fp16 (IEEE-754 half: 1 sign / 5 exp / 10 mantissa) =============

f32 fp16_to_f32(u16 h) {
#if THVM_HAVE_FLOAT16_BUILTIN
  _Float16 v;
  memcpy(&v, &h, sizeof(v));
  return (f32)v;
#else
  // Manual bit-pack fallback.  Decompose; renormalize denormals;
  // map inf / NaN exponents.
  u32 sign = (u32)(h >> 15) & 0x1u;
  u32 exp  = (u32)(h >> 10) & 0x1Fu;
  u32 mant = (u32)h & 0x3FFu;
  u32 f32_bits;
  if (exp == 0) {
    if (mant == 0) {
      f32_bits = sign << 31;
    } else {
      // Denormal: renormalize.
      u32 e = 1;
      while ((mant & 0x400u) == 0) { mant <<= 1; e++; }
      mant &= 0x3FFu;
      u32 f32_exp = 127 - 15 - e + 1;
      f32_bits = (sign << 31) | (f32_exp << 23) | (mant << 13);
    }
  } else if (exp == 0x1F) {
    // inf or NaN.
    f32_bits = (sign << 31) | (0xFFu << 23) | (mant << 13);
  } else {
    u32 f32_exp = exp - 15 + 127;
    f32_bits = (sign << 31) | (f32_exp << 23) | (mant << 13);
  }
  f32 r;
  memcpy(&r, &f32_bits, sizeof(r));
  return r;
#endif
}

u16 f32_to_fp16(f32 v) {
#if THVM_HAVE_FLOAT16_BUILTIN
  _Float16 h = (_Float16)v;
  u16 bits;
  memcpy(&bits, &h, sizeof(bits));
  return bits;
#else
  u32 f32_bits;
  memcpy(&f32_bits, &v, sizeof(f32_bits));
  u32 sign = (f32_bits >> 31) & 0x1u;
  u32 exp  = (f32_bits >> 23) & 0xFFu;
  u32 mant = f32_bits & 0x7FFFFFu;
  u16 h;
  if (exp == 0xFF) {
    // inf / NaN.
    h = (u16)((sign << 15) | (0x1Fu << 10) | (mant ? 0x200u : 0u));
  } else if (exp <= 127 - 15) {
    // Subnormal or zero in fp16.  Round-to-nearest-even on the
    // shifted mantissa.
    if (exp < 127 - 24) {
      h = (u16)(sign << 15);   // underflow to signed zero
    } else {
      u32 m = (mant | 0x800000u) >> ((127 - 14) - exp);
      // Rounding: add 0x1000 then drop low 13 bits, with bankers
      // tiebreak.
      u32 round = m & 0x1FFFu;
      u32 r = m >> 13;
      if (round > 0x1000u || (round == 0x1000u && (r & 1))) r += 1;
      h = (u16)((sign << 15) | r);
    }
  } else if (exp >= 127 + 16) {
    // Overflow to inf.
    h = (u16)((sign << 15) | (0x1Fu << 10));
  } else {
    u32 fp16_exp = exp - 127 + 15;
    u32 round = mant & 0x1FFFu;
    u32 m     = mant >> 13;
    if (round > 0x1000u || (round == 0x1000u && (m & 1))) {
      m += 1;
      if (m & 0x400u) { m = 0; fp16_exp += 1; }
    }
    if (fp16_exp >= 0x1Fu) {
      h = (u16)((sign << 15) | (0x1Fu << 10));
    } else {
      h = (u16)((sign << 15) | (fp16_exp << 10) | m);
    }
  }
  return h;
#endif
}

// === bfloat16 (1 sign / 8 exp / 7 mantissa: same exp range as f32) ==

f32 bf16_to_f32(u16 b) {
  // bf16 = high 16 bits of f32 layout.  Lower 16 bits are zero.
  u32 bits = (u32)b << 16;
  f32 r;
  memcpy(&r, &bits, sizeof(r));
  return r;
}

u16 f32_to_bf16(f32 v) {
  u32 bits;
  memcpy(&bits, &v, sizeof(bits));
  // NaN: preserve as a quiet NaN; flag via the canonical mantissa MSB.
  u32 exp  = (bits >> 23) & 0xFFu;
  u32 mant = bits & 0x7FFFFFu;
  if (exp == 0xFFu && mant != 0u) {
    // any NaN -> quiet bf16 NaN with the sign + qbit; matches
    // the common "preserve NaN-ness only" convention.
    u32 sign = bits & 0x80000000u;
    return (u16)(((sign | (0xFFu << 23) | (1u << 22)) >> 16));
  }
  // Round-to-nearest-even via banker's rounding on the truncated
  // 16 trailing mantissa bits (matches tinygrad/dtype.py:221-225).
  u32 rounded = (bits + 0x7FFFu + ((bits >> 16) & 1u)) & 0xFFFF0000u;
  return (u16)(rounded >> 16);
}
