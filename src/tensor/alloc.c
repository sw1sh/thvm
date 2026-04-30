// tensor/alloc.c - reserve a fresh TenDesc slot and a backing buffer.
//
// Bump-only allocation in TENS[] (no freelist for step 12).  The
// buffer is allocated on the caller-provided backend; its refcount
// starts at 1.  The returned id is a tensor index usable in a
// TAG_TEN term (VAL field).

fn u32 tensor_alloc(Backend *b, Shape shape, u32 dtype) {
  if (TENS_NEXT >= TENS_CAP) {
    fprintf(stderr, "tensor_alloc: out of descriptor slots (cap=%llu)\n",
            (unsigned long long)TENS_CAP);
    exit(1);
  }
  u32 id = TENS_NEXT++;
  TenDesc *d = &TENS[id];
  d->dtype        = dtype;
  d->refcount     = 1;
  d->view         = view_create(shape);
  d->prior_views  = NULL;       // ShapeTracker: simple-single-view default
  d->nviews       = 0;
  d->backend      = b;
  d->producer_kid = 0;
  d->buf_id       = b->buf_alloc(dtype_storage_bytes(dtype, (u64)d->view.numel));
  return id;
}
