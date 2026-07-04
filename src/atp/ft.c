// ft.c - constructors, boundary converters, structural
// equality / hash, and pretty-print mirroring atp_pretty_term.
//
// Stage 2 is purely additive: no caller in the rest of the engine
// reads through these yet.  The unit test tests/test_ft.c builds 1000
// random Terms and round-trips them via ft_from_term / ft_to_term,
// also asserting ft_hash agrees with the existing atp_term_struct_hash
// (the latter is the bridge for the Stage-9 memo re-key).
//
// Var encoding: AtpFtCell.sym uses WM's convention (high bit set =
// variable, low 31 bits = var id).  This matches the doc in ft.h:36
// and lets the wmfpa flatterm matcher round-trip Term<->WfNode
// through the same encoding (see WF_VAR_BIT in src/wmfpa/wmfpa.h:54;
// reused here directly via the named macros).
//
// Gated on THVM_ATPFT_CONVERT.  src/atp/_.c pulls this TU in under
// THVM_ATPFT_RULES (default-on); the stage-2 standalone test
// (tests/test_ft.c) also includes it directly.  The
// ATPFT_FT_C_INCLUDED guard makes the second textual inclusion a no-op.

#ifdef THVM_ATPFT_CONVERT
#ifndef ATPFT_FT_C_INCLUDED
#define ATPFT_FT_C_INCLUDED 1

#include "../thvm.h"
#include "ft.h"

// --- Var-bit helpers ---------------------------------------------------
//
// Reuse the existing WF_VAR_BIT macros (src/wmfpa/wmfpa.h:54-57) rather
// than minting a new ATPFT_VAR_BIT: the cell-layout doc in ft.h:36
// mandates exactly that encoding ("High bit set marks a variable") and
// the flat WM matcher in _.c already round-trips Term<->WF through it.
// Numerically identical (0x80000000), same id mask -- same namespace.
#include "../wmfpa/wmfpa.h"

static inline int  ft_is_var (const AtpFtCell *c) {
  return (c->sym & WF_VAR_BIT) != 0u;
}
static inline u32  ft_var_id (const AtpFtCell *c) {
  return c->sym & ~WF_VAR_BIT;
}
static inline u32  ft_ctr_sym(const AtpFtCell *c) {
  // Only valid when ft_is_var(c) is false.  Caller's responsibility.
  return c->sym;
}

// --- Internal alloc routing -------------------------------------------
//
// scratch=0 -> Arena A (persistent); scratch=1 -> Arena B (bump).  The
// Stage 1 allocators already zero every field on hand-out
// (ft_alloc.c:103-108, 164-169), so ftnew_* only has to fill in the
// fields it owns (next/end/sym/arity + GROUND).  SUBST_FRESH and
// LPO_RANK_VALID stay clear by construction.
static inline AtpFtCell *ft_alloc_one(AtpFt *a, int scratch) {
  return scratch ? ft_alloc_scratch(a) : ft_alloc_persistent(a);
}

// --- Construction ------------------------------------------------------
//
// A variable cell is a single cell: end == self, no children, arity 0.
// Variables are NEVER ground (a ground term has no FVR leaves), so the
// GROUND flag stays clear.

AtpFtCell *ftnew_var(AtpFt *a, u32 var_id, int scratch) {
  AtpFtCell *c = ft_alloc_one(a, scratch);
  c->next  = NULL;        // caller stitches the next-sibling link when
                          // inserting into a parent CTR.
  c->end   = c;
  c->sym   = WF_VAR_BIT | (var_id & ~WF_VAR_BIT);
  c->arity = 0;
  c->flags = 0;           // SUBST_FRESH=0, GROUND=0, LPO_RANK_VALID=0.
  return c;
}

