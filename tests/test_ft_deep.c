// test_ft_deep.c - the ft.c walks on deep terms, pinned to the
// recursion they replaced.
//
// Every walk in src/atp/ft.c keeps an explicit work stack, so a term
// 100,000 levels deep converts Term -> span -> Term (and hashes,
// compares, matches, copies, instantiates and prints) where the
// recursive walks overflow the 8 MB C stack.  Those recursive walks are
// kept below, verbatim, as references: on a few thousand random terms
// the iterative walks must return the same values and allocate the
// same arena cells and heap words in the same order (the WM parity
// pins depend on the heap layout).

#include "../src/thvm.c"

#ifndef THVM_ATPFT_ALLOC
#define THVM_ATPFT_ALLOC 1
#endif
#ifndef THVM_ATPFT_CONVERT
#define THVM_ATPFT_CONVERT 1
#endif

#include "../src/atp/ft.h"
#include "../src/atp/ft_alloc.c"
#include "../src/atp/ft.c"

#include "test.h"

// ---- Reference: the recursive walks, as ft.c had them -----------------

static AtpFtCell *ref_ft_from_term_rec(AtpFt *a, Term t, int scratch) {
  extern unsigned char g_atp_phase_detail;
  if (g_atp_phase_detail) { extern u64 g_ft_from_nodes; g_ft_from_nodes++; }
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
      kids[i] = ref_ft_from_term_rec(a, term_ctr_at(t, i), scratch);
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

static AtpFtCell *ref_ft_from_term_pre(AtpFt *a, Term t, int scratch) {
  extern unsigned char g_atp_phase_detail;
  if (g_atp_phase_detail) { extern u64 g_ft_from_nodes; g_ft_from_nodes++; }
  u8 tag = term_tag(t);
  if (tag == TAG_FVR) {
    AtpFtCell *c = ft_alloc_one(a, scratch);
    c->next  = NULL;
    c->end   = c;
    c->sym   = WF_VAR_BIT | (term_ext(t) & ~WF_VAR_BIT);
    c->arity = 0;
    c->flags = 0;
    ft_raw_check(c);
    return c;
  }
  if (tag == TAG_CTR) {
    u32 label = term_ext(t);
    u32 n     = term_ctr_n(t);
    AtpFtCell *c = ft_alloc_one(a, scratch);   // PARENT FIRST (pre-order)
    c->sym   = label & ~WF_VAR_BIT;
    c->arity = (u16)n;
    c->flags = ATPFT_FLAG_GROUND;              // OR-folded below
    if (n == 0u) { c->next = NULL; c->end = c; ft_raw_check(c); return c; }
    AtpFtCell *prev_end = NULL;
    for (u32 i = 0; i < n; i++) {
      AtpFtCell *kid = ref_ft_from_term_pre(a, term_ctr_at(t, i), scratch);
      if (i == 0u) c->next = kid;              // kid is the physically next cell
      else         prev_end->next = kid;
      if ((kid->flags & ATPFT_FLAG_GROUND) == 0u) c->flags = 0;
      prev_end = kid->end;
    }
    c->end         = prev_end;
    c->end->next   = NULL;
    ft_raw_check(c);
    return c;
  }
  if (tag == TAG_NUM) {
    // Marker parent, then the two field constants, pre-order.
    u32 dtype = term_ext(t);
    u32 raw   = (u32)term_val(t);
    AtpFtCell *c = ft_alloc_one(a, scratch);
    c->sym   = ATPFT_NUM_MARKER;
    c->arity = 2u;
    c->flags = ATPFT_FLAG_GROUND;
    AtpFtCell *dt_cell = ft_alloc_one(a, scratch);
    dt_cell->next = NULL; dt_cell->end = dt_cell;
    dt_cell->sym = dtype & ~WF_VAR_BIT; dt_cell->arity = 0;
    dt_cell->flags = ATPFT_FLAG_GROUND;
    ft_raw_check(dt_cell);
    AtpFtCell *raw_cell = ft_alloc_one(a, scratch);
    raw_cell->next = NULL; raw_cell->end = raw_cell;
    raw_cell->sym = raw & ~WF_VAR_BIT; raw_cell->arity = 0;
    raw_cell->flags = ATPFT_FLAG_GROUND;
    ft_raw_check(raw_cell);
    c->next        = dt_cell;
    dt_cell->next  = raw_cell;
    c->end         = raw_cell;
    raw_cell->next = NULL;
    ft_raw_check(c);
    return c;
  }
  AtpFtCell *c = ft_alloc_one(a, scratch);
  c->next  = NULL;
  c->end   = c;
  c->sym   = 0x20000000u | (u32)tag;
  c->arity = 0;
  c->flags = ATPFT_FLAG_GROUND;
  ft_raw_check(c);
  return c;
}

static AtpFtCell *ref_ft_clone_span(AtpFt *a, const AtpFtCell *x, int scratch) {
  if (ft_is_var(x)) {
    return ftnew_var(a, ft_var_id(x), scratch);
  }
  u32 sym   = ft_ctr_sym(x);
  u16 arity = x->arity;
  if (arity == 0u) return ftnew_const(a, sym, scratch);
  enum { KIDS_STACK = 16 };
  AtpFtCell *kids_stack[KIDS_STACK];
  AtpFtCell **kids      = kids_stack;
  AtpFtCell **kids_heap = NULL;
  if (arity > KIDS_STACK) {
    kids_heap = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)arity);
    if (kids_heap == NULL) { fprintf(stderr, "ref_ft_clone_span: kids OOM\n"); exit(1); }
    kids = kids_heap;
  }
  const AtpFtCell *child = x->next;
  for (u16 i = 0; i < arity; i++) {
    kids[i] = ref_ft_clone_span(a, child, scratch);
    child = child->end->next;
  }
  AtpFtCell *c = ftnew_ctr(a, sym, arity, kids, scratch);
  free(kids_heap);
  return c;
}

