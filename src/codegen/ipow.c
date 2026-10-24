// codegen/ipow.c - tinygrad's xpow_int (codegen/decomp/transcendental.py:267-283)
// for an int POW: square and multiply over e's bits in the unsigned type of `bits`,
// so products wrap, and a negative e (`neg`) truncates to 0 but for a base of 1 or
// -1.  One text is both compiled here, for the walker, and written into each C
// kernel's prologue (THVM_IPOW_SRC), so the two compute the same ints: rendered as
// a DAG of squarings, as tinygrad's linearizer shares them, it would render and
// walk as a tree, each square doubling it.
#define THVM_IPOW_TEXT(...) __VA_ARGS__ static char const THVM_IPOW_SRC[] = #__VA_ARGS__;

THVM_IPOW_TEXT(
static inline unsigned long long thvm_ipow(unsigned long long b, unsigned long long e, int neg,
                                           unsigned bits) {
  unsigned long long m = bits >= 64 ? ~0ull : (1ull << bits) - 1, r = 1;
  b &= m;
  if (neg) {
    if (b == 1) return 1;
    return b == m ? ((e & 1) ? m : 1) : 0;
  }
  for (; e; e >>= 1) {
    if (e & 1) r = (r * b) & m;
    b = (b * b) & m;
  }
  return r;
}
)
