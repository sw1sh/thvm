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
  d->requires_grad = 0;          // promoted to 1 via set_requires_grad
  d->grad         = 0;           // chain-rule accumulator; populated lazily
  d->assign_kvar_id = 0;         // not a kvar-offset assign dst (set by view_resolve)
  d->backend      = b;
  d->producer_kid = 0;
  d->buf_id       = b->buf_alloc(dtype_storage_bytes(dtype, (u64)d->view.numel));
  return id;
}

// tensor_alloc over an EXISTING backend buffer: the capture-recycle adoption
// (jit_caprecycle_pop hands a dead same-size captured intermediate to a later
// captured kernel output; see jit/capture.c).  Mirrors the freelist recycle's
// aliasing -- the prior owner's TenDesc may still name buf_id but is dead
// (its last consumer fired before the pool push).  The adopter takes a REAL
// buffer reference: WL-visible adopters (a per-projection realize root) get
// tensor_release'd later, and that decrement must land on the capture
// retain's floor of 1 (re-parking the id via jit_caprecycle_release_hook),
// never on 0 mid-capture -- an unmatched release would hard-free a buffer the
// replay still dispatches into.  The caller guarantees
// dtype_storage_bytes(dtype, numel) == the buffer's allocated nbytes (the
// pool pop is exact-size keyed).
fn u32 tensor_alloc_adopt(Backend *b, Shape shape, u32 dtype, u32 buf_id) {
  if (buf_id == 0) return 0;
  if (TENS_NEXT >= TENS_CAP) {
    fprintf(stderr, "tensor_alloc_adopt: out of descriptor slots (cap=%llu)\n",
            (unsigned long long)TENS_CAP);
    exit(1);
  }
  u32 id = TENS_NEXT++;
  TenDesc *d = &TENS[id];
  d->dtype        = dtype;
  d->refcount     = 1;
  d->view         = view_create(shape);
  d->prior_views  = NULL;
  d->nviews       = 0;
  d->requires_grad = 0;
  d->grad         = 0;
  d->assign_kvar_id = 0;
  d->backend      = b;
  d->producer_kid = 0;
  d->buf_id       = buf_id;
  if (b->buf_incref != NULL) b->buf_incref(buf_id);
  return id;
}
