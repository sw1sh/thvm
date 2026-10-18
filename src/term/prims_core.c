// THVM_PRIM_PRI, arity 3: (PRI slot val cont) forces val and, for slot != 0,
// hands it to the WL callback in that slot, whose nonzero result replaces cont.
// Slot 0 only sequences: a recursive optimizer loop forces each step's ASSIGN.

// No-op weak default for standalone builds. THVM_HAS_WL_BRIDGE skips it: the WL
// bridge (thvmlink.c) defines the callback, and AOT dylibs resolve it at load.
#ifndef THVM_HAS_WL_BRIDGE
__attribute__((weak)) Term thvm_pri_wl_invoke_returning(u32 slot, Term value) {
  (void)slot; (void)value;
  return 0;
}
#else
extern Term thvm_pri_wl_invoke_returning(u32 slot, Term value);
#endif

static Term prim_pri(Term *args) {
  Term slot_num = wnf(args[0]);
  if (term_tag(slot_num) != TAG_NUM) return args[2];
  u32  slot = (u32)term_val(slot_num);
  // Each force is its own ASSIGN pass, so a recursive training loop that
  // re-forces one shared step cell assigns on every iteration.
  assign_pass_epoch_bump();
  Term v    = wnf(args[1]);
  if (slot != 0) {
    Term override = thvm_pri_wl_invoke_returning(slot, v);
    if (override != 0) return override;
  }
  return args[2];
}

fn void thvm_register_core_prims(void) {
  prim_register(THVM_PRIM_PRI, prim_pri, 3);
}
