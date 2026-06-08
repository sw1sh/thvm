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

  if (dd->backend == NULL || sd->backend == NULL) return dst;

  u32 numel = dd->view.numel;
  if (sd->view.numel != numel) return dst;

  // Phase A: gate on the dtypes whose buf_read/buf_write paths exist;
  // dtype_storage_bytes aborts on unwired dtypes, so the gate keeps
  // us safe until later phases enable them.
  if (dd->dtype != DT_FP32 && dd->dtype != DT_INT32) return dst;
  u64 nbytes = dtype_storage_bytes(dd->dtype, numel);

  // Offset-aware in-place write.  A plain weight ASSIGN has a contig
  // dst at offset 0 (byte_off == 0 -> identical to the old whole-buffer
  // path).  A KV-cache append's dst is a SHRUNK row view: dd->view.numel
  // is the SHRUNK element count (e.g. dim), dd->view.offset the start
  // element (e.g. row*dim).  We size from the shrunk numel and write at
  // the element offset so the other rows stay untouched.  The CONTIGUOUS
  // single-row case ({1,dim} into {nCtx,dim} row-major) is a pure offset
  // memcpy -- the shrunk view's strides match the cache's row-major
  // layout, so no strided scatter is needed.
  u64 byte_off = dtype_storage_bytes(dd->dtype, (u64)(u32)dd->view.offset);

  // JIT capture: record the (dst, src) tid pair so a TJit closure
  // can replay the assign as part of its captured sequence.  This
  // happens BEFORE the memcpy so a capture failure (table full)
  // doesn't make us double-execute -- jit_capture_record_assign
  // is itself a no-op when JIT_ACTIVE_SLOT == 0.
  if (jit_is_capturing()) {
    jit_capture_record_assign(dst_tid, src_tid);
  }

  // Same-backend: a native buf_copy is the fast path.  Prefer the
  // offset-aware buf_copy_at when the backend wires it (writes the
  // shrunk span at byte_off, leaving the rest of the dst buffer alone);
  // fall back to whole-buffer buf_copy only at byte_off == 0.
  if (dd->backend == sd->backend && dd->backend->buf_copy_at != NULL
      && dd->backend->buf_copy_at(dd->buf_id, byte_off, sd->buf_id, nbytes) == 0) {
    kernel_assign_write_record(dd->backend, dd->buf_id);
    kernel_fire_gen_bump();
    ITRS++;
    multi_emit(RULE_UOP_ASSIGN, MULTI_TERM, (u64)dst, (u64)src, 0);
    return dst;
  }
  if (byte_off == 0 && dd->backend == sd->backend && dd->backend->buf_copy != NULL
      && dd->backend->buf_copy(dd->buf_id, sd->buf_id, nbytes) == 0) {
    kernel_assign_write_record(dd->backend, dd->buf_id);
    kernel_fire_gen_bump();
    ITRS++;
    multi_emit(RULE_UOP_ASSIGN, MULTI_TERM, (u64)dst, (u64)src, 0);
    return dst;
  }

  // Round-trip via a host buffer.  Cross-backend (a device transfer --
  // the fire-time half of a UOP_COPY whose src is compute on another
  // device) reads through the SOURCE backend and writes through the DEST
  // backend; same-backend without a native buf_copy uses one backend for
  // both.  This is the memcpy a COPY boundary lowers to, fired in
  // dependency order (after the src kernel) by wnf -- mirrors tinygrad
  // engine/realize.py exec_copy.
  void *tmp = malloc((size_t)nbytes);
  if (!tmp) return dst;
  sd->backend->buf_read (sd->buf_id, tmp, nbytes);
  if (dd->backend->buf_write_at != NULL) {
    dd->backend->buf_write_at(dd->buf_id, byte_off, tmp, nbytes);
  } else {
    dd->backend->buf_write(dd->buf_id, tmp, nbytes);  // whole-buffer (byte_off == 0)
  }
  free(tmp);

  kernel_assign_write_record(dd->backend, dd->buf_id);
  kernel_fire_gen_bump();
  ITRS++;
  multi_emit(RULE_UOP_ASSIGN, MULTI_TERM, (u64)dst, (u64)src, 0);
  return dst;
}

fn Term interact_assign(Term assign_term) {
  u64  loc = term_val(assign_term);
  Term dst = term_resolve(heap_read(loc + 0));
  Term src = term_resolve(heap_read(loc + 1));
  // Stuck if either side isn't a TEN yet (the redex.c caller bails on
  // result == assign_term and re-drives once the src kernel chain has
  // landed a TEN).
  if (term_tag(dst) != TAG_TEN || term_tag(src) != TAG_TEN) return assign_term;
  // Fire the buffer write at most once per pass (see assign_fire_claim):
  // a multiply-reachable ASSIGN cell must not re-apply its update.
  if (!assign_fire_claim(loc)) return dst;
  return interact_assign_with(dst, src);
}
