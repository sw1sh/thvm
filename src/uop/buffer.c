// Heap layout: [NUM(scope), NUM(dtype), NUM(ndim), NUM(instance), NUM(d0),
// ...].  Equal buffers hash-cons to one Term and one kernel slot; instance
// (input slot + 1, 0 for the output) keeps same-shaped slots apart.

fn Term uop_buffer(u32 scope, u32 dtype, u32 ndim, const u32 *dims) {
  return uop_buffer_inst(scope, dtype, ndim, dims, 0);
}

fn Term uop_buffer_inst(u32 scope, u32 dtype, u32 ndim, const u32 *dims,
                        u32 instance) {
  u32 key_buf[4 + MAX_DIM];
  key_buf[0] = scope;
  key_buf[1] = dtype;
  key_buf[2] = ndim;
  key_buf[3] = instance;
  for (u32 i = 0; i < ndim; i++) key_buf[4 + i] = dims[i];
  u64 key = uop_mov_hash(UOP_BUFFER, 0, key_buf, 4 + ndim);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(4 + ndim);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, scope));
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, dtype));
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_INT32, ndim));
  heap_set(loc + 3, term_new(0, TAG_NUM, DT_INT32, instance));
  for (u32 i = 0; i < ndim; i++) {
    heap_set(loc + 4 + i, term_new(0, TAG_NUM, DT_INT32, dims[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_BUFFER, loc);
  uop_mov_insert(key, t);
  return t;
}

// The accessors return 0 for a Term that is not a UOP_BUFFER, so a caller can
// probe any Term.

fn u32 uop_buffer_scope(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_BUFFER) return 0;
  return term_val(heap_read(term_val(t) + 0));
}

fn u32 uop_buffer_dtype(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_BUFFER) return 0;
  return term_val(heap_read(term_val(t) + 1));
}

fn u32 uop_buffer_ndim(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_BUFFER) return 0;
  return term_val(heap_read(term_val(t) + 2));
}

fn u32 uop_buffer_dim(Term t, u32 d) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_BUFFER) return 0;
  u32 ndim = term_val(heap_read(term_val(t) + 2));
  if (d >= ndim) return 0;
  return term_val(heap_read(term_val(t) + 4 + d));
}

fn u32 uop_buffer_inst_get(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_BUFFER) return 0;
  return term_val(heap_read(term_val(t) + 3));
}
