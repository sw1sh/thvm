// tensor/decref.c - decrement TenDesc refcount without freeing.
//
// Pairs with tensor_incref for symmetric tests.  Production paths
// should call tensor_release instead, which also releases the
// underlying backend buffer when refcount hits zero.

fn void tensor_decref(u32 id) {
  if (id == 0 || id >= TENS_NEXT) return;
  if (TENS[id].refcount == 0)     return;
  TENS[id].refcount--;
}
