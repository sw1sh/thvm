// Reduce over `axes` (any order, repeats count once) one axis at a time,
// innermost first so the outer indices stay valid; uop_reduce's chain-fuse
// merges the links into one REDUCE.  The frontends' multi-axis reduce.
fn Term uop_reduce_axes(u32 kind, u32 n, u32 const *axes, Term src) {
  u32 sorted[MAX_DIM];
  u32 m = 0;
  for (u32 i = 0; i < n && m < MAX_DIM; i++) {
    u32 j = m;
    while (j > 0 && sorted[j - 1] > axes[i]) {
      j--;
    }
    if (j > 0 && sorted[j - 1] == axes[i]) {
      continue;
    }
    memmove(&sorted[j + 1], &sorted[j], (m - j) * sizeof sorted[0]);
    sorted[j] = axes[i];
    m++;
  }
  while (m > 0) {
    src = uop_reduce(kind, sorted[--m], src);
  }
  return src;
}
