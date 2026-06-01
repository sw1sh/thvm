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

// --- AC matching ---------------------------------------------------
//
// `atp_match_ac(pattern, subject, ac, subst)` is `thvm_match` lifted
// to AC: a one-way match that accepts ANY permutation of AC-symbol
// children as a valid alignment.  Returns 1 on success with the
// substitution recorded in `*subst`; 0 on failure.  Caller must
// zero-init `subst` before the first call (or use a freshly cleared
// one) -- on partial-match failure we DO NOT roll back bindings the
// caller had pre-installed; we may add bindings before failing.
// Callers that want clean rollback save subst state externally.
//
// Algorithm (simplified, suitable for unit-AC rules):
//
//   1. If the pattern's top is NOT an AC label, fall through to the
//      standard syntactic match.  (The match still has to descend
//      into AC subterms recursively -- handled here too.)
//   2. If the pattern's top IS an AC label, the subject must also
//      be CTR with the same label; AC-flatten both into leaf
//      multisets `P[]` (size `np`) and `S[]` (size `ns`).  Fail if
//      `np > ns`.
//   3. Sort P into four kinds (ground / non-trivial CTR / bound
//      var / unbound var).  Greedy assignment:
//        - For each ground leaf, find an unused subject leaf with
//          which `kbo_eq` holds.  Fail if none.
//        - For each non-trivial-CTR leaf, find an unused subject
//          leaf where atp_match_ac succeeds (recursive).  Fail if
//          none.  Backtracking left to a future stage; greedy here.
//        - For each bound var, find an unused subject leaf with
//          `kbo_eq` against the binding.
//   4. Distribute remaining subject leaves over the unbound vars:
//        - 0 unbound vars + 0 leaves left: succeed.
//        - 0 unbound vars + N leaves left: fail.
//        - 1 unbound var: bind to (a) the single remaining leaf,
//          or (b) an AC-rebuilt right-assoc chain of the leaves.
//        - >1 unbound var: bail conservatively (return 0).
//          (The full multi-var partition algorithm is a future arc.)
//
// Coverage at this stage:
//   * `f(x, e) → x`            -- e ground, x unbound var.  Works.
//   * `f(x, x) → x`            -- both leaves are the same var;
//                                 first sighting binds, second checks
//                                 consistency.  Works for any
//                                 subject `f(a, a)`.
//   * `f(x, y, z) → ...`       -- one var per leaf, direct match.
//   * `f(x, f(y, z)) → ...`    -- canonicalize to flat `f(x, y, z)`
//                                 first, then match.  Caller is
//                                 responsible for invoking on
//                                 canonicalized pattern -- or use
//                                 atp_match_ac which flattens.
//
// Not yet covered:
//   * `f(x, y) → ...`          against subject `f(a, b, c)` with
//                                 unbound x and y -- two vars,
//                                 multiple leaves; partition
//                                 enumeration deferred.
//   * Backtracking when a greedy non-trivial-CTR match consumes
//     a leaf that should have gone to a var.  Conservative fail.

// Forward decl from rewrite/_.c.
fn u8 thvm_match(Term pattern, Term term, RewriteSubst *subst);

// Build a right-associative chain of `label` over `leaves[0..n)`.
// Caller guarantees n >= 1.  Used to bind a single AC-var to a
// multi-leaf remainder.
static Term atp_ac_chain_from(u32 label, const Term *leaves, u32 n) {
  return atp_ac_build_chain(label, leaves, n);
}

static u8 atp_match_ac(Term pattern, Term subject,
                       const AtpAcInfo *ac, RewriteSubst *subst);

