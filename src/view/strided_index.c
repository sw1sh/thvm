// view/strided_index.c - map an output flat index to the underlying
// buffer index through a (possibly non-contiguous) View.
//
// For contiguous views: short-circuits to flat_idx + offset (no
// per-axis walk).  For strided views: decomposes flat_idx into
// per-axis coords (using the shape's row-major layout) then sums
// c[i] * strides[i] + offset.
//
// strides[i] may be:
//   0   -> broadcast (same buffer element repeated along axis i)
//   <0  -> mirror   (axis i flipped)
//   >0  -> normal stride into the underlying buffer.
//
// Caller is responsible for ensuring buffer_idx falls inside the
// underlying buffer; the helper does no bounds checking.

fn u32 view_strided_index(View const *v, u32 flat_idx) {
  if (v->contiguous) return flat_idx + (u32)v->offset;
  int64_t acc = v->offset;
  u32 rem = flat_idx;
  // Walk axes back-to-front so c[i] = (rem / product_after) % dim.
  for (i32 axis = (i32)v->shape.ndim - 1; axis >= 0; axis--) {
    // Resolve a symbolic (kvar) dim to its upper bound: the buffer layout is
    // hi-padded, so this back-to-front unravel must divide/mod by hi.  The raw
    // kvar-packed extent (~2^31) would zero `rem` on the inner axis and drop
    // every outer-axis coordinate to 0 (all reads collapse to element 0).
    u32 dim = kvar_extent_static(v->shape.dims[axis]);
    if (dim == 0) continue;
    u32 c = rem % dim;
    rem /= dim;
    acc += (int64_t)c * (int64_t)v->strides[axis];
  }
  return (u32)acc;
}

// Chain-aware index for a TenDesc's full ShapeTracker
// (`view` + `prior_views[0..nviews)`).  Walks outermost-to-innermost
// matching tinygrad's `views_to_indexed_uops`: each step takes the
// current flat index, unravels through the next inner view's shape
// to apply its strides+offset, producing the next inner flat index.
// prior_views[0] is the innermost; its output is the underlying
// buffer index.  When nviews == 0 this collapses to view_strided_index.
fn u32 tendesc_strided_index(TenDesc const *t, u32 flat_idx) {
  u32 idx = view_strided_index(&t->view, flat_idx);
  for (i32 i = (i32)t->nviews - 1; i >= 0; i--) {
    idx = view_strided_index(&t->prior_views[i], idx);
  }
  return idx;
}
