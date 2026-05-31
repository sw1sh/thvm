// ft_ri.c - Stage 6b of AtpFt: AtpFt-native discrim-tree descent.
//
// Reuses the existing AtpRuleIndex discrim tree (built from s->lhs[]
// via atp_ri_rebuild).  The tree's edge labels are pure symbol-skeleton
// (CTR_BASE+label / NUM / STAR_BASE+var_idx), which are byte-identical
// to what the FT-side subject produces -- s->lhs[i] and s->lhs_ft[i]
// share constructor ids, share the first-appearance var renumbering of
// the rule LHS, and share the NUM collapse.  Only the TERMINAL action
// differs: a Term-path leaf-collect reads g_atp_ri_flat / qsubj; an
// FT-path leaf-collect reads cell pointers from g_atp_ri_star_ft[].
//
// What this stage adds:
//
//  - A sibling pattern table g_atp_ri_lhs_ft[rule] : AtpFtCell*,
//    refreshed from s->lhs_ft[] whenever the tree rebuilds.  Cost is
//    one pointer per live rule; the tree itself is not duplicated.
//
//  - atp_ri_descend_ft / atp_ri_find_redex_ft: pre-order walk over
//    AtpFtCell* (cell->next for sibling step, cell->next + recurse for
//    a CTR child step, cell->end->next to skip a subtree -- all O(1)).
//
//  - g_atp_ri_star_ft[k] : AtpFtCell* replaces the flat-position
//    star[].  First bind installs the subject cell pointer; a repeat
//    bind structural-checks via ft_eq.  Unwound on backtrack, mirroring
//    the Term-path's star[k] = NIL idiom.
//
//  - In ft_norm.c (Stage 6), when THVM_ATPFT_RI is enabled, the
//    linear lhs_ft[] scan is replaced with atp_ri_find_redex_ft.
//
// Re-entrancy: the subject is threaded as PARAMETERS through the
// recursive descent (no g_atp_ri_subj_cell global).  Only the STAR
// bindings are file-static, the same way they are on the Term path.
//
// Gated on THVM_ATPFT_RI (implies THVM_ATPFT_NORM).

#ifdef THVM_ATPFT_RI

#include "../thvm.h"
#include "ft.h"

// Symbols brought in from the Term-side rule-index in _.c (same TU):
//   AtpRuleIndex, AtpRiNode, AtpRiRec
//   ATP_RI_NIL, ATP_RI_NUM, ATP_RI_STAR_BASE, ATP_RI_CTR_BASE, ATP_RI_MAXVARS
//   ATP_DT_DESCENT_DEPTH_CAP
//   ft_eq (from ft.c)
//   ft_is_var, ft_ctr_sym (static inline in ft.c)
//
// We do not redeclare them here -- this file is #include'd from _.c
// after all of the above are visible.

// --- Sibling FT pattern table ---------------------------------------
//
// Indexed by rule index, populated by atp_ri_sync_ft_patterns from
// s->lhs_ft[].  Sized to grow alongside s->n_rules.  Dead rules carry
// NULL; the descent skips a NULL pattern at the leaf-collect stage
// (same gate as r_dead[] on the Term path).

static AtpFtCell **g_atp_ri_lhs_ft = NULL;
static u32         g_atp_ri_lhs_ft_cap = 0;

static void atp_ri_lhs_ft_ensure(u32 need) {
  if (need <= g_atp_ri_lhs_ft_cap) return;
  u32 cap = g_atp_ri_lhs_ft_cap ? g_atp_ri_lhs_ft_cap * 2u : 64u;
  while (cap < need) cap *= 2u;
  AtpFtCell **p = (AtpFtCell **)realloc(g_atp_ri_lhs_ft,
                                        cap * sizeof(AtpFtCell *));
  if (p == NULL) {
    fprintf(stderr, "atp_ri_ft: lhs_ft mirror OOM\n");
    exit(1);
  }
  for (u32 i = g_atp_ri_lhs_ft_cap; i < cap; i++) p[i] = NULL;
  g_atp_ri_lhs_ft = p;
  g_atp_ri_lhs_ft_cap = cap;
}

