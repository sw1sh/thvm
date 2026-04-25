// schedule/wl_pin.c - wpt1 of the WL-pinned-Terms arc.
//
// `WL_PINNED_TERMS[]` is the side table that tracks every Term
// the WL caller is currently holding.  Populated by the WL
// bridge functions (wpt2 will wire thvm_wl_term_new /
// thvm_wl_realize / etc. to call wl_pin_term on every Term they
// hand out); consumed by gc_collect_roots (wpt3) so the
// tracing-GC walk treats WL-held Terms as live roots.
//
// The pin table is the missing signal that bm4 / hrp / gc all
// stumbled on: forward intermediate kernel outputs sit in HEAP[]
// as TAG_TEN cells but no GC root traces to them; the WL caller
// holds them via TTerm[id] handles only WL knows about.  Once
// wpt2 wires the bridge, the heap-rooted overlay in
// mark_gc_preserve can finally come off (wpt3) and the
// freelist in bm4b actually receives intermediate slots
// (wpt4 measures).
//
// Linear scan for unpin: WL_PINNED_TERMS_CAP=2048 is generous
// for our scale (a single TRealize hands back at most a few
// dozen Terms) and a hash table would be premature complexity.
// Saturated push is a silent no-op -- correctness preserved
// (we just over-preserve until cleared).

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
  // Term not in the table: no-op.  Common case is double-free
  // / WL-side cleanup races; treat as benign.
}

fn void wl_pin_clear(void) {
  WL_PINNED_TERMS_LEN = 0;
}

// Iterator helper.  Used by gc_collect_roots; passes each
// non-zero pinned Term to the callback.  Order is insertion
// order (with swaps from unpin); callers that depend on a
// stable ordering should sort.
fn void wl_pinned_for_each(void (*cb)(Term)) {
  if (cb == NULL) return;
  for (u32 i = 0; i < WL_PINNED_TERMS_LEN; i++) {
    if (WL_PINNED_TERMS[i] != 0) cb(WL_PINNED_TERMS[i]);
  }
}
