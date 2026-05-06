// uop/buffer.c - construct a UOP_BUFFER leaf.
//
// Heap layout: [NUM(scope), NUM(dtype), NUM(ndim), NUM(d0), ...,
//               NUM(d_{ndim-1})].
//
// Scope (UOP_SCOPE_GLOBAL/LOCAL/REG) carries the storage tier:
// GLOBAL = device memory (kernel argument), LOCAL = threadgroup-shared,
// REG = per-thread register fragment.
//
// Hash-cons via uop_mov_cache: (scope, dtype, ndim, dims) round-trip
// to one heap loc.  Repeated `uop_buffer(GLOBAL, DT_FP32, 1, &n)` calls
// for the same shape return the same Term so kernel boundaries dedup
// to single buffer slots.

fn Term uop_buffer(u32 scope, u32 dtype, u32 ndim, const u32 *dims) {
  return uop_buffer_inst(scope, dtype, ndim, dims, 0);
}

// Variant carrying an instance disambiguator.  Two buffers with the
// same (scope, dtype, ndim, dims) but different instance fields hash-
// cons to distinct Terms.  Used by kernel_lift_to_uop to keep input
// slots distinguishable from each other and from the output even when
// they share shape.  instance=0 is the default (shareable).
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

// Read accessors.  Return 0 / sentinel when `t` is not a UOP_BUFFER
// term so callers can defensively probe terms of unknown opcode.

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

fn u32 uop_buffer_instance(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_BUFFER) return 0;
  return term_val(heap_read(term_val(t) + 3));
}