// Refresh g_atp_ri_lhs_ft[] from s->lhs_ft[] for every CURRENTLY-LIVE
// rule (same dead/orient gates as atp_ri_rebuild).  Cheap O(n_rules)
// pointer assignment; called whenever the (Term-side) rule index is
// (re)built, since rule index visibility = rule presence in the FT
// pattern table on this path.
static void atp_ri_sync_ft_patterns(AtpState *s) {
  atp_ri_lhs_ft_ensure(s->n_rules);
  // Clear any stragglers above n_rules (we don't truncate the mirror,
  // only widen; an old pointer above n_rules is harmless but cleared
  // for tidy debug).
  for (u32 i = 0; i < g_atp_ri_lhs_ft_cap; i++) g_atp_ri_lhs_ft[i] = NULL;
  for (u32 i = 0; i < s->n_rules; i++) {
    if (s->n_unorient > 0u && !s->r_orient[i]) continue;
    if (s->r_dead != NULL && s->r_dead[i]) continue;
    g_atp_ri_lhs_ft[i] = s->lhs_ft[i];
  }
}

// --- STAR bindings (file-static, same shape as g_atp_ri_star) -------
static AtpFtCell *g_atp_ri_star_ft[ATP_RI_MAXVARS];
static u32        g_atp_ri_best_ft   = ATP_RI_NIL;
static AtpFtCell *g_atp_ri_best_star_ft[ATP_RI_MAXVARS];

// --- Descent helpers -------------------------------------------------
//
// Per the design: the descent reads the SUBJECT through stack
// parameters (subj cell + subj_end exclusive), and the STAR table
// from file-static g_atp_ri_star_ft[].  This is option (b): the
// descent stays callable from arbitrary call sites without a flat
// pre-pass.

// Map an AtpFtCell to the discrim tree edge label.  This is the FT-side
// analog of atp_ri_flatsym_raw (Term-side).  No first-appearance
// renumbering -- the tree was BUILT with first-appearance under the
// rule LHS; the SUBJECT side just consumes the symbols as it walks.
//
// Returns the edge sym; the caller compares against tree node syms
// (which were inserted by atp_ri_flatsym -- a CTR_BASE+label, NUM-marker
// CTR_BASE+ATPFT_NUM_MARKER as a CTR, or STAR_BASE+var_idx).
//
// Subject vars don't fold because there's no subject renumbering --
// the subject cell carries the bare var id in (cell->sym & ~WF_VAR_BIT),
// and the discrim tree's STAR edges match by structure (any subterm),
// not by id.  So a subject var simply doesn't head a CTR edge; the
// caller handles the STAR-only branch on its own.
//
// For a CTR-shaped cell we return CTR_BASE + cell->sym.  This matches
// the Term-path encoding because s->lhs's term_ext(CTR) == cell->sym
// (ft_from_term uses term_ext as the label, ft.c:170).
static inline u32 atp_ri_ft_ctr_sym(const AtpFtCell *c) {
  // ATPFT_NUM_MARKER is what NUM converts to in AtpFt-land (ft.c:162);
  // on the Term side NUM maps to ATP_RI_NUM (0).  Translate so the
  // edge label agrees with what the tree was built with.
  u32 raw = c->sym;
  if (raw == 0x40000000u /* ATPFT_NUM_MARKER, ft.c:162 */) {
    return ATP_RI_NUM;
  }
  return ATP_RI_CTR_BASE + raw;
}

// Forward-declared recursive shape.  Depth-capped exactly like
// atp_ri_descend_rec on the Term side.
static void atp_ri_descend_ft_rec(u32 node, AtpFtCell *subj, u32 depth);

