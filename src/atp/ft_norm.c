// ft_norm.c - AtpFt-native innermost-rewrite
// normalize fixpoint.
//
// Loop shape (mirrors WM `BL_NormalformInnermost2` in NFBildung.c:622):
//
//   1. Entry-clear pass: walk root, clear ATPFT_FLAG_SUBST_FRESH on
//      every cell (O(|t|) once per call).
//
//   2. Step loop (bounded by `step_cap`):
//      a. find_redex_ft: pre-order walk via `cell->next`, skipping any
//         cell with SUBST_FRESH=1 (the innermost-rewrite shortcut --
//         a cell installed by an earlier step in this same fixpoint
//         doesn't need a redex re-check, by construction of WM-style
//         right-reduction).
//      b. For each non-fresh cell, attempt one-way match against
//         every rule LHS (s->lhs_ft[0..n_rules)).  Rule selection is
//         "first-match-wins" -- the rule-index discrimination tree
//         would replace this linear scan; Stage 6 ships the simple
//         scan to keep the splice splice's correctness gate decoupled
//         from the index plumbing.
//      c. If no redex -> break.
//      d. Splice (ft_splice).  The root may change identity (regime
//         (c) root rewrite); we propagate via the new-root return.
//
// All cells live in Arena A (the persistent slab pool).  Stage 6 leaks
// Arena A growth across normalize calls -- the Stage 4 GC sweeps on
// schedule.  Scratch (Arena B) is unused on this path.
//
// Gated on THVM_ATPFT_NORM.

#ifdef THVM_ATPFT_NORM

#include "../thvm.h"
#include "ft.h"
#include "../wmfpa/wmfpa.h"

// Symbols brought in from sibling stages.  The AtpFtSubst type is
// declared by ft_match.c (always included earlier in the same TU);
// `ft_match` / `ft_subst_reset` keep their natural typed signatures.
extern int  ft_eq      (const AtpFtCell *x, const AtpFtCell *y);

// Forward decl for the trace-push helper -- defined in src/atp/_.c
// further down the same TU.  ft_norm.c is included BEFORE the helper
// (the include ordering wires _.c's later trace machinery), so a
// matching `static` forward decl here is what the record-mode splice
// loop below references.
static u32 atp_trace_push_norm_step(AtpState *s, u32 p_a, u32 rule_idx,
                                    Term lhs, Term rhs,
                                    u8 side, u8 fwd,
                                    const u8 *pos, u8 pos_len);

extern AtpFtCell *ft_splice(AtpFt          *a,
                            AtpFtCell      *root,
                            AtpFtCell      *parent,
                            AtpFtCell      *redex,
                            const AtpFtCell *rhs_tmpl,
                            const void     *subst);

// Stage 6b hooks -- defined in ft_ri.c when THVM_ATPFT_RI is on.
#ifdef THVM_ATPFT_RI
extern int  atp_ri_find_redex_ft_pub(AtpState *s, AtpFtCell *root,
                                     AtpFtCell **redex_out, u32 *rule_out);
extern void atp_ri_ft_sync(AtpState *s);
#endif

// --- Extension-variable grounding for unorientable equations --------
//
// Mirrors `atp_unorient_template` in src/atp/_.c (the Term-side WM
// free-variable-instance port, after WM `TP_RechteSeiteUnfrei` /
// `RechtsUnfreiErzeugen`, INF/RUndEVerwaltung.c:366-397): once the
// match side of an unorientable direction has bound its variables,
// every template variable left unbound is an extension variable of
// the direction; bind it to the minimal constant (s->min_const) so
// sigma(template) is the grounded WM instance.  The caller's KBO_GT
// gate then orders the instance, exactly like a variable-contained
// direction.
//
// Gated like the Term side on a ground-total reduction order: KBO/LPO
// (total precedence by config contract) qualify, WPO/RPO do not --
// and this FT path only ever runs the streaming KBO anyway.
//
// Returns 1 when the subst covers every template variable (extras now
// bound), 0 when the direction must be skipped: out-of-range var id
// (>= ATPFT_MAX_VARS, the subst address space) or non-ground-total
// order.  Walks the template via the `next` / `end` chain -- the same
// stride find_redex_ft uses; no recursion, O(|cells|).
static int ft_subst_ground_extras(AtpState *s, const AtpFtCell *tmpl,
                                  AtpFtSubst *subst) {
  const AtpFtCell *end_after = (tmpl->end != NULL) ? tmpl->end->next : NULL;
  AtpFtCell *mc = NULL;
  for (const AtpFtCell *p = tmpl; p != NULL && p != end_after; p = p->next) {
    if ((p->sym & WF_VAR_BIT) == 0u) continue;
    u32 id = p->sym & ~WF_VAR_BIT;
    if (id >= ATPFT_MAX_VARS) return 0;   // outside the subst address space
    if (subst->bind[id] != NULL) continue;
    if (s->wpo != NULL || s->rpo != NULL) return 0;  // not ground-total
    if (mc == NULL) {
      mc = ft_from_term((AtpFt *)s->ft_arena_ptr, s->min_const, 0);
      if (mc == NULL) return 0;
    }
    subst->bind[id] = mc;
    subst->bound_ids[subst->wm++] = id;
  }
  return 1;
}

// `ft_to_term` / `atp_compare` are defined elsewhere in the same TU
// (src/atp/_.c) when ft_norm.c is `#include`'d after them.  No extern
// declarations needed -- the call-site sees the in-TU defs directly.
// AtpFtSubst typedef comes from ft_match.c (already included earlier).
//
// FT-native KBO compare lives in src/atp/ft_order.c, which is NOT
// pulled in by _.c -- the symbol is fn (exported), so we forward-
// declare it here before find_redex_ft / its discrim variant.
extern KboCmp thvm_kbo_ft(const AtpFtCell *a, const AtpFtCell *b,
                          const KboConfig *cfg);
// Streaming variant: encodes (tmpl, subst) inline into the kbo_flat
// buffer instead of materialising subst(tmpl) as AtpFtCell* first.
extern KboCmp thvm_kbo_ft_subst(const AtpFtCell *redex,
                                const AtpFtCell *tmpl,
                                const void      *subst,
                                const KboConfig *cfg);
// Two-stage variant: encode redex once into the shared buffer, reuse
// across many rule attempts at the same cell.
extern u32    thvm_kbo_ft_subst_prepare_redex(const AtpFtCell *redex,
                                              const KboConfig *cfg);
extern KboCmp thvm_kbo_ft_subst_with_prepared(u32 na,
                                              const AtpFtCell *tmpl,
                                              const void      *subst,
                                              const KboConfig *cfg);

// --- SUBST_FRESH entry-clear ----------------------------------------
//
// Walk via `cell->next` pre-order.  This visits every cell of the
// term exactly once (the AtpFt invariant: a term's cells form a
// next-threaded chain from `root` to `root->end`, with `root->end->next`
// being NULL or pointing into the parent's chain -- and for a root
// term the latter is NULL).
//
// We stop AT root->end, inclusive: that's the last cell of THIS term.
static void ft_clear_subst_fresh(AtpFtCell *root) {
  if (root == NULL) return;
  AtpFtCell *end_after = (root->end != NULL) ? root->end->next : NULL;
  for (AtpFtCell *p = root; p != NULL && p != end_after; p = p->next) {
    p->flags &= (u8)~ATPFT_FLAG_SUBST_FRESH;
  }
}

// --- find_redex_ft --------------------------------------------------
//
// Walk `root` pre-order via `cell->next`.  For each cell whose
// SUBST_FRESH bit is clear, try the rule LHSs in the slice
// [slice_first, slice_first + slice_count).  If a rule matches,
// fill `subst_out`, set `redex_out` to the cell and `parent_out` to
// the cell whose `next` reaches the redex (NULL for the root case),
// return 1.  Return 0 if no redex.
//
// `parent_out`: the cell BEFORE the redex in the pre-order chain.  We
// track it as we walk; it's the cell whose `next` is the current
// candidate.  For the root, no predecessor exists -> NULL.
//
// `dir_out`: 0 = forward (LHS -> RHS), 1 = backward (RHS -> LHS).
// Always 0 in `try_orient` mode (orientable rules only).
// May be either when `try_unorient` is set.
//
// `try_orient`: when set, the linear scan considers orientable rules
//   (s->r_orient[r] == 1, or every rule when s->n_unorient == 0).
// `try_unorient`: when set, the linear scan considers unorientable
//   equations (s->r_orient[r] == 0, s->n_unorient > 0).
//
// Per-position redex priority is WM's (BL_RegelOderGleichungAngewendet,
// NF/NFBildung.c:503-531; path-based NFB_ variant :219-238): at each
// cell the oriented rules (Regelbaum) are tried BEFORE the unorientable
// equations (Gleichungsbaum), and an equation fires only when no
// oriented rule matches there.  Across cells the walk stays pre-order,
// so an equation redex at an earlier cell still beats an oriented redex
// at a later one -- the WM traversal contract, NOT a global
// rules-to-fixpoint phase split.
//
// Unorientable equations are gated like atp_ordered_try_top
// (src/atp/_.c):
//   1. Both directions are TRIED via ft_match.
//   2. A direction whose replacement template carries extension
//      variables (vars not bound by the match) grounds them to the
//      minimal constant -- ft_subst_ground_extras, the WM
//      free-variable instance.  ft_splice then sees a fully-bound
//      template, never an unbound var (which would make it bail with
//      NULL and loop until step_cap).
//   3. The instantiated rewrite must strictly decrease the redex in
//      the reduction order (streaming KBO == atp_compare verdict).
//
// The full-range entry point (`atp_rewrite_normalize_ft`) passes
// slice_first=0 / slice_count=s->n_rules; the slice entry point
// (`atp_rewrite_normalize_ft_slice`) passes the caller's [first,count)
// directly.  Iterating only the slice is the whole point of the slice
// path -- interreduce wants to rewrite an EXISTING rule against the
// just-added rules, NOT against itself (would loop) or against the
// older rules (those gave it CPs; rewriting through them is a no-op
// since the rule is already in normal form w.r.t. the older R).

// AC-aware match dispatch.  When an AC mask has been registered
// (thvm_atp_get_ac_mask() != 0), we route through the AC-modulo
// matcher in ft_ac_match.c by default.  Set THVM_ATPFT_AC_MATCH=0 in
// env to fall back to the syntactic ft_match path.  Flipped on by
// default 2026-06-07 after measuring: ac-ring iters 24 -> 14 (-42%),
// ac-abelian QUEUE_EMPTY iters 14 -> 10, no regression on mccune
// (14.9s) / thm / test_atp (135623/135623) / test_atp_ft_rules.
// The env flag is cached on first call -- changing it mid-process
// has no effect, by design (same convention as THVM_ATPFT_KBO_DIFF).
static inline int ft_match_maybe_ac(AtpFt *a,
                                    const AtpFtCell *pat,
                                    const AtpFtCell *subj,
                                    AtpFtSubst *subst) {
#if defined(THVM_ATP_AC) && defined(THVM_ATPFT_MATCH)
  static int ac_on = -1;
  if (ac_on < 0) {
    const char *e = getenv("THVM_ATPFT_AC_MATCH");
    ac_on = (e == NULL || (e[0] != '0' && e[0] != '\0')) ? 1 : 0;
  }
  if (ac_on) {
    u64 mask = thvm_atp_get_ac_mask();
    if (mask != 0ull) {
      AtpAcInfo ac = { .ac_mask = mask };
      return ft_match_ac(a, pat, subj, &ac, subst);
    }
  }
#endif
  (void)a;
  return ft_match(pat, subj, subst);
}

// WM Regelbaum/Gleichungsbaum retrieval order (MO_RegelGefunden,
// INF/MatchOperationen.c:565-651): the DFS over the shared
// discrimination tree tries, at every node, the EXACT function-symbol
// edge first (tryFct :600-606), then the NEW-variable edge (tryNVar
// :607-612), then the already-bound variable edges in ascending
// symbol code = DESCENDING variable index (tryOVar1 :614-625), with
// full backtracking (:626-632).  The first leaf reached is therefore
// the lexicographic minimum over the canonically var-renumbered flat
// pattern strings under the per-entry rank
//     exact symbol (0) < first variable occurrence (1)
//                      < repeated variable (2; newer variable first),
// restricted to the patterns that match the subject.  Among matching
// candidates the aligned common prefix is entry-identical, so a
// pairwise first-divergence comparator computes exactly the DFS
// winner.  Returns 1 iff pattern `a` strictly precedes pattern `b`.
// Alpha-identical patterns return 0 both ways; the caller's tie-break
// (newest slot wins) mirrors the leaf chain order (DSBaumOperationen
// insert-at-head, newest first).
static int ft_wm_pattern_before(const AtpFtCell *a, const AtpFtCell *b) {
  u16 map_a[ATPFT_MAX_VARS], map_b[ATPFT_MAX_VARS];
  memset(map_a, 0xff, sizeof(map_a));
  memset(map_b, 0xff, sizeof(map_b));
  u16 na = 0u, nb = 0u;
  const AtpFtCell *ea = (a->end != NULL) ? a->end->next : NULL;
  const AtpFtCell *eb = (b->end != NULL) ? b->end->next : NULL;
  const AtpFtCell *pa = a;
  const AtpFtCell *pb = b;
  while (pa != NULL && pa != ea && pb != NULL && pb != eb) {
    u8  va = (u8)((pa->sym & WF_VAR_BIT) != 0u);
    u8  vb = (u8)((pb->sym & WF_VAR_BIT) != 0u);
    u32 ra = 0u, rb = 0u;   // entry rank
    u16 ia = 0u, ib = 0u;   // canonical var index (rank 2 only)
    if (va) {
      u32 id = pa->sym & ~WF_VAR_BIT;
      if (id >= ATPFT_MAX_VARS) return 0;   // defensive: incomparable
      if (map_a[id] == 0xffffu) { map_a[id] = na++; ra = 1u; }
      else                      { ia = map_a[id]; ra = 2u; }
    }
    if (vb) {
      u32 id = pb->sym & ~WF_VAR_BIT;
      if (id >= ATPFT_MAX_VARS) return 0;
      if (map_b[id] == 0xffffu) { map_b[id] = nb++; rb = 1u; }
      else                      { ib = map_b[id]; rb = 2u; }
    }
    if (ra != rb) return ra < rb;
    if (ra == 0u && pa->sym != pb->sym) {
      // Two distinct exact symbols on an identical aligned prefix
      // cannot both match one subject; order by code for totality.
      return pa->sym < pb->sym;
    }
    if (ra == 2u && ia != ib) return ia > ib;   // newer variable first
    pa = pa->next;
    pb = pb->next;
  }
  return 0;
}

// --- Candidate-pruning fingerprint index over rule LHS flatterms -----
//
// The per-cell scan in ft_cell_try_rules is O(n_rules): for a cell that
// is already in normal form -- the overwhelming majority on a large
// saturation -- it calls ft_match against EVERY rule LHS and all fail.
// Waldmeister's Regelbaum (a perfect discrimination tree) descends by
// term structure and rejects a non-matching cell in O(term depth); that
// is the ~2x throughput gap.  The existing top-symbol pre-filter only
// discriminates the ROOT symbol -- useless on a single-operator
// signature (combinators / Sheffer) where every rule shares it.
//
// We index the rule LHS flatterms by a fixed-position FINGERPRINT
// (Schulz-style): the concrete symbol (or VAR/ABSENT marker) at seven
// sampled path positions to depth 2 -- the root, the two depth-1
// arguments (arg0, arg1), and the four depth-2 grandchildren (arg0.arg0,
// arg0.arg1, arg1.arg0, arg1.arg1).  A rule is a CANDIDATE for a subject
// cell iff at every sampled position EITHER the rule's LHS has a
// variable there (a variable matches any subterm), OR the subject has a
// variable there (defensive -- the redex cell is CTR by construction),
// OR the position is ABSENT on either side (low arity, caught by
// ft_match's arity check), OR the two concrete symbols AGREE.  A
// position prunes ONLY when both sides are concrete and DIFFER -- a
// subset of the cases ft_match rejects on a per-position compare.  So
// the candidate set is a SOUND SUPERSET of the rules ft_match would
// match (INVARIANT 1): no actual match is ever missed, and adding deeper
// positions only ever TIGHTENS (drops guaranteed non-matches).
//
// Buckets are keyed by ROOT symbol so a CTR-rooted subject visits only
// same-root rules + the (rare) var-rooted rules; within a bucket the
// arg / grandchild fingerprint prunes further -- this is what cracks the
// single-operator combinator case (one root bucket = all rules, where
// the depth-1 arg signature alone still leaves ~150 candidates sharing
// the `app(app(...),...)` left-nested prefix, and the depth-2
// grandchildren split them apart).  Each bucket's rule slots are kept in
// ASCENDING order, and a query MERGES the var-rooted list with the
// matched bucket so candidates emerge in ascending global slot order --
// the exact order the linear scan visits them, so the existing
// first-match / WM-DFS selection picks the IDENTICAL winner (INVARIANT
// 2: result is byte-identical).
//
// The index reflects s->lhs_ft[] for the FULL rule range and is rebuilt
// whenever s->r_revision moves (the monotone counter bumped on every
// rule add / drop / orient-flip / soft-delete), so add/kill maintenance
// is automatic (INVARIANT 4).  The slice path (interreduce) and the AC
// path (ft_ac_match matches modulo AC, which a syntactic fingerprint
// cannot capture) fall back to the unpruned linear scan (INVARIANTS 3
// and 5): the index narrows ONLY the full-range, AC-off hot path.
//
// Gated on THVM_ATPFT_NORM (always compiled) but only consulted when
// useful -- a tiny rule set takes the linear scan (the index would only
// add overhead).  THVM_ATPFT_NO_PRUNE=1 forces the linear scan for
// before/after measurement.

#define FTPI_FP_VAR    0xFFFFFFFEu   // a variable at this position (wildcard)
#define FTPI_FP_ABSENT 0xFFFFFFFDu   // the position does not exist (low arity)

// Number of sampled path positions in the fingerprint.  fp[0]=root (the
// bucket key); fp[1..2]=arg0,arg1 (depth 1); fp[3..6]=arg0.arg0,
// arg0.arg1, arg1.arg0, arg1.arg1 (depth 2).  The 4 depth-2 positions
// split the depth-1-degenerate combinator / Sheffer buckets (a single
// root + an `app(app(...),...)` left-nested prefix shared by most rules)
// into far finer classes -- the depth-1 arg signature alone leaves ~150
// candidates/query that differ only DEEPER in the spine; depth 2 cuts
// that to ~24 and is the wall-time optimum.  Depth 3 (sampling the
// arg0-spine great-grandchildren) cuts candidates further to ~13, but
// the wider per-entry key + longer per-query compatibility loop cost
// more wall than the extra ft_match calls they save -- past the
// diminishing-returns crossover -- so we stop at depth 2.  Soundness is
// unchanged at any depth: every position prunes ONLY when both rule and
// subject are concrete-and-differ; a VAR or ABSENT on either side never
// prunes (ftpi_pos_compatible), so deepening can only DROP guaranteed
// non-matches, never miss a real one.
#define FTPI_FP_N   7u    // sampled positions: 1 root + 2 depth-1 + 4 depth-2
#define FTPI_FP_ARG (FTPI_FP_N - 1u)   // per-entry stored positions (all but root)

// Sampled-position fingerprint of a flatterm cell, fp[0..FTPI_FP_N).
typedef struct {
  u32 fp[FTPI_FP_N];
} FtPiFp;

static inline u32 ftpi_cell_sym(const AtpFtCell *c) {
  return (c->sym & WF_VAR_BIT) ? FTPI_FP_VAR : c->sym;
}

// The i-th child of `c` (0-based), walked via the AtpFt sibling stride
// (first child = c->next; next sibling = child->end->next).  Returns
// NULL for a variable / nullary cell, an out-of-arity index, or a torn
// chain.  Stride is O(arity) but arity here is tiny (<=2 for the
// combinator/Sheffer operators we index).
static const AtpFtCell *ftpi_child(const AtpFtCell *c, u32 i) {
  if (c == NULL || (c->sym & WF_VAR_BIT) != 0u || i >= c->arity) return NULL;
  const AtpFtCell *ch = c->next;
  while (i-- > 0u && ch != NULL) ch = (ch->end != NULL) ? ch->end->next : NULL;
  return ch;
}

// Sample fp[slot] = symbol of `c` (ABSENT-on-NULL is the caller's
// pre-init), returning `c` for further descent.
static inline const AtpFtCell *ftpi_sample(const AtpFtCell *c, FtPiFp *out, u32 slot) {
  if (c != NULL) out->fp[slot] = ftpi_cell_sym(c);
  return c;
}

// Compute the FTPI_FP_N-position path fingerprint of `c`.  Each absent
// subterm (low arity, variable, nullary) is FTPI_FP_ABSENT; a variable
// subterm is FTPI_FP_VAR.  Depth-2 positions hang off both depth-1
// children (arg0.arg0, arg0.arg1, arg1.arg0, arg1.arg1).
static void ftpi_fingerprint(const AtpFtCell *c, FtPiFp *out) {
  for (u32 i = 0; i < FTPI_FP_N; i++) out->fp[i] = FTPI_FP_ABSENT;
  out->fp[0] = ftpi_cell_sym(c);
  const AtpFtCell *a0 = ftpi_sample(ftpi_child(c, 0u), out, 1u);
  const AtpFtCell *a1 = ftpi_sample(ftpi_child(c, 1u), out, 2u);
  ftpi_sample(ftpi_child(a0, 0u), out, 3u);
  ftpi_sample(ftpi_child(a0, 1u), out, 4u);
  ftpi_sample(ftpi_child(a1, 0u), out, 5u);
  ftpi_sample(ftpi_child(a1, 1u), out, 6u);
}

