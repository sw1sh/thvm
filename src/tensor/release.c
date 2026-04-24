// tensor/release.c - decrement TenDesc refcount; free buffer at zero.
//
// Called on ERA-on-TAG_TEN (step 12+ interaction).  When the descriptor
// refcount hits zero we ask the backend to drop the buffer reference
// (which itself frees only when *its* own refcount hits zero, so view
// aliases keep the storage alive).  The descriptor slot is leaked for
// step 12; the freelist arrives in step 13.

fn void tensor_release(u32 id) {
  if (id == 0 || id >= TENS_NEXT) return;
  if (TENS[id].refcount == 0)     return;
  if (--TENS[id].refcount == 0) {
    Backend *b = TENS[id].backend;
    if (b && b->buf_decref) b->buf_decref(TENS[id].buf_id);
    TENS[id].buf_id  = 0;
    TENS[id].backend = NULL;
  }
}