// Walks the discrim tree from `node` against the SUBJECT cell `subj`
// and its full subterm span.  No "subj_end" parameter is needed because
// the descent visits the subj-rooted subterm exactly once: each STAR
// edge consumes the WHOLE subterm (advances past cell->end->next);
// each CTR edge consumes the head and recurses into the children
// (cell->next, with cell->end->next as their parent's continuation).
//
// At the END of the descent, "leaf" is detected by the same condition
// as the Term path: the tree node has no further children we can
// follow -- but the cleaner encoding is "we consumed the whole subj
// subterm".  We model that by passing a SECOND parameter to the
// child-loop that tracks whether we've walked off the end of the
// pattern's expected subterm shape.
//
// Implementation: parallel to atp_ri_descend_rec.  At each tree node:
//   1. If the subject is a var-cell, only STAR edges match.
//   2. If the subject is CTR, try the CTR edge (exactly one matches the
//      head sym), and try every STAR edge (a STAR matches any subterm).
//   3. After consuming the head (CTR), recurse on the first child;
//      siblings advance via cell->end->next.  The "end of the rule's
//      pattern subterm" coincides with the tree node having no children
//      -- that IS the leaf the rule was inserted at.
//
// We collapse the "advance" step by representing the SUBJECT POSITION
// via a stack of (cell, end_after) pairs.  But for an FT-native descent
// the equivalent is simpler: the descent recurses into the CTR's first
// child, and the caller's loop walks the head's siblings -- there is
// no flat-position to track.  So:
//
//   - "We're at the leaf": tree node has no children AND no STAR
//     children either (every path ends in a record at a leaf node, and
//     that's where rec_head != ATP_RI_NIL).  The Term-path detects this
//     by "pos == qend"; the FT path detects it by "we just consumed
//     the last cell of the subterm AND the tree node has rec_head".
//
// The clean way: we recurse with a SUBJ pointer that's the current
// SUBTERM HEAD.  On a CTR-edge step, the rule expects the subject's
// head to be that CTR; we recurse into the children PAIRWISE with the
// pattern's children.  That's what ft_match does -- and the discrim
// tree is just an n-ary trie over LHS pattern preorder.
//
// We implement the descent identically to atp_ri_descend_rec but in
// "AtpFtCell* with sibling stride cell->end->next" instead of "u32
// preorder index advancing by subsz[pos]".
//
// `subj` is the current SUBJECT subterm head.  At a tree node:
//   - If node has rec_head != NIL AND we've consumed all of subj's
//     subterm (i.e. arrived at the FINAL leaf the rule was inserted at),
//     collect the leaf.  This is the Term-path's "pos == qend" gate.
//   - Try each STAR child: bind subj's WHOLE subterm to slot k, recurse
//     on (c, ADVANCE PAST subj) -- but "advance past subj" in the FT
//     descent means "the descent at this level is done, the OUTER walk
//     resumes on the next sibling".  So the STAR child's continuation
//     is to consume zero more cells at THIS level.
//   - Try the unique CTR child whose sym matches subj's head.  Then
//     recurse into the children: subj->next is the first child cell,
//     subj->next->end->next is the second child cell, etc.
//
// This is intricate enough that we use an EXPLICIT "node match" recursion
// per pattern position, mirroring ft_match's child walk shape but
// driven by the discrim tree's children at each node.

// --- atp_ri_leaf_collect_ft ----------------------------------------
//
// At a leaf node (one whose rec_head != NIL), collect the minimum live
// rule index whose FT pattern is non-NULL.  When the rule index is
// "perfect" (any_folded == 0), the descent has proved structural
// equality + variable consistency exactly -- the rule fires without a
// re-verify.  When folded (rule-side var renumbering coarsened), the
// descent's structural match may still be loose; fall back via ft_match
// against the FT pattern.

// Leaf collect: pick the minimum live rule index.  Stage 6b enforces
// "perfect descent only" -- when ix->any_folded is set (some rule LHS
// folded a var id beyond REWRITE_MAX_VAR or beyond ATP_RI_MAXVARS),
// the descent does not reliably prove the match; we skip leaf
// collection in that case, find_redex_ft returns no-hit, and the
// caller in ft_norm.c falls back to its linear scan.  This matches
// the design's "perfect/fallback split is shared" between Term and FT.
static void atp_ri_leaf_collect_ft(u32 node, AtpRuleIndex *ix) {
  if (ix->any_folded) return;   // imperfect: caller handles via linear scan
  for (u32 r = ix->nodes[node].rec_head; r != ATP_RI_NIL;
       r = ix->recs[r].next) {
    ix->q_candidates++;
    u32 rule = ix->recs[r].rule;
    if (rule >= g_atp_ri_best_ft) continue;
    if (rule >= g_atp_ri_lhs_ft_cap) continue;
    if (g_atp_ri_lhs_ft[rule] == NULL) continue;  // dead -- no FT pattern
    g_atp_ri_best_ft = rule;
    for (u32 k = 0; k < ATP_RI_MAXVARS; k++) {
      g_atp_ri_best_star_ft[k] = g_atp_ri_star_ft[k];
    }
  }
}