// AC-flat-vs-flat match: pattern leaves P[] vs subject leaves S[],
// both already AC-flattened under the same AC label `label`.  Handles
// ground / bound-var / CTR-with-vars / unbound-var pattern leaves.
//
// Algorithm:
//   1. Identify each distinct unbound var with its multiplicity (count
//      of P leaves equal to FVR(vid)).  Cap at MAX_AC_UNBOUND_VARS;
//      more than that bails conservatively.
//   2. Pass 1: match every NON-unbound-var pattern leaf greedily
//      against an unused subject leaf via kbo_eq (ground / bound-var
//      / NUM) or atp_match_ac (recursive CTR).
//   3. Pass 2: handle the unbound vars.
//        - n_unbound == 0: leftover subject leaves must be 0.
//        - n_unbound == 1 with multiplicity 1: bind to single leftover
//          (1 leaf) or AC chain over leftovers (>1 leaves).
//        - n_unbound == 1 with multiplicity m > 1: leftover must be
//          exactly m copies of one value X; bind to X.
//        - n_unbound > 1: pair-up.  Requires sum-of-multiplicities ==
//          leftover.count; greedy bind (mult 1 vars take 1 leaf each;
//          mult m > 1 takes m kbo_eq copies).  Bails when the structure
//          can't be satisfied greedily (a full partition enumerator is
//          a future stage).
#define ATPFT_AC_MAX_UNBOUND 8u

static u8 atp_match_ac_flat(u32 label,
                            const Term *P, u32 np,
                            Term *S, u32 ns,
                            const AtpAcInfo *ac, RewriteSubst *subst) {
  if (np > ns) return 0;
  if (ns > ATP_AC_FLATTEN_CAP) return 0;

  // Identify each distinct unbound var + its multiplicity.
  u32 ub_vid [ATPFT_AC_MAX_UNBOUND];
  u32 ub_mult[ATPFT_AC_MAX_UNBOUND];
  u32 n_ub = 0u;
  for (u32 i = 0; i < np; i++) {
    Term p = P[i];
    if (term_tag(p) != TAG_FVR) continue;
    u32 vid = term_ext(p);
    if (vid >= REWRITE_MAX_VAR) return 0;
    if (subst->bindings[vid] != 0) continue;
    // unbound
    u32 k = 0u;
    while (k < n_ub && ub_vid[k] != vid) k++;
    if (k < n_ub) {
      ub_mult[k] += 1u;
    } else if (n_ub < ATPFT_AC_MAX_UNBOUND) {
      ub_vid [n_ub] = vid;
      ub_mult[n_ub] = 1u;
      n_ub += 1u;
    } else {
      return 0;            // too many distinct unbound vars
    }
  }

  // Pass 1: every NON-unbound-var pattern leaf consumes a subject leaf.
  u8 used[ATP_AC_FLATTEN_CAP] = {0};
  for (u32 i = 0; i < np; i++) {
    Term p = P[i];
    if (term_tag(p) == TAG_FVR) {
      u32 vid = term_ext(p);
      if (subst->bindings[vid] == 0) continue;   // unbound, deferred
      // Bound var: kbo_eq against binding.
      Term bound = subst->bindings[vid];
      u8 matched = 0u;
      for (u32 j = 0; j < ns; j++) {
        if (used[j]) continue;
        if (kbo_eq(S[j], bound)) {
          used[j] = 1u;
          matched = 1u;
          break;
        }
      }
      if (!matched) return 0;
    } else if (term_tag(p) == TAG_CTR) {
      u8 matched = 0u;
      for (u32 j = 0; j < ns; j++) {
        if (used[j]) continue;
        if (atp_match_ac(p, S[j], ac, subst)) {
          used[j] = 1u;
          matched = 1u;
          break;
        }
      }
      if (!matched) return 0;
    } else {
      u8 matched = 0u;
      for (u32 j = 0; j < ns; j++) {
        if (used[j]) continue;
        if (kbo_eq(S[j], p)) {
          used[j] = 1u;
          matched = 1u;
          break;
        }
      }
      if (!matched) return 0;
    }
  }

  // Pass 2: leftover subj distribution over unbound vars.
  Term left_buf[ATP_AC_FLATTEN_CAP];
  u32 leftover = 0u;
  for (u32 j = 0; j < ns; j++) {
    if (!used[j]) left_buf[leftover++] = S[j];
  }

  if (n_ub == 0u) {
    return leftover == 0u ? 1u : 0u;
  }

  if (n_ub == 1u) {
    u32 m = ub_mult[0];
    if (leftover < m) return 0;
    if (m == 1u) {
      // Single unbound var; single or chain binding.
      Term binding = (leftover == 1u)
                       ? left_buf[0]
                       : atp_ac_chain_from(label, left_buf, leftover);
      subst->bindings[ub_vid[0]] = binding;
      return 1u;
    }
    // Multiplicity > 1: leftover must be exactly m copies of one value.
    if (leftover != m) return 0;
    Term candidate = left_buf[0];
    for (u32 j = 1; j < leftover; j++) {
      if (!kbo_eq(left_buf[j], candidate)) return 0;
    }
    subst->bindings[ub_vid[0]] = candidate;
    return 1u;
  }

  // n_ub >= 2.  Greedy pair-up: requires sum-of-multiplicities ==
  // leftover.count.  Otherwise the assignment is ambiguous and a
  // full partition enumerator is needed (future stage).
  u32 total_slots = 0u;
  for (u32 k = 0; k < n_ub; k++) total_slots += ub_mult[k];
  if (total_slots != leftover) return 0;

  // For each unbound var in declaration order, consume `ub_mult[k]`
  // copies of the first available leftover value.
  u8 used2[ATP_AC_FLATTEN_CAP] = {0};
  for (u32 k = 0; k < n_ub; k++) {
    u32 m = ub_mult[k];
    // Find the first unused leftover leaf as the binding candidate.
    u32 base = (u32)-1;
    for (u32 j = 0; j < leftover; j++) {
      if (!used2[j]) { base = j; break; }
    }
    if (base == (u32)-1) return 0;
    Term candidate = left_buf[base];
    used2[base] = 1u;
    // Consume m-1 more copies of `candidate`.
    u32 need = m - 1u;
    for (u32 j = base + 1u; need > 0u && j < leftover; j++) {
      if (used2[j]) continue;
      if (kbo_eq(left_buf[j], candidate)) {
        used2[j] = 1u;
        need -= 1u;
      }
    }
    if (need > 0u) return 0;
    subst->bindings[ub_vid[k]] = candidate;
  }
  return 1u;
}

