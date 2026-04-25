// schedule/wl_pin.c - WL-pinned-Terms side table.
//
// Tracks every Term the WL caller is currently holding via a
// TTerm[id] handle, so gc_collect_roots can treat them as live
// roots.  WL-held Terms can reach forward intermediate kernel
// outputs (TAG_TEN cells in HEAP[]) that no other root traces
// to but a future TGrad realize structurally needs.
//
// Capacity is generous and saturated push silently drops:
// over-preserving until the next thvm_init / thvm_free is safe;
// dropping a real pin is not.  A hash table would be premature.

#define WL_PINNED_TERMS_CAP 2048

static Term WL_PINNED_TERMS    [WL_PINNED_TERMS_CAP];
static u32  WL_PINNED_TERMS_LEN = 0;

fn void wl_pin_term(Term t) {
  if (t == 0) return;
  if (WL_PINNED_TERMS_LEN >= WL_PINNED_TERMS_CAP) return;
  WL_PINNED_TERMS[WL_PINNED_TERMS_LEN++] = t;
}

fn void wl_unpin_term(Term t) {
  if (t == 0) return;
  for (u32 i = 0; i < WL_PINNED_TERMS_LEN; i++) {
    if (WL_PINNED_TERMS[i] == t) {
      WL_PINNED_TERMS[i] = WL_PINNED_TERMS[WL_PINNED_TERMS_LEN - 1];
      WL_PINNED_TERMS_LEN--;
      return;
    }
  }
}

fn void wl_pin_clear(void) {
  WL_PINNED_TERMS_LEN = 0;
}

fn void wl_pinned_for_each(void (*cb)(Term)) {
  if (cb == NULL) return;
  for (u32 i = 0; i < WL_PINNED_TERMS_LEN; i++) {
    if (WL_PINNED_TERMS[i] != 0) cb(WL_PINNED_TERMS[i]);
  }
}
