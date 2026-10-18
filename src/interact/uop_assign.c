// interact/uop_assign.c - UOP_ASSIGN(dst, src), tinygrad's Ops.ASSIGN: once
// both children are TAG_TENs, write src's elements into dst's buffer in place
// (differing dtypes store src's values at dst's dtype, tensor_assign) and
// return dst, whose tid, buffer and view are unchanged.  An optimizer loop
// steps its weights this way, so kernels keep naming them by tid.
//
// interact_assign_with takes the resolved TENs: wnf/_.c leaves the heap cells
// unmutated so a recursive training loop re-fires the ASSIGN against each
// iteration's kernel outputs.  interact_assign (redex_fire) reads the cells.

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

  if (jit_is_capturing()) {
    jit_capture_record_assign(dst_tid, src_tid);
  }

  // A KV-cache append's dst is a shrunk row view: writing its numel at its
  // element offset leaves the other rows untouched.
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
  // Stuck until both sides are TENs: redex_fire re-drives it once the src
  // kernel chain has landed one.
  if (term_tag(dst) != TAG_TEN || term_tag(src) != TAG_TEN) return assign_term;
  // Fire the buffer write at most once per pass (see assign_fire_claim):
  // a multiply-reachable ASSIGN cell must not re-apply its update.
  if (!assign_fire_claim(loc)) return dst;
  return interact_assign_with(dst, src);
}
