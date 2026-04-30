// backend/cpu/op/reshape.c - rewrap a contiguous source as a new
// shape.
//
// For contiguous tensors RESHAPE is a no-op in memory: the row-
// major layout of N elements is the same regardless of how those
// N elements are viewed as a multi-dim shape.  We therefore just
// memcpy the input bytes into the freshly-allocated output buffer.
// (out_numel must equal in_numel; the materialize step computes
// the output shape from the heap NUM cells and the caller is
// responsible for keeping the products equal.)
//
// View-aware (non-contiguous) reshape lands when ShapeTracker
// arrives in step 14.

fn void cpu_op_reshape(void *out, void **srcs, u32 const *src_numels,
                       KProgOp const *p, u32 out_numel) {
  void *src     = srcs[0];
  u32   in_numel = src_numels[0];
  u32   esz      = dtype_itemsize(p->dtype);
  u32   n        = (in_numel < out_numel) ? in_numel : out_numel;
  memcpy(out, src, (size_t)n * esz);
}