// Position-compatibility: a rule-LHS sampled symbol `r` against a
// subject sampled symbol `subj`.  Prune ONLY when both are concrete and
// differ.  VAR on either side, or ABSENT alignment, never prunes (the
// rule's ABSENT arity-mismatch is caught by ft_match's arity check, and
// keeping it is sound -- we only ever DROP guaranteed non-matches).
static inline int ftpi_pos_compatible(u32 r, u32 subj) {
  if (r == FTPI_FP_VAR || subj == FTPI_FP_VAR) return 1;
  if (r == FTPI_FP_ABSENT || subj == FTPI_FP_ABSENT) return 1;
  return r == subj;
}

// One indexed rule entry: its slot + the non-root fingerprint positions
// (arg0, arg1, arg0.arg0, arg0.arg1, arg1.arg0, arg1.arg1).  The root
// symbol is the bucket key, so it is not stored per entry.  arg[k] holds
// fingerprint position k+1 (fp[1..FTPI_FP_N)).
typedef struct {
  u32 rule;             // rule slot index (s->lhs_ft[rule])
  u32 arg[FTPI_FP_ARG]; // fingerprint positions fp[1..FTPI_FP_N)
} FtPiEnt;

// A bucket: ascending-slot list of rules whose LHS roots at this symbol.
typedef struct {
  u32      sym;      // root symbol (concrete CTR/NUM sym)
  FtPiEnt *ents;
  u32      n, cap;
} FtPiBucket;

// The index.  Hangs off file-static storage keyed by AtpState pointer +
// revision (a single index is enough -- ATP runs one saturation at a
// time on this path; a different AtpState forces a rebuild, which is the
// same cost as a revision bump).
typedef struct {
  const AtpState *owner;
  u32         built_revision;
  u32         built_n_rules;
  FtPiBucket *buckets;
  u32         n_buckets, cap_buckets;
  // Var-rooted rules (LHS root is a variable -- matches any cell), kept
  // ascending.  These are merged into every query.
  u32        *var_rules;
  u32         n_var, cap_var;
  // Scratch candidate buffer (ascending slot order), reused per query.
  u32        *cand;
  u32         cand_cap;
  // Scratch ping-pong buffer for the LSD-radix candidate sort (sized in
  // lockstep with `cand`; reused per query, no per-call malloc).
  u32        *cand_tmp;
  u32         cand_tmp_cap;
  // Instrumentation (THVM_ATPFT_PRUNE_STATS=1 prints on teardown).
  u64         q_queries;
  u64         q_candidates;   // candidates returned (summed over queries)
  u64         q_rules_seen;   // n_rules summed over queries (the no-index cost)
  u64         n_rebuilds;     // full index rebuilds (revision/count changes)
  u64         rebuild_work;   // rules processed across all rebuilds
  // Root-symbol CONCENTRATION at the last rebuild: largest bucket size /
  // indexed rule count.  When it's high the rule set is dominated by one
  // root symbol -- the cheap top-symbol fast-fail in ft_match prunes
  // NOTHING, so the arg-fingerprint index is the only lever and the
  // pruning pays off (combinators / Sheffer).  When it's low (a
  // multi-operator signature) ft_match's first per-position compare
  // fast-fails on the root and the linear scan is already cheap, so the
  // index would be pure per-query overhead -- ftpi_applicable then keeps
  // the dense scan.  Result is byte-identical either way; this gate only
  // chooses the faster of two correct paths.
  u32         max_bucket;     // largest bucket size at last rebuild
  u32         n_indexed;      // total live indexed rules at last rebuild
  // --- Full-depth perfect discrimination tree (Regelbaum), built in
  // lockstep with the buckets above (see ftdt_* below).  Node 0 is the
  // root (created on first build).  Each node owns its edge arrays + an
  // ascending leaf-slot list (see FtDtNode below).
  struct FtDtNode_s *dt_nodes;   // typed FtDtNode; fwd-named to keep order
  u32         dt_n_nodes;        // live nodes this build (<= dt_hi_nodes)
  u32         dt_hi_nodes;       // high-water: nodes ever allocated (buffers kept)
  u32         dt_cap_nodes;      // node array capacity
  // Insert-time canon->var-id maps, one nvar-long slice per inserted
  // pattern face (see FtDtRec).  Reset with the tree on rebuild.
  u16        *dt_idmap;
  u32         dt_idmap_n, dt_idmap_cap;
  // Per-query descent-binding capture (reset at each ftdt_candidates):
  // bind snapshots + acceptance records.
  const AtpFtCell **dtb_snap;
  u32         dtb_snap_n, dtb_snap_cap;
  struct FtDtRec_s *dtb_rec;
  u32         dtb_rec_n, dtb_rec_cap;
} FtPiIndex;

static FtPiIndex g_ftpi = {0};

// Per-query snapshot of the detail-tier gate (THVM_ATP_PROFILE=2 push
// scope) for the descend's per-node shape counters -- one u8 test per
// visited node instead of two global loads.  Set in ftdt_candidates.
static u8 g_ftdt_pd = 0;
// Tree depth of the NEXT ftdt_descend call (edges from the root) --
// maintained only under g_ftdt_pd for the per-depth visit histogram;
// the caller sets it before each recursion, so no unwind is needed.
static u32 g_ftdt_dp = 0;

// Profile-only subject-subtree cell count (the ft_eq cost proxy).
static u32 ftdt_prof_cells(const AtpFtCell *c) {
  const AtpFtCell *e = (c->end != NULL) ? c->end->next : NULL;
  u32 n = 0u;
  for (; c != NULL && c != e; c = c->next) n++;
  return n;
}

// Discrimination-tree builder hooks (defined below ftpi_candidates, but
// called by ftpi_insert_rule / ftpi_rebuild above their definition).
static void ftdt_insert_pattern(FtPiIndex *ix, const AtpFtCell *pat, u32 rule,
                                u8 unor, u8 face);
static void ftdt_reset(FtPiIndex *ix);
static void ftdt_shape_dump(void);
static void ftnfm_bump_gen(void);

// THVM_ATPFT_PRUNE_STATS=1 prints the prune ratio at process exit:
// candidates-per-query vs the n_rules a linear scan would have visited.
static void ftpi_stats_atexit(void) {
  FtPiIndex *ix = &g_ftpi;
  if (ix->q_queries == 0u) return;
  fprintf(stderr,
          "[ftpi] queries=%llu  cand/query=%.2f  no-index n_rules/query=%.2f  "
          "prune-ratio=%.4f  ft_match-calls-saved~%.1fx\n",
          (unsigned long long)ix->q_queries,
          (double)ix->q_candidates / (double)ix->q_queries,
          (double)ix->q_rules_seen / (double)ix->q_queries,
          ix->q_rules_seen ? (double)ix->q_candidates / (double)ix->q_rules_seen
                           : 0.0,
          ix->q_candidates ? (double)ix->q_rules_seen / (double)ix->q_candidates
                           : 0.0);
  fprintf(stderr, "[ftpi] rebuilds=%llu  rebuild_work=%llu rules\n",
          (unsigned long long)ix->n_rebuilds,
          (unsigned long long)ix->rebuild_work);
  ftdt_shape_dump();
}

static void ftpi_stats_arm(void) {
  static int armed = -1;
  if (armed >= 0) return;
  const char *e = getenv("THVM_ATPFT_PRUNE_STATS");
  armed = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
  if (armed) atexit(ftpi_stats_atexit);
}

static FtPiBucket *ftpi_bucket_find(FtPiIndex *ix, u32 sym) {
  for (u32 b = 0; b < ix->n_buckets; b++) {
    if (ix->buckets[b].sym == sym) return &ix->buckets[b];
  }
  return NULL;
}

static FtPiBucket *ftpi_bucket_get(FtPiIndex *ix, u32 sym) {
  FtPiBucket *b = ftpi_bucket_find(ix, sym);
  if (b != NULL) return b;
  if (ix->n_buckets == ix->cap_buckets) {
    u32 nc = ix->cap_buckets ? ix->cap_buckets * 2u : 16u;
    FtPiBucket *nb = (FtPiBucket *)realloc(ix->buckets, nc * sizeof(FtPiBucket));
    if (nb == NULL) thvm_fatal("ftpi: bucket array OOM");
    ix->buckets = nb;
    ix->cap_buckets = nc;
  }
  b = &ix->buckets[ix->n_buckets++];
  b->sym = sym;
  b->ents = NULL;
  b->n = 0u;
  b->cap = 0u;
  return b;
}

// Push rule `rule` into bucket `b` with the non-root fingerprint
// positions `fp->fp[1..FTPI_FP_N)` (the root fp[0] is the bucket key).
static void ftpi_bucket_push(FtPiBucket *b, u32 rule, const FtPiFp *fp) {
  if (b->n == b->cap) {
    u32 nc = b->cap ? b->cap * 2u : 8u;
    FtPiEnt *ne = (FtPiEnt *)realloc(b->ents, nc * sizeof(FtPiEnt));
    if (ne == NULL) thvm_fatal("ftpi: bucket ents OOM");
    b->ents = ne;
    b->cap = nc;
  }
  b->ents[b->n].rule = rule;
  for (u32 k = 0; k < FTPI_FP_ARG; k++) b->ents[b->n].arg[k] = fp->fp[k + 1u];
  b->n++;
}

static void ftpi_var_push(FtPiIndex *ix, u32 rule) {
  if (ix->n_var == ix->cap_var) {
    u32 nc = ix->cap_var ? ix->cap_var * 2u : 16u;
    u32 *nv = (u32 *)realloc(ix->var_rules, nc * sizeof(u32));
    if (nv == NULL) thvm_fatal("ftpi: var list OOM");
    ix->var_rules = nv;
    ix->cap_var = nc;
  }
  ix->var_rules[ix->n_var++] = rule;
}

// Rebuild the index from s->lhs_ft[0..n_rules) for every CURRENTLY-LIVE
// rule.  We index a rule under both pass-1 (orient) and pass-2
// (unorient) gates by indexing EVERY live rule by its LHS root -- the
// per-query loops still apply their own orient / r_dead / face gates, so
// the index is a pure SUPERSET filter (the scan logic is unchanged).
//
// The unorientable BACKWARD face matches the subject against the RHS, so
// pass 2's backward leg needs an RHS-keyed lookup too.  Rather than a
// second tree we add each unorientable rule's RHS fingerprint into a
// per-entry "alt" position by ALSO bucketing it under its RHS root.  To
// keep the merge order identical to the linear scan (which visits each
// rule slot once, trying forward THEN backward), we instead keep ONE
// list per query and let the caller test both faces -- so the index
// only needs to ANSWER "could rule r match cell p on EITHER face".  We
// therefore bucket an unorientable rule under BOTH its LHS root and its
// RHS root (a rule can appear in two buckets); the query dedups by slot.
// Insert a single live rule slot `i` into the index (both faces).  Adds
// rules in ASCENDING slot order to keep every bucket sorted -- callers
// iterate i = 0..n_rules (rebuild) or the appended tail (extend), both
// monotone.  A dead / sentinel slot is skipped here.
static void ftpi_insert_rule(FtPiIndex *ix, AtpState *s, u32 i) {
  if (s->r_dead != NULL && s->r_dead[i]) return;
  AtpFtCell *lhs = s->lhs_ft[i];
  AtpFtCell *rhs = s->rhs_ft[i];
  if (lhs == NULL || rhs == NULL) return;
  u8 unor = (u8)(s->n_unorient > 0u && !s->r_orient[i]);
  // LHS face (forward; oriented rules only ever use this face).
  FtPiFp fp;
  ftpi_fingerprint(lhs, &fp);
  if (fp.fp[0] == FTPI_FP_VAR) {
    ftpi_var_push(ix, i);
  } else {
    FtPiBucket *b = ftpi_bucket_get(ix, fp.fp[0]);
    ftpi_bucket_push(b, i, &fp);
  }
  // Discrimination tree (full-depth) -- same faces as the buckets, so the
  // two indexes return the identical candidate SET; the tree just prunes
  // deeper.  A var-rooted LHS becomes a STAR-root path (matches any cell).
  // The leaf class is the RULE's orientation (both faces of an
  // unorientable rule are class UNORIENT -- the pass guards test
  // r_orient[r], not which face produced the candidate).
  ftdt_insert_pattern(ix, lhs, i, unor, /*face=*/0u);
  // RHS face (unorientable rules also try backward: subject matched
  // against the RHS).  Bucket the RHS root too; the query dedups by slot
  // so the rule is offered once even when both faces match.
  if (unor) {
    FtPiFp rp;
    ftpi_fingerprint(rhs, &rp);
    if (rp.fp[0] == FTPI_FP_VAR) {
      ftpi_var_push(ix, i);   // dedup handles a double var-push
    } else {
      // Same root as LHS or distinct: in either case bucket the RHS arg
      // fingerprint so backward-only matches are not pruned.  A duplicate
      // slot in one bucket is deduped by the query.
      FtPiBucket *b = ftpi_bucket_get(ix, rp.fp[0]);
      ftpi_bucket_push(b, i, &rp);
    }
    // Tree RHS face: insert the RHS preorder under the same slot.  When
    // both faces lead to the same leaf the rule lands twice on that leaf;
    // the query's post-sort dedup removes the duplicate.
    ftdt_insert_pattern(ix, rhs, i, /*unor=*/1u, /*face=*/1u);
  }
}

// Refresh the concentration metric + build stamps after a (re)build.
static void ftpi_finalize(FtPiIndex *ix, AtpState *s) {
  u32 maxb = 0u, nidx = ix->n_var;
  for (u32 b = 0; b < ix->n_buckets; b++) {
    nidx += ix->buckets[b].n;
    if (ix->buckets[b].n > maxb) maxb = ix->buckets[b].n;
  }
  ix->max_bucket     = maxb;
  ix->n_indexed      = nidx;
  ix->owner          = s;
  ix->built_revision = s->r_revision;
  ix->built_n_rules  = s->n_rules;
}

static void ftpi_rebuild(AtpState *s) {
  FtPiIndex *ix = &g_ftpi;
  // Reset every bucket's entry count to 0 but KEEP the bucket structs
  // (their sym key + ents allocation) so the array and per-bucket
  // capacity are reused across rebuilds -- ftpi_bucket_get find-or-
  // appends, so a stale empty bucket simply gets refilled.  We do NOT
  // reset n_buckets (that would orphan the ents allocations); an empty
  // bucket left over from a now-absent symbol is harmless (it returns 0
  // candidates) and cheap.
  for (u32 b = 0; b < ix->n_buckets; b++) ix->buckets[b].n = 0u;
  ix->n_var = 0u;
  ftdt_reset(ix);             // tree rebuilt in lockstep with the buckets
  ftnfm_bump_gen();           // tree content changes: all memo entries die
  ix->rebuild_work += s->n_rules;
  for (u32 i = 0; i < s->n_rules; i++) ftpi_insert_rule(ix, s, i);
  ftpi_finalize(ix, s);
  ix->n_rebuilds++;
}

// Incremental append: insert only the newly-added tail [built_n_rules,
// n_rules).  Sound ONLY when the existing index is a clean prefix of the
// current rule set -- the same pure-append witness atp_ri_extend uses:
// every push bumps r_revision once, every drop / orient-flip / soft-
// delete bumps it WITHOUT growing n_rules, so the tail is a pure append
// iff the revision delta equals the count delta.  Otherwise an existing
// rule changed (e.g. a face was soft-deleted) and the index must be
// fully rebuilt.  Returns 1 if appended, 0 if a full rebuild is needed.
static int ftpi_try_extend(AtpState *s) {
  FtPiIndex *ix = &g_ftpi;
  if (ix->owner != s) return 0;
  if (ix->built_n_rules > s->n_rules) return 0;            // shrunk
  if (ix->built_n_rules == s->n_rules) {                   // count unchanged
    return ix->built_revision == s->r_revision;            // append iff nothing changed
  }
  if (s->r_revision - ix->built_revision != s->n_rules - ix->built_n_rules) {
    return 0;   // an existing rule was revised -> not a clean append
  }
  ix->rebuild_work += (s->n_rules - ix->built_n_rules);
  ftnfm_bump_gen();           // appended rules can revive a memoized-normal subtree
  for (u32 i = ix->built_n_rules; i < s->n_rules; i++) ftpi_insert_rule(ix, s, i);
  ftpi_finalize(ix, s);
  return 1;
}

// True iff the pruning index is applicable for THIS call: full rule
// range, AC matching not engaged, and a non-trivial rule set (the
// linear scan wins on a handful of rules).  Cached env disable knob for
// before/after measurement.
static int ftpi_applicable(const AtpState *s, u32 slice_first, u32 slice_end) {
  static int disabled = -1;
  if (disabled < 0) {
    const char *e = getenv("THVM_ATPFT_NO_PRUNE");
    disabled = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
  }
  if (disabled) return 0;
  if (slice_first != 0u || slice_end != s->n_rules) return 0;  // slice path
  if (s->n_rules < 16u) return 0;                              // tiny: scan wins
#if defined(THVM_ATP_AC) && defined(THVM_ATPFT_MATCH)
  if (thvm_atp_get_ac_mask() != 0ull) return 0;                // AC: matches modulo AC
#endif
  return 1;
}

// Ensure the index reflects the current rule set (rebuild on revision /
// count / owner change).  O(n_rules) amortised; far cheaper than the
// per-cell O(n_rules) scan it replaces.
static void ftpi_ensure(AtpState *s) {
  FtPiIndex *ix = &g_ftpi;
  if (ix->owner == s && ix->built_revision == s->r_revision &&
      ix->built_n_rules == s->n_rules) {
    return;
  }
  // Try the O(rules-added) append fast-path; fall back to a full rebuild
  // when an existing rule changed (the pure-append witness fails).
  if (ftpi_try_extend(s)) return;
  ftpi_rebuild(s);
}

// Engage the arg-fingerprint pruning only when a single root symbol
// dominates the rule set: the largest bucket holds at least half of all
// indexed rules AND is large in absolute terms (so the dominant-symbol
// linear scan would itself issue many ft_match calls).  This is exactly
// the single-operator regime (combinators / Sheffer) where ft_match's
// top-symbol fast-fail prunes nothing.  On a multi-operator signature
// max_bucket is a small fraction, so we keep the dense scan and pay zero
// index overhead.  The threshold is a PERFORMANCE heuristic only --
// soundness + order are guaranteed independently of which path runs.
static int ftpi_pruning_worthwhile(void) {
  FtPiIndex *ix = &g_ftpi;
  return ix->max_bucket >= 48u && ix->max_bucket * 2u >= ix->n_indexed;
}

// Fill the scratch candidate buffer with the rule slots compatible with
// `p` (a CTR subject cell), in ASCENDING slot order.  Returns the count;
// the candidates are in g_ftpi.cand[0..count).  Merges the var-rooted
// list with the matched root bucket (filtered by the arg0/arg1
// fingerprint), deduping the bucket's possible double RHS entry.
static u32 ftpi_candidates(AtpState *s, const AtpFtCell *p) {
  FtPiIndex *ix = &g_ftpi;
  FtPiFp sp;
  ftpi_fingerprint(p, &sp);
  FtPiBucket *b = (sp.fp[0] == FTPI_FP_VAR) ? NULL
                                            : ftpi_bucket_find(ix, sp.fp[0]);
  u32 bn = (b != NULL) ? b->n : 0u;
  u32 need = ix->n_var + bn;
  if (need > ix->cand_cap) {
    u32 nc = ix->cand_cap ? ix->cand_cap : 64u;
    while (nc < need) nc *= 2u;
    u32 *nb = (u32 *)realloc(ix->cand, nc * sizeof(u32));
    if (nb == NULL) thvm_fatal("ftpi: candidate buffer OOM");
    ix->cand = nb;
    ix->cand_cap = nc;
  }
  // Merge the two ascending lists (var_rules and the bucket's compatible
  // entries) into ascending order, deduping equal slots.
  u32 vi = 0u, bi = 0u, out = 0u;
  u32 last = 0xFFFFFFFFu;
  for (;;) {
    // Advance bucket index past arg-fingerprint-incompatible entries.
    // A rule survives only when EVERY sampled position is compatible
    // (rule-VAR / subject-VAR / ABSENT never prune; both-concrete-and-
    // differ prunes).
    while (bi < bn) {
      const FtPiEnt *e = &b->ents[bi];
      int ok = 1;
      for (u32 k = 0; k < FTPI_FP_ARG; k++) {
        if (!ftpi_pos_compatible(e->arg[k], sp.fp[k + 1u])) { ok = 0; break; }
      }
      if (ok) break;
      bi++;
    }
    u32 vr = (vi < ix->n_var) ? ix->var_rules[vi] : 0xFFFFFFFFu;
    u32 br = (bi < bn)        ? b->ents[bi].rule   : 0xFFFFFFFFu;
    if (vr == 0xFFFFFFFFu && br == 0xFFFFFFFFu) break;
    u32 pick;
    if (vr <= br) { pick = vr; vi++; }
    else          { pick = br; bi++; }
    if (pick != last) {   // dedup (var double-push or bucket double-entry)
      ix->cand[out++] = pick;
      last = pick;
    }
  }
  ix->q_queries++;
  ix->q_candidates += out;
  ix->q_rules_seen += s->n_rules;
  return out;
}

