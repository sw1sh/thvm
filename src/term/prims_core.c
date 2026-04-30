// term/prims_core.c - core THVM primitives registered in the TAG_PRI table.
//
// THVM_PRIM_PRI (arity 3):
//   APP(APP(APP(PRI(PRI), slot_NUM), val), cont)
//
//   Pipeline:
//     1. wnf(val)  -- forces whatever computation val sits over;
//                     fires kernel chains and ASSIGN side effects.
//     2. if slot != 0, invoke the WL callback registered under `slot`
//                     with the wnf'd value.  The callback's return
//                     Term (if non-zero) OVERRIDES the redex result;
//                     0 means "use cont" (the default trace mode).
//                     The bridge defines thvm_pri_wl_invoke_returning;
//                     see CSource/thvmlink.c.
//     3. return cont -- the surrounding APP-PRI redex rewrites to
//                       the continuation, OR to the callback's return.
//
//   slot=0 is the "pure sequencer" mode: just force val, return cont,
//   no WL roundtrip.  Used by recursive optimizer loops to drive a
//   per-iteration ASSIGN before recursing.
//
//   slot>0 dispatches to a user-registered WL function -- loss logging
//   (return 0), conditional control flow (return alternative cont),
//   value transformation (return modified Term), etc.

// Weak default: C-only builds (unit tests, bench harnesses) link this
// no-op stub.  The WL bridge in CSource/thvmlink.c provides a strong
// override that takes precedence when the paclet dylib is linked.
//
// THVM_HAS_WL_BRIDGE is defined by the WL bridge source before
// pulling in thvm.c; when set, the bridge supplies its own definition
// directly and we must skip this default to avoid a redefinition.
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