fn u8 atp_match_ac(Term pattern, Term subject,
                   const AtpAcInfo *ac, RewriteSubst *subst) {
  // Fast path: pattern is a var -- standard thvm_match handling
  // (bind on first sighting / kbo_eq on second).
  if (term_tag(pattern) == TAG_FVR) {
    u32 id = term_ext(pattern);
    if (id >= REWRITE_MAX_VAR) return 0;
    if (subst->bindings[id] == 0) {
      subst->bindings[id] = subject;
      return 1;
    }
    return kbo_eq(subst->bindings[id], subject);
  }

  // Pattern is not a CTR? (NUM etc.) Use kbo_eq.
  if (term_tag(pattern) != TAG_CTR) {
    return kbo_eq(pattern, subject) ? 1u : 0u;
  }

  // Pattern is CTR.  Subject must also be CTR.
  if (term_tag(subject) != TAG_CTR) return 0;
  if (term_ext(pattern) != term_ext(subject)) return 0;

  u32 lab = term_ext(pattern);
  if (atp_ac_is_ac_label(ac, lab)) {
    // AC top: flatten both sides and do AC-flat match.
    Term P[ATP_AC_FLATTEN_CAP];
    Term S[ATP_AC_FLATTEN_CAP];
    u32  np = 0u, ns = 0u;
    if (!atp_ac_flatten_under(pattern, lab, P, &np, ATP_AC_FLATTEN_CAP))
      return 0;
    if (!atp_ac_flatten_under(subject, lab, S, &ns, ATP_AC_FLATTEN_CAP))
      return 0;
    return atp_match_ac_flat(lab, P, np, S, ns, ac, subst);
  }

  // Non-AC CTR: same-arity recursive match.
  u32 np = term_ctr_n(pattern);
  u32 ns = term_ctr_n(subject);
  if (np != ns) return 0;
  for (u32 i = 0; i < np; i++) {
    if (!atp_match_ac(term_ctr_at(pattern, i),
                      term_ctr_at(subject, i),
                      ac, subst)) {
      return 0;
    }
  }
  return 1;
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
