// ! &L{x0, x1} = UOP(...)
// ---------------------- DUP-UOP (share by heap-loc identity)
// x0 <- UOP(...)
// x1 <- UOP(...)
//
// Tensor compute (UOP_ADD, UOP_MUL, UOP_KERNEL, etc.) is shared by
// heap-loc identity rather than copied: both projections reference
// the same UOP heap cell.  When the kernel fires, materialize+wnf
// compile and run it once; both consumers see the resulting TEN.
//
// This is the analogue of DUP-NUM for compound non-IC values: the
// "atomic" copy on a tensor compute node is a reference share, not a
// structural duplication.
fn Term interact_dup_uop(u8 side, u64 loc, Term uop) {
  ITRS++;
  return heap_subst_cop(side, loc, uop, uop);
}
