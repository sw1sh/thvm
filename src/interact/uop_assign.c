// interact/uop_assign.c - in-place buffer write.
//
// Wnf fires UOP_ASSIGN(dst, src) once both children resolve to TAG_TEN.
// Semantics: copy `src.buf` bytes into `dst.buf` (preserving dst's
// tid/buf_id/view) and return the dst Term.  Used by optimizer loops
// to mutate weight tensors in-place; weights stay tid-stable so the
// surrounding kernels can keep referencing them by tid across loop
// iterations.
//
// Two entry points:
//   interact_assign_with(dst, src)  -- caller passes already-resolved
//                                      TAG_TEN terms.  Used by wnf/_.c
//                                      where the heap cells deliberately
//                                      stay un-mutated so a recursive
//                                      training loop can re-fire the
//                                      ASSIGN against fresh upstream
//                                      kernel outputs each iter.
//   interact_assign(assign_term)    -- redex_fire entry point: reads
//                                      the assign cells off the heap
//                                      then delegates to _with.
//
// Mirrors tinygrad's UOps.ASSIGN.  Not a kernel -- there's no
// program, no fused dispatch, just a buffer round-trip via the
// backend's buf_read + buf_write pair.

fn Term interact_assign_with(Term dst, Term src) {
  if (term_tag(dst) != TAG_TEN || term_tag(src) != TAG_TEN) return dst;

  u32 dst_tid = (u32)term_val(dst);
  u32 src_tid = (u32)term_val(src);
  if (dst_tid == 0 || dst_tid >= TENS_NEXT) return dst;
  if (src_tid == 0 || src_tid >= TENS_NEXT) return dst;

  TenDesc *dd = &TENS[dst_tid];
  TenDesc *sd = &TENS[src_tid];

  if (dd->backend != sd->backend) return dst;
  if (dd->backend == NULL) return dst;

  u32 numel = dd->view.numel;
  if (sd->view.numel != numel) return dst;

  // Phase A: gate on the dtypes whose buf_read/buf_write paths exist;
  // dtype_storage_bytes aborts on unwired dtypes, so the gate keeps
  // us safe until later phases enable them.
  if (dd->dtype != DT_FP32 && dd->dtype != DT_INT32) return dst;
  u64 nbytes = dtype_storage_bytes(dd->dtype, numel);

  // JIT capture: record the (dst, src) tid pair so a TJit closure
  // can replay the assign as part of its captured sequence.  This
  // happens BEFORE the memcpy so a capture failure (table full)
  // doesn't make us double-execute -- jit_capture_record_assign
  // is itself a no-op when JIT_ACTIVE_SLOT == 0.
  if (jit_is_capturing()) {
    jit_capture_record_assign(dst_tid, src_tid);
  }

  // Round-trip via host buffer.  The backend interface gives us
  // buf_read + buf_write but no direct buf_copy; the host hop is
  // straightforward and keeps this independent of backend internals.
  void *tmp = malloc((size_t)nbytes);
  if (!tmp) return dst;
  dd->backend->buf_read (sd->buf_id, tmp, nbytes);
  dd->backend->buf_write(dd->buf_id, tmp, nbytes);
  free(tmp);

  ITRS++;
  return dst;
}

fn Term interact_assign(Term assign_term) {
  u64  loc = term_val(assign_term);
  Term dst = term_resolve(heap_read(loc + 0));
  Term src = term_resolve(heap_read(loc + 1));
  Term r = interact_assign_with(dst, src);
  // Stuck if interact_assign_with returned dst directly without firing
  // (the !TEN guard at the top), or if the backend mismatch failed.
  // The redex.c caller bails on result == assign_term; we return the
  // unchanged ASSIGN term in that case.
  if (term_tag(dst) != TAG_TEN || term_tag(src) != TAG_TEN) return assign_term;
  return r;
}
