// ac.c -- AC (associative + commutative) symbol declarations and
// canonical-form flattening for the ATP engine.
//
// A symbol `f` is AC when the axiom set contains both
// `f(x, y) ≈ f(y, x)` (commutativity) and `f(f(x, y), z) ≈ f(x, f(y, z))`
// (associativity).  Detection of those shapes lives in
// `precedence.c` (`atp_analyze_axioms` fills `AtpSymProps.is_commutative`
// + `is_associative` per label).  This file lifts that per-label flag
// into an `AtpAcInfo` bit-mask and provides two operations that the
// hot loop will consume in later stages of the AC arc:
//
//   atp_ac_flatten(t, ac, out, *n, cap)
//     For an AC-symbol top `t = f(t1, t2)` with `f` AC, recursively
//     gather every leaf of the same-`f` subterm tree into `out[]` as a
//     pre-order multiset.  E.g. `f(a, f(b, c))` flattens to
//     `{a, b, c}`.  For non-AC `t` (or non-CTR) the output is a
//     singleton `{t}`.
//
//   atp_ac_canon(t, ac)
//     Produce a Term that's AC-canonical at every AC-symbol position
//     (children sorted by structural hash, descended into recursively).
//     Returns `t` unchanged when no AC subterm was found, so the
//     common case is allocation-free.
//
// AC-modulo equality and AC-hashing build on these in Stage 2.  AC-
// matching during rewrite and AC-superposition come in Stages 3-4.
//
// Gated on THVM_ATP_AC.

#ifdef THVM_ATP_AC

#include "../thvm.h"

// --- AcInfo: which CTR labels are AC -------------------------------
//
// 64-bit bit-mask, indexed by CTR label.  AC label range matches the
// existing perm_sub mask (`g_atp_perm_subsume_mask` in `_.c`) so the
// two filters can co-exist or be derived from the same source.
//
// Future stages may add a per-label arity check (AC only meaningful
// for binary symbols) and an "AC unit" companion (the identity
// element, if any -- e.g. 0 for `+`, 1 for `*`).

typedef struct AtpAcInfo {
  u64 ac_mask;          // bit i set iff CTR label i is AC
} AtpAcInfo;

// Compute the AcInfo from a previously-filled AtpSymProps array.
// `props[i]` contains the per-label analysis from
// `atp_analyze_axioms` (in precedence.c).  Caller is responsible for
// running that pass first.
//
// A label is AC iff its commutativity AND associativity axioms are
// both present.  Idempotent + unit elements are tracked separately
// (`is_idempotent`, `has_left_unit`, etc.) and don't gate AC status;
// they're refinements layered on top of AC matching in later stages.
static void atp_acinfo_compute(AtpAcInfo *out,
                               const AtpSymProps *props, u32 n_labels) {
  out->ac_mask = 0ull;
  u32 cap = n_labels < 64u ? n_labels : 64u;
  for (u32 i = 0; i < cap; i++) {
    if (props[i].is_commutative && props[i].is_associative) {
      out->ac_mask |= (1ull << i);
    }
  }
}

// Test whether a CTR label is AC under this AcInfo.
static inline u8 atp_ac_is_ac_label(const AtpAcInfo *ac, u32 label) {
  if (label >= 64u) return 0;
  return (u8)((ac->ac_mask >> label) & 1ull);
}

// Test whether a Term's top symbol is AC.  False for variables and
// for CTR labels outside the 64-bit mask range.
static u8 atp_ac_is_ac_top(const AtpAcInfo *ac, Term t) {
  if (term_tag(t) != TAG_CTR) return 0;
  return atp_ac_is_ac_label(ac, term_ext(t));
}

// Flatten an AC-symbol subterm into its leaf multiset.
//
// `t` is the input Term; `top_label` is the AC label being flattened.
// Walks all same-`top_label` descendants recursively; non-`top_label`
// children become leaves.  Pre-order traversal.
//
// Writes the leaf cells into `out[*n .. *n + leaf_count)`; advances
// `*n` accordingly.  Returns 0 on cap overflow (`*n` left at the
// last successful write); 1 on success.
//
// For `t` whose top is NOT `top_label` (or not a CTR at all), emits
// `t` as a single leaf -- this lets callers seed the walk with a
// non-AC top and still get a one-element "multiset".
static u8 atp_ac_flatten_under(Term t, u32 top_label,
                               Term *out, u32 *n, u32 cap) {
  if (term_tag(t) == TAG_CTR && term_ext(t) == top_label) {
    u32 n_kids = term_ctr_n(t);
    for (u32 i = 0; i < n_kids; i++) {
      if (!atp_ac_flatten_under(term_ctr_at(t, i), top_label,
                                out, n, cap)) {
        return 0;
      }
    }
    return 1;
  }
  if (*n >= cap) return 0;
  out[*n] = t;
  *n += 1u;
  return 1;
}