// --- atp_ri_descend_ft (recursive) ----------------------------------
//
// `node` is the current tree node; `subj` is the SUBJECT cell at the
// current preorder position.  `subj_end` is the cell ONE PAST the
// current subterm slice -- used to detect "end of slice" (the
// Term-path's pos == qend).
//
// When we arrive at a tree node whose subj has been fully consumed
// (subj == subj_end), we're at a leaf.

static void atp_ri_descend_ft_at(u32 node, AtpFtCell *subj,
                                 AtpFtCell *subj_end, u32 depth,
                                 AtpRuleIndex *ix);

static void atp_ri_descend_ft_at(u32 node, AtpFtCell *subj,
                                 AtpFtCell *subj_end, u32 depth,
                                 AtpRuleIndex *ix) {
  if (depth >= ATP_DT_DESCENT_DEPTH_CAP) {
    ix->q_depth_capped++;
    return;
  }
  for (;;) {
    ix->q_nodevisits++;
    if (subj == subj_end) {
      // End of subject slice: this is a leaf position.
      atp_ri_leaf_collect_ft(node, ix);
      return;
    }
    // Compute the subject edge sym at the current cell.
    u32 csym_exact;
    AtpFtCell *subj_skip;          // cell after the current subterm
    int subj_is_var = ft_is_var(subj);
    if (subj_is_var) {
      // A subject var matches no CTR edge in the tree.  It only matches
      // STAR edges, AS A WHOLE SUBTERM (a leaf, subsz==1).
      csym_exact = 0xFFFFFFFFu;    // sentinel: no CTR match
      subj_skip  = subj->end->next; // a var cell's end == itself, so == subj->next
    } else {
      csym_exact = atp_ri_ft_ctr_sym(subj);
      subj_skip  = subj->end->next;
    }

    // Walk the tree node's children: STARs fan out (may recurse),
    // CTR/NUM matches at most one child.
    u32 ctr_next = ATP_RI_NIL;
    for (u32 c = ix->nodes[node].child; c != ATP_RI_NIL;
         c = ix->nodes[c].sibling) {
      u32 csym = ix->nodes[c].sym;
      if (csym >= ATP_RI_STAR_BASE && csym < ATP_RI_CTR_BASE) {
        u32 k = csym - ATP_RI_STAR_BASE;
        if (k >= ATP_RI_MAXVARS) continue;
        AtpFtCell *bound = g_atp_ri_star_ft[k];
        if (bound == NULL) {
          // First bind: this subterm.  Recurse with the subj
          // ADVANCED past the whole subterm.
          g_atp_ri_star_ft[k] = subj;
          atp_ri_descend_ft_at(c, subj_skip, subj_end, depth + 1u, ix);
          g_atp_ri_star_ft[k] = NULL;
        } else if (ft_eq(bound, subj)) {
          atp_ri_descend_ft_at(c, subj_skip, subj_end, depth + 1u, ix);
        }
      } else if (csym == csym_exact) {
        ctr_next = c;
      }
    }
    if (ctr_next == ATP_RI_NIL) return;
    // CTR continuation: tail-loop.  The CTR consumes the HEAD cell;
    // its children (in the rule's pattern) line up with subj's
    // children, which in pre-order are subj->next (first child), then
    // subj's next sibling at end-of-subterm is subj_skip.  So the
    // tail-continuation advances `subj` to subj->next and leaves
    // subj_end unchanged (the slice for THIS subterm).  But we are
    // INSIDE this subterm now: we want subj_end to remain the
    // ENCLOSING slice's end; the descent uses "subj == subj_end" as
    // the termination, and the slice we're walking is the ENTIRE
    // initial subterm.
    node = ctr_next;
    subj = subj->next;
    // subj_end unchanged
  }
}

