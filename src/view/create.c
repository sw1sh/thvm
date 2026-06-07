// view/create.c - build a contiguous row-major View from a Shape.
//
// strides[i] = product of dims[i+1..ndim-1].  offset = 0.
// contiguous = 1.

fn View view_create(Shape shape) {
  View v;
  v.shape      = shape;
  v.offset     = 0;
  v.contiguous = 1;
  v.numel = shape_numel(shape);
  i32 stride = 1;
  for (i32 i = (i32)shape.ndim - 1; i >= 0; i--) {
    v.strides[i] = stride;
    // A symbolic (kvar) inner dim contributes its UPPER BOUND to outer
    // strides: the buffer is the worst-case product (shape_numel), so a row
    // is `hi` wide and element [.., j] (j < bound) sits at j.  Using the raw
    // kvar-packed extent here would corrupt every outer stride.
    stride *= (i32)kvar_extent_static(shape.dims[i]);
  }
  for (u32 i = shape.ndim; i < MAX_DIM; i++) v.strides[i] = 0;
  return v;
}
