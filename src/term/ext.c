fn u32 term_ext(Term t) {
  return (u32)((t >> EXT_SHIFT) & EXT_MASK);
}