static AtpFtCell *ref_ft_rename_span(AtpFt *a, const AtpFtCell *x, u32 offset,
                          int scratch) {
  if (ft_is_var(x)) {
    return ftnew_var(a, ft_var_id(x) + offset, scratch);
  }
  u32 sym   = ft_ctr_sym(x);
  u16 arity = x->arity;
  if (arity == 0u) return ftnew_const(a, sym, scratch);
  enum { KIDS_STACK = 16 };
  AtpFtCell *kids_stack[KIDS_STACK];
  AtpFtCell **kids      = kids_stack;
  AtpFtCell **kids_heap = NULL;
  if (arity > KIDS_STACK) {
    kids_heap = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * (size_t)arity);
    if (kids_heap == NULL) { fprintf(stderr, "ref_ft_rename_span: kids OOM\n"); exit(1); }
    kids = kids_heap;
  }
  const AtpFtCell *child = x->next;
  for (u16 i = 0; i < arity; i++) {
    kids[i] = ref_ft_rename_span(a, child, offset, scratch);
    child = child->end->next;
  }
  AtpFtCell *c = ftnew_ctr(a, sym, arity, kids, scratch);
  free(kids_heap);
  return c;
}

static Term ref_ft_to_term_rec(const AtpFtCell *x) {
  extern unsigned char g_atp_phase_detail;
  if (g_atp_phase_detail) { extern u64 g_ft_to_nodes; g_ft_to_nodes++; }
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
    kids[i] = ref_ft_to_term_rec(child);
    child = child->end->next;
  }
  Term out = term_new_ctr(sym, kids, (u32)arity);
  free(kids_heap);
  return out;
}

static int ref_ft_eq_term(const AtpFtCell *c, Term t) {
  u8 tag = term_tag(t);
  if (tag == TAG_FVR) {
    return ft_is_var(c) && ft_var_id(c) == (term_ext(t) & ~WF_VAR_BIT);
  }
  if (ft_is_var(c)) return 0;
  if (tag == TAG_CTR) {
    if (ft_ctr_sym(c) != (term_ext(t) & ~WF_VAR_BIT)) return 0;
    u32 n = term_ctr_n(t);
    if ((u32)c->arity != n) return 0;
    const AtpFtCell *child = c->next;
    for (u32 i = 0; i < n; i++) {
      if (!ref_ft_eq_term(child, term_ctr_at(t, i))) return 0;
      child = child->end->next;
    }
    return 1;
  }
  if (tag == TAG_NUM) {
    if (ft_ctr_sym(c) != ATPFT_NUM_MARKER || c->arity != 2u) return 0;
    const AtpFtCell *dt_cell  = c->next;
    const AtpFtCell *raw_cell = dt_cell->end->next;
    return ft_ctr_sym(dt_cell)  == (term_ext(t) & ~WF_VAR_BIT)
        && ft_ctr_sym(raw_cell) == (((u32)term_val(t)) & ~WF_VAR_BIT);
  }
  return 0;
}

// Span-pattern one-way match against a HEAP-TERM subject -- the exact
// sibling of thvm_match with the pattern read off mirror cells:
// pattern var binds the subject SUBTREE (repeated var re-checks with
// kbo_eq), constructor cells compare label + arity + children, and a
// TAG_NUM-marker pattern never matches (thvm_match's default arm).
// Bindings land in the same RewriteSubst (Term-typed), so the caller's
// substitution application is unchanged.
static int ref_ft_match_term(const AtpFtCell *pat, Term term, RewriteSubst *subst) {
  if (ft_is_var(pat)) {
    u32 id = ft_var_id(pat);
    if (id >= REWRITE_MAX_VAR) return 0;
    if (subst->bindings[id] == 0) {
      subst->bindings[id] = term;
      return 1;
    }
    return kbo_eq(subst->bindings[id], term);
  }
  u32 sym = ft_ctr_sym(pat);
  if (sym == ATPFT_NUM_MARKER) return 0;   // thvm_match default: no match
  if (term_tag(term) != TAG_CTR) return 0;
  if (sym != term_ext(term)) return 0;
  u32 n = (u32)pat->arity;
  if (term_ctr_n(term) != n) return 0;
  const AtpFtCell *kid = pat->next;
  for (u32 i = 0; i < n; i++) {
    if (!ref_ft_match_term(kid, term_ctr_at(term, i), subst)) return 0;
    kid = kid->end->next;
  }
  return 1;
}

