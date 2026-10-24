// codegen/xexp2f.c - tinygrad's xexp2 (codegen/decomp/transcendental.py:193-217)
// for a float32 exp2, which tinygrad decomposes where the device has no exp2
// instruction, as the CPU has none: a degree-6 polynomial on the fraction of d,
// scaled by 2^q through the exponent bits, with tinygrad's coefficients and
// edge cases.  One text is both compiled here, for the walker, and written into
// each C kernel's prologue (THVM_XEXP2F_SRC), so the two compute the same
// floats; inlined, clang vectorizes it where libm's exp2 was a call per element.
// Each product is its own statement: clang's default -ffp-contract=on fuses a
// multiply-add only inside one expression, and the kernels compile with it off.
#define THVM_XEXP2F_TEXT(...) __VA_ARGS__ static char const THVM_XEXP2F_SRC[] = #__VA_ARGS__;

THVM_XEXP2F_TEXT(
static inline float thvm_xexp2f(float d) {
  float x = (d == d && d != INFINITY && d != -INFINITY) ? d : 0.0f;
  int   q = (int)(x + (x < 0.0f ? -0.5f : 0.5f));
  float s = x - (float)q;
  float u = 0.1535920892e-3f, t;
  t = u * s; u = t + 0.1339262701e-2f;
  t = u * s; u = t + 0.9618384764e-2f;
  t = u * s; u = t + 0.5550347269e-1f;
  t = u * s; u = t + 0.2402264476e+0f;
  t = u * s; u = t + 0.6931471825e+0f;
  t = u * s; u = t + 1.0f;
  int h = q >> 1;
  unsigned b0 = (unsigned)(h + 127) << 23, b1 = (unsigned)(q - h + 127) << 23;
  float p0, p1;
  memcpy(&p0, &b0, sizeof p0);
  memcpy(&p1, &b1, sizeof p1);
  u = u * p0 * p1;
  u = d < 128.0f ? u : INFINITY;
  u = d < -150.0f ? 0.0f : u;
  return d == d ? u : NAN;
}
)