// --- atp_ri_find_redex_ft -------------------------------------------
//
// Pre-order walk of the subject; for each CTR-headed cell, descend the
// tree.  STAR-headed cells (var leaves) can never head a redex (rule
// LHSs are CTR-rooted -- a var LHS would orient as x->t, and our
// orient pass rejects those).
//
// `clean_before` (a cell pointer): any cell whose subterm ends BEFORE
// clean_before in the pre-order chain is known non-redex (it was not
// touched by the most recent splice; the previous find_redex already
// proved it had no redex).  Stage 6b's pos-memo extension is reserved
// for a follow-up commit.

static int atp_ri_find_redex_ft(AtpRuleIndex *ix, AtpFtCell *root,
                                AtpFtCell *clean_before,
                                AtpFtCell **redex_out,
                                u32 *rule_out) {
  if (root == NULL) return 0;
  AtpFtCell *end_after = (root->end != NULL) ? root->end->next : NULL;
  for (AtpFtCell *p = root; p != NULL && p != end_after; p = p->next) {
    // Skip subterms fully consumed BEFORE the clean_before fence.
    // Cell pointer ordering is by allocation slot; pre-order walk
    // monotonically advances in memory only for FRESHLY-arena'd cells.
    // Stage 6b's full pos-memo is a follow-up; for now keep
    // clean_before disabled (callers pass NULL).
    (void)clean_before;

    if (ft_is_var(p)) continue;       // var heads never match a CTR-rooted rule

    // Reset STAR bindings for THIS descent.  The descent unwinds every
    // bind on backtrack so the table is all-NULL on exit, but we
    // defensively NULL-out before each query to harden against an
    // earlier-aborted descent (depth-cap, OOM).
    for (u32 k = 0; k < ATP_RI_MAXVARS; k++) g_atp_ri_star_ft[k] = NULL;
    g_atp_ri_best_ft = ATP_RI_NIL;

    ix->q_queries++;
    AtpFtCell *subj_end = p->end->next;
    atp_ri_descend_ft_at(ix->root, p, subj_end, 0u, ix);
    if (g_atp_ri_best_ft != ATP_RI_NIL) {
      *redex_out = p;
      *rule_out  = g_atp_ri_best_ft;
      return 1;
    }
  }
  return 0;
}

// --- Public API used by ft_norm.c (Stage 6) -------------------------
//
// Callers in ft_norm.c (when THVM_ATPFT_RI is on) bring up the index
// once per normalize entry (or when rule_index_dirty fires) and then
// drive find_redex from the find_redex hook.

int atp_ri_ft_enabled(void);
int atp_ri_ft_enabled(void) {
  return 1;
}

// Sync the FT pattern mirror.  Idempotent; called whenever rule_index
// needs (re)building from the FT-norm caller.  We do not duplicate the
// Term-side atp_ri_rebuild here -- the caller invokes the existing
// rebuild (so the SHARED discrim tree is up to date) and then asks us
// to refresh the FT pattern mirror.
void atp_ri_ft_sync(AtpState *s);
void atp_ri_ft_sync(AtpState *s) {
  atp_ri_sync_ft_patterns(s);
}

// The find-redex entry point invoked from ft_norm.c.
int atp_ri_find_redex_ft_pub(AtpState *s, AtpFtCell *root,
                             AtpFtCell **redex_out, u32 *rule_out);
int atp_ri_find_redex_ft_pub(AtpState *s, AtpFtCell *root,
                             AtpFtCell **redex_out, u32 *rule_out) {
  if (s->rule_index == NULL) return 0;
  AtpRuleIndex *ix = s->rule_index;
  // Make sure the Term-side tree is current; the caller is responsible
  // for rebuilding it (this gate just defensively bails on a stale tree).
  if (ix->n_rules_built != s->n_rules) return 0;
  return atp_ri_find_redex_ft(ix, root, NULL, redex_out, rule_out);
}

#endif // THVM_ATPFT_RI