// A 0-arity constructor (a "constant") is a single cell, ground by
// construction (no children that could carry an FVR).
AtpFtCell *ftnew_const(AtpFt *a, u32 sym, int scratch) {
  AtpFtCell *c = ft_alloc_one(a, scratch);
  c->next  = NULL;
  c->end   = c;
  // sym MUST NOT have the high bit set (that would alias the var-flag
  // namespace).  Mask defensively -- the caller almost certainly
  // passes a low-bit constructor id; the mask only kicks in if a
  // misuse drives a sym into the var range.
  c->sym   = sym & ~WF_VAR_BIT;
  c->arity = 0;
  c->flags = ATPFT_FLAG_GROUND;
  return c;
}

// A CTR cell: allocate one cell for the parent, then stitch the
// `kids` subterm spans into a pre-order chain.  Layout invariants
// (from ft.h):
//
//   parent->next == first cell of kid[0] (== kid[0] itself).
//   for i in 0..arity-2:
//     kid[i]->end->next == first cell of kid[i+1]
//   parent->end == last cell of kid[arity-1]'s subterm (== kid[n-1]->end)
//   parent->end->next == NULL initially (the caller stitches the
//                        next-sibling link if this parent is itself a
//                        child of an outer CTR; for a top-level term
//                        the trailing NULL is fine).
//
// Each kid was previously built by ftnew_var / ftnew_const / ftnew_ctr
// and currently has kid->end->next == NULL (top-level), so the stitch
// is purely overwriting that trailing NULL with the next sibling's
// head.  This matches how a multi-kid persistent term threads in
// pre-order under WM's TermzellenT.
//
// GROUND is OR-folded: a CTR is ground iff every child is ground.
AtpFtCell *ftnew_ctr(AtpFt *a, u32 sym, u16 arity,
                     AtpFtCell *const *kids, int scratch) {
  AtpFtCell *c = ft_alloc_one(a, scratch);
  c->sym   = sym & ~WF_VAR_BIT;
  c->arity = arity;
  c->flags = ATPFT_FLAG_GROUND;     // tentatively; OR-folded below
  if (arity == 0u) {
    c->next = NULL;
    c->end  = c;
    return c;
  }
  // Stitch first child.
  AtpFtCell *first = kids[0];
  c->next = first;
  AtpFtCell *prev_end = first->end;
  if ((first->flags & ATPFT_FLAG_GROUND) == 0u) c->flags = 0;
  for (u16 i = 1; i < arity; i++) {
    AtpFtCell *k = kids[i];
    prev_end->next = k;
    prev_end       = k->end;
    if ((k->flags & ATPFT_FLAG_GROUND) == 0u) c->flags = 0;
  }
  c->end           = prev_end;
  prev_end->next   = NULL;
  return c;
}

// --- Boundary: Term -> AtpFt -----------------------------------------
//
// Recursive walk over a heap-cell Term.  TAG_FVR -> ftnew_var, TAG_CTR
// -> ftnew_ctr (recursing into children), everything else falls back
// to a constant cell keyed by tag (the only other thing the ATP layer
// ever puts into a term position is TAG_NUM, which we encode as a
// 0-arity constructor with sym = (TAG_NUM << 24) | dtype<<16 |
// raw_lo16 -- chosen so two distinct NUM Terms hash differently and
// the round-trip back to Term reproduces the original).  The narrow
// boundary is exactly TAG_FVR + TAG_CTR; the fallback exists so a
// stray sibling-tagged Term doesn't crash the converter during
// bring-up.

// Forward decls for mutual recursion through term_ctr_at children.
static AtpFtCell *ft_from_term_rec(AtpFt *a, Term t, int scratch);

// Pack a TAG_NUM into a constructor sym + a child carrying the raw
// bits.  TAG_NUM has no children in heap-cell land (atom; val = raw
// u32/f32 bits, ext = dtype -- thvm.h:118), but we still need to
// round-trip both fields.  Encode as:
//   sym = (TAG_NUM_MARKER) for the outer CTR cell
//   children = [dtype-const-cell, rawbits-const-cell]
// where each child is a 0-arity ftnew_const carrying the field in its
// sym.  ft_to_term inverts.  Marker value picks a sym range that
// won't collide with any constructor label the ATP layer actually
// uses (labels in the harvested wolfram.pr / sheffer.pr live in
// [0, 256) -- this marker lives at 0x40000000, well below
// WF_VAR_BIT and well above any plausible label).
#define ATPFT_NUM_MARKER 0x40000000u

