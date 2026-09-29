// test_fp16_convert.c - f16 decoding is exact for all 65536 patterns.
//
// fp16_to_f32 uses the compiler's _Float16 where it exists (clang on
// arm64) and a manual bit decode elsewhere (gcc 11 on x86, the Linux
// pods).  Both must return the IEEE-754 value of every pattern: a
// subnormal is mant * 2^-24, a normal (1024 + mant) * 2^(exp - 25), and
// a NaN keeps its sign and payload and comes out quiet, as hardware
// conversions do.  The reference below computes that independently; on
// x86 with F16C it is also held to the VCVTPH2PS instruction, and where
// _Float16 exists the manual decode is held to the builtin.  fp8e5m2
// decodes through the f16 layout, so its subnormals ride on the same
// path.

#include "../src/thvm.c"
#include "test.h"

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#include <cpuid.h>
#include <immintrin.h>
#define HAVE_F16C_PROBE 1
__attribute__((target("f16c"))) static f32 f16c_decode(u16 h) {
  return _cvtsh_ss(h);
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