// Public entry: flatten under whatever top-label `t` has, if AC.
// For non-AC top, `out[0] = t`, `*n_out = 1`.  Returns 0 on cap.
static u8 atp_ac_flatten(Term t, const AtpAcInfo *ac,
                         Term *out, u32 *n_out, u32 cap) {
  *n_out = 0u;
  if (atp_ac_is_ac_top(ac, t)) {
    return atp_ac_flatten_under(t, term_ext(t), out, n_out, cap);
  }
  if (cap == 0u) return 0;
  out[0] = t;
  *n_out = 1u;
  return 1;
}

// --- AC canonical form (sorted multiset) ---------------------------
//
// `atp_ac_canon(t, ac)` returns a Term equivalent to `t` modulo AC,
// where every AC-symbol position has its children sorted by structural
// hash and re-built as a right-associative chain.  The chain is
// `f(c0, f(c1, f(c2, ... cN)))` with `c0 <= c1 <= ... <= cN` under
// `atp_term_struct_hash`.
//
// For a term with no AC subterm, returns `t` unchanged (no
// allocation).  For a term with AC subterms, the rebuild is bottom-up
// so canonicalization is idempotent: `canon(canon(t)) == canon(t)`.

// Forward decl from `_.c` -- the structural hash recurrence used as
// the canonical sort key.
static u64 atp_term_struct_hash(Term t);

// Cap on flattened leaves per AC position.  64 is the same per-rule
// variable cap (`ATP_RI_MAXVARS`); any AC node with more than 64
// leaves falls through to syntactic representation (canonical-form
// computation bails, returning `t` unchanged at that node).
#define ATP_AC_FLATTEN_CAP 64u

// Insertion-sort an array of Terms by `atp_term_struct_hash` ascending.
// O(n^2) but `n <= ATP_AC_FLATTEN_CAP = 64`; cost is dwarfed by the
// caller's recursive walk.  Stable on equal hashes.
static void atp_ac_sort_leaves(Term *leaves, u32 n) {
  for (u32 i = 1; i < n; i++) {
    Term ki = leaves[i];
    u64  hi = atp_term_struct_hash(ki);
    u32 j = i;
    while (j > 0u) {
      u64 hj = atp_term_struct_hash(leaves[j - 1u]);
      if (hj <= hi) break;
      leaves[j] = leaves[j - 1u];
      j -= 1u;
    }
    leaves[j] = ki;
  }
}

static Term atp_ac_canon_rec(Term t, const AtpAcInfo *ac);

// Helper: rebuild an AC node from its leaf multiset as a right-
// associative chain over `label`.  Caller guarantees `n >= 1`.
static Term atp_ac_build_chain(u32 label, const Term *leaves, u32 n) {
  if (n == 1u) return leaves[0];
  // Build from the right: tail = f(leaves[n-2], leaves[n-1]),
  // then folder over the remaining leaves left-to-right.
  Term acc = leaves[n - 1u];
  for (i32 i = (i32)n - 2; i >= 0; i--) {
    Term kids[2] = { leaves[(u32)i], acc };
    acc = term_new_ctr(label, kids, 2u);
  }
  return acc;
}

static Term atp_ac_canon_rec(Term t, const AtpAcInfo *ac) {
  if (term_tag(t) != TAG_CTR) return t;
  u32 label = term_ext(t);
  if (atp_ac_is_ac_label(ac, label)) {
    Term leaves[ATP_AC_FLATTEN_CAP];
    u32  n = 0u;
    if (!atp_ac_flatten_under(t, label, leaves, &n, ATP_AC_FLATTEN_CAP)) {
      return t;        // overflow: leave node as-is
    }
    // Recurse into each leaf so nested AC nodes canonicalize too.
    for (u32 i = 0; i < n; i++) {
      leaves[i] = atp_ac_canon_rec(leaves[i], ac);
    }
    atp_ac_sort_leaves(leaves, n);
    return atp_ac_build_chain(label, leaves, n);
  }
  // Non-AC CTR: recurse into children, rebuild only if any changed.
  u32 n_kids = term_ctr_n(t);
  if (n_kids == 0u) return t;
  if (n_kids > REWRITE_MAX_ARITY) return t;
  Term kids[REWRITE_MAX_ARITY];
  u8 changed = 0u;
  for (u32 i = 0; i < n_kids; i++) {
    Term orig = term_ctr_at(t, i);
    kids[i] = atp_ac_canon_rec(orig, ac);
    if (kids[i] != orig) changed = 1u;
  }
  if (!changed) return t;
  return term_new_ctr(label, kids, n_kids);
}