static AtpFtCell *ft_from_term_rec(AtpFt *a, Term t, int scratch) {
  extern unsigned char g_atp_phase_detail;
  if (g_atp_phase_detail) { extern unsigned long long g_ft_from_nodes; g_ft_from_nodes++; }
  u8 tag = term_tag(t);
  if (tag == TAG_FVR) {
    return ftnew_var(a, term_ext(t), scratch);
  }
  if (tag == TAG_CTR) {
    u32 label = term_ext(t);
    u32 n     = term_ctr_n(t);
    if (n == 0u) return ftnew_const(a, label, scratch);
    // Build children first, then ftnew_ctr stitches them.  Stack
    // buffer of fixed size for the typical small arity (constructors
    // in the harvested ATP corpora are <= 4-ary; even Sheffer maxes
    // at 2).  Fall back to a heap alloc for the unlikely large case.
    enum { KIDS_STACK = 16 };
    AtpFtCell *kids_stack[KIDS_STACK];
    AtpFtCell **kids = kids_stack;
    AtpFtCell **kids_heap = NULL;
    if (n > KIDS_STACK) {
      kids_heap = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)n);
      if (kids_heap == NULL) { fprintf(stderr, "ft_from_term: kids OOM\n"); exit(1); }
      kids = kids_heap;
    }
    for (u32 i = 0; i < n; i++) {
      kids[i] = ft_from_term_rec(a, term_ctr_at(t, i), scratch);
    }
    AtpFtCell *c = ftnew_ctr(a, label, (u16)n, kids, scratch);
    free(kids_heap);
    return c;
  }
  if (tag == TAG_NUM) {
    u32 dtype = term_ext(t);
    u32 raw   = (u32)term_val(t);
    AtpFtCell *dt_cell  = ftnew_const(a, dtype & ~WF_VAR_BIT, scratch);
    AtpFtCell *raw_cell = ftnew_const(a, raw   & ~WF_VAR_BIT, scratch);
    AtpFtCell *kids[2]  = { dt_cell, raw_cell };
    return ftnew_ctr(a, ATPFT_NUM_MARKER, 2u, kids, scratch);
  }
  // Fallback: encode any other tag as a 0-arity constant whose sym
  // carries the tag id ORed with a tag-namespace marker so distinct
  // tags don't collide with constructor labels.  No live ATP path
  // produces these, but Stage 2 needs the converter to be total to
  // simplify bring-up.
  return ftnew_const(a, 0x20000000u | (u32)tag, scratch);
}

AtpFtCell *ft_from_term(AtpFt *a, Term t, int scratch) {
  extern unsigned char g_atp_phase_detail;
  if (g_atp_phase_detail) { extern unsigned long long g_ft_from_calls; g_ft_from_calls++; }
  return ft_from_term_rec(a, t, scratch);
}

// --- Boundary: AtpFt -> Term -----------------------------------------
//
// Mirror of the above: a var cell becomes term_new_fvr; a 0-arity
// CTR cell becomes term_new_ctr with no children; an arity>0 CTR
// recurses over its children (walked via cell->end->next sibling
// jumps).  ATPFT_NUM_MARKER round-trips back to a TAG_NUM via
// term_new(0, TAG_NUM, dtype, raw).

static Term ft_to_term_rec(const AtpFtCell *x);

