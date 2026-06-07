// view/shape_numel.c - element count of a Shape (product of dims).

fn u32 shape_numel(Shape s) {
  u32 n = 1;
  // A symbolic (kvar) dim sizes at its upper bound (kvar_extent_static):
  // buffers + the dispatch shape are worst-case; the per-realize loop
  // count is the bound value (kvar_extent_runtime), read at execution.
  for (u32 i = 0; i < s.ndim; i++) n *= kvar_extent_static(s.dims[i]);
  return n;
}