// --- True full-depth perfect discrimination tree (Regelbaum) ---------
//
// The depth-2 arg fingerprint above prunes the COMBINATOR case well
// (one root, but the `app(app(...),...)` left spine differs by depth 2),
// yet prunes NOTHING on the Sheffer / NAND class: every rule is
// nand-rooted with VARIABLES at the four sampled depth-2 grandchild
// positions, so ftpi_pos_compatible never fires and cand/query == n_rules
// (prune-ratio ~1.0).  Sheffer rules only diverge DEEPER -- e.g.
// nand(nand(x,nand(x,x)),y) vs nand(x,nand(nand(y,y),z)) agree to depth 2
// and split at depth 3+.  A fixed-depth fingerprint can never reach that.
//
// This is a TRUE perfect discrimination tree (WM Regelbaum,
// DSBaumOperationen): an edge-labelled trie over the rule-LHS flatterm
// PREORDER symbol string.  Each cell of a pattern contributes ONE edge:
//   * a concrete function symbol f  -> an edge labelled f (the subject
//     must carry f at this position to descend);
//   * a variable                    -> the single STAR edge `*` (the
//     pattern variable matches ANY subterm, so the subject SKIPS its
//     whole subterm under this edge).
// The path from the root to a leaf spells one pattern's full preorder;
// the leaf carries the ascending list of rule slots whose LHS (or, for
// unorientable rules, RHS) has exactly that preorder shape.
//
// A QUERY descends by the SUBJECT's own preorder, so it touches only the
// leaves of STRUCTURALLY-COMPATIBLE patterns and fails early on a
// non-matching cell -- O(subject preorder length) instead of O(n_rules).
//
// SOUNDNESS (superset of ft_match's matches).  At a subject cell sc:
//   * sc concrete (sym=f, arity a): a pattern matches here iff the
//     pattern has either f (then ft_match recurses into f's a children --
//     follow the f edge and descend into sc's children) OR a variable
//     (ft_match binds the whole sc subterm -- follow the STAR edge and
//     skip sc's subterm to sc->end->next).  Both edges are explored; no
//     match is dropped.  A concrete pattern symbol g != f at this
//     position would make ft_match fail (sym mismatch), and indeed no
//     g edge is followed -- the ONLY pruning, exactly ft_match's per-
//     position symbol/arity reject.
//   * sc a variable (rare on this path -- the redex root is ground, but
//     deeper CP/RHS cells can be vars): ft_match with a CONCRETE pattern
//     symbol vs a variable subject FAILS (pat CTR, subj var -> 0); only
//     a pattern variable matches.  So we follow ONLY the STAR edge,
//     skipping sc's subterm.  This is both sound and tighter.
// Arity is implied by the symbol (a constructor's arity is fixed per
// symbol id), so a same-symbol edge always has matching arity -- no
// separate arity check is needed in the tree.
//
// ORDER-PRESERVATION.  The tree returns a SET of candidate slots; the
// caller's selection (ascending slot scan + WM-DFS first-match) needs
// them in ASCENDING slot order, identical to the linear scan's visit
// order.  We collect leaf rule-lists during the descent (each leaf list
// is itself ascending) and SORT the merged candidate buffer ascending
// before returning.  The candidate SET is identical to the fingerprint's
// (both are sound supersets of ft_match's matches restricted by the
// structural reject), so the downstream winner is byte-identical.
//
// INCREMENTAL MAINTENANCE.  Built/extended in lockstep with the
// fingerprint (ftpi_ensure): a pure-append tail inserts only new rules'
// preorders; any revision-without-count change forces a full rebuild
// (same pure-append witness).  Add inserts a path + appends the slot to
// the leaf list (ascending, since rules are inserted in slot order);
// kill/orient-flip bump r_revision -> rebuild.  Node pool + leaf-list
// arena are reused across rebuilds (reset counts, keep allocations).
//
// AC mask / slice path: the tree, like the fingerprint, is consulted
// only on the full-range AC-off path (ftpi_applicable); AC matches
// modulo AC (a syntactic trie cannot capture that) and the slice path
// iterates a caller sub-range -- both fall back to the dense scan.

// A STAR (variable) edge is labelled with this reserved symbol; concrete
// edges carry the cell's real `sym` (always < WF_VAR_BIT, so distinct).
#define FTDT_STAR  0xFFFFFFFFu

// Variable edges are labelled FTDT_VAR0 + k, where k is the CANONICAL
// first-occurrence index of the variable within the inserted pattern
// face (WM Regelbaum semantics: x1's edge, x2's edge, ...).  The
// descent binds the subject subterm at V_k's first traversal and
// ft_eq-compares at every repeat -- enforcing NON-LINEAR variable
// consistency inside the tree, shared across every pattern in the
// subtree, instead of deferring it to a per-candidate ft_match that
// fails 99% of the time on the heavily non-linear Sheffer/Wolfram
// rule sets (measured 535M attempts / 4.3M hits on OrAssociativity).
// The candidate set becomes EXACTLY ft_match's acceptance set (the
// per-candidate ft_match in ft_cell_try_rules remains as the binding
// producer + final gate), so the downstream winner selection is
// byte-identical.  FTDT_STAR is retained as the defensive overflow
// label (pattern var id >= ATPFT_MAX_VARS, which ft_match rejects
// anyway): an unconditional skip edge, sound-superset like the
// historic linearized tree.  Concrete syms are < WF_VAR_BIT
// (0x80000000), so the [FTDT_VAR0, FTDT_STAR] label band is disjoint.
#define FTDT_VAR0  0xFFFFFF00u

// Sentinel for "no such child edge".
#define FTDT_NO_NODE 0xFFFFFFFFu

// One trie node.  Children are an unsorted small array of (label, child)
// edges -- arity here is tiny (<=2 for the indexed operators) and the
// branching factor is the live signature size, so a linear edge scan is
// cheaper than a hash.  Leaves (the rule slots whose preorder ENDS at this
// node) are PARTITIONED BY ORIENTATION CLASS: `leaf` holds oriented rules,
// `leaf_un` unorientable ones (each ascending; rules are inserted in slot
// order so plain appends keep them sorted).  `sub_mask` says which classes
// have a leaf anywhere at-or-below this node (bit 0 = oriented, bit 1 =
// unorientable), so a query that wants only one class -- the rules-only
// (doR / WM KPBehandelt) normalize is ~half of all queries and every one
// of its unorient candidates is a guaranteed pass-1 `!r_orient` skip --
// prunes whole subtrees at descent time instead of collecting, sorting,
// deduping and loop-skipping candidates its passes can never fire.  The
// class is fixed at insert time; any orient-flip / kill bumps r_revision
// which forces a full rebuild (the same pure-append witness the two-face
// unorientable insert already relies on), so insert-time class always
// equals query-time class.  Byte-identity: pass 1 skips unorient slots
// and pass 2 skips oriented slots BEFORE any subst_buf mutation, so
// removing them from the candidate list leaves each pass's visited
// sequence -- and the winner -- unchanged.
typedef struct FtDtNode_s {
  // Edge arrays are PARTITIONED for the query loop: var/star edges live
  // in [0, n_var_edge) (insert order), concrete edges in [n_var_edge,
  // n_edge) sorted ASCENDING by label.  A concrete-subject query then
  // scans only the var region plus a short early-break run of the
  // sorted concrete region for its one possible symbol edge; a var
  // subject skips the concrete region entirely.  Exploration order
  // across edges is free: the candidate buffer is sorted + deduped
  // before use and each (rule, face) has exactly one leaf/record, so
  // any edge order yields byte-identical results.
  //
  // Layout: the fields the descend's chain-follow loop touches lead the
  // struct (one likely-cold load per visited node instead of chasing
  // the labels/kids allocations), with label0/kid0 mirroring
  // labels[0]/kids[0] so a single-edge hop never leaves the header.
  u32  n_edge;
  u32  label0;     // labels[0] mirror (valid when n_edge >= 1)
  u32  kid0;       // kids[0] mirror
  u32  n_var_edge; // var/star edges at the front of labels/kids
  u32  leaf_n, leaf_un_n;
  u8   sub_mask;   // classes present in this subtree (1=orient, 2=unorient)
  u32  cap_edge, leaf_cap, leaf_un_cap;
  u32 *labels;     // edge labels (concrete sym, FTDT_VAR0+k, or FTDT_STAR)
  u32 *kids;       // child node indices, parallel to labels
  u8  *kid_mask;   // kids[e]'s sub_mask, mirrored parent-side so the
                   // query's per-edge prune test reads this sequential
                   // byte array instead of fetching each child's node
                   // struct (a random ~90B line per considered edge).
                   // Maintained at the two insert-path sites only
                   // (ftdt_edge_get seeds 0, ftdt_insert_pattern ORs the
                   // class bit alongside the child's own sub_mask).
  u32 *leaf;       // ascending ORIENTED rule slots ending at this node
  u32 *leaf_aux;   // parallel: (idmap_ofs << 1) | face  (see FtDtRec)
  u32 *leaf_un;    // ascending UNORIENTABLE rule slots ending at this node
  u32 *leaf_un_aux;
} FtDtNode;

// Per-query descent-binding record: one per (rule, face) the descent
// accepted this query.  Because the tree enforces full non-linear
// variable consistency (the FTDT_VAR0 bind/ft_eq discipline), a
// STAR-free path acceptance IS an ft_match acceptance -- so the caller
// reconstructs ft_match's substitution from the leaf-time canonical
// bindings (dtb_snap) + the pattern face's insert-time canon->var-id
// map (dt_idmap) instead of re-running ft_match per candidate
// (measured 5.29M re-verify passes per OrAssociativity run).
// bound_ids fill order = canonical order = pattern-preorder first-
// occurrence order = exactly ft_match's binding order, so the
// resulting AtpFtSubst is byte-equal in every observable field
// (bind[], bound_ids[0..wm), wm).  THVM_ATPFT_DTB_CHECK=1 certifies
// that per candidate against a real ft_match.
typedef struct FtDtRec_s {
  u32 rule;        // rule slot
  u32 snap_ofs;    // offset into dtb_snap (nvar cells)
  u32 idmap_ofs;   // offset into dt_idmap (nvar u16 ids)
  u16 nvar;        // distinct pattern vars (== path var count)
  u8  face;        // 0 = LHS pattern, 1 = RHS pattern (unorientable bwd)
} FtDtRec;

// Candidate-class want bits (ftdt_candidates / ftdt_descend).
#define FTDT_WANT_ORIENT   1u
#define FTDT_WANT_UNORIENT 2u


// Allocate a fresh empty node, returning its index.  Node 0 is the root.
// Slots below the high-water mark (dt_hi_nodes) are REUSED with their
// existing labels/kids/leaf allocations -- just zero the live counts --
// so a rebuild does not leak the previous build's per-node buffers; only
// genuinely new slots get fresh (NULL) buffers.
static u32 ftdt_node_new(FtPiIndex *ix) {
  u32 i = ix->dt_n_nodes;
  if (i < ix->dt_hi_nodes) {              // reuse an already-allocated slot
    FtDtNode *nd = &ix->dt_nodes[i];
    nd->n_edge = 0u;
    nd->n_var_edge = 0u;
    nd->leaf_n = 0u;
    nd->leaf_un_n = 0u;
    nd->sub_mask = 0u;
    ix->dt_n_nodes++;
    return i;
  }
  if (ix->dt_n_nodes == ix->dt_cap_nodes) {
    u32 nc = ix->dt_cap_nodes ? ix->dt_cap_nodes * 2u : 64u;
    FtDtNode *nn = (FtDtNode *)realloc(ix->dt_nodes, nc * sizeof(FtDtNode));
    if (nn == NULL) thvm_fatal("ftdt: node pool OOM");
    ix->dt_nodes = nn;
    ix->dt_cap_nodes = nc;
  }
  FtDtNode *nd = &ix->dt_nodes[i];
  nd->labels = NULL; nd->kids = NULL; nd->kid_mask = NULL;
  nd->n_edge = 0u; nd->cap_edge = 0u;
  nd->n_var_edge = 0u;
  nd->leaf = NULL; nd->leaf_aux = NULL; nd->leaf_n = 0u; nd->leaf_cap = 0u;
  nd->leaf_un = NULL; nd->leaf_un_aux = NULL;
  nd->leaf_un_n = 0u; nd->leaf_un_cap = 0u;
  nd->sub_mask = 0u;
  ix->dt_n_nodes++;
  ix->dt_hi_nodes = ix->dt_n_nodes;
  return i;
}

// Find the child of `node` reached by `label`, or FTDT_NO_NODE.
static u32 ftdt_edge_find(const FtPiIndex *ix, u32 node, u32 label) {
  const FtDtNode *nd = &ix->dt_nodes[node];
  for (u32 e = 0; e < nd->n_edge; e++) {
    if (nd->labels[e] == label) return nd->kids[e];
  }
  return FTDT_NO_NODE;
}

// OR `bit` into node's parent-side kid_mask mirror for the edge
// labelled `label` (insert path only -- cold; the query loop reads the
// mirror instead of the child's node struct).
static void ftdt_edge_mask_or(FtPiIndex *ix, u32 node, u32 label, u8 bit) {
  FtDtNode *nd = &ix->dt_nodes[node];
  for (u32 e = 0; e < nd->n_edge; e++) {
    if (nd->labels[e] == label) { nd->kid_mask[e] |= bit; return; }
  }
}

// Find-or-create the child of `node` reached by `label`; returns its
// index.  Realloc may move ix->dt_nodes, so re-fetch the node pointer
// after any node allocation.
static u32 ftdt_edge_get(FtPiIndex *ix, u32 node, u32 label) {
  u32 hit = ftdt_edge_find(ix, node, label);
  if (hit != FTDT_NO_NODE) return hit;
  u32 child = ftdt_node_new(ix);   // may realloc dt_nodes
  FtDtNode *nd = &ix->dt_nodes[node];
  if (nd->n_edge == nd->cap_edge) {
    u32 nc = nd->cap_edge ? nd->cap_edge * 2u : 4u;
    u32 *nl = (u32 *)realloc(nd->labels, nc * sizeof(u32));
    u32 *nk = (u32 *)realloc(nd->kids,   nc * sizeof(u32));
    u8  *nm = (u8  *)realloc(nd->kid_mask, nc * sizeof(u8));
    if (nl == NULL || nk == NULL || nm == NULL) {
      thvm_fatal("ftdt: edge array OOM");
    }
    nd->labels = nl;
    nd->kids   = nk;
    nd->kid_mask = nm;
    nd->cap_edge = nc;
  }
  // Keep the query partition (see FtDtNode): a var/star edge appends at
  // the end of the var region, a concrete edge at its ascending-label
  // slot within the concrete region.  Insert path only -- cold.
  u32 pos;
  if (label >= FTDT_VAR0) {
    pos = nd->n_var_edge;
    nd->n_var_edge++;
  } else {
    pos = nd->n_var_edge;
    while (pos < nd->n_edge && nd->labels[pos] < label) pos++;
  }
  memmove(&nd->labels[pos + 1u], &nd->labels[pos],
          (nd->n_edge - pos) * sizeof(u32));
  memmove(&nd->kids[pos + 1u], &nd->kids[pos],
          (nd->n_edge - pos) * sizeof(u32));
  memmove(&nd->kid_mask[pos + 1u], &nd->kid_mask[pos],
          (nd->n_edge - pos) * sizeof(u8));
  nd->labels[pos] = label;
  nd->kids[pos]   = child;
  nd->kid_mask[pos] = 0u;   // fresh child; insert_pattern ORs the class bit
  nd->n_edge++;
  nd->label0 = nd->labels[0];   // keep the header mirror current
  nd->kid0   = nd->kids[0];
  return child;
}

// Append rule slot `rule` to node `node`'s ascending leaf list, with
// its parallel aux word `(idmap_ofs << 1) | face` (see FtDtRec).
static void ftdt_leaf_push(FtPiIndex *ix, u32 node, u32 rule, u8 unor,
                           u32 aux) {
  FtDtNode *nd = &ix->dt_nodes[node];
  u32 **lp  = unor ? &nd->leaf_un     : &nd->leaf;
  u32 **ap  = unor ? &nd->leaf_un_aux : &nd->leaf_aux;
  u32  *np  = unor ? &nd->leaf_un_n   : &nd->leaf_n;
  u32  *cp  = unor ? &nd->leaf_un_cap : &nd->leaf_cap;
  if (*np == *cp) {
    u32 nc = *cp ? *cp * 2u : 4u;
    u32 *nv = (u32 *)realloc(*lp, nc * sizeof(u32));
    u32 *na = (u32 *)realloc(*ap, nc * sizeof(u32));
    if (nv == NULL || na == NULL) thvm_fatal("ftdt: leaf list OOM");
    *lp = nv;
    *ap = na;
    *cp = nc;
  }
  (*ap)[*np]   = aux;
  (*lp)[(*np)++] = rule;
}

// Append var id `id` to the insert-time canon->id map pool.
static void ftdt_idmap_push(FtPiIndex *ix, u16 id) {
  if (ix->dt_idmap_n == ix->dt_idmap_cap) {
    u32 nc = ix->dt_idmap_cap ? ix->dt_idmap_cap * 2u : 256u;
    u16 *nv = (u16 *)realloc(ix->dt_idmap, nc * sizeof(u16));
    if (nv == NULL) thvm_fatal("ftdt: idmap pool OOM");
    ix->dt_idmap = nv;
    ix->dt_idmap_cap = nc;
  }
  ix->dt_idmap[ix->dt_idmap_n++] = id;
}

// Insert one PATTERN (lhs/rhs flatterm) into the tree under rule slot
// `rule`.  Walks the pattern preorder via `next`; each cell emits one
// edge (concrete sym, or FTDT_VAR0 + canonical first-occurrence index
// for a variable -- see the FTDT_VAR0 header).  The terminal node
// gets `rule` appended to its leaf list.  `face` (0 = LHS, 1 = RHS)
// plus the canon->var-id slice appended to dt_idmap here (one id per
// first occurrence, in canonical order) let the query rebuild
// ft_match's substitution from the descent bindings (see FtDtRec).
static void ftdt_insert_pattern(FtPiIndex *ix, const AtpFtCell *pat, u32 rule,
                                u8 unor, u8 face) {
  const AtpFtCell *end_after = (pat->end != NULL) ? pat->end->next : NULL;
  u32 node = 0u;   // root
  u8  class_bit = unor ? (u8)FTDT_WANT_UNORIENT : (u8)FTDT_WANT_ORIENT;
  ix->dt_nodes[0].sub_mask |= class_bit;
  u32 idmap_ofs = ix->dt_idmap_n;
  // Per-face canonical variable numbering: pattern var id -> first-
  // occurrence index.  Faces are canonicalized independently (the
  // descent's binding array is per-query, keyed by this index).
  u16 canon[ATPFT_MAX_VARS];
  memset(canon, 0xff, sizeof canon);
  u32 nvar = 0u;
  for (const AtpFtCell *p = pat; p != NULL && p != end_after; p = p->next) {
    u32 label;
    if ((p->sym & WF_VAR_BIT) != 0u) {
      u32 id = p->sym & ~WF_VAR_BIT;
      if (id < ATPFT_MAX_VARS) {
        if (canon[id] == 0xffffu) {
          canon[id] = (u16)nvar++;
          ftdt_idmap_push(ix, (u16)id);
        }
        label = FTDT_VAR0 + (u32)canon[id];
      } else {
        // ft_match rejects ids >= ATPFT_MAX_VARS; a STAR edge keeps the
        // tree a sound superset for this (never-matching) pattern.  The
        // descent marks any STAR path impure, so no binding record is
        // ever produced for it (the caller then treats it as the
        // non-match ft_match would return).
        label = FTDT_STAR;
      }
    } else {
      label = p->sym;
    }
    u32 parent = node;
    node = ftdt_edge_get(ix, node, label);   // may realloc; node index stable
    ix->dt_nodes[node].sub_mask |= class_bit;
    ftdt_edge_mask_or(ix, parent, label, class_bit);   // parent-side mirror
  }
  ftdt_leaf_push(ix, node, rule, unor, (idmap_ofs << 1) | (u32)face);
}

// Reset the tree to a single empty root, reusing the node pool +
// per-node allocations.  Called at the start of a full rebuild.
static void ftdt_reset(FtPiIndex *ix) {
  ix->dt_n_nodes = 0u;
  ix->dt_idmap_n = 0u;       // idmap slices are re-appended with the patterns
  (void)ftdt_node_new(ix);   // node 0 = root (reuses slot 0 if present)
}

