fn u8 term_tag(Term t) {
  return (u8)((t >> TAG_SHIFT) & TAG_MASK);
}
