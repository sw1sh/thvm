// uop/gather.c - UOP_GATHER: one element of `src` per position of an index
// tensor, taken along axis `dim` at the position the index names.
//
// Heap layout: [src, idx, NUM(dim)]; arity 2 (src, idx).  The node has
// idx's shape and src's dtype:
//
//   out[i] = src[i with i_dim := idx[i]]   if 0 <= idx[i] < src.shape[dim]
//          = 0                            otherwise
//
// This is tinygrad's Tensor.gather (mixin/op.py:1023-1043).  tinygrad sums
// a one-hot mask, so an index outside [0, src.shape[dim]) selects nothing;
// rangeify lowers the node straight to the form pm_reduce_load_collapse
// leaves once that sum collapses (codegen/simplify.py:143-145): a load of
// src whose address holds the loaded index, gated by the range check.
//
// uop_gather returns 0 unless the operands have equal ndim, idx has an
// integer dtype, dim < ndim, idx.shape[d] <= src.shape[d] for every d !=
// dim (tinygrad's assert), and both shapes pass uop_indexed_shape_ok.  The
// frontends raise tinygrad's exception types on a 0.  uop_gather_node
// builds the node unchecked.  Both hash-cons, dim included in the key.

fn Term uop_gather_node(Term src, Term idx, u32 dim) {
  u32 args[5] = { (u32)src, (u32)(src >> 32),
                  (u32)idx, (u32)(idx >> 32), dim };
  u64 key = uop_mov_hash(UOP_GATHER, 0, args, 5);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) {
    return hit;
  }
  u64 loc = heap_alloc(3);
  heap_set(loc + 0, src);
  heap_set(loc + 1, idx);
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_INT32, dim));
  Term t = term_new(0, TAG_UOP, UOP_GATHER, loc);
  uop_mov_insert(key, t);
  return t;
}

fn Term uop_gather(Term src, Term idx, u32 dim) {
  Shape ss, is;
  u32   idt;
  if (!term_shape_in(src, 0, &ss) || !term_shape_in(idx, 0, &is)) {
    return 0;
  }
  if (ss.ndim != is.ndim || dim >= ss.ndim) {
    return 0;
  }
  if (!term_dtype_in(idx, 0, &idt) || !dtype_is_int(idt) || dtype_is_packed(idt)) {
    return 0;
  }
  if (!uop_indexed_shape_ok(&ss) || !uop_indexed_shape_ok(&is)) {
    return 0;
  }
  for (u32 d = 0; d < ss.ndim; d++) {
    if (d != dim && is.dims[d] > ss.dims[d]) {
      return 0;
    }
  }
  return uop_gather_node(src, idx, dim);
}

fn Term uop_gather_src(Term g) {
  return heap_read(term_val(g) + 0);
}

fn Term uop_gather_idx(Term g) {
  return heap_read(term_val(g) + 1);
}

fn u32 uop_gather_dim(Term g) {
  return (u32)term_val(heap_read(term_val(g) + 2));
}
