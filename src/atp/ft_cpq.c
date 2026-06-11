// ft_cpq.c - AtpFt: native Arena-A CP queue (dual-store).
//
// Under -DTHVM_ATPFT_CPQ the CP queue gains a parallel storage
// `s->cp_packed_ft[]` (one `AtpCpEntry` per slot) holding the
// lhs/rhs AtpFt cells in Arena A.  The legacy packed-byte queue
// (`s->cp_packed[]`) is kept populated in parallel for the FV
// index, peek/stash and trace paths.  See docs/atp/engineering.md.
// Byte queue retires once those consumers are re-keyed onto the
// AtpFt structural hash.
//
// What this TU provides:
//
//   - struct AtpCpEntry             -- 32 B per slot (cache-line)
//   - atp_cp_ft_ensure_cap          -- grow / NULL-init the array
//   - atp_cp_ft_set                 -- write a fresh entry (allocates
//                                      the two FT spans from Arena A)
//   - atp_cp_ft_clear               -- free a slot's FT spans
//   - atp_cp_ft_swap                -- pointer swap, matches the heap
//                                      swap on the legacy arrays
//   - atp_cp_ft_transfer            -- ownership move (queue -> rule)
//   - atp_cp_trivially_joinable_ft  -- ft_eq verdict on FT cells
//                                      (NORM-fixpoint via existing
//                                      atp_rewrite_normalize_ft); no
//                                      Term ever materialized
//
// Lifetime: every populated `cp_packed_ft[i]` owns its two FT spans.
// On CP drop the entry's spans are returned to Arena A's free list
// via `ft_free_span`.  On CP commit-as-rule the cells transfer (no
// copy) into `s->lhs_ft[k]` / `s->rhs_ft[k]` and the slot is zeroed
// so a later sift / backfill does not double-free.
//
// Heap-key invariance: `cp_pri[i]`, `cp_pri2[i]`, `cp_seq[i]`,
// `cp_goal[i]` are byte-identical to the Term path.  Selection
// trajectory parity is the gated acceptance test (see
// tests/test_ft_cpq.c).
//
// Gated on THVM_ATPFT_CPQ (implies THVM_ATPFT_RULES + THVM_ATPFT_NORM
// for the FT-norm fixpoint inside the joinability check).

#ifdef THVM_ATPFT_CPQ

#include "../thvm.h"
#include "ft.h"

// Symbols brought in from the Term-side TU (same single-TU build):
//   AtpState (thvm.h)
//   ft_from_term, ft_eq, ft_free_span (ft.c / ft_alloc.c)
//   atp_rewrite_normalize_ft (ft_norm.c, requires THVM_ATPFT_NORM)
// We do not redeclare these here -- this file is #include'd from _.c
// after all of the above are visible.

// --- Entry layout ---------------------------------------------------
//
// 32 bytes / cache-line aligned.  `lhs` and `rhs` are AtpFt cell spans
// allocated in Arena A (persistent).  `size` is the ft node count
// (free goal weight); `weight` is the cached KBO/structural weight
// for the auto-MaxWeight gates.  `priority` mirrors cp_pri[i] for
// cache locality; `origin_rule` is the rule index that birthed the
// CP (provenance; 0xffff = unknown).

typedef struct AtpCpEntry {
  AtpFtCell *lhs;        // 8B -- Arena-A span head
  AtpFtCell *rhs;        // 8B
  u32        size;       // 4B -- ft node count
  u32        weight;     // 4B -- cached structural weight (mirrors cp_pri)
  u16        priority;   // 2B -- mirror of cp_pri[i] truncated (cache hint)
  u16        origin_rule;// 2B -- provenance; 0xffff = none / unknown
  u32        _pad;       // 4B -- pad to 32 B
} AtpCpEntry;

_Static_assert(sizeof(AtpCpEntry) == 32, "AtpCpEntry must be 32 bytes");

// --- ft node count ---------------------------------------------------
//
// Walk the span [first..first->end] counting cells.  The flat layout
// makes this O(n) with one pointer chase per cell -- cheaper than the
// recursive symbol_count walk it replaces.
static u32 ft_node_count(const AtpFtCell *first) {
  if (first == NULL) return 0u;
  u32 n = 0u;
  const AtpFtCell *p = first;
  const AtpFtCell *end = first->end;
  while (p != NULL) {
    n++;
    if (p == end) break;
    p = p->next;
  }
  return n;
}

