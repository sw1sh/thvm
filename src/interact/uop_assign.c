// interact/uop_assign.c - in-place buffer write.
//
// Wnf fires UOP_ASSIGN(dst, src) once both children resolve to TAG_TEN.
// Semantics: copy `src.buf` bytes into `dst.buf` (preserving dst's
// tid/buf_id/view) and return the dst Term.  Used by optimizer loops
// to mutate weight tensors in-place; weights stay tid-stable so the
// surrounding kernels can keep referencing them by tid across loop
// iterations.
//
// Mirrors tinygrad's UOps.ASSIGN.  Not a kernel -- there's no
// program, no fused dispatch, just a buffer round-trip via the
// backend's buf_read + buf_write pair.  The two-memcpy cost is OK
// for the optimizer-update use case (weights are tiny relative to
// activations); a future buf_copy backend op could collapse it.

fn Term interact_assign(Term assign_term) {
  u64  loc = term_val(assign_term);
  Term dst = term_resolve(heap_read(loc + 0));
  Term src = term_resolve(heap_read(loc + 1));

  // Both children must be TAG_TEN by fire time.  is_redex enforces
  // this; defensive re-check here in case the redex set lost sync.
  if (term_tag(dst) != TAG_TEN || term_tag(src) != TAG_TEN) {
    return assign_term;
  }

  u32 dst_tid = (u32)term_val(dst);
  u32 src_tid = (u32)term_val(src);
  if (dst_tid == 0 || dst_tid >= TENS_NEXT) return assign_term;
  if (src_tid == 0 || src_tid >= TENS_NEXT) return assign_term;

  TenDesc *dd = &TENS[dst_tid];
  TenDesc *sd = &TENS[src_tid];

  // Same backend -- can't cross CPU/Metal in one ASSIGN.  A future
  // arc could insert an implicit transfer kernel; for now bail so the
  // user sees an unreduced ASSIGN they can debug.
  if (dd->backend != sd->backend) return assign_term;
  if (dd->backend == NULL) return assign_term;

  // Numel match (the dtype-encoded element count, NOT view shape).
  // ASSIGN's contract: dst's view-defined region is overwritten with
  // src's view-defined region, element-for-element.
  u32 numel = dd->view.numel;
  if (sd->view.numel != numel) return assign_term;

  // Element size from dtype.  Matches tensor_alloc's per-element
  // sizing (DT_F32 / DT_I32 are both 4 bytes today; spelt out so a
  // future DT_F16 / DT_I64 doesn't silently miscompute).
  u32 elem_bytes;
  switch (dd->dtype) {
    case DT_F32: case DT_I32: elem_bytes = 4; break;
    default: return assign_term;
  }
  u64 nbytes = (u64)numel * (u64)elem_bytes;

  // Round-trip via host buffer.  The backend interface gives us
  // buf_read + buf_write but no direct buf_copy; the host hop is
  // straightforward and keeps this independent of backend internals.
  void *tmp = malloc((size_t)nbytes);
  if (!tmp) return assign_term;
  dd->backend->buf_read (sd->buf_id, tmp, nbytes);
  dd->backend->buf_write(dd->buf_id, tmp, nbytes);
  free(tmp);

  ITRS++;
  return dst;
}
