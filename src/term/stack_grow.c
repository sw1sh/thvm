// Work stack of an iterative term walk, so depth costs heap, not C stack: `buf`
// starts as the caller's inline array `inl` (*cap entries of `esz` bytes); grow
// doubles it onto the heap (updating *cap), release frees a heap block.
fn void *term_stack_grow(void *buf, const void *inl, u32 *cap, size_t esz) {
  size_t n  = (size_t)*cap;
  size_t nn = n * 2u;
  if (nn > 0xFFFFFFFFu) {
    thvm_fatal("term_stack_grow: walk stack over 2^32 entries");
  }
  void *nb;
  if (buf == inl) {
    nb = malloc(nn * esz);
    if (nb != NULL) {
      memcpy(nb, inl, n * esz);
    }
  } else {
    nb = realloc(buf, nn * esz);
  }
  if (nb == NULL) {
    thvm_fatal("term_stack_grow: out of memory");
  }
  *cap = (u32)nn;
  return nb;
}

fn void term_stack_release(void *buf, const void *inl) {
  if (buf != inl) {
    free(buf);
  }
}