static Term atp_ac_canon(Term t, const AtpAcInfo *ac) {
  return atp_ac_canon_rec(t, ac);
}

// --- AC equality and AC hash ---------------------------------------
//
// `atp_ac_eq(s, t, ac)` decides `s ≡_AC t` -- structural equality up
// to (a) commuting AC-symbol children and (b) re-associating AC-
// symbol subterms.  Implemented as `kbo_eq(canon(s), canon(t))`
// where `canon` is the right-associative hash-sorted chain produced
// by `atp_ac_canon`.  Allocation cost is bounded by the union of
// AC subterms across `s` and `t`; the common case is allocation-free
// (no AC subterm, identity-return from canon, kbo_eq on the
// originals).
//
// `atp_ac_hash(t, ac)` is a 64-bit FNV-style hash invariant under
// AC.  Computed as `atp_term_struct_hash(canon(t))`, so by
// construction `atp_ac_eq(s, t, ac) == 1 ==> atp_ac_hash(s, ac) ==
// atp_ac_hash(t, ac)`.

// Forward decl from _.c (kbo_eq lives outside ac.c's TU).
static u8 kbo_eq(Term a, Term b);

static u8 atp_ac_eq(Term s, Term t, const AtpAcInfo *ac) {
  if (kbo_eq(s, t)) return 1;
  if (ac == NULL || ac->ac_mask == 0ull) return 0;
  Term cs = atp_ac_canon(s, ac);
  Term ct = atp_ac_canon(t, ac);
  return kbo_eq(cs, ct);
}

static u64 atp_ac_hash(Term t, const AtpAcInfo *ac) {
  if (ac == NULL || ac->ac_mask == 0ull) return atp_term_struct_hash(t);
  return atp_term_struct_hash(atp_ac_canon(t, ac));
}

// --- Engine-global AcInfo + setters --------------------------------
//
// One file-static `g_atp_ac_info` carries the AC bit-mask used by
// hot-path callers (`atp_cp_trivially_joinable`, future AC-matching
// in the rewriter).  Mirrors the global-static discipline of
// `g_atp_perm_subsume_mask` in `_.c`.
//
// Callers can set the mask explicitly (e.g. WL paclet writes the
// auto-detected mask via a setter) or run `thvm_atp_auto_ac(s)`
// after `thvm_atp_add_equation` calls to derive the mask from the
// engine's current rule set.

static AtpAcInfo g_atp_ac_info = { .ac_mask = 0ull };

fn void thvm_atp_set_ac_mask(u64 mask) {
  g_atp_ac_info.ac_mask = mask;
}

fn u64 thvm_atp_get_ac_mask(void) {
  return g_atp_ac_info.ac_mask;
}

// Derive the AC mask by analyzing an explicit axiom set.  The caller
// supplies the per-equation lhs[] / rhs[] arrays + count -- the
// engine's `s->lhs[]`/`rhs[]` storage holds POST-ORIENTATION rules,
// not the raw axiom equations, so a caller that wants AC inferred
// from the user-supplied axioms must keep its own copy and feed
// them through here.  (The WL bridge does this naturally; tests
// supply their corpus directly.)
fn void thvm_atp_auto_ac(const Term *lhs, const Term *rhs, u32 n_eqns) {
  if (lhs == NULL || rhs == NULL || n_eqns == 0u) {
    g_atp_ac_info.ac_mask = 0ull;
    return;
  }
  AtpSymProps props[WALD_MAX_SYMBOLS];
  for (u32 i = 0; i < WALD_MAX_SYMBOLS; i++) {
    props[i].seen = 0u;
    props[i].arity = 0u;
    props[i].is_commutative = 0u;
    props[i].is_associative = 0u;
    props[i].is_idempotent = 0u;
    props[i].has_left_unit = 0u;
    props[i].has_right_unit = 0u;
    props[i].has_inverse = 0u;
    props[i].is_unit_symbol = 0u;
    props[i].is_inverse_symbol = 0u;
    props[i].distributes = 0u;
    props[i].distributes_over = 0u;
  }
  atp_analyze_axioms(lhs, rhs, n_eqns, props, WALD_MAX_SYMBOLS);
  atp_acinfo_compute(&g_atp_ac_info, props, WALD_MAX_SYMBOLS);
}

#endif // THVM_ATP_AC