// Instantiate a span TEMPLATE under a Term-typed substitution,
// producing a fresh heap Term -- the exact sibling of
// thvm_subst_apply(template_term, subst) minus the hash-cons reuse of
// unchanged subtrees (there is no original Term to reuse; downstream
// reads are content-based).  Unbound vars pass through as fresh FVRs;
// the TAG_NUM marker round-trips to its NUM.
static Term ref_ft_subst_apply_term(const AtpFtCell *tmpl, const RewriteSubst *subst) {
  if (ft_is_var(tmpl)) {
    u32 id = ft_var_id(tmpl);
    if (id < REWRITE_MAX_VAR && subst->bindings[id] != 0) {
      return subst->bindings[id];
    }
    return term_new_fvr(id);
  }
  u32 sym = ft_ctr_sym(tmpl);
  u32 n   = (u32)tmpl->arity;
  if (sym == ATPFT_NUM_MARKER && n == 2u) {
    const AtpFtCell *dt_cell  = tmpl->next;
    const AtpFtCell *raw_cell = dt_cell->end->next;
    return term_new(0, TAG_NUM, ft_ctr_sym(dt_cell), ft_ctr_sym(raw_cell));
  }
  if (n > REWRITE_MAX_ARITY) return 0;
  Term children[REWRITE_MAX_ARITY];
  const AtpFtCell *kid = tmpl->next;
  for (u32 i = 0; i < n; i++) {
    children[i] = ref_ft_subst_apply_term(kid, subst);
    kid = kid->end->next;
  }
  return term_new_ctr(sym, children, n);
}

static u64 ref_ft_hash(const AtpFtCell *x) {
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
    h ^= ref_ft_hash(child);
    h *= ATPFT_HASH_FNV_PRIME;
    child = child->end->next;
  }
  return h ^ ((u64)arity * ATPFT_HASH_ARITY_MIX);
}

static u32 ref_atp_pretty_ft_rec(const AtpFtCell *x, char *buf, u32 cap);

static u32 ref_atp_pretty_ft_ctr(const AtpFtCell *x, char *buf, u32 cap) {
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
    w += ref_atp_pretty_ft_rec(child, buf + w, cap - w);
    if (w >= cap) return cap - 1u;
    child = child->end->next;
  }
  if (w + 1u < cap) w += (u32)snprintf(buf + w, cap - w, ")");
  return w;
}

static u32 ref_atp_pretty_ft_rec(const AtpFtCell *x, char *buf, u32 cap) {
  if (cap == 0u) return 0u;
  if (x == NULL) return (u32)snprintf(buf, cap, "?T0");
  if (ft_is_var(x)) {
    return (u32)snprintf(buf, cap, "x_%u", ft_var_id(x));
  }
  return ref_atp_pretty_ft_ctr(x, buf, cap);
}

static u32 ref_atp_pretty_ft(const AtpFtCell *x, char *buf, u32 cap) {
  return ref_atp_pretty_ft_rec(x, buf, cap);
}

// ---- Random terms -------------------------------------------------------

static u32 rng32(u32 *seed) {
  u32 x = *seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *seed = x ? x : 0xdeadbeefu;
  return *seed;
}

// Leaves cover every arm of the converters: variables (some past
// REWRITE_MAX_VAR = 64), constants, NUM atoms (one with the var bit set
// in its raw field, which the encoding masks) and the odd ERA, the
// converter's fallback arm.  `g_plain` keeps to variables and
// constants, so instances of a pattern really match it.
static u8 g_plain = 0u;
static Term mk_leaf(u32 *seed) {
  u32 r = rng32(seed);
  if (!g_plain && (r & 255u) == 0u) return term_new(0, TAG_ERA, 0, 0);
  switch ((r >> 6) % (g_plain ? 7u : 10u)) {
    case 0: case 1: case 2: case 3:
      return term_new_fvr((r >> 10) % 10u);
    case 4: case 5: case 6:
      return term_new_ctr((r >> 10) % 12u, NULL, 0u);
    case 7:
      return term_new_fvr(60u + (r >> 10) % 8u);
    case 8:
      return term_new(0, TAG_NUM, (r >> 10) & 3u, (r >> 12) & 0xffu);
    default:
      return term_new(0, TAG_NUM, 2u, 0x80000005u);
  }
}

// Depth-bounded, at most `*budget` nodes.  Arity mostly 1..3; a node
// next to the leaves is sometimes 17..20 wide (past the 16-slot kid
// buffers and REWRITE_MAX_ARITY).
static Term mk_term(u32 *seed, u32 depth, u32 *budget) {
  u32 r = rng32(seed);
  if (depth == 0u || *budget == 0u || (r & 7u) < 2u) {
    if (*budget > 0u) (*budget)--;
    return mk_leaf(seed);
  }
  (*budget)--;
  u32 n = 1u + ((r >> 9) % 3u);
  if (depth == 1u && ((r >> 3) & 31u) == 0u) n = 17u + ((r >> 12) & 3u);
  Term kids[20];
  for (u32 i = 0; i < n; i++) kids[i] = mk_term(seed, depth - 1u, budget);
  return term_new_ctr((r >> 14) % 12u, kids, n);
}

static Term mk_random(u32 *seed) {
  u32 budget = 2000u;
  return mk_term(seed, 6u, &budget);
}

