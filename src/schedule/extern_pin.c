// schedule/extern_pin.c - external-caller pin table.
//
// Tracks every Term that a foreign caller (today: the WL
// LibraryLink bridge) is currently holding outside the heap,
// so gc_collect_roots can treat them as live GC roots.
// Without this, the trace can't see those references and
// would reclaim their backing buffers between realizes.
//
// Capacity is generous and saturated push silently drops:
// over-preserving until the next thvm_init / thvm_free is
// safe; dropping a real pin is not.  Linear-scan unpin keeps
// the implementation trivial; a hash table would be premature.

#define EXTERN_PINNED_TERMS_CAP 2048

static Term EXTERN_PINNED_TERMS    [EXTERN_PINNED_TERMS_CAP];
static u32  EXTERN_PINNED_TERMS_LEN = 0;

fn void extern_pin_term(Term t) {
  if (t == 0) return;
  if (EXTERN_PINNED_TERMS_LEN >= EXTERN_PINNED_TERMS_CAP) return;
  EXTERN_PINNED_TERMS[EXTERN_PINNED_TERMS_LEN++] = t;
}

fn void extern_unpin_term(Term t) {
  if (t == 0) return;
  for (u32 i = 0; i < EXTERN_PINNED_TERMS_LEN; i++) {
    if (EXTERN_PINNED_TERMS[i] == t) {
      EXTERN_PINNED_TERMS[i] = EXTERN_PINNED_TERMS[EXTERN_PINNED_TERMS_LEN - 1];
      EXTERN_PINNED_TERMS_LEN--;
      return;
    }
  }
}

fn void extern_pin_clear(void) {
  EXTERN_PINNED_TERMS_LEN = 0;
}

fn void extern_pinned_for_each(void (*cb)(Term)) {
  if (cb == NULL) return;
  for (u32 i = 0; i < EXTERN_PINNED_TERMS_LEN; i++) {
    if (EXTERN_PINNED_TERMS[i] != 0) cb(EXTERN_PINNED_TERMS[i]);
  }
}