// Snapshot the current canonical bindings bind[0..nbound) into the
// per-query pool; returns the snapshot offset.
static u32 ftdt_snap_push(FtPiIndex *ix, const AtpFtCell **bind, u32 nbound) {
  if (ix->dtb_snap_n + nbound > ix->dtb_snap_cap) {
    u32 nc = ix->dtb_snap_cap ? ix->dtb_snap_cap : 128u;
    while (nc < ix->dtb_snap_n + nbound) nc *= 2u;
    const AtpFtCell **nb =
        (const AtpFtCell **)realloc(ix->dtb_snap, nc * sizeof(*nb));
    if (nb == NULL) thvm_fatal("ftdt: binding snapshot pool OOM");
    ix->dtb_snap = nb;
    ix->dtb_snap_cap = nc;
  }
  u32 ofs = ix->dtb_snap_n;
  for (u32 k = 0; k < nbound; k++) ix->dtb_snap[ofs + k] = bind[k];
  ix->dtb_snap_n += nbound;
  return ofs;
}

// Append one per-query acceptance record (see FtDtRec).
static void ftdt_rec_push(FtPiIndex *ix, u32 rule, u32 snap_ofs, u32 aux,
                          u16 nvar) {
  if (ix->dtb_rec_n == ix->dtb_rec_cap) {
    u32 nc = ix->dtb_rec_cap ? ix->dtb_rec_cap * 2u : 64u;
    FtDtRec *nb = (FtDtRec *)realloc(ix->dtb_rec, nc * sizeof(FtDtRec));
    if (nb == NULL) thvm_fatal("ftdt: binding record pool OOM");
    ix->dtb_rec = nb;
    ix->dtb_rec_cap = nc;
  }
  FtDtRec *r = &ix->dtb_rec[ix->dtb_rec_n++];
  r->rule      = rule;
  r->snap_ofs  = snap_ofs;
  r->idmap_ofs = aux >> 1;
  r->nvar      = nvar;
  r->face      = (u8)(aux & 1u);
}

// Collect the leaf rule slots of node `node` into the candidate buffer --
// only the orientation classes in `want` (the emit order across classes is
// irrelevant: the caller sorts + dedups the whole candidate list).
static void ftdt_emit_leaf_class(FtPiIndex *ix, const u32 *lf, const u32 *aux,
                                 u32 n, u32 *out, u32 snap_ofs, u16 nvar,
                                 u8 rec_on) {
  if (*out + n > ix->cand_cap) {
    u32 nc = ix->cand_cap ? ix->cand_cap : 64u;
    while (nc < *out + n) nc *= 2u;
    u32 *nb = (u32 *)realloc(ix->cand, nc * sizeof(u32));
    if (nb == NULL) thvm_fatal("ftdt: candidate buffer OOM");
    ix->cand = nb;
    ix->cand_cap = nc;
  }
  for (u32 i = 0; i < n; i++) {
    ix->cand[(*out)++] = lf[i];
    if (rec_on) ftdt_rec_push(ix, lf[i], snap_ofs, aux[i], nvar);
  }
}
static void ftdt_emit_leaf(FtPiIndex *ix, u32 node, u32 *out, u8 want,
                           const AtpFtCell **bind, u32 nbound, u8 pure) {
  const FtDtNode *nd = &ix->dt_nodes[node];
  u32 n_or = (want & FTDT_WANT_ORIENT)   ? nd->leaf_n    : 0u;
  u32 n_un = (want & FTDT_WANT_UNORIENT) ? nd->leaf_un_n : 0u;
  if (n_or + n_un == 0u) return;
  // One snapshot per leaf emission: every pattern ending here shares the
  // path, hence the same canonical bindings.  A STAR (impure) path never
  // records -- ft_match rejects those patterns, and the caller maps
  // "no record" to "no match".
  u32 snap_ofs = pure ? ftdt_snap_push(ix, bind, nbound) : 0u;
  if (n_or)
    ftdt_emit_leaf_class(ix, nd->leaf, nd->leaf_aux, n_or, out,
                         snap_ofs, (u16)nbound, pure);
  if (n_un)
    ftdt_emit_leaf_class(ix, nd->leaf_un, nd->leaf_un_aux, n_un, out,
                         snap_ofs, (u16)nbound, pure);
}

// Recursive descent: match the subject preorder [sc, sc_end) against the
// tree rooted at `node`, appending every reachable leaf's rule slots to
// ix->cand[0..*out).  `sc` is the current subject cell, `sc_end` is the
// one-past cell of the WHOLE redex subterm (descent succeeds only when a
// pattern consumes EXACTLY up to sc_end -- mirroring ft_match matching
// the ENTIRE lhs against the ENTIRE redex).
//
//   * sc == sc_end  -> the subject is fully consumed; this node's leaf
//     list is a candidate set (those patterns whose preorder ended here).
//   * sc concrete f -> follow the f edge (descend into sc's children, the
//     preorder continues at sc->next) AND every variable edge V_k: on
//     V_k's FIRST traversal bind[k] takes sc's whole subterm (skip to
//     sc->end->next, unbind on backtrack); on a REPEAT traversal descend
//     only when the bound subterm ft_eq-equals sc's -- the same
//     consistency test ft_match applies to a repeated pattern variable.
//   * sc a variable -> only variable edges (a concrete pattern symbol
//     cannot match a variable subject; ft_match returns 0 there).
//
// `bind[k]` is the subject subterm bound to canonical pattern variable
// k on the current descent path (NULL = unbound).  The bind/unbind
// bracket around each V_k first-traversal recursion restores the array
// to all-NULL by the time the outermost call returns, so the caller's
// (statically zeroed) array needs no per-query reset.
// `nbound` counts the distinct canonical vars bound along the current
// path (first occurrences are numbered 0,1,2,... in path order at
// insert time, so a first traversal of V_k always has k == nbound and
// bind[0..nbound) are exactly the path's bindings).  `pure` clears on
// any STAR (overflow) edge: bindings under it are incomplete, so no
// acceptance record is emitted for those leaves.
// Loop-invariant descent state, threaded as ONE pointer so each
// recursion passes (ctx, node, sc, nbound, pure) instead of nine
// arguments -- the recursion fires once per visited node (~400M/run).
typedef struct {
  FtPiIndex       *ix;
  const AtpFtCell *sc_end;
  u32             *out;
  const AtpFtCell **bind;
  u8               want;
} FtDtDesc;

static void ftdt_descend(const FtDtDesc *d, u32 node,
                         const AtpFtCell *sc, u32 nbound, u8 pure) {
  FtPiIndex *ix = d->ix;
  const FtDtNode *nd = &ix->dt_nodes[node];
  const u8 want = d->want;
  // Single-concrete-edge chain follow: most tree nodes sit on unshared
  // pattern tails (one outgoing edge, concrete label).  Following such a
  // chain needs no backtracking -- either the subject cell carries the
  // same symbol (advance both in lockstep) or the whole subtree fails.
  // Iterating here instead of recursing per cell removes the dominant
  // call/frame cost of the walk (the descend was the top self-sample at
  // ~3.5s); the visited (node, subject-cell) sequence and every verdict
  // are unchanged, so candidates/records are byte-identical.  Chain
  // nodes can still carry leaves (a shorter pattern ending mid-chain):
  // the sc == sc_end emit check runs per node exactly as before.
  // (Prefix-compressed label runs were built + A/B'd 2026-07-03 and
  // REFUTED: runs vs per-node step measured 23.3s vs 23.4s -- on the
  // single-op signatures the walk concentrates in the bushy shared-
  // prefix region, not on chains, so the run machinery was reverted.)
  u32 dp = 0u;
  if (g_ftdt_pd) dp = g_ftdt_dp;
  for (;;) {
    // Orientation-class prune: no leaf of a wanted class anywhere at-or-
    // below this node -- nothing this subtree could emit survives the
    // caller's pass guards, so skip it entirely.
    if ((nd->sub_mask & want) == 0u) {
      if (g_ftdt_pd) g_atp_pushn_dt_prune++;
      return;
    }
    if (g_ftdt_pd) {
      g_atp_pushn_dt_node++;
      g_atp_pushn_dt_hist[(dp < ATP_DT_HIST_N - 1u) ? dp
                                                    : ATP_DT_HIST_N - 1u]++;
    }
    if (sc == d->sc_end) {            // subject consumed: emit this node's leaves
      if (g_ftdt_pd) g_atp_pushn_dt_emit++;
      ftdt_emit_leaf(ix, node, d->out, want, d->bind, nbound, pure);
      return;
    }
    if (nd->n_edge != 1u) break;
    u32 label0 = nd->label0;
    if (label0 >= FTDT_VAR0) break;
    if ((sc->sym & WF_VAR_BIT) != 0u || label0 != sc->sym) return;
    if (g_ftdt_pd) { g_atp_pushn_dt_chain++; dp++; }
    node = nd->kid0;
    nd   = &ix->dt_nodes[node];
    sc   = sc->next;
  }
  // Branch node.  Var/star edges sit in [0, n_var_edge) and are always
  // explored; the concrete region [n_var_edge, n_edge) is sorted
  // ascending, so a concrete subject probes it with an early-break run
  // for its ONE possible symbol edge and a var subject skips it whole
  // (a concrete pattern symbol never matches a variable subject --
  // exactly ft_match's reject, as before).
  const AtpFtCell *skip = (sc->end != NULL) ? sc->end->next : d->sc_end;
  const AtpFtCell **bind = d->bind;
  u32 nv = nd->n_var_edge;
  if (g_ftdt_pd) {
    g_atp_pushn_dt_edge += nd->n_edge;
    g_atp_pushn_dt_branch++;
  }
  for (u32 e = 0; e < nv; e++) {
    u32 label = nd->labels[e];
    if (label == FTDT_STAR) {
      // Defensive overflow edge (see FTDT_VAR0): unconditional skip.
      if (g_ftdt_pd) g_ftdt_dp = dp + 1u;
      ftdt_descend(d, nd->kids[e], skip, nbound, 0u);
      continue;
    }
    u32 k = label - FTDT_VAR0;
    if (bind[k] == NULL) {
      bind[k] = sc;
      if (g_ftdt_pd) { g_atp_pushn_dt_bind1++; g_ftdt_dp = dp + 1u; }
      ftdt_descend(d, nd->kids[e], skip,
                   (k + 1u > nbound) ? k + 1u : nbound, pure);
      bind[k] = NULL;
    } else {
      if (g_ftdt_pd) {
        g_atp_pushn_dt_fteq++;
        g_atp_pushn_dt_fteq_cell += ftdt_prof_cells(sc);
      }
      if (ft_eq(bind[k], sc)) {
        if (g_ftdt_pd) { g_atp_pushn_dt_fteq_ok++; g_ftdt_dp = dp + 1u; }
        ftdt_descend(d, nd->kids[e], skip, nbound, pure);
      }
    }
  }
  if ((sc->sym & WF_VAR_BIT) == 0u) {
    for (u32 e = nv; e < nd->n_edge; e++) {
      u32 label = nd->labels[e];
      if (g_ftdt_pd) g_atp_pushn_dt_cprobe++;
      if (label < sc->sym) continue;
      if (label == sc->sym) {
        if (g_ftdt_pd) g_ftdt_dp = dp + 1u;
        ftdt_descend(d, nd->kids[e], sc->next, nbound, pure);
      }
      break;
    }
  }
}

// Redesigned walk (default; THVM_NO_FT_DTV2=1 reverts to ftdt_descend
// for the same-binary A/B).  Same visited-pattern acceptance set,
// records and bindings as ftdt_descend -- the candidate list is sorted
// + deduped downstream, so exploration order is free and only the SET
// must match (certified by THVM_ATPFT_DT_CHECK / DTB_CHECK).  Three
// structural changes, all driven by the 2026-07-03 deep counters
// (bind1=257M fteq=373M[84.7% fail] prune=99.6M cprobe=117M on
// OrAssociativity -- the cost is var-edge exploration in the bushy
// mid-tree, NOT concrete dispatch):
//   1. CHILD-PRUNE HOIST: the parent tests the child's sub_mask
//      before binding / ft_eq / recursing, so the ~100M recursions
//      into want-barren subtrees (25% of all visits) never happen and
//      their preceding repeat-compares are skipped too.
//   2. SINGLE-VAR-EDGE CHAIN FOLLOW: a node whose only edge is V_k is
//      followed iteratively like the concrete chain -- bind (logged in
//      a local undo list, unwound at every exit) or repeat-compare,
//      then advance.  The deep pattern tails are almost pure var
//      chains (shape dump: d>=11 edges are ~100% var), so this kills
//      the recursion+bind/unbind bracket for the tail region.
//   3. FIRST-CELL FAIL-FAST: a repeat compare first tests the two root
//      syms inline (sym mismatch decides 85% of repeats; the average
//      failing subtree is ~2.5 cells) and only calls ft_eq on a root
//      match.  Identical verdict: ft_eq's first lockstep step is this
//      exact compare (same-sym CTR cells always agree on arity).
static void ftdt_descend2(const FtDtDesc *d, u32 node,
                          const AtpFtCell *sc, u32 nbound, u8 pure) {
  FtPiIndex *ix = d->ix;
  const FtDtNode *nd = &ix->dt_nodes[node];
  const u8 want = d->want;
  const AtpFtCell **bind = d->bind;
  // Local bind undo log: canonical var ids bound by the iterative var
  // chain (each id binds at most once per path, so <= ATPFT_MAX_VARS).
  u16 ub[ATPFT_MAX_VARS];
  u32 un = 0u;
  u32 dp = 0u;
  if (g_ftdt_pd) dp = g_ftdt_dp;
  // Entry prune: only the root call arrives unchecked -- recursions
  // below pre-check the child mask before calling.
  if ((nd->sub_mask & want) == 0u) {
    if (g_ftdt_pd) g_atp_pushn_dt_prune++;
    return;
  }
  for (;;) {
    if (g_ftdt_pd) {
      g_atp_pushn_dt_node++;
      g_atp_pushn_dt_hist[(dp < ATP_DT_HIST_N - 1u) ? dp
                                                    : ATP_DT_HIST_N - 1u]++;
    }
    if (sc == d->sc_end) {            // subject consumed: emit this node's leaves
      if (g_ftdt_pd) g_atp_pushn_dt_emit++;
      ftdt_emit_leaf(ix, node, d->out, want, bind, nbound, pure);
      goto out;
    }
    if (nd->n_edge != 1u) break;
    u32 label0 = nd->label0;
    if (label0 < FTDT_VAR0) {
      // Single concrete edge: lockstep hop (as ftdt_descend's chain).
      if ((sc->sym & WF_VAR_BIT) != 0u || label0 != sc->sym) goto out;
      const FtDtNode *kd = &ix->dt_nodes[nd->kid0];
      if ((kd->sub_mask & want) == 0u) {
        if (g_ftdt_pd) g_atp_pushn_dt_prune++;
        goto out;
      }
      if (g_ftdt_pd) { g_atp_pushn_dt_chain++; dp++; }
      node = nd->kid0;
      nd   = kd;
      sc   = sc->next;
      continue;
    }
    if (label0 == FTDT_STAR) break;   // defensive overflow edge: branch path
    // Single variable edge V_k: follow iteratively.
    u32 k = label0 - FTDT_VAR0;
    const AtpFtCell *bk = bind[k];
    if (bk != NULL) {
      if (g_ftdt_pd) {
        g_atp_pushn_dt_fteq++;
        g_atp_pushn_dt_fteq_cell += ftdt_prof_cells(sc);
      }
      if (bk->sym != sc->sym) goto out;       // first-cell fail-fast
      if (!ft_eq(bk, sc)) goto out;
      if (g_ftdt_pd) g_atp_pushn_dt_fteq_ok++;
    }
    const FtDtNode *kd = &ix->dt_nodes[nd->kid0];
    if ((kd->sub_mask & want) == 0u) {
      if (g_ftdt_pd) g_atp_pushn_dt_prune++;
      goto out;
    }
    if (bk == NULL) {
      if (g_ftdt_pd) g_atp_pushn_dt_bind1++;
      bind[k] = sc;
      ub[un++] = (u16)k;
      if (k + 1u > nbound) nbound = k + 1u;
    }
    if (g_ftdt_pd) { g_atp_pushn_dt_chain++; dp++; }
    node = nd->kid0;
    nd   = kd;
    sc   = (sc->end != NULL) ? sc->end->next : d->sc_end;
  }
  // Branch node (or single STAR edge).  Var/star region first, then
  // the ascending concrete region -- as ftdt_descend, with the child
  // sub_mask tested BEFORE any bind / ft_eq / recursion.
  {
    const AtpFtCell *skip = (sc->end != NULL) ? sc->end->next : d->sc_end;
    u32 nv = nd->n_var_edge;
    if (g_ftdt_pd) {
      g_atp_pushn_dt_edge += nd->n_edge;
      g_atp_pushn_dt_branch++;
    }
    for (u32 e = 0; e < nv; e++) {
      u32 label = nd->labels[e];
      if (label == FTDT_STAR) {
        // Defensive overflow edge (see FTDT_VAR0): unconditional skip.
        if ((ix->dt_nodes[nd->kids[e]].sub_mask & want) == 0u) {
          if (g_ftdt_pd) g_atp_pushn_dt_prune++;
          continue;
        }
        if (g_ftdt_pd) g_ftdt_dp = dp + 1u;
        ftdt_descend2(d, nd->kids[e], skip, nbound, 0u);
        continue;
      }
      u32 k = label - FTDT_VAR0;
      const AtpFtCell *bk = bind[k];
      if (bk != NULL) {
        if (g_ftdt_pd) {
          g_atp_pushn_dt_fteq++;
          g_atp_pushn_dt_fteq_cell += ftdt_prof_cells(sc);
        }
        if (bk->sym != sc->sym) continue;     // first-cell fail-fast
        if (!ft_eq(bk, sc)) continue;
        if (g_ftdt_pd) g_atp_pushn_dt_fteq_ok++;
      }
      if ((ix->dt_nodes[nd->kids[e]].sub_mask & want) == 0u) {
        if (g_ftdt_pd) g_atp_pushn_dt_prune++;
        continue;
      }
      if (bk == NULL) {
        if (g_ftdt_pd) { g_atp_pushn_dt_bind1++; g_ftdt_dp = dp + 1u; }
        bind[k] = sc;
        ftdt_descend2(d, nd->kids[e], skip,
                      (k + 1u > nbound) ? k + 1u : nbound, pure);
        bind[k] = NULL;
      } else {
        if (g_ftdt_pd) g_ftdt_dp = dp + 1u;
        ftdt_descend2(d, nd->kids[e], skip, nbound, pure);
      }
    }
    if ((sc->sym & WF_VAR_BIT) == 0u) {
      for (u32 e = nv; e < nd->n_edge; e++) {
        u32 label = nd->labels[e];
        if (g_ftdt_pd) g_atp_pushn_dt_cprobe++;
        if (label < sc->sym) continue;
        if (label == sc->sym) {
          if ((ix->dt_nodes[nd->kids[e]].sub_mask & want) != 0u) {
            if (g_ftdt_pd) g_ftdt_dp = dp + 1u;
            ftdt_descend2(d, nd->kids[e], sc->next, nbound, pure);
          } else if (g_ftdt_pd) {
            g_atp_pushn_dt_prune++;
          }
        }
        break;
      }
    }
  }
out:
  while (un > 0u) bind[ub[--un]] = NULL;
}

