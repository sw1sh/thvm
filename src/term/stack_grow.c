// term/stack_grow.c - work stack of an iterative term walk.
//
// A walk over a term keeps its pending frames in a small array on the C
// stack (`inl`) and moves them to the heap only when the term is deeper
// than that array.  Depth then costs heap memory, never C stack: a
// recursive walk over a 100k-deep term overflows the 8 MB main stack.

// Work stack of an iterative term walk: `buf` starts as the caller's
// inline array `inl` of *cap entries of `esz` bytes; grow doubles it
// onto the heap (updating *cap), release frees a heap block.
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
