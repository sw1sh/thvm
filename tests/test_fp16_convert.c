// test_fp16_convert.c - f16 conversions are exact.
//
// fp16_to_f32 / f32_to_fp16 use the compiler's _Float16 where it exists
// (clang on arm64) and a manual bit conversion elsewhere (gcc 11 on x86,
// the Linux pods).  Both must give the IEEE-754 result.  Decoding: a
// subnormal is mant * 2^-24, a normal (1024 + mant) * 2^(exp - 25), and a
// NaN keeps its sign and payload and comes out quiet, as hardware
// conversions do; all 65536 patterns are checked.  Encoding rounds to
// nearest even, through the subnormals (anything above half of 2^-24
// rounds up to it) and into inf at 65520, and a NaN keeps its sign and
// top payload bits and comes out quiet; the binades where those rules
// meet are checked for every mantissa, the rest on a strided sweep.  The
// references below compute the results independently; on x86 with F16C
// they are also held to the VCVTPH2PS / VCVTPS2PH instructions, and where
// _Float16 exists the manual conversions are held to the builtin.
// fp8e5m2 decodes through the f16 layout, so its subnormals ride on the
// same path.

#include "../src/thvm.c"
#include "test.h"

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#include <cpuid.h>
#include <immintrin.h>
#define HAVE_F16C_PROBE 1
__attribute__((target("f16c"))) static f32 f16c_decode(u16 h) {
  return _cvtsh_ss(h);
}
__attribute__((target("f16c"))) static u16 f16c_encode(f32 v) {
  return (u16)_cvtss_sh(v, 0);
}
static int cpu_has_f16c(void) {
  unsigned a, b, c, d;
  return __get_cpuid(1, &a, &b, &c, &d) && (c & bit_F16C) && (c & bit_OSXSAVE);
}
#else
#define HAVE_F16C_PROBE 0
#endif

static u32 bits_of(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return b;
}

static u32 f16_reference_bits(u16 h) {
  u32 sign = (u32)(h >> 15) & 1u;
  u32 exp  = (u32)(h >> 10) & 0x1Fu;
  u32 mant = (u32)h & 0x3FFu;
  if (exp == 0x1F) {
    return (sign << 31) | 0x7F800000u | (mant != 0 ? 0x00400000u | (mant << 13) : 0u);
  }
  f32 mag = (exp == 0) ? ldexpf((f32)mant, -24) : ldexpf((f32)(mant | 0x400u), (int)exp - 25);
  return bits_of(mag) | (sign << 31);
}

static f32 f32_of(u32 b) {
  f32 v;
  memcpy(&v, &b, sizeof v);
  return v;
}

static u16 f16_encode_reference(f32 v) {
  u32 x    = bits_of(v);
  u16 sign = (u16)((x >> 16) & 0x8000u);
  if (isnan(v)) {
    return (u16)(sign | 0x7E00u | ((x & 0x7FFFFFu) >> 13));
  }
  f64 a = fabs((f64)v);
  if (a >= 65520.0) {
    return (u16)(sign | 0x7C00u);
  }
  if (a < 0x1p-14) {
    return (u16)(sign | (u16)nearbyint(a * 0x1p24));
  }
  int e;
  f64 n = nearbyint(ldexp(frexp(a, &e), 11));
  if (n == 2048.0) {
    n = 1024.0;
    e++;
  }
  return (u16)(sign | ((u32)(e + 14) << 10) | ((u32)n - 1024u));
}

// Every positive mantissa of the binades where subnormal rounding, the
// normal range, inf and NaN meet, plus a strided sweep of all f32 (both
// signs).
static u32 const ENCODE_BINADES[] = { 0, 101, 102, 103, 111, 112, 113, 142, 143, 255 };

static u32 encode_mismatch(char const *what, u16 (*encode)(f32), u16 (*want)(f32),
                           u32 b, u32 bad) {
  u16 got = encode(f32_of(b));
  u16 ref = want(f32_of(b));
  if (got != ref && bad < 4) {
    fprintf(stderr, "  %s(0x%08x) = 0x%04x, want 0x%04x\n", what, b, got, ref);
  }
  return got != ref;
}

static u32 count_encode_mismatches(char const *what, u16 (*encode)(f32),
                                   u16 (*want)(f32)) {
  u32 bad = 0;
  u32 nb  = sizeof ENCODE_BINADES / sizeof ENCODE_BINADES[0];
  for (u32 i = 0; i < nb; i++) {
    for (u32 m = 0; m < (1u << 23); m++) {
      bad += encode_mismatch(what, encode, want, (ENCODE_BINADES[i] << 23) | m, bad);
    }
  }
  for (u64 b = 0; b <= 0xFFFFFFFFull; b += 4099) {
    bad += encode_mismatch(what, encode, want, (u32)b, bad);
  }
  return bad;
}

