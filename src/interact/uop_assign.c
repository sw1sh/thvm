// interact/uop_assign.c - in-place buffer write.
//
// Wnf fires UOP_ASSIGN(dst, src) once both children resolve to TAG_TEN.
// Semantics: write src's elements into `dst.buf` (preserving dst's
// tid/buf_id/view) and return the dst Term.  Any dtype: matching dtypes
// copy the bytes, differing dtypes store src's values at dst's dtype
// (tensor_assign, src/tensor/assign.c).  Used by optimizer loops
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
// program, no fused dispatch, just the backend's buffer copy or a
// round-trip via its buf_read + buf_write pair.

fn Term interact_assign_with(Term dst, Term src) {
  if (term_tag(dst) != TAG_TEN || term_tag(src) != TAG_TEN) return dst;

  u32 dst_tid = (u32)term_val(dst);
  u32 src_tid = (u32)term_val(src);
  if (dst_tid == 0 || dst_tid >= TENS_NEXT) return dst;
  if (src_tid == 0 || src_tid >= TENS_NEXT) return dst;

  TenDesc *dd = &TENS[dst_tid];
  TenDesc *sd = &TENS[src_tid];

  if (dd->backend == NULL || sd->backend == NULL) return dst;

  if (sd->view.numel != dd->view.numel) return dst;

  // JIT capture: record the (dst, src) tid pair so a TJit closure
  // can replay the assign as part of its captured sequence.  This
  // happens BEFORE the write so a capture failure (table full)
  // doesn't make us double-execute -- jit_capture_record_assign
  // is itself a no-op when JIT_ACTIVE_SLOT == 0.
  if (jit_is_capturing()) {
    jit_capture_record_assign(dst_tid, src_tid);
  }

  // Offset-aware in-place write.  A plain weight ASSIGN has a contig
  // dst at offset 0 (the whole buffer).  A KV-cache append's dst is a
  // SHRUNK row view: dd->view.numel is the SHRUNK element count (e.g.
  // dim), dd->view.offset the start element (e.g. row*dim).  We size
  // from the shrunk numel and write at the element offset so the other
  // rows stay untouched.  The CONTIGUOUS single-row case ({1,dim} into
  // {nCtx,dim} row-major) is a pure offset copy -- the shrunk view's
  // strides match the cache's row-major layout, so no strided scatter
  // is needed.
  if (tensor_assign(dst_tid, src_tid, (u64)(u32)dd->view.offset) != 0) {
    return dst;
  }
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