static Term ft_to_term_rec(const AtpFtCell *x) {
  extern unsigned char g_atp_phase_detail;
  if (g_atp_phase_detail) { extern unsigned long long g_ft_to_nodes; g_ft_to_nodes++; }
  if (ft_is_var(x)) {
    return term_new_fvr(ft_var_id(x));
  }
  u32 sym   = ft_ctr_sym(x);
  u16 arity = x->arity;
  if (sym == ATPFT_NUM_MARKER && arity == 2u) {
    // Recover (dtype, raw) from the two child constant cells.
    const AtpFtCell *dt_cell  = x->next;
    const AtpFtCell *raw_cell = dt_cell->end->next;
    u32 dtype = ft_ctr_sym(dt_cell);
    u32 raw   = ft_ctr_sym(raw_cell);
    return term_new(0, TAG_NUM, dtype, raw);
  }
  if (arity == 0u) {
    return term_new_ctr(sym, NULL, 0u);
  }
  enum { KIDS_STACK = 16 };
  Term  kids_stack[KIDS_STACK];
  Term *kids      = kids_stack;
  Term *kids_heap = NULL;
  if (arity > KIDS_STACK) {
    kids_heap = (Term *)malloc(sizeof(Term) * (size_t)arity);
    if (kids_heap == NULL) { fprintf(stderr, "ft_to_term: kids OOM\n"); exit(1); }
    kids = kids_heap;
  }
  const AtpFtCell *child = x->next;
  for (u16 i = 0; i < arity; i++) {
    kids[i] = ft_to_term_rec(child);
    child = child->end->next;
  }
  Term out = term_new_ctr(sym, kids, (u32)arity);
  free(kids_heap);
  return out;
}

Term ft_to_term(const AtpFtCell *x) {
  extern unsigned char g_atp_phase_detail;
  if (g_atp_phase_detail) { extern unsigned long long g_ft_to_calls; g_ft_to_calls++; }
  return ft_to_term_rec(x);
}

// --- Structural equality ----------------------------------------------
//
// Recursive: same shape, same sym at every cell (which also captures
// var-id identity, since the high-bit-set sym carries the id).
// Walks the AtpFt sibling chain via cell->end->next at each level.

int ft_eq(const AtpFtCell *x, const AtpFtCell *y) {
  if (x == y) return 1;
  if (x == NULL || y == NULL) return 0;
  // Iterative lockstep walk of the two flatterm preorders.  Two terms
  // are structurally equal iff their preorders agree cell-for-cell on
  // (sym, arity): arity fully determines the shape, so matching every
  // aligned (sym, arity) pair forces identical trees of identical
  // length -- y exhausts its subterm exactly when x reaches x_end.
  // Collapses the recursion to a single linear scan (no call frame).
  const AtpFtCell *x_end = (x->end != NULL) ? x->end->next : NULL;
  const AtpFtCell *px = x, *py = y;
  while (px != x_end) {
    if (py == NULL) return 0;
    if (px->sym != py->sym || px->arity != py->arity) return 0;
    px = px->next;
    py = py->next;
  }
  return 1;
}

// --- Structural hash --------------------------------------------------
//
// Mirrors atp_term_struct_hash (_.c:326-342) exactly so a Term and
// its ft_from_term image produce the SAME u64.  This is the bridge
// that lets Stage 9 re-key atp_unf_memo / atp_norm_memo to ft_hash
// without invalidating any in-flight entries.
//
// Recurrence summary:
//   CTR(sym, c0..c_{n-1})
//     h := FNV_OFFSET ^ (sym * FNV_PRIME)
//     for each child:  h := (h ^ rec(child)) * FNV_PRIME
//     return h ^ (n * MIX)
//   FVR(id):  return FVR_TAG ^ (id * FNV_PRIME)
//   (other tags fall back to TAG_TAG ^ (tag * FNV_PRIME), but the
//    ATP layer only ever stores CTR + FVR + the NUM-marker
//    encoding above, which is a CTR in AtpFt-land.)
//
// For the FVR case in AtpFt-land, the "id" is the bare var id (low
// 31 bits of sym), NOT the sym with the high bit set -- this matches
// what atp_term_struct_hash sees in Term-land (term_ext(FVR) = bare
// var id, no flag bit).
#define ATPFT_HASH_FNV_OFFSET   0xcbf29ce484222325ull
#define ATPFT_HASH_FNV_PRIME    0x100000001b3ull
#define ATPFT_HASH_ARITY_MIX    0x9e3779b97f4a7c15ull
#define ATPFT_HASH_FVR_TAG      0xfacefacefaceull