static u32 count_mismatches(char const *what, f32 (*decode)(u16)) {
  u32 bad = 0;
  for (u32 h = 0; h <= 0xFFFF; h++) {
    u32 got  = bits_of(decode((u16)h));
    u32 want = f16_reference_bits((u16)h);
    if (got != want) {
      if (bad < 4) {
        fprintf(stderr, "  %s(0x%04x) = 0x%08x, want 0x%08x\n", what, h, got, want);
      }
      bad++;
    }
  }
  return bad;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("fp16/decode-every-pattern-exactly");
  CHECK_EQ(count_mismatches("fp16_to_f32", fp16_to_f32), 0);

  TEST_BEGIN("fp16/decode-subnormals");
  CHECK(fp16_to_f32(0x0001) == 0x1p-24f);
  CHECK(fp16_to_f32(0x0200) == 0x1p-15f);
  CHECK(fp16_to_f32(0x03FF) == 1023.0f * 0x1p-24f);
  CHECK(fp16_to_f32(0x8001) == -0x1p-24f);

#if THVM_HAVE_FLOAT16_BUILTIN
  TEST_BEGIN("fp16/manual-decode-every-pattern-exactly");
  CHECK_EQ(count_mismatches("fp16_to_f32_manual", fp16_to_f32_manual), 0);

  TEST_BEGIN("fp16/manual-decode-matches-builtin");
  u32 differ = 0;
  for (u32 h = 0; h <= 0xFFFF; h++) {
    differ += bits_of(fp16_to_f32_manual((u16)h)) != bits_of(fp16_to_f32((u16)h));
  }
  CHECK_EQ(differ, 0);
#endif

#if HAVE_F16C_PROBE
  TEST_BEGIN("fp16/reference-matches-f16c-hardware");
  if (cpu_has_f16c()) {
    CHECK_EQ(count_mismatches("vcvtph2ps", f16c_decode), 0);
  }
#endif

  TEST_BEGIN("fp16/encode-rounds-exactly");
  CHECK_EQ(count_encode_mismatches("f32_to_fp16", f32_to_fp16, f16_encode_reference), 0);

  TEST_BEGIN("fp16/encode-subnormals");
  CHECK_EQ(f32_to_fp16(0x1.000002p-25f), 0x0001);
  CHECK_EQ(f32_to_fp16(0x1p-25f), 0x0000);
  CHECK_EQ(f32_to_fp16(-0x1.8p-25f), 0x8001);
  CHECK_EQ(f32_to_fp16(f32_of(0x38002001u)), 0x0201);   // 512.50006 * 2^-24, just past a tie
  CHECK_EQ(f32_to_fp16(0x1.ffcp-15f), 0x0400);

  TEST_BEGIN("fp16/encode-nan-keeps-top-payload");
  CHECK_EQ(f32_to_fp16(f32_of(0x7FC00000u)), 0x7E00);
  CHECK_EQ(f32_to_fp16(f32_of(0x7F802000u)), 0x7E01);
  CHECK_EQ(f32_to_fp16(f32_of(0xFFFFFFFFu)), 0xFFFF);

#if THVM_HAVE_FLOAT16_BUILTIN
  TEST_BEGIN("fp16/manual-encode-matches-builtin");
  CHECK_EQ(count_encode_mismatches("f32_to_fp16_manual", f32_to_fp16_manual, f32_to_fp16), 0);
#endif

#if HAVE_F16C_PROBE
  TEST_BEGIN("fp16/encode-reference-matches-f16c-hardware");
  if (cpu_has_f16c()) {
    CHECK_EQ(count_encode_mismatches("vcvtps2ph", f16c_encode, f16_encode_reference), 0);
  }
#endif

  TEST_BEGIN("fp8e5m2/decode-subnormals");
  CHECK(fp8e5m2_to_f32(0x01) == 0x1p-16f);
  CHECK(fp8e5m2_to_f32(0x02) == 0x1p-15f);
  CHECK(fp8e5m2_to_f32(0x03) == 3.0f * 0x1p-16f);
  CHECK(fp8e5m2_to_f32(0x83) == -3.0f * 0x1p-16f);

  TEST_BEGIN("fp8e5m2/decode-every-finite-pattern-exactly");
  u32 bad = 0;
  for (u32 x = 0; x <= 0xFF; x++) {
    if ((x & 0x7Cu) == 0x7Cu) {
      continue;
    }
    bad += bits_of(fp8e5m2_to_f32((u8)x)) != f16_reference_bits((u16)(x << 8));
  }
  CHECK_EQ(bad, 0);

  thvm_free();
  TEST_REPORT();
}
