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
    u32 dim = v->shape.dims[axis];
    if (dim == 0) continue;
    u32 c = rem % dim;
    rem /= dim;
    acc += (int64_t)c * (int64_t)v->strides[axis];
  }
  return (u32)acc;
}