// v3 walk: ftdt_descend2 with two mechanical changes, byte-identical in
// exploration order, candidate set, and binding records (kill switch
// THVM_NO_FT_DTV3=1 reverts to ftdt_descend2; the THVM_FT_DTV2_DIFF
// harness certifies the default walk against the classic per query):
//   1. CONCRETE-EDGE TAIL ITERATION: at a branch node the (at most one)
//      matching concrete edge is the LAST exploration step -- descend2
//      recursed into it and immediately unwound.  v3 folds it back into
//      the outer for(;;) as an iterative hop, eliminating the call
//      bracket for the most common branch continuation (~10^8
//      recursions per OrAssociativity batch).  Var-region bindings made
//      before the hop stay logged in the SAME frame's undo list and
//      unwind at the shared exit, exactly as they would across the
//      recursive call.
//   2. PARENT-SIDE CHILD-MASK MIRROR: every per-edge prune test reads
//      nd->kid_mask[e] (a sequential byte array maintained at insert
//      time) instead of ix->dt_nodes[kids[e]].sub_mask (a dependent
//      random load of the child's ~90B struct per considered edge).
static void ftdt_descend3(const FtDtDesc *d, u32 node,
                          const AtpFtCell *sc, u32 nbound, u8 pure) {
  FtPiIndex *ix = d->ix;
  const FtDtNode *nd = &ix->dt_nodes[node];
  const u8 want = d->want;
  const AtpFtCell **bind = d->bind;
  u16 ub[ATPFT_MAX_VARS];
  u32 un = 0u;
  u32 dp = 0u;
  if (g_ftdt_pd) dp = g_ftdt_dp;
  // Entry prune: only the root call arrives unchecked -- recursions and
  // iterative hops below pre-check the child mask before advancing.
  if ((nd->sub_mask & want) == 0u) {
    if (g_ftdt_pd) g_atp_pushn_dt_prune++;
    return;
  }
  for (;;) {
    if (g_ftdt_pd) {
      g_atp_pushn_dt_node++;
      g_atp_pushn_dt_hist[(dp < ATP_DT_HIST_N - 1u) ? dp
                                                    : ATP_DT_HIST_N - 1u]++;
    }
    if (sc == d->sc_end) {            // subject consumed: emit this node's leaves
      if (g_ftdt_pd) g_atp_pushn_dt_emit++;
      ftdt_emit_leaf(ix, node, d->out, want, bind, nbound, pure);
      goto out;
    }
    if (nd->n_edge == 1u) {
      u32 label0 = nd->label0;
      if (label0 < FTDT_VAR0) {
        // Single concrete edge: lockstep hop (as descend2's chain).
        if ((sc->sym & WF_VAR_BIT) != 0u || label0 != sc->sym) goto out;
        if ((nd->kid_mask[0] & want) == 0u) {
          if (g_ftdt_pd) g_atp_pushn_dt_prune++;
          goto out;
        }
        if (g_ftdt_pd) { g_atp_pushn_dt_chain++; dp++; }
        node = nd->kid0;
        nd   = &ix->dt_nodes[node];
        sc   = sc->next;
        continue;
      }
      if (label0 != FTDT_STAR) {
        // Single variable edge V_k: follow iteratively.
        u32 k = label0 - FTDT_VAR0;
        const AtpFtCell *bk = bind[k];
        if (bk != NULL) {
          if (g_ftdt_pd) {
            g_atp_pushn_dt_fteq++;
            g_atp_pushn_dt_fteq_cell += ftdt_prof_cells(sc);
          }
          if (bk->sym != sc->sym) goto out;     // first-cell fail-fast
          if (!ft_eq(bk, sc)) goto out;
          if (g_ftdt_pd) g_atp_pushn_dt_fteq_ok++;
        }
        if ((nd->kid_mask[0] & want) == 0u) {
          if (g_ftdt_pd) g_atp_pushn_dt_prune++;
          goto out;
        }
        if (bk == NULL) {
          if (g_ftdt_pd) g_atp_pushn_dt_bind1++;
          bind[k] = sc;
          ub[un++] = (u16)k;
          if (k + 1u > nbound) nbound = k + 1u;
        }
        if (g_ftdt_pd) { g_atp_pushn_dt_chain++; dp++; }
        node = nd->kid0;
        nd   = &ix->dt_nodes[node];
        sc   = (sc->end != NULL) ? sc->end->next : d->sc_end;
        continue;
      }
      // Single STAR edge: fall through to the branch path below.
    }
    // Branch node (or single STAR edge).  Var/star region first, then
    // the ascending concrete region.  All per-edge prune tests read the
    // parent-side kid_mask mirror.
    {
      const AtpFtCell *skip = (sc->end != NULL) ? sc->end->next : d->sc_end;
      u32 nv = nd->n_var_edge;
      if (g_ftdt_pd) {
        g_atp_pushn_dt_edge += nd->n_edge;
        g_atp_pushn_dt_branch++;
      }
      for (u32 e = 0; e < nv; e++) {
        u32 label = nd->labels[e];
        if (label == FTDT_STAR) {
          if ((nd->kid_mask[e] & want) == 0u) {
            if (g_ftdt_pd) g_atp_pushn_dt_prune++;
            continue;
          }
          if (g_ftdt_pd) g_ftdt_dp = dp + 1u;
          ftdt_descend3(d, nd->kids[e], skip, nbound, 0u);
          continue;
        }
        u32 k = label - FTDT_VAR0;
        const AtpFtCell *bk = bind[k];
        if (bk != NULL) {
          if (g_ftdt_pd) {
            g_atp_pushn_dt_fteq++;
            g_atp_pushn_dt_fteq_cell += ftdt_prof_cells(sc);
          }
          if (bk->sym != sc->sym) continue;     // first-cell fail-fast
          if (!ft_eq(bk, sc)) continue;
          if (g_ftdt_pd) g_atp_pushn_dt_fteq_ok++;
        }
        if ((nd->kid_mask[e] & want) == 0u) {
          if (g_ftdt_pd) g_atp_pushn_dt_prune++;
          continue;
        }
        if (bk == NULL) {
          if (g_ftdt_pd) { g_atp_pushn_dt_bind1++; g_ftdt_dp = dp + 1u; }
          bind[k] = sc;
          ftdt_descend3(d, nd->kids[e], skip,
                        (k + 1u > nbound) ? k + 1u : nbound, pure);
          bind[k] = NULL;
        } else {
          if (g_ftdt_pd) g_ftdt_dp = dp + 1u;
          ftdt_descend3(d, nd->kids[e], skip, nbound, pure);
        }
      }
      if ((sc->sym & WF_VAR_BIT) == 0u) {
        for (u32 e = nv; e < nd->n_edge; e++) {
          u32 label = nd->labels[e];
          if (g_ftdt_pd) g_atp_pushn_dt_cprobe++;
          if (label < sc->sym) continue;
          if (label == sc->sym) {
            if ((nd->kid_mask[e] & want) != 0u) {
              // Last exploration step at this node: iterate instead of
              // recursing (descend2 recursed here and then fell out).
              if (g_ftdt_pd) dp++;
              node = nd->kids[e];
              nd   = &ix->dt_nodes[node];
              sc   = sc->next;
              goto next_node;
            }
            if (g_ftdt_pd) g_atp_pushn_dt_prune++;
          }
          break;
        }
      }
    }
    goto out;
  next_node:;
  }
out:
  while (un > 0u) bind[ub[--un]] = NULL;
}

// Order-preserving ascending-u32 sort of a[0..n) -- the byte-identical
// replacement for `qsort(a, n, sizeof(u32), ftdt_u32_cmp)`.  An ascending
// sort over distinct-or-not u32 keys has a UNIQUE result, so the dedup that
// follows is unaffected by which sort produces it.  Two regimes:
//   * n < FTDT_SORT_INS: inlined insertion sort -- no qsort function-pointer
//     indirection (the comparator was the hottest sampled leaf at ~800M
//     calls; the small-n bucket is the bulk of them).
//   * n >= FTDT_SORT_INS: 8-bit LSD radix with the pass count adapted to the
//     max key (rule slots live in [0, n_rules), typically < 65536 -> 2
//     passes).  Uses ix->cand_tmp as the ping-pong buffer.
enum { FTDT_SORT_INS = 24u };
static void ftdt_sort_u32(FtPiIndex *ix, u32 *a, u32 n) {
  if (n < FTDT_SORT_INS) {
    for (u32 i = 1u; i < n; i++) {
      u32 v = a[i], j = i;
      while (j > 0u && a[j - 1u] > v) { a[j] = a[j - 1u]; j--; }
      a[j] = v;
    }
    return;
  }
  // Max key -> number of 8-bit passes (1 for <256, 2 for <65536, ...).
  u32 mx = a[0];
  for (u32 i = 1u; i < n; i++) if (a[i] > mx) mx = a[i];
  u32 passes = 1u;
  for (u64 lim = 0xffull; mx > lim && passes < 4u; lim = (lim << 8) | 0xffull)
    passes++;
  if (ix->cand_tmp_cap < n) {
    u32 nc = ix->cand_tmp_cap ? ix->cand_tmp_cap : 64u;
    while (nc < n) nc *= 2u;
    u32 *nb = (u32 *)realloc(ix->cand_tmp, (size_t)nc * sizeof(u32));
    if (nb == NULL) thvm_fatal("ftdt: radix scratch OOM");
    ix->cand_tmp = nb;
    ix->cand_tmp_cap = nc;
  }
  u32 *src = a, *dst = ix->cand_tmp;
  for (u32 pass = 0u; pass < passes; pass++) {
    u32 shift = pass * 8u;
    u32 cnt[256] = {0};
    for (u32 i = 0u; i < n; i++) cnt[(src[i] >> shift) & 0xffu]++;
    u32 acc = 0u;
    for (u32 b = 0u; b < 256u; b++) { u32 c = cnt[b]; cnt[b] = acc; acc += c; }
    for (u32 i = 0u; i < n; i++) dst[cnt[(src[i] >> shift) & 0xffu]++] = src[i];
    u32 *t = src; src = dst; dst = t;
  }
  // If an odd number of passes left the sorted data in cand_tmp, copy back
  // so the caller's `a` (== ix->cand) holds the result.
  if (src != a)
    for (u32 i = 0u; i < n; i++) a[i] = src[i];
}

// One-shot static tree-shape dump at process teardown (armed with the
// THVM_ATPFT_PRUNE_STATS printout): per-depth node / edge / leaf
// aggregates of the FINAL tree build, so the bushy region is readable
// next to the per-depth visit histogram the descend collects.
static void ftdt_shape_dump(void) {
  FtPiIndex *ix = &g_ftpi;
  u32 n = ix->dt_n_nodes;
  if (n == 0u) return;
  enum { SHN = 24 };
  u64 nodes[SHN] = {0}, edges[SHN] = {0}, vedges[SHN] = {0};
  u64 leaves[SHN] = {0};
  u32 *bfs   = (u32 *)malloc((size_t)n * sizeof(u32));
  u32 *depth = (u32 *)malloc((size_t)n * sizeof(u32));
  if (bfs == NULL || depth == NULL) { free(bfs); free(depth); return; }
  u32 head = 0u, tail = 0u;
  bfs[tail] = 0u; depth[tail] = 0u; tail++;
  u64 tot_edges = 0u, tot_leaves = 0u, max_d = 0u;
  while (head < tail) {
    u32 nd_i = bfs[head], d = depth[head]; head++;
    const FtDtNode *nd = &ix->dt_nodes[nd_i];
    u32 db = (d < SHN - 1u) ? d : SHN - 1u;
    if (d > max_d) max_d = d;
    nodes[db]++;
    edges[db]  += nd->n_edge;
    vedges[db] += nd->n_var_edge;
    leaves[db] += nd->leaf_n + nd->leaf_un_n;
    tot_edges  += nd->n_edge;
    tot_leaves += nd->leaf_n + nd->leaf_un_n;
    for (u32 e = 0; e < nd->n_edge && tail < n; e++) {
      bfs[tail] = nd->kids[e]; depth[tail] = d + 1u; tail++;
    }
  }
  fprintf(stderr,
          "[ftdt shape] nodes=%u (hi=%u) edges=%llu leaves=%llu maxdepth=%llu "
          "node-bytes=%zu pool=%.1fKB\n",
          n, ix->dt_hi_nodes, (unsigned long long)tot_edges,
          (unsigned long long)tot_leaves, (unsigned long long)max_d,
          sizeof(FtDtNode), (double)ix->dt_hi_nodes * sizeof(FtDtNode) / 1024.0);
  for (u32 d = 0; d < SHN; d++) {
    if (nodes[d] == 0u) continue;
    fprintf(stderr,
            "[ftdt shape]   d=%-2u nodes=%-6llu edges=%-6llu var-edges=%-5llu "
            "leaves=%llu\n",
            d, (unsigned long long)nodes[d], (unsigned long long)edges[d],
            (unsigned long long)vedges[d], (unsigned long long)leaves[d]);
  }
  free(bfs); free(depth);
}

// Tree-content generation stamp for the subtree-NF memo below (see
// FtNfmEnt): bumped on EVERY index rebuild/extend (any rule add/kill/
// flip changes the "is this subtree normal" verdict function, so all
// memo entries die with the generation).
static u64 g_ftnfm_gen = 1u;

static void ftnfm_bump_gen(void) { g_ftnfm_gen++; }

// Fill ix->cand[0..count) with the rule slots whose LHS/RHS preorder is
// STRUCTURALLY COMPATIBLE with subject cell `p`, in ASCENDING slot order
// with duplicates removed (a rule appears in two leaves only via the
// unorientable LHS+RHS double-insert; the dedup offers it once).  This is
// a SOUND SUPERSET of ft_match's matches at `p`, in the same ascending
// order the linear scan visits -- so the caller's first-match / WM-DFS
// selection picks the byte-identical winner.
static u32 ftdt_candidates(AtpState *s, const AtpFtCell *p, u8 want,
                           u8 rec_on) {
  FtPiIndex *ix = &g_ftpi;
  const AtpFtCell *p_end = (p->end != NULL) ? p->end->next : NULL;
  u32 out = 0u;
  // Canonical-variable binding array for the consistency-checked descent.
  // Statically zeroed once; ftdt_descend's bind/unbind bracket restores
  // every slot to NULL before returning (no early exits), so no per-query
  // reset is needed.
  static const AtpFtCell *ftdt_bind[ATPFT_MAX_VARS];
  ix->dtb_snap_n = 0u;      // per-query binding capture (see FtDtRec)
  ix->dtb_rec_n  = 0u;
  g_ftdt_pd = (u8)(g_atp_phase_detail && g_atp_pushn_active);
  if (g_ftdt_pd) g_ftdt_dp = 0u;
  // `pure` doubles as the recording gate: a caller that will not
  // consume the records (THVM_NO_FT_DTB sweep) starts the descent
  // impure, so no snapshot/record is ever taken -- the candidate list
  // itself is unchanged either way.
  FtDtDesc d = { ix, p_end, &out, ftdt_bind, want };
  // Default walk = ftdt_descend3 (descend2 + concrete-edge tail
  // iteration + parent-side kid_mask prunes); THVM_NO_FT_DTV3=1 falls
  // back to ftdt_descend2, THVM_NO_FT_DTV2=1 to the classic
  // ftdt_descend -- same-binary A/B ladder.  Identical candidate /
  // record SET at every rung.
  static int dtv2_off = -1;
  static int dtv3_off = -1;
  if (dtv2_off < 0) {
    const char *e = getenv("THVM_NO_FT_DTV2");
    dtv2_off = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
    const char *e3 = getenv("THVM_NO_FT_DTV3");
    dtv3_off = (e3 != NULL && e3[0] != '0' && e3[0] != '\0') ? 1 : 0;
  }
  // Divergence harness (THVM_FT_DTV2_DIFF=1): run BOTH walks per query
  // and fatal on the first candidate-set difference, dumping the
  // subject preorder + both sets.  Diagnostic only.
  static int dtv2_diff = -1;
  if (dtv2_diff < 0) {
    const char *e = getenv("THVM_FT_DTV2_DIFF");
    dtv2_diff = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
  }
  if (dtv2_diff) {
    u32 out1 = 0u;
    FtDtDesc d1 = { ix, p_end, &out1, ftdt_bind, want };
    ftdt_descend(&d1, 0u, p, 0u, 0u);   // classic, impure (no records)
    static u32 c1[65536];
    u32 n1 = (out1 < 65536u) ? out1 : 65536u;
    for (u32 i = 0; i < n1; i++) c1[i] = ix->cand[i];
    ftdt_sort_u32(ix, c1, n1);
    out1 = 0u;   // discard; rerun v2 below fills for real
    ix->dtb_snap_n = 0u;
    ix->dtb_rec_n  = 0u;
    u32 out2 = 0u;
    FtDtDesc d2 = { ix, p_end, &out2, ftdt_bind, want };
    if (dtv3_off) ftdt_descend2(&d2, 0u, p, 0u, 0u);
    else          ftdt_descend3(&d2, 0u, p, 0u, 0u);
    static u32 c2[65536];
    u32 n2 = (out2 < 65536u) ? out2 : 65536u;
    for (u32 i = 0; i < n2; i++) c2[i] = ix->cand[i];
    ftdt_sort_u32(ix, c2, n2);
    int bad = (n1 != n2);
    for (u32 i = 0; !bad && i < n1; i++) bad = (c1[i] != c2[i]);
    if (bad) {
      fprintf(stderr, "[dtv2 DIFF] want=%u classic n=%u:", want, n1);
      for (u32 i = 0; i < n1 && i < 32u; i++) fprintf(stderr, " %u", c1[i]);
      fprintf(stderr, "\n[dtv2 DIFF] v2 n=%u:", n2);
      for (u32 i = 0; i < n2 && i < 32u; i++) fprintf(stderr, " %u", c2[i]);
      fprintf(stderr, "\n[dtv2 DIFF] subject:");
      for (const AtpFtCell *c = p; c != NULL && c != p_end; c = c->next)
        fprintf(stderr, " %x/%u", c->sym, (u32)c->arity);
      fprintf(stderr, "\n");
      u32 miss = (n1 > 0u) ? c1[0] : ((n2 > 0u) ? c2[0] : 0u);
      const AtpFtCell *ml = s->lhs_ft[miss];
      if (ml != NULL) {
        const AtpFtCell *me = (ml->end != NULL) ? ml->end->next : NULL;
        fprintf(stderr, "[dtv2 DIFF] rule %u lhs:", miss);
        for (const AtpFtCell *c = ml; c != NULL && c != me; c = c->next)
          fprintf(stderr, " %x/%u", c->sym, (u32)c->arity);
        fprintf(stderr, "\n");
      }
      thvm_fatal("dtv2: candidate-set divergence");
    }
    ix->dtb_snap_n = 0u;
    ix->dtb_rec_n  = 0u;
  }
  if (dtv2_off)      ftdt_descend (&d, 0u, p, 0u, rec_on ? 1u : 0u);
  else if (dtv3_off) ftdt_descend2(&d, 0u, p, 0u, rec_on ? 1u : 0u);
  else               ftdt_descend3(&d, 0u, p, 0u, rec_on ? 1u : 0u);
  if (out > 1u) {
    ftdt_sort_u32(ix, ix->cand, out);
    u32 w = 1u;
    for (u32 r = 1u; r < out; r++) {
      if (ix->cand[r] != ix->cand[w - 1u]) ix->cand[w++] = ix->cand[r];
    }
    out = w;
  }
  ix->q_queries++;
  ix->q_candidates += out;
  ix->q_rules_seen += s->n_rules;
  // THVM_ATPFT_DT_CHECK=1: airtight superset proof.  For EVERY live rule,
  // if ft_match accepts its LHS (or, for an unorientable rule, its RHS)
  // against `p`, that slot MUST appear in the tree candidate set -- else
  // the tree dropped a real match (unsound).  O(n_rules) per query, so
  // gated off by default; run it on a saturation to certify soundness.
  static int dt_check = -1;
  if (dt_check < 0) {
    const char *e = getenv("THVM_ATPFT_DT_CHECK");
    dt_check = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
  }
  if (dt_check) {
    AtpFtSubst chk = {0};
    for (u32 r = 0; r < s->n_rules; r++) {
      if (s->r_dead != NULL && s->r_dead[r]) continue;
      AtpFtCell *lhs = s->lhs_ft[r];
      AtpFtCell *rhs = s->rhs_ft[r];
      if (lhs == NULL || rhs == NULL) continue;
      u8 unor = (u8)(s->n_unorient > 0u && !s->r_orient[r]);
      ft_subst_reset(&chk);
      int m = ft_match(lhs, p, &chk);
      if (!m && unor) { ft_subst_reset(&chk); m = ft_match(rhs, p, &chk); }
      if (!m) continue;
      // The tree emits only the caller's wanted orientation classes
      // (FTDT_WANT_*): a matching rule of an un-wanted class is
      // guaranteed-skipped by the caller's pass guards and is correctly
      // absent from the candidate set.
      u8 r_class = unor ? (u8)FTDT_WANT_UNORIENT : (u8)FTDT_WANT_ORIENT;
      if ((r_class & want) == 0u) continue;
      int found = 0;
      for (u32 k = 0; k < out; k++) if (ix->cand[k] == r) { found = 1; break; }
      if (!found) {
        fprintf(stderr, "[ftdt SOUNDNESS VIOLATION] rule %u (orient=%u) "
                "ft_matches p but is NOT in the tree candidate set "
                "(out=%u want=%u)\n", r, (unsigned)s->r_orient[r], out,
                (unsigned)want);
        thvm_fatal("ftdt: unsound (dropped a real match)");
      }
    }
  }
  return out;
}

// Find this query's acceptance record for (rule, face), or NULL.  The
// record list length equals the (tiny) per-query emission count, so a
// linear scan beats any keyed structure here.
static const FtDtRec *ftdt_rec_find(const FtPiIndex *ix, u32 rule, u8 face) {
  for (u32 k = 0; k < ix->dtb_rec_n; k++) {
    const FtDtRec *r = &ix->dtb_rec[k];
    if (r->rule == rule && r->face == face) return r;
  }
  return NULL;
}

// Rebuild ft_match's substitution from a descent acceptance record:
// reset, then bind each pattern var id (insert-time canon->id map) to
// its snapshotted subject subterm, in canonical order == ft_match's
// first-occurrence binding order.  The resulting AtpFtSubst is
// identical in every observable field (bind[], bound_ids[0..wm), wm).
static void ftdt_fill_subst(const FtPiIndex *ix, const FtDtRec *rec,
                            AtpFtSubst *out) {
  ft_subst_reset(out);
  const AtpFtCell **snap = ix->dtb_snap + rec->snap_ofs;
  const u16       *ids   = ix->dt_idmap + rec->idmap_ofs;
  for (u16 k = 0; k < rec->nvar; k++) {
    u32 id = ids[k];
    out->bind[id]           = (AtpFtCell *)snap[k];
    out->bound_ids[out->wm] = id;
    out->wm                += 1u;
  }
}