// A copy of `t` with the subterm at one random position replaced.
static Term mk_mutant(u32 *seed, Term t) {
  u32 r = rng32(seed);
  u32 n = term_tag(t) == TAG_CTR ? term_ctr_n(t) : 0u;
  if (n == 0u || (r % 3u) == 0u) {
    Term leaf = mk_leaf(seed);
    if (leaf == t) leaf = term_new_ctr(11u, NULL, 0u);
    return leaf;
  }
  Term kids[20];
  for (u32 i = 0; i < n; i++) kids[i] = term_ctr_at(t, i);
  u32 k = (r >> 4) % n;
  kids[k] = mk_mutant(seed, kids[k]);
  return term_new_ctr(term_ext(t), kids, n);
}

// ---- Layout comparisons -------------------------------------------------

static long cell_off(const AtpFtCell *c, const AtpFtCell *base) {
  return c == NULL ? -1L : (long)((const char *)c - (const char *)base);
}

// Same preorder cells (sym / arity / flags) at the same offsets from
// each arena's first cell, with the same next / end links: the two
// walks built the same span AND allocated its cells in the same order.
static int span_layout_same(const AtpFtCell *x, const AtpFtCell *xb,
                            const AtpFtCell *y, const AtpFtCell *yb) {
  const AtpFtCell *px = x;
  const AtpFtCell *py = y;
  for (;;) {
    if (px->sym != py->sym || px->arity != py->arity || px->flags != py->flags) return 0;
    if (cell_off(px, xb) != cell_off(py, yb)) return 0;
    if (cell_off(px->end, xb) != cell_off(py->end, yb)) return 0;
    if (cell_off(px->next, xb) != cell_off(py->next, yb)) return 0;
    if (px == x->end) return py == y->end;
    if (py == y->end) return 0;
    px = px->next;
    py = py->next;
  }
}

// A CTR word pointing into [lo, hi) moved to the same offset from `to`.
static Term reloc(Term w, u64 lo, u64 hi, u64 to) {
  if (term_tag(w) == TAG_CTR && term_val(w) >= lo && term_val(w) < hi) {
    return (w & ~VAL_MASK) | ((term_val(w) - lo + to) & VAL_MASK);
  }
  return w;
}

// Heap words [a0, a1) and [b0, b1) are the same allocation sequence.
static int heap_regions_same(u64 a0, u64 a1, u64 b0, u64 b1) {
  if (a1 - a0 != b1 - b0) return 0;
  for (u64 k = 0; k < a1 - a0; k++) {
    if (reloc(heap_read(a0 + k), a0, a1, b0) != heap_read(b0 + k)) return 0;
  }
  return 1;
}

// ---- Iterative checkers for the deep terms -------------------------------

static int term_eq_iter(Term a, Term b) {
  u32   cap = 1024u, n = 0u;
  Term *st  = (Term *)malloc(2u * cap * sizeof(Term));
  int   eq  = 1;
  st[n++] = a;
  st[n++] = b;
  while (n > 0u && eq) {
    Term y = st[--n];
    Term x = st[--n];
    if (term_tag(x) != term_tag(y) || term_ext(x) != term_ext(y)) { eq = 0; break; }
    if (term_tag(x) != TAG_CTR) {
      if (term_val(x) != term_val(y) && term_tag(x) != TAG_FVR) eq = 0;
      continue;
    }
    u32 k = term_ctr_n(x);
    if (k != term_ctr_n(y)) { eq = 0; break; }
    if (n + 2u * k > 2u * cap) {
      while (n + 2u * k > 2u * cap) cap *= 2u;
      st = (Term *)realloc(st, 2u * cap * sizeof(Term));
    }
    for (u32 i = 0; i < k; i++) {
      st[n++] = term_ctr_at(x, i);
      st[n++] = term_ctr_at(y, i);
    }
  }
  free(st);
  return eq;
}

// ft_hash of the span ft_from_term builds for `t`, from the Term side:
// atp_term_struct_hash's recurrence, with a NUM atom read as the
// marker CTR over its two field constants.
static u64 term_hash_iter(Term t) {
  typedef struct { Term t; u32 i, n; u64 h; } Fr;
  u32  cap = 1024u, n = 0u;
  Fr  *st  = (Fr *)malloc(cap * sizeof(Fr));
  u64  v   = 0u;
  for (;;) {
    u8 leaf = 1u;
    if (term_tag(t) == TAG_FVR) {
      v = 0xfacefacefaceull ^ ((u64)term_ext(t) * 0x100000001b3ull);
    } else if (term_tag(t) == TAG_CTR) {
      u64 h = 0xcbf29ce484222325ull ^ ((u64)term_ext(t) * 0x100000001b3ull);
      u32 k = term_ctr_n(t);
      if (k == 0u) {
        v = h;
      } else {
        if (n == cap) { cap *= 2u; st = (Fr *)realloc(st, cap * sizeof(Fr)); }
        st[n].t = t; st[n].i = 0u; st[n].n = k; st[n].h = h;
        n++;
        leaf = 0u;
      }
    } else if (term_tag(t) == TAG_NUM) {
      u64 h  = 0xcbf29ce484222325ull ^ ((u64)ATPFT_NUM_MARKER * 0x100000001b3ull);
      u64 dt = 0xcbf29ce484222325ull ^ ((u64)(term_ext(t) & ~WF_VAR_BIT) * 0x100000001b3ull);
      u64 rw = 0xcbf29ce484222325ull ^ ((u64)((u32)term_val(t) & ~WF_VAR_BIT) * 0x100000001b3ull);
      h = (h ^ dt) * 0x100000001b3ull;
      h = (h ^ rw) * 0x100000001b3ull;
      v = h ^ (2u * 0x9e3779b97f4a7c15ull);
    } else {
      v = 0xdeadbeefcafebabeull ^ ((u64)term_tag(t) * 0x100000001b3ull);
    }
    if (leaf) {
      while (n > 0u) {
        Fr *f = &st[n - 1u];
        f->h = (f->h ^ v) * 0x100000001b3ull;
        f->i++;
        if (f->i < f->n) break;
        v = f->h ^ ((u64)f->n * 0x9e3779b97f4a7c15ull);
        n--;
      }
      if (n == 0u) break;
    }
    t = term_ctr_at(st[n - 1u].t, st[n - 1u].i);
  }
  free(st);
  return v;
}

