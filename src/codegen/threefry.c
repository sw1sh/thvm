// codegen/threefry.c - tinygrad's THREEFRY (uop/decompositions.py:308-320): threefry2x32, 20
// rounds over x's two 32-bit halves keyed by key's, back in one uint64.  One text is both
// compiled here, for the walker, and written into each C, Metal and (as a __device__ function)
// CUDA kernel's prologue (THVM_THREEFRY_SRC) after its thvm_u64 typedef (THVM_IPOW_U64): as a
// DAG of rounds, each reading both halves, it would render and walk as a tree, doubling per
// round.
#define THVM_THREEFRY_TEXT(...) __VA_ARGS__ static char const THVM_THREEFRY_SRC[] = #__VA_ARGS__;

THVM_THREEFRY_TEXT(
static inline thvm_u64 thvm_threefry(thvm_u64 x, thvm_u64 key) {
  unsigned k0 = (unsigned)key, k1 = (unsigned)(key >> 32);
  unsigned ks[3] = { k1, k0 ^ k1 ^ 0x1BD11BDAu, k0 };
  unsigned rot[8] = { 13, 15, 26, 6, 17, 29, 16, 24 };
  unsigned x0 = (unsigned)x + k0, x1 = (unsigned)(x >> 32) + k1;
  for (unsigned i = 0; i < 5; i++) {
    for (unsigned j = 0; j < 4; j++) {
      unsigned r = rot[(i % 2) * 4 + j];
      x0 += x1;
      x1 = x0 ^ ((x1 << r) | (x1 >> (32 - r)));
    }
    x0 += ks[i % 3];
    x1 += ks[(i + 1) % 3] + i + 1;
  }
  return ((thvm_u64)x1 << 32) | x0;
}
)