// THVM_ATPFT_DTB_CHECK=1: certify, per consulted candidate, that the
// descent record agrees with a REAL ft_match -- presence iff ft_match
// accepts, and on acceptance the reconstructed substitution is
// field-identical.  O(|pattern|) per candidate, so gated off by
// default; run a saturation with it on to certify the fused path.
static void ftdt_dtb_verify(const FtPiIndex *ix, const FtDtRec *rec,
                            const AtpFtCell *pat, const AtpFtCell *p) {
  static AtpFtSubst chk;   // static zero-init satisfies first-use contract
  ft_subst_reset(&chk);
  int m = ft_match(pat, p, &chk);
  if (m != (rec != NULL)) {
    fprintf(stderr, "[ftdt DTB MISMATCH] ft_match=%d rec=%p\n", m, (void *)rec);
    thvm_fatal("ftdt: descent-binding acceptance diverges from ft_match");
  }
  if (!m) return;
  static AtpFtSubst fil;
  ftdt_fill_subst(ix, rec, &fil);
  if (fil.wm != chk.wm) {
    fprintf(stderr, "[ftdt DTB MISMATCH] wm fill=%u match=%u\n", fil.wm, chk.wm);
    thvm_fatal("ftdt: descent-binding count diverges from ft_match");
  }
  for (u32 k = 0; k < chk.wm; k++) {
    if (fil.bound_ids[k] != chk.bound_ids[k] ||
        fil.bind[chk.bound_ids[k]] != chk.bind[chk.bound_ids[k]]) {
      fprintf(stderr, "[ftdt DTB MISMATCH] slot %u id fill=%u match=%u\n",
              k, fil.bound_ids[k], chk.bound_ids[k]);
      thvm_fatal("ftdt: descent bindings diverge from ft_match");
    }
  }
}

