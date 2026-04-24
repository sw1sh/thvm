// tensor/view_of.c - alias an existing tensor's buffer with a new view.
//
// The new TenDesc shares src's buf_id; we bump the backend's buf_incref
// so the storage stays alive as long as either descriptor refers to it.
// Used by reshape / permute / expand / pad / shrink / flip in step 14.

fn u32 tensor_view_of(u32 src_id, View new_view) {
  if (src_id == 0 || src_id >= TENS_NEXT) return 0;
  if (TENS_NEXT >= TENS_CAP) {
    fprintf(stderr, "tensor_view_of: out of descriptor slots\n");
    exit(1);
  }
  TenDesc *src = &TENS[src_id];
  u32 id = TENS_NEXT++;
  TenDesc *d = &TENS[id];
  d->dtype    = src->dtype;
  d->refcount = 1;
  d->view     = new_view;
  d->buf_id   = src->buf_id;
  d->backend  = src->backend;
  if (d->backend && d->backend->buf_incref) d->backend->buf_incref(d->buf_id);
  return id;
}