// The text atp_pretty_ft prints for a span of `t`, when it fits.
static u32 pretty_iter(Term t, char *buf, u32 cap) {
  typedef struct { Term t; u32 i, n; } Fr;
  u32  fcap = 1024u, n = 0u, w = 0u;
  Fr  *st   = (Fr *)malloc(fcap * sizeof(Fr));
  for (;;) {
    if (term_tag(t) == TAG_FVR) {
      w += (u32)snprintf(buf + w, cap - w, "x_%u", term_ext(t));
    } else if (term_tag(t) == TAG_NUM) {
      w += (u32)snprintf(buf + w, cap - w, "#%u", (u32)term_val(t) & ~WF_VAR_BIT);
    } else if (term_ctr_n(t) == 0u) {
      w += (u32)snprintf(buf + w, cap - w, "C%u", term_ext(t));
    } else {
      w += (u32)snprintf(buf + w, cap - w, "C%u(", term_ext(t));
      if (n == fcap) { fcap *= 2u; st = (Fr *)realloc(st, fcap * sizeof(Fr)); }
      st[n].t = t; st[n].i = 0u; st[n].n = term_ctr_n(t);
      n++;
      t = term_ctr_at(t, 0u);
      continue;
    }
    while (n > 0u && st[n - 1u].i + 1u == st[n - 1u].n) {
      w += (u32)snprintf(buf + w, cap - w, ")");
      n--;
    }
    if (n == 0u) break;
    st[n - 1u].i++;
    w += (u32)snprintf(buf + w, cap - w, ", ");
    t = term_ctr_at(st[n - 1u].t, st[n - 1u].i);
  }
  free(st);
  return w;
}

// A spine `depth` binary nodes deep: node k is f(node k-1, leaf k) for
// even k and f(leaf k, node k-1) for odd k, so the deep path runs
// through both argument positions.  Leaves cycle through a variable
// x_{k mod 5} (or vals[k mod 5] when `vals` is given), a constant and a
// NUM atom.
static Term mk_deep(u32 depth, u32 lab, Term bottom, const Term *vals) {
  Term t = bottom;
  for (u32 k = 0; k < depth; k++) {
    Term leaf;
    switch (k % 3u) {
      case 0:  leaf = vals ? vals[k % 5u] : term_new_fvr(k % 5u); break;
      case 1:  leaf = term_new_ctr(5u, NULL, 0u); break;
      default: leaf = term_new(0, TAG_NUM, 1u, k); break;
    }
    Term kids[2];
    if (k & 1u) { kids[0] = leaf; kids[1] = t; }
    else        { kids[0] = t;    kids[1] = leaf; }
    t = term_new_ctr(lab + (k & 1u), kids, 2u);
  }
  return t;
}

static u64 span_cells(const AtpFtCell *x) {
  u64 n = 1u;
  for (const AtpFtCell *c = x; c != x->end; c = c->next) n++;
  return n;
}

// Cells ft_from_term spends on `t`: one per variable / constructor
// node, three per NUM atom (marker + two field constants).
static u64 term_cells_iter(Term t) {
  u32   cap = 1024u, n = 0u;
  Term *st  = (Term *)malloc(cap * sizeof(Term));
  u64   cells = 0u;
  st[n++] = t;
  while (n > 0u) {
    Term x = st[--n];
    if (term_tag(x) == TAG_NUM) { cells += 3u; continue; }
    cells++;
    if (term_tag(x) != TAG_CTR) continue;
    u32 k = term_ctr_n(x);
    if (n + k > cap) {
      while (n + k > cap) cap *= 2u;
      st = (Term *)realloc(st, cap * sizeof(Term));
    }
    for (u32 i = 0; i < k; i++) st[n++] = term_ctr_at(x, i);
  }
  free(st);
  return cells;
}

