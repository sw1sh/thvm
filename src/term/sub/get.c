fn u8 term_sub_get(Term t) {
  return (u8)((t >> SUB_SHIFT) & SUB_MASK);
}
