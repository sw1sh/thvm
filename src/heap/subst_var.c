// Install `value` at the binder's heap cell with the SUB flag set, so a
// subsequent VAR/DP0/DP1 enter on `loc` will pick it up and clear the flag.
//
// Release-store ordering: under T>1, another worker's VAR(loc)
// resolution must see a fully-formed Term when it observes the SUB
// bit -- not a half-published (val bits set, sub bit not yet) cell.
// On x86 stores are already release-ordered (TSO); on arm64 this
// compiles to a STLR.  Single-thread cost is identical to the legacy
// plain store.
fn void heap_subst_var(u64 loc, Term value) {
  __atomic_store_n(&HEAP[loc],
                   (u64)term_sub_set(value, 1),
                   __ATOMIC_RELEASE);
  WIRE_PROV_BUMP(loc);
}