// --- Capacity / init -------------------------------------------------
//
// Grow `s->cp_packed_ft` to hold at least `need` slots, NULL-init any
// new tail.  Called from atp_ensure_cp_cap (the existing growth
// helper).  Mirrors the doubling discipline of the legacy
// cp_packed[] grow site.
static void atp_cp_ft_ensure_cap(AtpState *s, u32 need, u32 old_cap) {
  if (need <= old_cap) return;
  // The legacy atp_ensure_cp_cap already computed the new cap; this
  // helper just sees both edges via the caller's parameters.  Caller
  // passes the OLD cap so the NULL-init loop covers only the fresh
  // tail.
  AtpCpEntry *fresh = (AtpCpEntry *)realloc(
      (AtpCpEntry *)s->cp_packed_ft, need * sizeof(AtpCpEntry));
  if (fresh == NULL) {
    fprintf(stderr, "atp_cp_ft_ensure_cap: realloc to %u slots failed\n", need);
    exit(1);
  }
  for (u32 i = old_cap; i < need; i++) {
    fresh[i].lhs = NULL;
    fresh[i].rhs = NULL;
    fresh[i].size = 0u;
    fresh[i].weight = 0u;
    fresh[i].priority = 0u;
    fresh[i].origin_rule = 0xffffu;
    fresh[i]._pad = 0u;
  }
  s->cp_packed_ft = fresh;
}

// --- Slot write ------------------------------------------------------
//
// Allocate two persistent FT spans from Arena A as the AtpFt image of
// (lhs, rhs) and stash into slot i.  `priority` is the truncated
// cp_pri[i] (16-bit cache hint); `origin_rule` is the trace's rule id
// (or 0xffff when unknown).  Returns the slot's node-count sum so the
// caller can mirror it into cp_pri's symbol-count base if needed --
// today we just record it on the entry.
static u32 atp_cp_ft_set(AtpState *s, u32 i, Term lhs, Term rhs,
                         u32 priority_hint, u32 origin_rule) {
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  AtpFtCell *fl = ft_from_term(a, lhs, /*scratch=*/0);
  AtpFtCell *fr = ft_from_term(a, rhs, /*scratch=*/0);
  u32 ln = ft_node_count(fl);
  u32 rn = ft_node_count(fr);
  AtpCpEntry *e = &((AtpCpEntry *)s->cp_packed_ft)[i];
  e->lhs = fl;
  e->rhs = fr;
  e->size = ln + rn;
  e->weight = e->size;                              // structural-weight cache
  e->priority = (u16)(priority_hint > 0xffffu ? 0xffffu : priority_hint);
  e->origin_rule = (u16)(origin_rule > 0xffffu ? 0xffffu : origin_rule);
  e->_pad = 0u;
  return e->size;
}

// --- Slot clear (free FT spans) -------------------------------------
//
// Returns both FT spans to Arena A's free list and zeros the entry so
// a later backfill / sift sees an empty slot.  Idempotent: a NULL
// slot is a no-op.
static void atp_cp_ft_clear(AtpState *s, u32 i) {
  AtpCpEntry *e = &((AtpCpEntry *)s->cp_packed_ft)[i];
  if (e->lhs == NULL && e->rhs == NULL) return;
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  if (e->lhs != NULL) ft_free_span(a, e->lhs, e->lhs->end);
  if (e->rhs != NULL) ft_free_span(a, e->rhs, e->rhs->end);
  e->lhs = NULL;
  e->rhs = NULL;
  e->size = 0u;
  e->weight = 0u;
  e->priority = 0u;
  e->origin_rule = 0xffffu;
}

// --- Slot swap (heap sift) -------------------------------------------
//
// Pointer swap; the entry's FT spans do not move so any borrow into
// them (none today, but Stages 8-9 will index off the cells) stays
// valid.
static void atp_cp_ft_swap(AtpState *s, u32 i, u32 j) {
  AtpCpEntry *arr = (AtpCpEntry *)s->cp_packed_ft;
  AtpCpEntry t = arr[i];
  arr[i] = arr[j];
  arr[j] = t;
}

