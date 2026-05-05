// uop/buffer.c - construct a UOP_BUFFER leaf (Phase D'1).
//
// Heap layout: [NUM(scope), NUM(dtype), NUM(ndim), NUM(d0), ...,
//               NUM(d_{ndim-1})].
//
// Scope (UOP_SCOPE_GLOBAL/LOCAL/REG) carries the storage tier per the
// TileLang correspondence in the migration plan: GLOBAL = T.Tensor
// argument (device memory), LOCAL = T.alloc_shared (threadgroup-shared),
// REG = T.alloc_fragment (per-thread register fragment).  D'2 wires
// UOP_STORE/UOP_AFTER to read these.  F0 wires the renderer.
//
// Hash-cons via uop_mov_cache: (scope, dtype, ndim, dims) round-trip
// to one heap loc.  Repeated `uop_buffer(GLOBAL, DT_FP32, 1, &n)` calls
// for the same shape return the same Term so kernel boundaries dedup
// to single buffer slots.

fn Term uop_buffer(u32 scope, u32 dtype, u32 ndim, const u32 *dims) {
  // Hash-cons key: pack (scope, dtype, ndim, dims) into a flat u32 array.
  u32 key_buf[3 + MAX_DIM];
  key_buf[0] = scope;
  key_buf[1] = dtype;
  key_buf[2] = ndim;
  for (u32 i = 0; i < ndim; i++) key_buf[3 + i] = dims[i];
  u64 key = uop_mov_hash(UOP_BUFFER, 0, key_buf, 3 + ndim);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(3 + ndim);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, scope));
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, dtype));
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_INT32, ndim));
  for (u32 i = 0; i < ndim; i++) {
    heap_set(loc + 3 + i, term_new(0, TAG_NUM, DT_INT32, dims[i]));
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
  return term_val(heap_read(term_val(t) + 3 + d));
}
