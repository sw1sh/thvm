// view/create.c - build a contiguous row-major View from a Shape.
//
// strides[i] = product of dims[i+1..ndim-1].  offset = 0.
// contiguous = 1.

fn View view_create(Shape shape) {
  View v;
  v.shape      = shape;
  v.offset     = 0;
  v.contiguous = 1;
  u32 numel = 1;
  for (u32 i = 0; i < shape.ndim; i++) numel *= shape.dims[i];
  v.numel = numel;
  i32 stride = 1;
  for (i32 i = (i32)shape.ndim - 1; i >= 0; i--) {
    v.strides[i] = stride;
    stride *= (i32)shape.dims[i];
  }
  for (u32 i = shape.ndim; i < MAX_DIM; i++) v.strides[i] = 0;
  return v;
}