u64 ft_hash(const AtpFtCell *x) {
  if (x == NULL) {
    // Match atp_term_struct_hash's default branch for "no real
    // term": 0xdeadbeefcafebabe ^ (0 * FNV_PRIME) == 0xdeadbeef..be.
    return 0xdeadbeefcafebabeull;
  }
  if (ft_is_var(x)) {
    return ATPFT_HASH_FVR_TAG
         ^ ((u64)ft_var_id(x) * ATPFT_HASH_FNV_PRIME);
  }
  u32 sym   = ft_ctr_sym(x);
  u16 arity = x->arity;
  u64 h = ATPFT_HASH_FNV_OFFSET ^ ((u64)sym * ATPFT_HASH_FNV_PRIME);
  const AtpFtCell *child = x->next;
  for (u16 i = 0; i < arity; i++) {
    h ^= ft_hash(child);
    h *= ATPFT_HASH_FNV_PRIME;
    child = child->end->next;
  }
  return h ^ ((u64)arity * ATPFT_HASH_ARITY_MIX);
}

// --- Pretty print -----------------------------------------------------
//
// Byte-identical mirror of atp_pretty_term (_.c:7028-7058): "x_<id>"
// for FVR, "C<lab>" or "C<lab>(c0, c1, ...)" for CTR, "#<val>" for
// the NUM marker, with the ", " (comma+space) separator on CTR
// children.  Buffer-aware, truncates silently.

static u32 atp_pretty_ft_rec(const AtpFtCell *x, char *buf, u32 cap);

static u32 atp_pretty_ft_ctr(const AtpFtCell *x, char *buf, u32 cap) {
  if (cap <= 1u) return 0u;
  u32 sym = ft_ctr_sym(x);
  u16 n   = x->arity;
  // Detect the NUM-marker shape so the pretty print round-trips back
  // to the "#<val>" format atp_pretty_term emits for TAG_NUM.
  if (sym == ATPFT_NUM_MARKER && n == 2u) {
    const AtpFtCell *raw_cell = x->next->end->next;
    u32 raw = ft_ctr_sym(raw_cell);
    int n_w = snprintf(buf, cap, "#%u", raw);
    if (n_w < 0) return 0u;
    u32 w = (u32)n_w;
    return w >= cap ? cap - 1u : w;
  }
  int n_w = snprintf(buf, cap, "C%u", sym);
  if (n_w < 0) return 0u;
  u32 w = (u32)n_w;
  if (w >= cap) return cap - 1u;
  if (n == 0u) return w;
  if (w + 1u >= cap) return w;
  w += (u32)snprintf(buf + w, cap - w, "(");
  const AtpFtCell *child = x->next;
  for (u16 i = 0; i < n; i++) {
    if (w + 2u >= cap) break;
    if (i > 0u) w += (u32)snprintf(buf + w, cap - w, ", ");
    if (w >= cap) return cap - 1u;
    w += atp_pretty_ft_rec(child, buf + w, cap - w);
    if (w >= cap) return cap - 1u;
    child = child->end->next;
  }
  if (w + 1u < cap) w += (u32)snprintf(buf + w, cap - w, ")");
  return w;
}

static u32 atp_pretty_ft_rec(const AtpFtCell *x, char *buf, u32 cap) {
  if (cap == 0u) return 0u;
  if (x == NULL) return (u32)snprintf(buf, cap, "?T0");
  if (ft_is_var(x)) {
    return (u32)snprintf(buf, cap, "x_%u", ft_var_id(x));
  }
  return atp_pretty_ft_ctr(x, buf, cap);
}

u32 atp_pretty_ft(const AtpFtCell *x, char *buf, u32 cap) {
  return atp_pretty_ft_rec(x, buf, cap);
}

#endif // !ATPFT_FT_C_INCLUDED
#endif // THVM_ATPFT_CONVERT
