// tensor/incref.c - bump TenDesc refcount (DUP on TAG_TEN).

fn void tensor_incref(u32 id) {
  if (id == 0 || id >= TENS_NEXT) return;
  TENS[id].refcount++;
}