// --- Slot transfer (queue -> rule) -----------------------------------
//
// Ownership move on CP commit-as-rule: the entry's FT spans become
// the rule's lhs_ft / rhs_ft.  Slot is zeroed so a subsequent
// backfill or destroy does not double-free.  The caller is
// responsible for actually writing s->lhs_ft[k] / s->rhs_ft[k] from
// the returned pointers (we keep the assignment at the caller for
// clarity -- this helper only releases ownership).
static void atp_cp_ft_transfer_out(AtpState *s, u32 i,
                                   AtpFtCell **lhs_out, AtpFtCell **rhs_out) {
  AtpCpEntry *e = &((AtpCpEntry *)s->cp_packed_ft)[i];
  if (lhs_out) *lhs_out = e->lhs;
  if (rhs_out) *rhs_out = e->rhs;
  e->lhs = NULL;
  e->rhs = NULL;
  e->size = 0u;
  e->weight = 0u;
  e->priority = 0u;
  e->origin_rule = 0xffffu;
}

// --- AtpFt-native joinability ----------------------------------------
//
// Sibling of atp_cp_trivially_joinable that operates ENTIRELY on FT
// cells.  Normalizes both sides to fixpoint via the Stage-6 ft-norm
// path (which descends through the discrim tree), then
// returns ft_eq(*lhs, *rhs).  Updates *lhs / *rhs to the NF cells so
// the caller can read them post-call.
//
// Mirrors the Term-path verdict bit-for-bit by construction (// VERIFY mode established equality of the two normalize paths on the
// differential corpus).  Joining is symmetric in lhs/rhs.
//
// Sole caller is atp_cp_trivially_joinable (generation-time CP push),
// which hands FT cells straight here, skipping the ft_from_term step
// that the plain THVM_ATPFT_NORM path pays at every joinability call.
static u8 atp_cp_trivially_joinable_ft(AtpState *s,
                                       AtpFtCell **lhs,
                                       AtpFtCell **rhs) {
  // Syntactic-equality fast-path: degenerate overlaps where the CP's
  // two sides are already equal are trivially joined; skip both
  // normalize calls.  ft_eq early-exits on the first symbol mismatch
  // so the overhead is sub-microsecond when sides differ.
  if (ft_eq(*lhs, *rhs)) return 1u;
  const u32 NORM_CAP = 64u;
  // Generation-time CP treatment = WM KPBehandelt under `-kg "r"`:
  // doR only -- unorientable equations never rewrite here (see the
  // Term-cell sibling atp_cp_trivially_joinable in _.c).
  AtpFtCell *l = atp_rules_only_normalize_ft(s, *lhs, NORM_CAP);
  AtpFtCell *r = atp_rules_only_normalize_ft(s, *rhs, NORM_CAP);
  *lhs = l;
  *rhs = r;
  return (u8)ft_eq(l, r);
}

// --- Slot move (backfill) --------------------------------------------
//
// Pointer move dst <- src + zero src so the now-vacated src does not
// double-free its (now-transferred) FT spans.  Mirrors the
// cp_packed[]/cp_trace[]/... backfill in select_cp; called after a
// pop to slide the last live slot into the vacated j.
static void atp_cp_ft_move(AtpState *s, u32 dst, u32 src) {
  if (dst == src) return;
  AtpCpEntry *arr = (AtpCpEntry *)s->cp_packed_ft;
  arr[dst] = arr[src];
  arr[src].lhs = NULL;
  arr[src].rhs = NULL;
  arr[src].size = 0u;
  arr[src].weight = 0u;
  arr[src].priority = 0u;
  arr[src].origin_rule = 0xffffu;
  arr[src]._pad = 0u;
}

// --- API surface (cross-TU within _.c) -------------------------------
//
// Static-by-default; the symbols are reachable from _.c because
// ft_cpq.c is #include'd into the single TU.  The signatures live in
// this file as the spec; _.c calls them as plain function calls.

#endif // THVM_ATPFT_CPQ