int main(void) {
  thvm_init();
  extern u8  g_atp_phase_detail;
  extern u64 g_ft_from_nodes;
  extern u64 g_ft_to_nodes;

  enum { N_PIN = 3000 };
  u32 seed = 0x2545f491u;
  Term *src = (Term *)malloc(sizeof(Term) * N_PIN);
  for (u32 i = 0; i < N_PIN; i++) src[i] = mk_random(&seed);

  // ---- Pins: the iterative walks equal the recursive ones ----------------
  g_atp_phase_detail = 1u;   // also pin the per-node counters

  TEST_BEGIN("ft_deep/pin-from-term-up");
  {
    u32 ok = 0;
    for (u32 i = 0; i < N_PIN; i++) {
      for (int scratch = 0; scratch < 2; scratch++) {
        AtpFt ra, na;
        ft_init(&ra);
        ft_init(&na);
        AtpFtCell *rb = scratch ? ft_alloc_scratch(&ra) : ft_alloc_persistent(&ra);
        AtpFtCell *nb = scratch ? ft_alloc_scratch(&na) : ft_alloc_persistent(&na);
        u64 c0 = g_ft_from_nodes;
        AtpFtCell *rx = ref_ft_from_term_rec(&ra, src[i], scratch);
        u64 c1 = g_ft_from_nodes;
        AtpFtCell *nx = ft_from_term_up(&na, src[i], scratch);
        u64 c2 = g_ft_from_nodes;
        if (span_layout_same(rx, rb, nx, nb) && c1 - c0 == c2 - c1) ok++;
        ft_destroy(&ra);
        ft_destroy(&na);
      }
    }
    CHECK_EQ(ok, 2u * N_PIN);
  }

  TEST_BEGIN("ft_deep/pin-from-term-pre");
  {
    u32 ok = 0;
    for (u32 i = 0; i < N_PIN; i++) {
      for (int scratch = 0; scratch < 2; scratch++) {
        AtpFt ra, na;
        ft_init(&ra);
        ft_init(&na);
        AtpFtCell *rb = scratch ? ft_alloc_scratch(&ra) : ft_alloc_persistent(&ra);
        AtpFtCell *nb = scratch ? ft_alloc_scratch(&na) : ft_alloc_persistent(&na);
        u64 c0 = g_ft_from_nodes;
        AtpFtCell *rx = ref_ft_from_term_pre(&ra, src[i], scratch);
        u64 c1 = g_ft_from_nodes;
        AtpFtCell *nx = ft_from_term_pre(&na, src[i], scratch);
        u64 c2 = g_ft_from_nodes;
        if (span_layout_same(rx, rb, nx, nb) && c1 - c0 == c2 - c1) ok++;
        ft_destroy(&ra);
        ft_destroy(&na);
      }
    }
    CHECK_EQ(ok, 2u * N_PIN);
  }

  // Spans of the corpus for the span-side walks, in one arena, plus a
  // corpus of variable / constant patterns whose instances match.
  AtpFt sa;
  ft_init(&sa);
  AtpFtCell **sp = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * N_PIN);
  for (u32 i = 0; i < N_PIN; i++) sp[i] = ft_from_term(&sa, src[i], 0);
  Term       *psrc = (Term *)malloc(sizeof(Term) * N_PIN);
  AtpFtCell **psp  = (AtpFtCell **)malloc(sizeof(AtpFtCell *) * N_PIN);
  g_plain = 1u;
  for (u32 i = 0; i < N_PIN; i++) {
    psrc[i] = mk_random(&seed);
    psp[i]  = ft_from_term(&sa, psrc[i], 0);
  }
  g_plain = 0u;

  TEST_BEGIN("ft_deep/pin-clone-rename-span");
  {
    static const u32 offs[3] = { 0u, 3u, 70u };
    u32 ok = 0;
    for (u32 i = 0; i < N_PIN; i++) {
      for (u32 k = 0; k < 4u; k++) {
        AtpFt ra, na;
        ft_init(&ra);
        ft_init(&na);
        int scratch = (int)(i & 1u);
        AtpFtCell *rb = scratch ? ft_alloc_scratch(&ra) : ft_alloc_persistent(&ra);
        AtpFtCell *nb = scratch ? ft_alloc_scratch(&na) : ft_alloc_persistent(&na);
        AtpFtCell *rx = k == 3u ? ref_ft_clone_span(&ra, sp[i], scratch)
                                : ref_ft_rename_span(&ra, sp[i], offs[k], scratch);
        AtpFtCell *nx = k == 3u ? ft_clone_span(&na, sp[i], scratch)
                                : ft_rename_span(&na, sp[i], offs[k], scratch);
        if (span_layout_same(rx, rb, nx, nb)) ok++;
        ft_destroy(&ra);
        ft_destroy(&na);
      }
    }
    CHECK_EQ(ok, 4u * N_PIN);
  }

  TEST_BEGIN("ft_deep/pin-to-term-heap-layout");
  {
    u32 ok = 0;
    for (u32 i = 0; i < N_PIN; i++) {
      u64 h0 = HEAP_NEXT;
      u64 c0 = g_ft_to_nodes;
      Term r = ref_ft_to_term_rec(sp[i]);
      u64 h1 = HEAP_NEXT;
      u64 c1 = g_ft_to_nodes;
      Term n = ft_to_term(sp[i]);
      u64 h2 = HEAP_NEXT;
      u64 c2 = g_ft_to_nodes;
      if (heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n &&
          c1 - c0 == c2 - c1) {
        ok++;
      }
    }
    CHECK_EQ(ok, (u32)N_PIN);
  }

  g_atp_phase_detail = 0u;

  TEST_BEGIN("ft_deep/pin-eq-term");
  {
    u32 ok = 0, hits = 0;
    for (u32 i = 0; i < N_PIN; i++) {
      Term other[3] = { src[i], src[(i + 1u) % N_PIN], mk_mutant(&seed, src[i]) };
      for (u32 k = 0; k < 3u; k++) {
        int r = ref_ft_eq_term(sp[i], other[k]);
        int n = ft_eq_term(sp[i], other[k]);
        if (r == n) ok++;
        hits += (u32)(n != 0);
      }
    }
    CHECK_EQ(ok, 3u * N_PIN);
    CHECK(hits >= (u32)N_PIN / 2u);
  }

  TEST_BEGIN("ft_deep/pin-match-term");
  {
    u32 ok = 0, hits = 0;
    for (u32 c = 0; c < 2u; c++) {
      Term       *ts = c ? psrc : src;
      AtpFtCell **ss = c ? psp  : sp;
      for (u32 i = 0; i < N_PIN; i++) {
        RewriteSubst inst;
        memset(&inst, 0, sizeof inst);
        for (u32 v = 0; v < 10u; v++) {
          u32 budget = 30u;
          inst.bindings[v] = mk_term(&seed, 2u, &budget);
        }
        Term s_inst = thvm_subst_apply(ts[i], &inst);
        Term subj[3] = { s_inst, ts[(i + 7u) % N_PIN], mk_mutant(&seed, s_inst) };
        for (u32 k = 0; k < 3u; k++) {
          RewriteSubst rs, ns;
          memset(&rs, 0, sizeof rs);
          if (k == 0u && (i & 3u) == 0u) {
            rs.bindings[(i >> 2) % 10u] = inst.bindings[(i >> 2) % 10u];
          }
          ns = rs;
          int r = ref_ft_match_term(ss[i], subj[k], &rs);
          int n = ft_match_term(ss[i], subj[k], &ns);
          if (r == n && memcmp(&rs, &ns, sizeof rs) == 0) ok++;
          hits += (u32)(c == 1u && k == 0u && n != 0);
        }
      }
    }
    CHECK_EQ(ok, 6u * N_PIN);
    CHECK_EQ(hits, (u32)N_PIN);
  }

  TEST_BEGIN("ft_deep/pin-subst-apply-term-heap-layout");
  {
    u32 ok = 0;
    for (u32 i = 0; i < N_PIN; i++) {
      RewriteSubst sub;
      memset(&sub, 0, sizeof sub);
      for (u32 v = 0; v < 10u; v++) {
        if (rng32(&seed) & 1u) {
          u32 budget = 30u;
          sub.bindings[v] = mk_term(&seed, 2u, &budget);
        }
      }
      sub.bindings[62] = term_new_ctr(3u, NULL, 0u);
      u64 h0 = HEAP_NEXT;
      Term r = ref_ft_subst_apply_term(sp[i], &sub);
      u64 h1 = HEAP_NEXT;
      Term n = ft_subst_apply_term(sp[i], &sub);
      u64 h2 = HEAP_NEXT;
      if (heap_regions_same(h0, h1, h1, h2) && reloc(r, h0, h1, h1) == n) ok++;
    }
    CHECK_EQ(ok, (u32)N_PIN);
  }

  TEST_BEGIN("ft_deep/pin-hash");
  {
    u32 ok = 0;
    for (u32 i = 0; i < N_PIN; i++) {
      if (ref_ft_hash(sp[i]) == ft_hash(sp[i])) ok++;
    }
    CHECK_EQ(ok, (u32)N_PIN);
    CHECK_EQ(ref_ft_hash(NULL), ft_hash(NULL));
  }

  TEST_BEGIN("ft_deep/pin-pretty");
  {
    static const u32 caps[14] = { 0u, 1u, 2u, 3u, 4u, 5u, 7u, 8u, 13u, 16u,
                                  33u, 128u, 1024u, 65536u };
    enum { PBUF = 65536 + 8 };
    char *rbuf = (char *)malloc(PBUF);
    char *nbuf = (char *)malloc(PBUF);
    u32 ok = 0;
    for (u32 i = 0; i < N_PIN; i++) {
      for (u32 k = 0; k < 14u; k++) {
        memset(rbuf, 0x5a, (size_t)caps[k] + 8u);
        memset(nbuf, 0x5a, (size_t)caps[k] + 8u);
        u32 r = ref_atp_pretty_ft(sp[i], rbuf, caps[k]);
        u32 n = atp_pretty_ft(sp[i], nbuf, caps[k]);
        if (r == n && memcmp(rbuf, nbuf, (size_t)caps[k] + 8u) == 0) ok++;
      }
    }
    CHECK_EQ(ok, 14u * N_PIN);
    memset(rbuf, 0x5a, PBUF);
    memset(nbuf, 0x5a, PBUF);
    CHECK_EQ(ref_atp_pretty_ft(NULL, rbuf, 64u), atp_pretty_ft(NULL, nbuf, 64u));
    CHECK(memcmp(rbuf, nbuf, PBUF) == 0);
    free(rbuf);
    free(nbuf);
  }

  // ---- A 100,000-deep term, both ways ------------------------------------
  enum { DEEP = 100000 };
  Term vals[5];
  for (u32 v = 0; v < 5u; v++) {
    Term kid = term_new_ctr(9u, NULL, 0u);
    vals[v] = term_new_ctr(20u + v, &kid, v & 1u);
  }
  Term pat  = mk_deep(DEEP, 1u, term_new_fvr(4u), NULL);
  Term subj = mk_deep(DEEP, 1u, vals[4], vals);
  Term near = mk_deep(DEEP, 1u, vals[3], vals);   // differs only at the bottom
  AtpFt da;
  ft_init(&da);

  TEST_BEGIN("ft_deep/deep-from-to-term");
  {
    AtpFtCell *xs = ft_from_term(&da, subj, 0);
    CHECK(xs != NULL);
    CHECK_EQ(span_cells(xs), term_cells_iter(subj));
    CHECK(ft_eq_term(xs, subj));
    CHECK(!ft_eq_term(xs, near));
    Term back = ft_to_term(xs);
    CHECK(term_eq_iter(back, subj));
    CHECK(!term_eq_iter(back, near));
    CHECK_EQ(ft_hash(xs), term_hash_iter(subj));
  }

  TEST_BEGIN("ft_deep/deep-from-term-pre-and-scratch");
  {
    AtpFtCell *up  = ft_from_term_up(&da, subj, 0);
    AtpFtCell *pre = ft_from_term_pre(&da, subj, 0);
    CHECK(ft_eq(up, pre));
    CHECK_EQ(up->flags, pre->flags);
    // Pre-grow the scratch region so the conversion never moves it.
    AtpFt sc;
    ft_init(&sc);
    for (u32 k = 0; k < 4u * DEEP; k++) (void)ft_alloc_scratch(&sc);
    ft_scratch_reset(&sc);
    AtpFtCell *xs = ft_from_term(&sc, subj, 1);
    CHECK(ft_eq(xs, up));
    CHECK(term_eq_iter(ft_to_term(xs), subj));
    ft_destroy(&sc);
  }

  TEST_BEGIN("ft_deep/deep-clone-rename");
  {
    AtpFtCell *xs = ft_from_term(&da, subj, 0);
    AtpFtCell *cl = ft_clone_span(&da, xs, 0);
    CHECK(ft_eq(cl, xs));
    CHECK_EQ(ft_hash(cl), ft_hash(xs));
    AtpFtCell *xp = ft_from_term(&da, pat, 0);
    AtpFtCell *rn = ft_rename_span(&da, xp, 10u, 0);
    u64 same = 0, cells = 0;
    const AtpFtCell *p = xp, *q = rn;
    for (;;) {
      cells++;
      u32 want = ft_is_var(p) ? (WF_VAR_BIT | (ft_var_id(p) + 10u)) : p->sym;
      if (q->sym == want && q->arity == p->arity) same++;
      if (p == xp->end) break;
      p = p->next;
      q = q->next;
    }
    CHECK_EQ(same, cells);
    CHECK(q == rn->end);
  }

  TEST_BEGIN("ft_deep/deep-match-and-instantiate");
  {
    AtpFtCell *xp = ft_from_term(&da, pat, 0);
    RewriteSubst sub;
    memset(&sub, 0, sizeof sub);
    CHECK(ft_match_term(xp, subj, &sub));
    for (u32 v = 0; v < 5u; v++) CHECK(sub.bindings[v] == vals[v]);
    RewriteSubst miss;
    memset(&miss, 0, sizeof miss);
    CHECK(!ft_match_term(xp, near, &miss));
    Term inst = ft_subst_apply_term(xp, &sub);
    CHECK(term_eq_iter(inst, subj));
    RewriteSubst none;
    memset(&none, 0, sizeof none);
    CHECK(term_eq_iter(ft_subst_apply_term(xp, &none), pat));
  }

  TEST_BEGIN("ft_deep/deep-pretty");
  {
    u32 cap = 16u * DEEP + 64u;
    char *want = (char *)malloc(cap);
    char *got  = (char *)malloc(cap);
    u32 nw = pretty_iter(subj, want, cap);
    AtpFtCell *xs = ft_from_term(&da, subj, 0);
    u32 ng = atp_pretty_ft(xs, got, cap);
    CHECK(nw + 1u < cap);
    CHECK_EQ(ng, nw);
    CHECK(memcmp(want, got, (size_t)nw + 1u) == 0);
    // A short window stays within the reference recursion's reach.
    char *rb = (char *)malloc(4096u);
    char *nb = (char *)malloc(4096u);
    memset(rb, 0x5a, 4096u);
    memset(nb, 0x5a, 4096u);
    CHECK_EQ(ref_atp_pretty_ft(xs, rb, 4096u), atp_pretty_ft(xs, nb, 4096u));
    CHECK(memcmp(rb, nb, 4096u) == 0);
    free(rb);
    free(nb);
    free(want);
    free(got);
  }

  ft_destroy(&da);
  ft_destroy(&sa);
  free(psp);
  free(psrc);
  free(sp);
  free(src);
  thvm_free();
  TEST_REPORT();
}
