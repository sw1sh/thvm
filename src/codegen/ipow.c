// codegen/ipow.c - tinygrad's xpow_int (codegen/decomp/transcendental.py:267-283)
// for an int POW: square and multiply over e's bits in the unsigned type of `bits`,
// so products wrap, and a negative e (`neg`) truncates to 0 but for a base of 1 or
// -1.  One text is both compiled here, for the walker, and written into each C,
// Metal and (as a __device__ function) CUDA kernel's prologue (THVM_IPOW_SRC)
// after its thvm_u64 typedef (THVM_IPOW_U64), so they compute the same ints: rendered as
// a DAG of squarings, as tinygrad's linearizer shares them, it would render and
// walk as a tree, each square doubling it.
#define THVM_IPOW_TEXT(...) __VA_ARGS__ static char const THVM_IPOW_SRC[] = #__VA_ARGS__;
typedef unsigned long long thvm_u64;
#define THVM_IPOW_U64(target) ((target) == CG_TARGET_METAL ? "typedef ulong thvm_u64;\n" \
                                                        : "typedef unsigned long long thvm_u64;\n")

THVM_IPOW_TEXT(
static inline thvm_u64 thvm_ipow(thvm_u64 b, thvm_u64 e, int neg, unsigned bits) {
  thvm_u64 m = bits >= 64 ? ~(thvm_u64)0 : ((thvm_u64)1 << bits) - 1, r = 1;
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
