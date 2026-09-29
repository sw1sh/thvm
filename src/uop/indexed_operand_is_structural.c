// uop/indexed_operand_is_structural.c - can an indexed op read this operand
// in place?
//
// An indexed op (GATHER slot 0) reads its storage operand at an address it
// builds itself, walking the operand's views down to storage
// (rangeify_unified.c ru_index_leaf).  That works when the views end at a
// TEN, a VAR, a KERNEL's output or a CONST, and holds no PAD (whose
// out-of-window mask the walk does not model).  Any other operand has a
// compute source bufferize_classify realizes first; both sides decide with
// this predicate so they agree.

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
