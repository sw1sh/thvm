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

// Handle table: maps a host-assigned handle id (e.g. a Wolfram
// ManagedLibraryExpression id) to the Term it pins.  The host
// creates a handle, calls extern_pin_handle_set(handle_id, term)
// to associate, and the host's GC eventually calls
// extern_pin_handle_drop(handle_id) which releases the pin.
//
// WL's ManagedLibraryExpression ids are monotonically increasing
// across the entire kernel session -- once a long-running test
// suite or notebook drives the counter past the array cap, every
// subsequent associate would silently drop, breaking the pin /
// GC contract.  We grow the backing array on demand instead.

static Term *EXTERN_PIN_HANDLES = NULL;
static u64   EXTERN_PIN_HANDLE_CAP = 0;

static void extern_pin_handle_reserve(u64 id) {
  if (id < EXTERN_PIN_HANDLE_CAP) return;
  u64 new_cap = EXTERN_PIN_HANDLE_CAP > 0 ? EXTERN_PIN_HANDLE_CAP : 8192;
  while (id >= new_cap) new_cap *= 2;
  EXTERN_PIN_HANDLES = (Term *)realloc(EXTERN_PIN_HANDLES, new_cap * sizeof(Term));
  for (u64 i = EXTERN_PIN_HANDLE_CAP; i < new_cap; i++) EXTERN_PIN_HANDLES[i] = 0;
  EXTERN_PIN_HANDLE_CAP = new_cap;
}

fn void extern_pin_handle_set(u64 id, Term t) {
  extern_pin_handle_reserve(id);
  EXTERN_PIN_HANDLES[id] = t;
  extern_pin_term(t);
}

// Look up the current Term for a handle id; returns 0 if the
// handle is unknown.  Used by the WL bridge to refresh stale raw
// Term values after a copying GC moves the handle's referenced
// loc; the C-side EXTERN_PIN_HANDLES table is the source of truth.
fn Term extern_pin_handle_get(u64 id) {
  if (id >= EXTERN_PIN_HANDLE_CAP) return 0;
  return EXTERN_PIN_HANDLES[id];
}

fn void extern_pin_handle_drop(u64 id) {
  if (id >= EXTERN_PIN_HANDLE_CAP) return;
  Term t = EXTERN_PIN_HANDLES[id];
  if (t == 0) return;
  extern_unpin_term(t);
  EXTERN_PIN_HANDLES[id] = 0;
}

fn void extern_pin_handle_clear(void) {
  if (EXTERN_PIN_HANDLES == NULL) return;
  memset(EXTERN_PIN_HANDLES, 0, EXTERN_PIN_HANDLE_CAP * sizeof(Term));
}
