// uop/conv2d.c - construct a UOP_CONV2D node.
//
// Heap layout: [input, weights, bias].
//   input   : {C_in, H, W}
//   weights : {C_out, C_in, kh, kw}
//   bias    : {C_out}
// Stride 1, no padding, no dilation; kernel size recovered from
// weights.shape at materialize time so the construction stays
// shape-agnostic.

fn Term uop_conv2d(Term input, Term weights, Term bias) {
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, input);
  heap_set(loc + 1, weights);
  heap_set(loc + 2, bias);
  return term_new(0, TAG_UOP, UOP_CONV2D, loc);
}