// Per-cell redex attempt -- the body shared by the legacy pre-order
// scan (find_redex_ft) and the WM mixmost walk
// (atp_normalize_mixmost_ft).  Tries every live oriented rule in the
// slice AT cell `p`, then (try_unorient) every unorientable equation
// under the ordered grounded-instance gate.  Fills subst/rule/dir on
// hit; never inspects other cells.  Variable cells never match.
//
// Rule choice when SEVERAL patterns match at the cell: the default
// engine keeps first-match-wins in slice order (the historic
// behaviour).  Under use_mixmost_nf the winner is WM's -- the
// Regelbaum DFS minimum per ft_wm_pattern_before, newest slot on
// alpha-identical ties -- because on a non-confluent mid-completion R
// different matching rules at the SAME position reach different
// normal forms (HigmanNeumann Associativity cp 597: fop(x1,fop(x2,x2))
// ->x1 keeps the shared variable and joins the CP, WM's more-specific
// fop(fop(x1,x1),fop(x2,x3))->fop(x3,x2) keeps it distinct and queues
// it).
static int ft_cell_try_rules(AtpState   *s,
                             AtpFtCell  *p,
                             u32         slice_first,
                             u32         slice_end,
                             u8          try_orient,
                             u8          try_unorient,
                             AtpFtSubst *subst_buf,
                             u32        *rule_out,
                             u8         *dir_out) {
  // Detail-tier shape counters (THVM_ATP_PROFILE=2, push-scope only) --
  // increments only; no timers in this per-position hot path.
  int _pd = (g_atp_phase_detail && g_atp_pushn_active);
  if (_pd) g_atp_pushn_tr_q++;
  // A free var is never a redex on the subject side.
  if ((p->sym & WF_VAR_BIT) != 0u) {
    if (_pd) g_atp_pushn_tr_var++;
    return 0;
  }
  u8 have_unorient = (u8)(s->n_unorient > 0u);
  // A/B gate for the orient-pass winner re-match skip (default ON).
  // THVM_NO_FT_REMATCH_SKIP=1 forces the unconditional re-match so an
  // ON-vs-OFF sweep proves the skip is byte-identical, never just faster.
  static int no_rematch_skip = -1;
  if (no_rematch_skip < 0) {
    const char *e = getenv("THVM_NO_FT_REMATCH_SKIP");
    no_rematch_skip = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  // Arena handle for the AC-match dispatch (AC-chain bindings allocate
  // into the scratch arena).  NULL-safe: the dispatch only deref's it
  // when the AC path actually fires.
  AtpFt *ft_arena_local = (AtpFt *)s->ft_arena_ptr;
  // Candidate-pruning index (ftpi_*): on the full-range / AC-off hot
  // path, replace the [slice_first, slice_end) iteration with only the
  // rules structurally compatible with `p`, in ascending slot order.
  // The candidate list is a SOUND SUPERSET in the SAME order the linear
  // scan visits, so the per-rule gates + selection below pick the
  // byte-identical winner; only guaranteed non-matches are skipped.
  // When the index is not applicable (slice / AC / tiny R), use_pi is 0
  // and the loops fall back to the dense [slice_first, slice_end) range.
  const u32 *pi_cand = NULL;
  u32        pi_ncand = 0u;
  int        use_pi   = 0;
  // Descent-binding fused acceptance: when the candidates came from the
  // discrimination tree (exact acceptance + per-candidate FtDtRec), the
  // per-candidate ft_match re-verify is skipped entirely -- record
  // presence IS the match verdict and the winner's substitution is
  // rebuilt from the record (ftdt_fill_subst, field-identical to
  // ft_match's).  THVM_NO_FT_DTB=1 forces the historic per-candidate
  // ft_match for an ON-vs-OFF byte-identity sweep;
  // THVM_ATPFT_DTB_CHECK=1 certifies every consulted record against a
  // real ft_match.
  int        use_dtb  = 0;
  static int dtb_off = -1;
  static int dtb_check = -1;
  if (dtb_off < 0) {
    const char *e = getenv("THVM_NO_FT_DTB");
    dtb_off = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
    const char *c = getenv("THVM_ATPFT_DTB_CHECK");
    dtb_check = (c != NULL && c[0] != '0' && c[0] != '\0') ? 1 : 0;
  }
  if (ftpi_applicable(s, slice_first, slice_end)) {
    ftpi_stats_arm();
    ftpi_ensure(s);
    // Engage pruning only when one root symbol dominates the rule set
    // (top-symbol fast-fail is then useless and a structural index is
    // the real lever).  See FtPiIndex.max_bucket.  A multi-operator
    // signature keeps the dense scan (ft_match's first per-position
    // compare already fast-fails on the root).
    if (ftpi_pruning_worthwhile()) {
      // Default: the full-depth perfect discrimination tree (ftdt_*),
      // which discriminates Sheffer rules at their deeper concrete
      // positions where the depth-2 fingerprint (ftpi_candidates) prunes
      // nothing.  THVM_NO_DISCTREE=1 reverts to the fingerprint path for
      // ON-vs-OFF byte-identity measurement.  Both return the identical
      // candidate SET in ascending slot order (the tree just prunes
      // deeper), so the winner below is byte-identical either way.
      static int no_disctree = -1;
      if (no_disctree < 0) {
        const char *e = getenv("THVM_NO_DISCTREE");
        no_disctree = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
      }
      use_pi   = 1;
      // Candidate classes this call can actually fire: pass 1 skips every
      // unorient slot, pass 2 every oriented one, both before touching
      // subst_buf -- so the tree emits only the wanted classes (the
      // rules-only doR normalize stops collecting + loop-skipping the
      // var-general unorientable faces entirely).  The fingerprint
      // fallback stays an unpartitioned superset; its extra candidates
      // die at the same guards.
      u8 want = (u8)((try_orient ? FTDT_WANT_ORIENT : 0u) |
                     ((try_unorient && have_unorient) ? FTDT_WANT_UNORIENT
                                                      : 0u));
      use_dtb  = !no_disctree && !dtb_off;
      pi_ncand = no_disctree ? ftpi_candidates(s, p)
                             : ftdt_candidates(s, p, want, (u8)use_dtb);
      pi_cand  = g_ftpi.cand;
    }
  }
  // Iteration bound: candidate count when indexed, else the dense slice
  // width.  FTPI_RULE(k) maps the loop counter to the rule slot.
#define FTPI_NITER (use_pi ? pi_ncand : (slice_end - slice_first))
#define FTPI_RULE(k) (use_pi ? pi_cand[(k)] : (slice_first + (k)))
  // WM per-position redex priority (BL_RegelOderGleichungAngewendet,
  // NF/NFBildung.c:503-531, and the path-based NFB_ variant :219-238
  // that the CLI default `-nf mixmost` installs): at every position
  // the rule tree (Regelbaum) is consulted BEFORE the equation tree
  // (Gleichungsbaum) -- an unorientable equation fires at this cell
  // only when NO oriented rule matches here.  Pass 1: oriented rules
  // in slice order (first match wins), or in Regelbaum DFS order
  // under use_mixmost_nf (see ft_wm_pattern_before).
  if (try_orient) {
    u32 best_rule = (u32)-1;
    const FtDtRec *best_rec = NULL;   // winner's descent record (use_dtb)
    // subst_buf holds best_rule's bindings iff no later attempt has run
    // ft_subst_reset since best_rule's match.  When the winner is the LAST
    // candidate the scan touches, that re-match is redundant -- skip it.
    // (Match-path only; the use_dtb path never touches subst_buf until
    // the single winner fill below.)
    u8  best_subst_live = 0u;
    u32 niter = FTPI_NITER;
    if (_pd) g_atp_pushn_tr_cand += niter;
    for (u32 k = 0; k < niter; k++) {
      u32 r = FTPI_RULE(k);
      // Filter dead (bwd-subsumed) rules first: their lhs_ft/rhs_ft are
      // overwritten with a sentinel FVR (id 255) at deletion time, which
      // an unorientable backward-direction match could spuriously bind.
      // The Term-side indexed paths (atp_ri_rebuild) skip them; mirror
      // that here so the linear FT scan agrees.
      if (s->r_dead != NULL && s->r_dead[r]) continue;
      AtpFtCell *lhs = s->lhs_ft[r];
      if (lhs == NULL || s->rhs_ft[r] == NULL) continue;
      if (have_unorient && !s->r_orient[r]) continue;
      // Forward rewrite, no order check needed (oriented).
      if (_pd) g_atp_pushn_tr_att++;
      const FtDtRec *rec = NULL;
      int matched;
      if (use_dtb) {
        rec = ftdt_rec_find(&g_ftpi, r, /*face=*/0u);
        if (dtb_check) ftdt_dtb_verify(&g_ftpi, rec, lhs, p);
        matched = (rec != NULL);
      } else {
        ft_subst_reset(subst_buf);   // clobbers any prior winner's bindings
        best_subst_live = 0u;
        matched = ft_match_maybe_ac(ft_arena_local, lhs, p, subst_buf);
      }
      if (matched) {
        if (_pd) g_atp_pushn_tr_hit++;
        if (!s->use_mixmost_nf) {
          if (use_dtb) ftdt_fill_subst(&g_ftpi, rec, subst_buf);
          *rule_out = r;
          *dir_out  = 0u;
          return 1;
        }
        // Ascending slot scan + "cand wins unless best strictly
        // precedes it" = DFS minimum with newest-slot tie-break.
        if (_pd && best_rule != (u32)-1) g_atp_pushn_tr_pb++;
        if (best_rule == (u32)-1 ||
            !ft_wm_pattern_before(s->lhs_ft[best_rule], lhs)) {
          best_rule = r;
          best_rec  = rec;
          best_subst_live = 1u;   // subst_buf now holds best_rule's match
        }
      }
    }
    if (best_rule != (u32)-1) {
      if (_pd) g_atp_pushn_tr_win++;
      if (use_dtb) {
        // Winner substitution rebuilt from its descent record -- the
        // per-candidate matches AND the winner re-match are all gone.
        ftdt_fill_subst(&g_ftpi, best_rec, subst_buf);
        *rule_out = best_rule;
        *dir_out  = 0u;
        return 1;
      }
      // Re-match the winner unless subst_buf still carries its bindings
      // (winner was the last candidate the loop matched + no later reset).
      if (best_subst_live && !no_rematch_skip) {
        *rule_out = best_rule;
        *dir_out  = 0u;
        return 1;
      }
      if (_pd) g_atp_pushn_tr_rematch++;
      ft_subst_reset(subst_buf);
      if (ft_match_maybe_ac(ft_arena_local, s->lhs_ft[best_rule], p,
                            subst_buf)) {
        *rule_out = best_rule;
        *dir_out  = 0u;
        return 1;
      }
    }
  }
  // Pass 2: unorientable equations -- reached only when no oriented
  // rule matched at this cell.  Same retrieval-order convention as
  // pass 1 (Gleichungsbaum = the same DSBaum machinery,
  // MO_GleichungGefunden): first passing candidate in slice order by
  // default, DFS minimum over the MATCHED face's pattern under
  // use_mixmost_nf.
  if (try_unorient && have_unorient) {
    u32 best_rule = (u32)-1;
    u8  best_dir  = 0u;
    // Lazy redex pre-encode: thvm_kbo_ft_subst encodes both sides on
    // every call; we attempt up to `slice_count` equations at this
    // cell, each running the unorient gate with the SAME redex `p`.
    // Pre-encode once, reuse across attempts.  redex_na==0 means
    // "not yet encoded"; the FIRST gate call lazily fills it.
    u32 redex_na = 0u;
    u32 niter = FTPI_NITER;
    for (u32 k = 0; k < niter; k++) {
      u32 r = FTPI_RULE(k);
      if (s->r_dead != NULL && s->r_dead[r]) continue;
      AtpFtCell *lhs = s->lhs_ft[r];
      AtpFtCell *rhs = s->rhs_ft[r];
      if (lhs == NULL || rhs == NULL) continue;
      if (s->r_orient[r]) continue;
      // Unorientable equation: try forward (l -> r) then backward (r -> l).
      // Forward: lhs matches p; rhs vars NOT bound by the lhs match
      // (extension variables) are grounded to the minimal constant --
      // ft_subst_ground_extras, the WM free-variable instance;
      // instantiated repl < redex under the reduction order.  Streaming
      // KBO via thvm_kbo_ft_subst_with_prepared; THVM_ATPFT_KBO_DIFF=1
      // runs a side-by-side Term-side atp_compare to surface verdict
      // divergences (a probe across the AC bench found zero -- the
      // streaming KBO matches atp_compare on the unorient gate inputs).
      // Under use_dtb each face's match verdict + bindings come from
      // the descent record (fill resets subst_buf first).
      const FtDtRec *rec_f = NULL, *rec_b = NULL;
      if (use_dtb) {
        rec_f = ftdt_rec_find(&g_ftpi, r, /*face=*/0u);
        rec_b = ftdt_rec_find(&g_ftpi, r, /*face=*/1u);
        if (dtb_check) {
          ftdt_dtb_verify(&g_ftpi, rec_f, lhs, p);
          ftdt_dtb_verify(&g_ftpi, rec_b, rhs, p);
        }
      }
      int fwd_matched;
      if (use_dtb) {
        fwd_matched = (rec_f != NULL);
        if (fwd_matched) ftdt_fill_subst(&g_ftpi, rec_f, subst_buf);
      } else {
        ft_subst_reset(subst_buf);
        fwd_matched = ft_match_maybe_ac(ft_arena_local, lhs, p, subst_buf);
      }
      if (fwd_matched) {
        if (ft_subst_ground_extras(s, rhs, subst_buf)) {
          if (redex_na == 0u) redex_na = thvm_kbo_ft_subst_prepare_redex(p, s->kbo);
          KboCmp ft_v = thvm_kbo_ft_subst_with_prepared(redex_na, rhs, subst_buf, s->kbo);
          static int dbg_diff = -1;
          if (dbg_diff < 0) dbg_diff = getenv("THVM_ATPFT_KBO_DIFF") != NULL ? 1 : 0;
          if (dbg_diff) {
            AtpFt *arena_chk = (AtpFt *)s->ft_arena_ptr;
            AtpFtCell *repl = ft_subst_apply(arena_chk, rhs, subst_buf, 1);
            if (repl != NULL) {
              Term t_p    = ft_to_term(p);
              Term t_repl = ft_to_term(repl);
              KboCmp tt_v = atp_compare(s, t_p, t_repl);
              if (ft_v != tt_v) {
                fprintf(stderr,
                        "[KBO DIFF fwd] ft=%d tt=%d  p=0x%016llx  sigma_r=0x%016llx\n",
                        (int)ft_v, (int)tt_v,
                        (unsigned long long)t_p,
                        (unsigned long long)t_repl);
              }
            }
          }
          if (ft_v == KBO_GT) {
            if (!s->use_mixmost_nf) {
              *rule_out = r;
              *dir_out  = 0u;
              return 1;
            }
            if (best_rule == (u32)-1 ||
                !ft_wm_pattern_before(
                    (best_dir == 0u) ? s->lhs_ft[best_rule]
                                     : s->rhs_ft[best_rule], lhs)) {
              best_rule = r;
              best_dir  = 0u;
            }
          }
        }
        ft_subst_reset(subst_buf);
      }
      // Backward: same gates with l/r swapped.
      int bwd_matched;
      if (use_dtb) {
        bwd_matched = (rec_b != NULL);
        if (bwd_matched) ftdt_fill_subst(&g_ftpi, rec_b, subst_buf);
      } else {
        ft_subst_reset(subst_buf);
        bwd_matched = ft_match_maybe_ac(ft_arena_local, rhs, p, subst_buf);
      }
      if (bwd_matched) {
        if (ft_subst_ground_extras(s, lhs, subst_buf)) {
          if (redex_na == 0u) redex_na = thvm_kbo_ft_subst_prepare_redex(p, s->kbo);
          KboCmp ft_v = thvm_kbo_ft_subst_with_prepared(redex_na, lhs, subst_buf, s->kbo);
          if (ft_v == KBO_GT) {
            if (!s->use_mixmost_nf) {
              *rule_out = r;
              *dir_out  = 1u;
              return 1;
            }
            if (best_rule == (u32)-1 ||
                !ft_wm_pattern_before(
                    (best_dir == 0u) ? s->lhs_ft[best_rule]
                                     : s->rhs_ft[best_rule], rhs)) {
              best_rule = r;
              best_dir  = 1u;
            }
          }
        }
        ft_subst_reset(subst_buf);
      }
    }
    if (best_rule != (u32)-1) {
      // Re-derive the winner's substitution (match + grounded extras);
      // the gate verdict already passed during the scan.  Under use_dtb
      // the match half comes from the winner's descent record.
      AtpFtCell *tpl = (best_dir == 0u) ? s->rhs_ft[best_rule]
                                        : s->lhs_ft[best_rule];
      if (use_dtb) {
        const FtDtRec *rec = ftdt_rec_find(&g_ftpi, best_rule, best_dir);
        if (rec != NULL) {
          ftdt_fill_subst(&g_ftpi, rec, subst_buf);
          if (ft_subst_ground_extras(s, tpl, subst_buf)) {
            *rule_out = best_rule;
            *dir_out  = best_dir;
            return 1;
          }
        }
      } else {
        AtpFtCell *pat = (best_dir == 0u) ? s->lhs_ft[best_rule]
                                          : s->rhs_ft[best_rule];
        ft_subst_reset(subst_buf);
        if (ft_match_maybe_ac(ft_arena_local, pat, p, subst_buf) &&
            ft_subst_ground_extras(s, tpl, subst_buf)) {
          *rule_out = best_rule;
          *dir_out  = best_dir;
          return 1;
        }
      }
    }
  }
#undef FTPI_NITER
#undef FTPI_RULE
  return 0;
}

static int find_redex_ft(AtpState        *s,
                         AtpFtCell       *root,
                         u32              slice_first,
                         u32              slice_count,
                         AtpFtCell      **parent_out,
                         AtpFtCell      **redex_out,
                         u32             *rule_out,
                         u8              *dir_out,
                         AtpFtSubst      *subst_buf,
                         u8               try_orient,
                         u8               try_unorient) {
  AtpFtCell *end_after = (root->end != NULL) ? root->end->next : NULL;
  AtpFtCell *prev = NULL;
  u32 slice_end = slice_first + slice_count;
  if (slice_end > s->n_rules) slice_end = s->n_rules;
  for (AtpFtCell *p = root; p != NULL && p != end_after; p = p->next) {
    if ((p->flags & ATPFT_FLAG_SUBST_FRESH) != 0u) {
      prev = p;
      continue;
    }
    if (ft_cell_try_rules(s, p, slice_first, slice_end,
                          try_orient, try_unorient,
                          subst_buf, rule_out, dir_out)) {
      *parent_out = (p == root) ? NULL : prev;
      *redex_out  = p;
      return 1;
    }
    prev = p;
  }
  return 0;
}

// --- WM mixmost normalization strategy --------------------------------
//
// Port of Waldmeister's DEFAULT normal-form strategy `-nf mixmost`
// (RUN/Parameter.c:418-419 DefaultInfoString "mixmost"; NF_Normalform =
// NormalformMixMost, NF/NFBildung.c:837-840 -> :637-643, which is
// NormalformZuRegelnOderGleichungenAufNochmal :349-377 verbatim).  The
// discipline, decoded from the Pfad (path-stack) machinery
// (NFB_PfadEinsWeiterSetzen :141-211, WeiterInVO :289-300,
// AlleAufsteigenBisReduziertOderStop :334-347,
// NFB_RegelOderGleichungAngewendet :219-238):
//
//   1. Reduce the ROOT to a local fixpoint.
//   2. Walk the term in PREORDER (try-reduce at a node BEFORE
//      descending into it).  On a successful reduction at the current
//      position:
//        a. re-reduce the SAME position to a local fixpoint (the new
//           subterm's root only -- not its insides),
//        b. ASCEND: try each ancestor along the path, deepest first up
//           to the root; if one reduces, fixpoint it and restart the
//           ascent from there,
//        c. resume the preorder walk at the LAST reduced position
//           (its children count was reset by the reduction, so the
//           changed area is re-walked; already-visited siblings of
//           ancestors are NOT revisited).
//
// The legacy thvm loop (find_redex_ft re-scan from the root after
// every splice) is WM's `-nf outermost` (BL_NormalformOutermost
// :591-613, `goto root`).  On a NON-CONFLUENT mid-completion R the two
// strategies reach DIFFERENT normal forms: outermost sees a root redex
// that a local reduction at the deeper position would have destroyed,
// mixmost performs the local reduction first.  That flipped
// generation-time joinability verdicts vs WM (KPBehandelt treats with
// NF_Normalform2 = the same strategy pointer) -- WM queued a CP copy
// thvm joined away (McCune EqualityOfInverses cp 1893, HigmanNeumann
// Associativity cp 597), the duplicate-CP multiplicity divergence
// class of the alignment matrix.
//
// WM's NFPfad entries map 1:1 onto AtpFtCell chains (WM terms ARE
// flatterms): Stelle = the subterm's head cell, EinfuegePunkt = the
// cell physically BEFORE the current child in the preorder chain
// (TO_Schwanz = ->next, TO_TermEnde = ->end),
// AnzNochZuBehandelnderTeilterme = children not yet visited.
//
// Gated by AtpState.use_mixmost_nf (Method "MixmostNF" /
// THVM_ATP_MIXMOST_NF): default OFF keeps the legacy walk
// byte-identical; ON in the Waldmeister* presets + the bench WM
// preset.
typedef struct {
  AtpFtCell *cell;   // WM Stelle: head cell of this path position
  AtpFtCell *ins;    // WM EinfuegePunkt: cell before the current child
  u32        left;   // WM AnzNochZuBehandelnderTeilterme
  u64        rw0;    // splice count at push (subtree-NF store witness)
  u8         nfm_hit;// NFM_CHECK: memo claimed this subtree normal
} FtNfPathEnt;

static int ft_find_position(AtpFtCell *root, AtpFtCell *target,
                            u8 *pos, u8 *pos_len);

static inline u32 ft_cell_arity(const AtpFtCell *c) {
  return ((c->sym & WF_VAR_BIT) != 0u) ? 0u : (u32)c->arity;
}

typedef struct {
  AtpState    *s;
  AtpFt       *arena;
  AtpFtCell   *root;
  FtNfPathEnt *st;
  u32          cap;        // allocated stack entries
  u32          depth;      // current path position (index into st)
  u32          slice_first, slice_end;
  u8           doE;
  u32          budget;     // remaining rewrites (caller step_cap)
  int          record;
  Term         eq_other;
  u8           side;
  u32         *chain_tail;
  AtpFtSubst  *subst;
  // Proof-chain recording as AtpProofStep[] (goal extractor), distinct
  // from the TRACE_NORM_STEP `record` path: when psteps != NULL each
  // reduction appends {side, rule, fwd, pos, before, after} so
  // thvm_atp_proof_extract reconstructs the CLOSING chain in the SAME
  // FT-mixmost (WM innermost) order the search proved with -- the
  // Term-side outermost extractor over-joins and loses the goal-closer.
  AtpProofStep *psteps;
  u32          *pn;
  u32           pcap;
  // Subtree-NF memo state (see FtNfmEnt): splices fired this call,
  // budget/cap starvation (blocks stores), gate + want-config key.
  u64           rw_count;
  u8            starved;
  u8            nfm_on;
  u8            nfm_want;
} FtMixmost;

// WM NFB_RegelOderGleichungAngewendet: one reduction attempt at the
// CURRENT path position.  On success the position's entry is re-seated
// on the new subterm (Stelle = TO_Schwanz(EinfuegePunkt) for depth>0,
// the patched-in-place root otherwise) with a fresh child count --
// exactly :226-231.  Budget-exhausted calls report "no redex" so every
// enclosing loop unwinds and the caller returns the partial form (the
// legacy path's cap semantics).
static int ft_mixmost_reduce_here(FtMixmost *m) {
  if (m->budget == 0u) { m->starved = 1u; return 0; }
  FtNfPathEnt *e = &m->st[m->depth];
  // Var-cell early-out hoisted from ft_cell_try_rules (a free var is never
  // a redex): ~half of all preorder positions are variable leaves, and the
  // full call pays frame + gate setup just to test this bit.  The shape
  // counters still see the query so the detail tier stays comparable.
  if ((e->cell->sym & WF_VAR_BIT) != 0u) {
    if (g_atp_phase_detail && g_atp_pushn_active) {
      g_atp_pushn_tr_q++;
      g_atp_pushn_tr_var++;
    }
    return 0;
  }
  u32 rule = 0u;
  u8  dir  = 0u;
  if (!ft_cell_try_rules(m->s, e->cell, m->slice_first, m->slice_end,
                         /*try_orient=*/1u,
                         (u8)(m->doE && m->s->n_unorient > 0u),
                         m->subst, &rule, &dir)) {
    return 0;
  }
  u8  pos[ATP_PROOF_MAX_DEPTH];
  u8  pos_len  = 0u;
  int have_pos = 0;
  if (m->record || m->psteps != NULL) {
    have_pos = ft_find_position(m->root, e->cell, pos, &pos_len);
  }
  static int dbg_goal_nf = -1;
  if (dbg_goal_nf < 0) dbg_goal_nf = getenv("THVM_ATP_GOAL_NF_DEBUG") != NULL;
  if (dbg_goal_nf) {
    fprintf(stderr, "[mixstep] depth=%u have_pos=%d pos_len=%u root_eq_cell=%d dir=%u\n",
            m->depth, have_pos, pos_len, (int)(m->root == e->cell), dir);
  }
  // AtpProofStep needs the term BEFORE the splice; capture it now.
  Term before_term = (m->psteps != NULL) ? ft_to_term(m->root) : (Term)0;
  AtpFtCell *parent   = (m->depth == 0u) ? NULL : m->st[m->depth - 1u].ins;
  AtpFtCell *rhs_tmpl = (dir == 0u) ? m->s->rhs_ft[rule]
                                    : m->s->lhs_ft[rule];
  AtpFtCell *new_root = ft_splice(m->arena, m->root, parent, e->cell,
                                  rhs_tmpl, m->subst);
  if (new_root == NULL) return 0;   // defensive: ft_splice returns the root
  if (g_atp_phase_detail && g_atp_pushn_active) g_atp_pushn_rw++;
  m->budget--;
  m->rw_count++;
  m->root = new_root;
  m->st[0].cell = m->root;
  e->cell = (m->depth == 0u) ? m->root : parent->next;
  e->left = ft_cell_arity(e->cell);
  e->ins  = NULL;
  if (m->psteps != NULL && *m->pn < m->pcap) {
    AtpProofStep *st = &m->psteps[*m->pn];
    st->side    = m->side;
    st->rule    = rule;
    st->fwd     = (dir == 0u) ? 1u : 0u;
    st->pos_len = have_pos ? pos_len : 0u;
    for (u8 k = 0; k < st->pos_len; k++) st->pos[k] = pos[k];
    st->before  = before_term;
    st->after   = ft_to_term(m->root);
    (*m->pn)++;
  }
  if (m->record) {
    Term step_term = ft_to_term(m->root);
    u8  fwd        = (dir == 0u) ? 1u : 0u;
    u32 rule_trace = (rule < m->s->n_rules) ? m->s->r_trace[rule]
                                            : ATP_TRACE_NONE;
    Term step_lhs = (m->side == 0u) ? step_term : m->eq_other;
    Term step_rhs = (m->side == 0u) ? m->eq_other : step_term;
    if (!have_pos) pos_len = 0u;
    u32 ti = atp_trace_push_norm_step(m->s, *m->chain_tail, rule_trace,
                                      step_lhs, step_rhs, m->side, fwd,
                                      pos, pos_len);
    if (ti != ATP_TRACE_NONE) *m->chain_tail = ti;
  }
  return 1;
}

// --- Subtree normal-form memo (mixmost walk) --------------------------
//
// A raw CP is one overlap step away from two normal-form parents, so
// most of its subtrees are ALREADY normal (92% of position queries
// fail), and the same subterm content recurs across the CPs of a
// batch.  "No rule of R rewrites anywhere inside preorder string S
// under want-config W" is a pure function of (S, W, tree content), so
// one content-keyed probe at a subtree head can skip the ENTIRE
// subtree's positions -- and a hit's output is its input (the subtree
// is untouched; nothing is materialized).  The subtree granularity is
// what pays: a finer per-QUERY fail memo at the ftdt_candidates level
// replaces one descend with a probe of about the same cost (built,
// measured fully subsumed by this memo -- 0.0s marginal on the OA
// gate -- and removed 2026-07-03).  Exactness discipline: full
// preorder memcmp (no hash-only verdicts), want in the key, the
// g_ftnfm_gen generation stamp (bumped on every index rebuild/extend,
// which ftpi_ensure syncs to r_revision -- rules cannot change inside
// one normalize call).  A subtree verdict is STORED when the walk pops
// a completed position with no splice recorded inside it (rw_count
// unchanged since push) and no budget/NF-cap starvation (a starved
// reduce_here reports "no redex" without trying, so completion proves
// nothing).  Kill switch THVM_NO_FT_NFMEMO=1; certifier
// THVM_ATPFT_NFM_CHECK=1 disables the skip but records each hit and
// fatals at pop when a splice fired inside a subtree the memo claimed
// normal.
enum { FTNFM_MAX_SYMS = 32, FTNFM_BITS = 16 };
typedef struct {
  u64 hash;
  u64 gen;
  u32 syms[FTNFM_MAX_SYMS];
  u8  len;
  u8  want;
} FtNfmEnt;
static FtNfmEnt *g_ftnfm = NULL;
u64 g_atp_ftnfm_probe = 0, g_atp_ftnfm_hit = 0, g_atp_ftnfm_store = 0;

static u32 ftnfm_key(const AtpFtCell *c, u32 *syms, u64 *hash_out) {
  const AtpFtCell *e = (c->end != NULL) ? c->end->next : NULL;
  u32 len = 0u;
  u64 h = 1469598103934665603ull;              // FNV-1a 64
  for (; c != e; c = c->next) {
    if (len == FTNFM_MAX_SYMS) return 0u;
    u32 sy = c->sym;
    syms[len++] = sy;
    h ^= (u64)sy;
    h *= 1099511628211ull;
  }
  *hash_out = h;
  return len;
}

static FtNfmEnt *ftnfm_ent(u64 hash) {
  if (g_ftnfm == NULL) {
    g_ftnfm = (FtNfmEnt *)calloc((size_t)1u << FTNFM_BITS, sizeof(FtNfmEnt));
    if (g_ftnfm == NULL) thvm_fatal("ftnfm: memo table OOM");
  }
  return &g_ftnfm[hash & (((u64)1u << FTNFM_BITS) - 1u)];
}

// 1 iff subtree `c` is memoized normal under want-config `wantk` at the
// current generation.  A single var cell never probes (the walk's var
// early-out is already cheaper than a probe).
static int ftnfm_probe(const AtpFtCell *c, u8 wantk) {
  if ((c->sym & WF_VAR_BIT) != 0u) return 0;
  u32 syms[FTNFM_MAX_SYMS];
  u64 h;
  u32 len = ftnfm_key(c, syms, &h);
  g_atp_ftnfm_probe++;
  if (len == 0u) return 0;                     // too big to memo
  FtNfmEnt *ent = ftnfm_ent(h);
  if (ent->gen == g_ftnfm_gen && ent->hash == h && ent->want == wantk &&
      ent->len == (u8)len && memcmp(ent->syms, syms, len * sizeof(u32)) == 0) {
    g_atp_ftnfm_hit++;
    return 1;
  }
  return 0;
}

static void ftnfm_store(const AtpFtCell *c, u8 wantk) {
  if ((c->sym & WF_VAR_BIT) != 0u) return;
  u32 syms[FTNFM_MAX_SYMS];
  u64 h;
  u32 len = ftnfm_key(c, syms, &h);
  if (len == 0u) return;
  FtNfmEnt *ent = ftnfm_ent(h);
  ent->gen  = g_ftnfm_gen;
  ent->hash = h;
  ent->want = wantk;
  ent->len  = (u8)len;
  memcpy(ent->syms, syms, len * sizeof(u32));
  g_atp_ftnfm_store++;
}

// Re-query provenance wrappers (THVM_ATP_PROFILE=2 push scope only):
// tag reduce_here calls issued by the local-fixpoint whiles vs the
// post-reduction ascent so the redundant-re-query mass is readable.
static int ft_mm_here_fix(FtMixmost *m) {
  if (g_atp_phase_detail && g_atp_pushn_active) g_atp_pushn_mm_fix++;
  return ft_mixmost_reduce_here(m);
}
static int ft_mm_here_asc(FtMixmost *m) {
  if (g_atp_phase_detail && g_atp_pushn_active) g_atp_pushn_mm_asc++;
  return ft_mixmost_reduce_here(m);
}

static AtpFtCell *atp_normalize_mixmost_ft(AtpState *s, AtpFtCell *t,
                                           u32 slice_first, u32 slice_end,
                                           u32 step_cap, u8 doE,
                                           int record, Term eq_other,
                                           u8 side, u32 *chain_tail,
                                           AtpFtSubst *subst,
                                           AtpProofStep *psteps, u32 *pn,
                                           u32 pcap) {
  FtNfPathEnt st_fixed[64];
  FtMixmost m = {
    .s = s, .arena = (AtpFt *)s->ft_arena_ptr, .root = t,
    .st = st_fixed, .cap = 64u, .depth = 0u,
    .slice_first = slice_first, .slice_end = slice_end,
    .doE = doE, .budget = step_cap, .record = record,
    .eq_other = eq_other, .side = side, .chain_tail = chain_tail,
    .subst = subst,
    .psteps = psteps, .pn = pn, .pcap = pcap,
  };
  FtNfPathEnt *heap_st = NULL;
  m.st[0].cell = t;
  m.st[0].ins  = NULL;
  m.st[0].left = ft_cell_arity(t);
  m.st[0].rw0  = 0u;
  m.st[0].nfm_hit = 0u;
  // Subtree-NF memo gate (see FtNfmEnt): full-range index-applicable
  // calls only, index synced FIRST so the generation stamp reflects the
  // CURRENT rule content (rules cannot change inside one call).
  static int nfm_off = -1;
  static int nfm_check = -1;
  if (nfm_off < 0) {
    const char *e = getenv("THVM_NO_FT_NFMEMO");
    nfm_off = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
    const char *c = getenv("THVM_ATPFT_NFM_CHECK");
    nfm_check = (c != NULL && c[0] != '0' && c[0] != '\0') ? 1 : 0;
  }
  u8 nfm_root_hit = 0u;
  if (!nfm_off && ftpi_applicable(s, slice_first, slice_end)) {
    ftpi_ensure(s);
    m.nfm_on   = 1u;
    m.nfm_want = (u8)(FTDT_WANT_ORIENT |
                      ((doE && s->n_unorient > 0u) ? FTDT_WANT_UNORIENT : 0u));
    if (ftnfm_probe(t, m.nfm_want)) {
      if (!nfm_check) return t;   // whole term memoized normal: identity
      nfm_root_hit = 1u;          // certifier: walk anyway, verify at exit
    }
  }
  // Critical-pair overlaps can normalize to pathologically large terms
  // (variable-multiplication blowup: a small rule's RHS instantiated under
  // a deep unifier -- the goal-directed OrAssociativity heap-blowup, ~650
  // -> 90000 nodes).  Such NFs are always far over the auto-MaxWeight bound
  // and get stashed/dropped at intake -- but only AFTER ft_to_term has
  // built the full term on the main heap, exhausting it.  When
  // THVM_ATP_NF_CAP>0, stop reducing once this normalize has grown the live
  // FT set past the cap: the result stays over-bound (the CP is discarded
  // exactly as before) but is never materialized at full size.  The cap is
  // set well above any legitimate NF (the faithful-preset proof's NFs stay
  // <~250 symbols), so on-path proofs are byte-identical.  Off (0) = no cap.
  AtpFt *nf_arena = m.arena;
  u64 nf_start = nf_arena->n_persistent_alive;
  static int nf_cap = -1;
  if (nf_cap < 0) { const char *e = getenv("THVM_ATP_NF_CAP");
    nf_cap = (e && *e) ? (int)strtol(e, NULL, 10) : 0; }
  // Root fixpoint (the loop before WeiterInVO, NFBildung.c:355).
  while (ft_mm_here_fix(&m)) {
    if (nf_cap > 0 &&
        nf_arena->n_persistent_alive - nf_start > (u64)nf_cap) break;
  }
  for (;;) {
    if (m.budget == 0u) break;
    if (nf_cap > 0 &&
        nf_arena->n_persistent_alive - nf_start > (u64)nf_cap) {
      m.starved = 1u;   // capped: completion below proves nothing
      break;
    }
    // WeiterInVO (:289-300): ascend past exhausted positions, then
    // step to the next unvisited child (PfadEinsWeiterSetzen).
    while (m.st[m.depth].left == 0u) {
      if (m.depth == 0u) goto done;
      // Pop of a COMPLETED position: no splice inside since its push
      // and no starvation -> its subtree is proven normal, store it.
      // NFM_CHECK: a memo-claimed-normal subtree that spliced anyway
      // is a stale/unsound entry -- fatal.
      FtNfPathEnt *pe = &m.st[m.depth];
      if (m.nfm_on && m.rw_count == pe->rw0) {
        if (!m.starved) ftnfm_store(pe->cell, m.nfm_want);
      } else if (pe->nfm_hit) {
        thvm_fatal("ftnfm: memoized-normal subtree was rewritten (stale)");
      }
      m.depth--;
    }
    if (m.depth + 1u >= m.cap) {
      // Grow the path stack (WM reallocs in NF_PfadArrayGroesse hops).
      u32 ncap = m.cap * 2u;
      FtNfPathEnt *ns = (FtNfPathEnt *)malloc(ncap * sizeof(FtNfPathEnt));
      if (ns == NULL) thvm_fatal("mixmost path stack OOM");
      memcpy(ns, m.st, m.cap * sizeof(FtNfPathEnt));
      free(heap_st);
      heap_st = ns;
      m.st  = ns;
      m.cap = ncap;
    }
    FtNfPathEnt *e = &m.st[m.depth];
    e->left--;
    // First child starts right after the head cell; each later child
    // right after the previous child's end (:141-211).
    e->ins = (e->ins == NULL) ? e->cell : e->ins->next->end;
    // Subtree-NF memo: child memoized normal -> skip its ENTIRE
    // subtree (no push; e->ins already marks it visited).  Certifier
    // mode walks it anyway and verifies at pop.
    u8 child_hit = 0u;
    if (m.nfm_on && ftnfm_probe(e->ins->next, m.nfm_want)) {
      if (!nfm_check) continue;
      child_hit = 1u;
    }
    m.depth++;
    m.st[m.depth].cell = e->ins->next;
    m.st[m.depth].ins  = NULL;
    m.st[m.depth].left = ft_cell_arity(m.st[m.depth].cell);
    m.st[m.depth].rw0  = m.rw_count;
    m.st[m.depth].nfm_hit = child_hit;
    if (ft_mixmost_reduce_here(&m)) {
      // Local fixpoint at the reduced position (:367).
      while (ft_mm_here_fix(&m)) {}
      u32 last_red = m.depth;   // WM letzteRed
      // Ascent (:370-373): try ancestors deepest-first up to the root;
      // a firing ancestor is fixpointed and the ascent restarts there.
      for (;;) {
        int fired = 0;
        m.depth = last_red;
        while (m.depth > 0u) {
          m.depth--;
          if (ft_mm_here_asc(&m)) { fired = 1; break; }
        }
        if (!fired) break;
        while (ft_mm_here_fix(&m)) {}
        last_red = m.depth;
      }
      // Resume the preorder walk at the last reduced position (:375).
      m.depth = last_red;
    }
  }
done:
  // Whole-term store / root-hit certify (the depth-0 "pop").
  if (m.nfm_on && m.rw_count == 0u) {
    if (!m.starved) ftnfm_store(m.root, m.nfm_want);
  } else if (nfm_root_hit) {
    thvm_fatal("ftnfm: memoized-normal root was rewritten (stale)");
  }
  free(heap_st);
  return m.root;
}

// --- Fixpoint --------------------------------------------------------
//
// Public entry: normalize `t` against the AtpFt rule mirror in
// `s->lhs_ft / s->rhs_ft`.  Returns the normal form (a cell in Arena A;
// may be the same pointer as the input if no rewrite fired, OR a fresh
// root cell when the root was rewritten).
//
// Step cap is the caller's budget -- mirrors the Term-side NORM_CAP=64
// in atp_cp_trivially_joinable.  Hitting the cap returns the partial
// reduction; the caller's downstream `ft_eq` check then treats it as
// "not joinable".

// find_redex_ft via the discrim-tree descent (Stage 6b).  When enabled,
// we ALSO need a parent-cell for the splice and a verified substitution.
// The discrim-tree descent's leaf-collect returns the rule + star_ft
// bindings; we then re-derive parent via a pre-order scan from root,
// and rebuild the subst via ft_match (cheap: pattern-side traversal).
//
// This keeps the splice contract unchanged.  The Stage-6b WIN is on
// the find phase: O(|subject| * average descent depth) instead of
// O(|subject| * n_rules).
#ifdef THVM_ATPFT_RI
static int find_redex_ft_via_ri(AtpState        *s,
                                AtpFtCell       *root,
                                AtpFtCell      **parent_out,
                                AtpFtCell      **redex_out,
                                u32             *rule_out,
                                u8              *dir_out,
                                AtpFtSubst      *subst_buf,
                                u8               try_orient) {
  // The discrim-tree descent indexes orientable rules only; if the
  // caller is doing an unorientable-only step, skip the index query.
  if (!try_orient) return 0;
  AtpFtCell *redex_cell = NULL;
  u32        rule       = 0u;
  if (!atp_ri_find_redex_ft_pub(s, root, &redex_cell, &rule)) {
    return 0;
  }
  // The discrim-tree descent already filters unorientable rules
  // (atp_ri_sync_ft_patterns skips them), so any rule we get here is
  // safe to fire forward without the ordered-equation gate.
  // Find parent by pre-order walk -- the cell whose `next` reaches
  // the redex.  O(|subject|) but limited by the linear-scan WIN above.
  AtpFtCell *parent = NULL;
  if (redex_cell != root) {
    AtpFtCell *end_after = (root->end != NULL) ? root->end->next : NULL;
    AtpFtCell *prev = root;
    for (AtpFtCell *p = root->next; p != NULL && p != end_after;
         p = p->next) {
      if (p == redex_cell) { parent = prev; break; }
      prev = p;
    }
  }
  // Rebuild subst: ft_match against the rule's pattern.
  ft_subst_reset(subst_buf);
  if (!ft_match(s->lhs_ft[rule], redex_cell, subst_buf)) {
    // Defensive: a perfect-descent hit MUST match.  If it doesn't,
    // the rule was bwd-subsumed mid-step or the index is stale; bail
    // and let the next find_redex re-query (or the caller fall back).
    return 0;
  }
  *parent_out = parent;
  *redex_out  = redex_cell;
  *rule_out   = rule;
  *dir_out    = 0u;
  return 1;
}
#endif

// --- Position helper -------------------------------------------------
//
// Compute the child-index path from `root` to `target` via FT child-walk
// semantics: the first child of a CTR cell is `cell->next`, and the next
// sibling of a child is `child->end->next`.  Variables and consts are
// leaves.
//
// On hit, fills `pos[0..*pos_len)` and returns 1.  On miss, returns 0.
// Caller-owned `pos` of size at least ATP_PROOF_MAX_DEPTH; if the path
// would exceed the cap, returns 0 (caller can fall back to a "no
// recording" step -- but in practice ATP terms stay shallow under the
// completion cap, so the cap is informational).
static int ft_find_position(AtpFtCell *root, AtpFtCell *target,
                            u8 *pos, u8 *pos_len) {
  // NB: do NOT reset *pos_len here.  This is a RECURSIVE DFS: the caller
  // has already pushed this cell's child index into pos[] and bumped
  // *pos_len before recursing, so the accumulated prefix IS the path to
  // `target`.  Zeroing it on a match wiped that prefix, mis-recording
  // every non-root redex at the ROOT position {} -- the mixmost goal
  // chain's inner comm step then reconstructed to the wrong term and the
  // ProofObject failed to verify (commutative_monoid).  The top-level
  // caller initializes *pos_len = 0, so a root match still yields {}.
  if (root == target) return 1;
  if (root == NULL || target == NULL) return 0;
  if ((root->sym & WF_VAR_BIT) != 0u) return 0;   // leaves don't host children
  u16 arity = root->arity;
  if (arity == 0u) return 0;
  // Recursive DFS over children.  Each child's head cell is root->next
  // for child 0, and child[i+1] = child[i]->end->next.
  AtpFtCell *child = root->next;
  for (u16 i = 0; i < arity; i++) {
    if (child == NULL) return 0;
    if (*pos_len >= ATP_PROOF_MAX_DEPTH) return 0;
    pos[*pos_len] = (u8)i;
    (*pos_len)++;
    if (ft_find_position(child, target, pos, pos_len)) return 1;
    (*pos_len)--;
    child = (child->end != NULL) ? child->end->next : NULL;
  }
  return 0;
}

// Shared internal entry: optional record-mode parameters.  When
// `record` is 0, behaves exactly like the historic `atp_rewrite_
// normalize_ft` (zero overhead -- no Term conversions, no trace
// pushes).  When `record` is 1, after each successful splice converts
// the new root via ft_to_term and emits a TRACE_NORM_STEP entry
// chained from `*chain_tail` with the rule's TRACE_ORIENT id; the
// (side, eq_other) pair matches the term-side
// atp_rewrite_normalize_record contract.
extern Term ft_to_term(const AtpFtCell *x);
static AtpFtCell *atp_rewrite_normalize_ft_impl(AtpState *s, AtpFtCell *t,
                                                u32 slice_first,
                                                u32 slice_count,
                                                u32 step_cap, u8 doE,
                                                int record,
                                                Term eq_other, u8 side,
                                                u32 *chain_tail);

AtpFtCell *atp_rewrite_normalize_ft(AtpState *s, AtpFtCell *t, u32 step_cap);
AtpFtCell *atp_rewrite_normalize_ft(AtpState *s, AtpFtCell *t, u32 step_cap) {
  if (t == NULL) return NULL;
  if (s->n_rules == 0u) return t;
  // Full-range delegation -- bench-neutral by construction (same scan
  // bounds, same RI path, same memset).  doE=TRUE: rules AND
  // unorientable equations rewrite -- WM's NF_NormalformRE flag pair
  // (selection-time `-ks "r:e:s:p"` / goal normalization).
  return atp_rewrite_normalize_ft_impl(s, t, /*slice_first=*/0u,
                                       /*slice_count=*/s->n_rules,
                                       step_cap, /*doE=*/1u, /*record=*/0,
                                       (Term){0}, 0u, NULL);
}

// WM doR-only variant: oriented rules rewrite, unorientable equations
// do NOT (the Gleichungsbaum is not consulted) -- NF_Normalform2's
// doE=FALSE leg, which WM's default proof configuration applies at
// exactly ONE live site: generation-time CP treatment (`KPBehandelt`,
// INF/KPVerwaltung.c:442, strength from `-kg` default "r",
// RUN/Parameter.c:397).  FT sibling of atp_rules_only_normalize.
AtpFtCell *atp_rules_only_normalize_ft(AtpState *s, AtpFtCell *t,
                                       u32 step_cap);
AtpFtCell *atp_rules_only_normalize_ft(AtpState *s, AtpFtCell *t,
                                       u32 step_cap) {
  if (t == NULL) return NULL;
  if (s->n_rules == 0u) return t;
  return atp_rewrite_normalize_ft_impl(s, t, /*slice_first=*/0u,
                                       /*slice_count=*/s->n_rules,
                                       step_cap, /*doE=*/0u, /*record=*/0,
                                       (Term){0}, 0u, NULL);
}

// Record-mode entry.  Mirrors atp_rewrite_normalize_record's contract
// on the term side: each FT splice emits one TRACE_NORM_STEP chained
// off `*chain_tail`, advancing the tail.  `eq_other` is the unchanged
// side of the equation we're normalizing (Term form); `side` is 0 for
// the LHS, 1 for the RHS.  The new tail is left in `*chain_tail`.
// Returns the FT NF (same allocator semantics as the bare variant).
// Always runs over the full rule range -- record-mode pairs with the
// term-side `atp_rewrite_normalize_record`, never with the slice
// interreduce path.
AtpFtCell *atp_rewrite_normalize_ft_record(AtpState *s, AtpFtCell *t,
                                           u32 step_cap, Term eq_other,
                                           u8 side, u32 *chain_tail);
AtpFtCell *atp_rewrite_normalize_ft_record(AtpState *s, AtpFtCell *t,
                                           u32 step_cap, Term eq_other,
                                           u8 side, u32 *chain_tail) {
  if (t == NULL) return NULL;
  if (s->n_rules == 0u) return t;
  return atp_rewrite_normalize_ft_impl(s, t, /*slice_first=*/0u,
                                       /*slice_count=*/s->n_rules,
                                       step_cap, /*doE=*/1u, /*record=*/1,
                                       eq_other, side, chain_tail);
}

// FT-mixmost (WM innermost) normalize that records each rewrite as an
// AtpProofStep into out[*n .. cap).  The ORDER-FAITHFUL goal-chain
// extractor: it reproduces goal_check's FT-mixmost join so the closing
// chain actually closes -- the legacy Term-side OUTERMOST extractor
// (atp_proof_record_side) over-joins the CPs WM keeps, loses the
// goal-closer, and returns an empty chain (buildCplDataset.empty-goal-
// chain -> ProofObject $Failed even though the engine PROVED).
AtpFtCell *atp_rewrite_normalize_ft_proofsteps(AtpState *s, AtpFtCell *t,
                                               u32 step_cap, u8 side,
                                               AtpProofStep *out, u32 *n,
                                               u32 cap);
AtpFtCell *atp_rewrite_normalize_ft_proofsteps(AtpState *s, AtpFtCell *t,
                                               u32 step_cap, u8 side,
                                               AtpProofStep *out, u32 *n,
                                               u32 cap) {
  if (t == NULL) return NULL;
  if (s->n_rules == 0u) return t;
  ft_clear_subst_fresh(t);
  AtpFtSubst subst;
  memset(&subst, 0, sizeof(subst));
  return atp_normalize_mixmost_ft(s, t, /*slice_first=*/0u,
                                  /*slice_end=*/s->n_rules, step_cap,
                                  /*doE=*/1u, /*record=*/0, /*eq_other=*/0,
                                  side, /*chain_tail=*/NULL, &subst,
                                  out, n, cap);
}

// Slice-aware fixpoint.  Rewrites `t` against only the rule slice
// [slice_first, slice_first + slice_count) of s->lhs_ft / s->rhs_ft.
// Mirrors the Term-side `atp_proof_rewrite_step_slice` /
// `atp_rewrite_normalize_slice_record` calling convention used by
// interreduce, except this path does NOT record TRACE_NORM_STEP --
// callers (e.g. the connectedness check) that want only the FT-side
// normal form invoke this directly; norm-step recording stays on the
// Term-side path until the FT loop owns the trace.
//
// When slice_first==0 && slice_count==s->n_rules this MUST be
// bench-neutral with `atp_rewrite_normalize_ft`: the linear scan
// iterates the same range, and the public `atp_rewrite_normalize_ft`
// delegates here in that case.  (The RI/discrim-tree descent path is
// skipped on slice calls -- the rule_index reflects the FULL R, so
// a hit could resolve to an out-of-slice rule and the linear-scan
// fallback would loop.  In the full-range delegation we keep RI on.)
AtpFtCell *atp_rewrite_normalize_ft_slice(AtpState *s, AtpFtCell *t,
                                          u32 slice_first, u32 slice_count,
                                          u32 step_cap);
AtpFtCell *atp_rewrite_normalize_ft_slice(AtpState *s, AtpFtCell *t,
                                          u32 slice_first, u32 slice_count,
                                          u32 step_cap) {
  if (t == NULL) return NULL;
  if (slice_count == 0u) return t;
  if (slice_first >= s->n_rules) return t;
  if (slice_first + slice_count > s->n_rules) {
    slice_count = s->n_rules - slice_first;
  }
  return atp_rewrite_normalize_ft_impl(s, t, slice_first, slice_count,
                                       step_cap, /*doE=*/1u, /*record=*/0,
                                       (Term){0}, 0u, NULL);
}

// Slice + record variant, for the post-PROVED proof-cone re-derivation
// (thvm_atp_record_goal_cone in _.c): normalize `t` against ONLY the
// rule slice [slice_first, slice_first + slice_count) of s->lhs_ft /
// s->rhs_ft, emitting one TRACE_NORM_STEP per splice chained off
// `*chain_tail` (the atp_rewrite_normalize_ft_record contract).  `doE`
// selects which historical WM flag set the replay mirrors: 1 = the
// pop-normalize doR+doE shape (`-ks "r:e:s:p"`), 0 = the demote-drain
// rules-only doR shape (`-kg "r"`).  The cone re-derivation installs
// the HISTORICAL rule set R(ti) as a temporary slot window past the
// live n_rules and points the slice at it, so the recorded chain cites
// rules by their historical TRACE_ORIENT / TRACE_FVI ids (r_trace of
// the window slots).
AtpFtCell *atp_rewrite_normalize_ft_slice_record(AtpState *s, AtpFtCell *t,
                                                 u32 slice_first,
                                                 u32 slice_count,
                                                 u32 step_cap, u8 doE,
                                                 Term eq_other, u8 side,
                                                 u32 *chain_tail);
AtpFtCell *atp_rewrite_normalize_ft_slice_record(AtpState *s, AtpFtCell *t,
                                                 u32 slice_first,
                                                 u32 slice_count,
                                                 u32 step_cap, u8 doE,
                                                 Term eq_other, u8 side,
                                                 u32 *chain_tail) {
  if (t == NULL) return NULL;
  if (slice_count == 0u) return t;
  return atp_rewrite_normalize_ft_impl(s, t, slice_first, slice_count,
                                       step_cap, doE, /*record=*/1,
                                       eq_other, side, chain_tail);
}

static AtpFtCell *atp_rewrite_normalize_ft_impl(AtpState *s, AtpFtCell *t,
                                                u32 slice_first,
                                                u32 slice_count,
                                                u32 step_cap, u8 doE,
                                                int record,
                                                Term eq_other, u8 side,
                                                u32 *chain_tail) {
  if (t == NULL) return NULL;
  if (slice_count == 0u) return t;
  if (slice_first >= s->n_rules) return t;
  if (slice_first + slice_count > s->n_rules) {
    slice_count = s->n_rules - slice_first;
  }
  AtpFt *a = (AtpFt *)s->ft_arena_ptr;
  // Entry-clear pass.
  ft_clear_subst_fresh(t);
  // Stack-allocate an AtpFtSubst.  See AtpFtSubst declaration in
  // ft_match.c; we zero before first use per the Stage 5 contract.
  AtpFtSubst subst;
  memset(&subst, 0, sizeof(subst));

  // WM mixmost strategy (the CLI `-nf` default): path-stack walk with
  // local fixpoint + ancestor ascent instead of the legacy
  // rescan-from-root step loop.  Same slice/doE/record contracts; the
  // RI redex retrieval is leftmost-outermost by construction and does
  // not apply here.
  if (s->use_mixmost_nf) {
    u32 mslice_end = slice_first + slice_count;
    if (mslice_end > s->n_rules) mslice_end = s->n_rules;
    return atp_normalize_mixmost_ft(s, t, slice_first, mslice_end,
                                    step_cap, doE, record, eq_other,
                                    side, chain_tail, &subst,
                                    /*psteps=*/NULL, /*pn=*/NULL, /*pcap=*/0u);
  }

  int use_full_range = (slice_first == 0u && slice_count == s->n_rules);
#ifdef THVM_ATPFT_RI
  // Stage 6b: opt-in via THVM_ATPFT_RI=1 (env knob).  Default OFF so
  // the baseline Stage 6 linear-scan path is unchanged for the default
  // build (which also has THVM_ATPFT_RI not defined at all).  The RI
  // path indexes the FULL rule set; only enable it when the slice IS
  // the full range.
  static int ri_env_cached = -1;
  if (ri_env_cached < 0) {
    const char *env = getenv("THVM_ATPFT_RI");
    ri_env_cached = (env != NULL && env[0] != '0') ? 1 : 0;
  }
  int use_ri = ri_env_cached && use_full_range;
  if (use_ri) {
    // Ensure the Term-side rule index is current; rebuild on dirty.
    if (s->rule_index == NULL || s->rule_index_dirty ||
        s->rule_index->n_rules_built != s->n_rules) {
      atp_ri_rebuild(s);
    }
    // Refresh FT pattern mirror from s->lhs_ft[].
    atp_ri_ft_sync(s);
  }
#else
  (void)use_full_range;
#endif

  // Mixed loop, mirroring the Term-side atp_rewrite_normalize_ordered
  // when n_unorient > 0:
  //   1. Run an orientable-only fixpoint (up to step_cap rewrites).
  //   2. Try ONE unorientable rewrite (KBO-gated).
  //   3. If step 2 fired, go to 1.  Otherwise we're done.
  // When n_unorient == 0 step 2 is a no-op and the loop collapses to
  // the single orientable fixpoint.  The outer step_cap caps unorient
  // rewrites; the inner cap is also step_cap, so the total orientable
  // budget is step_cap * step_cap -- matching the Term-side mixed loop
  // exactly (each outer iteration calls atp_rewrite_normalize_indexed
  // with step_cap).
  // doE gates the unorientable-equation leg per call site (WM
  // NF_Normalform2's second flag); doR is unconditionally TRUE at every
  // thvm site, as at every live WM site (NF/NFBildung.c:503-531).
  u8 have_unorient = (u8)(doE && s->n_unorient > 0u);
  // Batched-orient fast path: when we have a full-range non-record call
  // (i.e. NOT slice-restricted and NOT in trace-recording mode), the
  // orientable fixpoint can be run by the Term-side discrim-tree
  // normalizer (atp_rewrite_normalize_indexed) in ONE batched call.
  // That path keeps the subject flat across many rewrites; here we pay
  // a single ft_to_term/ft_from_term round-trip per outer iteration
  // instead of one ft_match per cell per rule per inner step.
  //
  // Opt-in via THVM_ATPFT_BATCH_ORIENT=1.  When the slice path is in
  // use, or when recording, we fall back to the per-step inner loop
  // (the indexed path would rewrite against the full R, not the
  // slice, and it does not emit TRACE_NORM_STEP entries).
  static int batch_env_cached = -1;
  if (batch_env_cached < 0) {
    const char *env = getenv("THVM_ATPFT_BATCH_ORIENT");
    batch_env_cached = (env != NULL && env[0] != '0' && env[0] != '\0') ? 1 : 0;
  }
  int use_batch_orient = batch_env_cached && use_full_range && !record;
  (void)use_batch_orient;  // legacy batched-orient hint; per-position loop below subsumes it
  // Single per-position loop (matches WM `BL_Normalform*` / `NFB_*` and
  // Term-side `atp_ordered_try_top`): every position tries oriented rules
  // first, unorientable equations only when no oriented rule matched there
  // (WM Regelbaum-before-Gleichungsbaum, NF/NFBildung.c:503-531).
  // The prior split-phase (orient-fixpoint, ONE unorient, repeat) produced
  // different NFs from Term/WM on AC-class workloads; blocked migrations
  // of goal_check + CP-set-IR.
  for (u32 i = 0; i < step_cap; i++) {
    AtpFtCell *parent = NULL;
    AtpFtCell *redex  = NULL;
    u32        rule   = 0u;
    u8         dir    = 0u;
    int hit = 0;
#ifdef THVM_ATPFT_RI
    if (use_ri) {
      hit = find_redex_ft_via_ri(s, t, &parent, &redex, &rule, &dir,
                                 &subst, /*try_orient=*/1u);
      if (!hit) {
        hit = find_redex_ft(s, t, slice_first, slice_count,
                            &parent, &redex, &rule, &dir, &subst,
                            /*try_orient=*/1u, /*try_unorient=*/have_unorient);
      }
    } else {
      hit = find_redex_ft(s, t, slice_first, slice_count,
                          &parent, &redex, &rule, &dir, &subst,
                          /*try_orient=*/1u, /*try_unorient=*/have_unorient);
    }
#else
    hit = find_redex_ft(s, t, slice_first, slice_count,
                        &parent, &redex, &rule, &dir, &subst,
                        /*try_orient=*/1u, /*try_unorient=*/have_unorient);
#endif
    if (!hit) break;
    u8  pos[ATP_PROOF_MAX_DEPTH];
    u8  pos_len  = 0u;
    int have_pos = 0;
    if (record) {
      have_pos = ft_find_position(t, redex, pos, &pos_len);
    }
    AtpFtCell *rhs_tmpl = (dir == 0u) ? s->rhs_ft[rule] : s->lhs_ft[rule];
    AtpFtCell *new_root = ft_splice(a, t, parent, redex, rhs_tmpl, &subst);
    if (new_root != NULL) t = new_root;
    if (record) {
      Term step_term = ft_to_term(t);
      u8  fwd        = (dir == 0u) ? 1u : 0u;
      u32 rule_trace = (rule < s->n_rules) ? s->r_trace[rule]
                                           : ATP_TRACE_NONE;
      Term step_lhs = (side == 0u) ? step_term : eq_other;
      Term step_rhs = (side == 0u) ? eq_other  : step_term;
      if (!have_pos) pos_len = 0u;
      u32 ti = atp_trace_push_norm_step(s, *chain_tail, rule_trace,
                                        step_lhs, step_rhs, side, fwd,
                                        pos, pos_len);
      if (ti != ATP_TRACE_NONE) *chain_tail = ti;
    }
  }
  return t;
}

#endif // THVM_ATPFT_NORM
