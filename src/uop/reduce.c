// uop/reduce.c - construct a UOP_REDUCE node.
//
// Heap layout: [src, NUM(kind), NUM(n_axes), NUM(axis_0), ...,
//                NUM(axis_{n-1})]
// Args live as TAG_NUM cells alongside the source (matches the variable-
// payload pattern used by RESHAPE/EXPAND/PERMUTE/BUFFERIZE).
//
// Mirrors tinygrad `Ops.REDUCE` with `src=(body, range_a, range_b, ...)`
// (tinygrad/uop/ops.py) -- a multi-axis REDUCE folds N axes in one shot
// instead of chaining N single-axis REDUCE nodes (which each materialise
// their own intermediate kernel).  See
// tinygrad/schedule/indexing.py:90 `convert_reduce_to_reduce_with_ranges`
// for the per-reduce range-set construction at rangeify time.

// Construct the canonical multi-axis REDUCE cell.  Internal builder;
// callers go through uop_reduce_multi which first applies chain-fuse.
static Term uop_reduce_cell(u32 kind, u32 n_axes, u32 const *axes, Term src) {
  if (n_axes > MAX_DIM) n_axes = MAX_DIM;
  u32 args[2 + MAX_DIM];
  args[0] = kind;
  args[1] = n_axes;
  for (u32 i = 0; i < n_axes; i++) args[2 + i] = axes[i];
  u64 key = uop_mov_hash(UOP_REDUCE, src, args, 2 + n_axes);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(3 + n_axes);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, kind));
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_INT32, n_axes));
  for (u32 i = 0; i < n_axes; i++) {
    heap_set(loc + 3 + i, term_new(0, TAG_NUM, DT_INT32, axes[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_REDUCE, loc);
  uop_mov_insert(key, t);
  return t;
}

fn Term uop_reduce_multi(u32 kind, u32 n_axes, u32 const *axes, Term src) {
  if (n_axes > MAX_DIM) n_axes = MAX_DIM;
  // Chain-fuse: collapse REDUCE(REDUCE(body, axes_inner), axes_outer) ->
  // REDUCE(body, axes_inner ++ axes_outer) when both reduces share the
  // same kind.  Mirrors tinygrad's REDUCE-of-REDUCE merge
  // (codegen/simplify.py:77-87 reduce_unparented + indexing.py:90-95
  // convert_reduce_to_reduce_with_ranges, both fold multi-axis
  // REDUCE.src=(body,)+tuple(ranges) directly).
  //
  // Activated by C7 after udg_decode_addr_coeffs (dag_scan.c
  // udg_addr_decode_leaf) handles the strided im2col fingerprint, so
  // conv-bwd MUL+REDUCE chains can still classify as a multi-K
  // contraction once collapsed into a single multi-axis REDUCE.  Without
  // C7.1 + C7.2 the fuse would hide the matmul signature from the BLAS
  // classifier (single-K dispatch only) and slow the bench ~4x.
  if (n_axes >= 1 && term_tag(src) == TAG_UOP && term_ext(src) == UOP_REDUCE) {
    u32 inner_kind = uop_reduce_kind(src);
    if (inner_kind == kind) {
      u32 inner_n = uop_reduce_n_axes(src);
      if (inner_n + n_axes <= MAX_DIM) {
        u32 merged[MAX_DIM];
        u32 m = 0;
        for (u32 i = 0; i < inner_n; i++) merged[m++] = uop_reduce_axis(src, i);
        for (u32 i = 0; i < n_axes;  i++) merged[m++] = axes[i];
        return uop_reduce_cell(kind, m, merged, uop_reduce_src(src));
      }
    }
  }
  return uop_reduce_cell(kind, n_axes, axes, src);
}

fn Term uop_reduce(u32 kind, u32 axis, Term src) {
  return uop_reduce_multi(kind, 1, &axis, src);
}

// Accessors -- centralised so the rest of the codebase reads through
// these instead of poking heap cells directly.
fn u32 uop_reduce_kind(Term red) {
  return (u32)term_val(heap_read(term_val(red) + 1));
}
fn u32 uop_reduce_n_axes(Term red) {
  return (u32)term_val(heap_read(term_val(red) + 2));
}
fn u32 uop_reduce_axis(Term red, u32 i) {
  return (u32)term_val(heap_read(term_val(red) + 3 + i));
}
fn Term uop_reduce_src(Term red) {
  return heap_read(term_val(red) + 0);
}
