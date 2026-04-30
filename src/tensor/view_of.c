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
  d->dtype        = src->dtype;
  d->refcount     = 1;
  d->view         = new_view;
  // Inherit src's prior_views chain unchanged: the public view is
  // being replaced (movement op absorbed into a single-view step)
  // but the chain underneath stays intact.  Chain ownership is
  // shared by reference here -- prior_views entries are immutable
  // once written, so multiple TenDescs can point at the same
  // chain array.  Free at TenDesc release time only when the last
  // reference goes away.
  d->prior_views  = src->prior_views;
  d->nviews       = src->nviews;
  d->buf_id       = src->buf_id;
  d->backend      = src->backend;
  // Inherit producer_kid so kernel_fire_by_id can chase the
  // upstream kernel that fills the shared buffer.  Without this
  // the alias acts like an external (uninitialized) tensor.
  d->producer_kid = src->producer_kid;
  if (d->backend && d->backend->buf_incref) d->backend->buf_incref(d->buf_id);
  return id;
}

// Append `new_outermost` as the new public-facing view; the previous
// public view (and src's prior_views chain, if any) becomes the inner
// chain for the new TenDesc.  Used when a movement op (typically
// RESHAPE on a non-contig source) can't be absorbed into the existing
// view's strides+offset and instead needs a fresh canonical view that
// composes through the chain at index time.  Mirrors tinygrad's
// `ShapeTracker(views + (new_view,))` append-and-simplify pattern.
fn u32 tensor_view_chain_append(u32 src_id, View new_outermost) {
  if (src_id == 0 || src_id >= TENS_NEXT) return 0;
  if (TENS_NEXT >= TENS_CAP) {
    fprintf(stderr, "tensor_view_chain_append: out of descriptor slots\n");
    exit(1);
  }
  TenDesc *src = &TENS[src_id];
  u8  new_n = src->nviews + 1;
  if (new_n == 0) {                             // u8 overflow
    fprintf(stderr, "tensor_view_chain_append: chain depth wraparound\n");
    return 0;
  }
  View *new_chain = (View *)malloc(sizeof(View) * new_n);
  if (new_chain == NULL) {
    fprintf(stderr, "tensor_view_chain_append: out of memory\n");
    return 0;
  }
  // Copy src's existing prior_views (the old inner chain) into the
  // new chain's lower indices.  prior_views[0] is the innermost
  // (closest to buffer), unchanged.
  for (u8 i = 0; i < src->nviews; i++) new_chain[i] = src->prior_views[i];
  // The src's primary view (which used to be the outermost public
  // face) becomes the new chain's outermost-prior, i.e. the
  // immediate inner of the new public view.
  new_chain[src->nviews] = src->view;

  u32 id = TENS_NEXT++;
  TenDesc *d = &TENS[id];
  d->dtype        = src->dtype;
  d->refcount     = 1;
  d->view         = new_outermost;
  d->prior_views  = new_chain;
  d->nviews       = new_n;
  d->buf_id       = src->buf_id;
  d->backend      = src->backend;
  d->producer_kid = src->producer_kid;
  if (d->backend && d->backend->buf_incref) d->backend->buf_incref(d->buf_id);
  return id;
}
