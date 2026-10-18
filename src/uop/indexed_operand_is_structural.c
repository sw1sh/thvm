// GATHER slot 0 and SCATTER slots 1-2 are read through their views at an
// address the op builds (ru_index_leaf), which cannot model a PAD's mask.
// bufferize_classify realizes any other operand; rangeify asks this too.
fn int uop_indexed_operand_is_structural(Term t) {
  for (;;) {
    Term r = term_resolve(t);
    u8 tag = term_tag(r);
    if (tag == TAG_TEN || tag == TAG_VAR) {
      return 1;
    }
    if (tag != TAG_UOP) {
      return 0;
    }
    switch (term_ext(r)) {
      case UOP_CONST: case UOP_KERNEL:
        return 1;
      case UOP_RESHAPE: case UOP_PERMUTE: case UOP_EXPAND:
      case UOP_SHRINK:  case UOP_FLIP:
        t = heap_read(term_val(r));
        break;
      default:
        return 0;
    }
  }
}
