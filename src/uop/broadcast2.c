// Broadcast t (shape `shape`, n dims) to `out` (nd dims): RESHAPE in leading 1s
// on a rank mismatch, then EXPAND an extent-1 axis.
static Term uop_broadcast_to(Term t, u32 n, u32 const *padded, u32 nd, u32 const *out) {
  if (n != nd) {
    t = uop_reshape(t, nd, padded);
  }
  if (memcmp(padded, out, nd * sizeof padded[0]) != 0) {
    t = uop_expand(t, nd, out);
  }
  return t;
}

// The numpy broadcast tinygrad applies to an elementwise pair (mixin/movement.py
// _broadcast_to): right-align, each axis equal or 1.  Rewrites *a and *b to the
// common shape `out` and returns its rank; -1 when an axis is neither.
fn int uop_broadcast2(Term *a, u32 na, u32 const *sa, Term *b, u32 nb, u32 const *sb,
                      u32 *out) {
  u32 nd = na > nb ? na : nb;
  if (nd > MAX_DIM) {
    return -1;
  }
  u32 pa[MAX_DIM], pb[MAX_DIM];
  for (u32 i = 0; i < nd; i++) {
    pa[i] = i < nd - na ? 1 : sa[i - (nd - na)];
    pb[i] = i < nd - nb ? 1 : sb[i - (nd - nb)];
    if (pa[i] != pb[i] && pa[i] != 1 && pb[i] != 1) {
      return -1;
    }
    out[i] = pa[i] > pb[i] ? pa[i] : pb[i];
  }
  *a = uop_broadcast_to(*a, na, pa, nd, out);
  *b = uop_broadcast_to(*b, nb, pb, nd, out);
  return (int)nd;
}
