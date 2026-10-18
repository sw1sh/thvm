fn u8 uop_is_movement(u8 op) {
  return op == UOP_RESHAPE || op == UOP_PERMUTE
      || op == UOP_EXPAND  || op == UOP_PAD
      || op == UOP_SHRINK  || op == UOP_FLIP;
}
