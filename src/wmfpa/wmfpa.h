// src/wmfpa/wmfpa.h -- a faithful port of Waldmeister's integrated
// flatterm + discrimination-tree normalization substrate ("FPA").
//
// This is package 1 of the port directed in the worktree brief: the
// flatterm REPRESENTATION (Nachf/Ende cells), a perfect DISCRIMINATION
// TREE (DSBaum) built from a variable-normalized oriented rule set, and
// a NormalformInnermost-faithful normalizer that retrieves redexes from
// the tree.  It is header-only and standalone: it touches the IC Term
// API only at the encode boundary (wf_encode), exactly as WM's
// TO_PlattklopfenLRMitMalloc converts external -> internal once.
//
// WM source followed (the spec; tinygrad-is-spec discipline):
//   include/TermOperationen.h        -- the flatterm cell {Symbol, Nachf,
//       Ende}; TO_NaechsterTeilterm = Ende->Nachf skips a subterm O(1).
//       We realize Nachf/Ende as a contiguous pre-order array + `size`
//       (subtree node count), so Ende->Nachf == self + size.
//   include/DSBaumKnoten.h           -- the DSBaum node: per-symbol
//       successor field, leaf holds the rule's flat lhs/rhs.
//   sources/INF/MatchOperationen.c:565 (MO_RegelGefunden) -- descend the
//       tree in lockstep with the subject flatterm, with backtracking
//       over already-bound variables; on a leaf, finish-match the rule
//       Rest and read off the substitution.
//   sources/NF/NFBildung.c:533 (BL_NormalformInnermost1) -- leftmost-
//       innermost: recurse children to fixpoint, then re-fire the redex
//       at this position; loop until no rule applies here.
//
// WM's StdS default is `mixmost` (Parameter.c:419 / NFBildung.c:637).
// A confluent terminating R reaches the SAME normal form under any
// fair strategy, so the innermost walk ported here computes the exact
// normal form of WM's mixmost on the (oriented) completion rule set --
// which the differential harness asserts against thvm's IC normalize on
// every subject (zero mismatches required before any timing is trusted).

#ifndef WMFPA_H
#define WMFPA_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ====================================================================
// == Flatterm representation =========================================
// ====================================================================
//
// WM's TermzellenT is {Symbol, Nachf, Ende}.  We lay a whole term out in
// one contiguous pre-order array; node i's `sym` is the Symbol, its
// children occupy [i+1, i+size), and `size` is the subtree node count.
// Thus:
//   TO_ErsterTeilterm(i)   == i+1               (first child)
//   TO_NaechsterTeilterm(i)== i + size[i]       (Ende->Nachf: skip subtree)
//   TO_TermEnde(i)         == i + size[i] - 1   (last cell of subtree)
// A variable leaf is sym = WF_VAR_BIT | id, size 1, arity 0.

#define WF_VAR_BIT   0x80000000u
#define WF_IS_VAR(s) (((s) & WF_VAR_BIT) != 0u)
#define WF_VAR_ID(s) ((s) & ~WF_VAR_BIT)
#define WF_MAX_VARS  64u             // matches REWRITE_MAX_VAR / KBO_MAX_VAR

typedef struct {
  uint32_t sym;     // function-symbol label, or WF_VAR_BIT|var_id
  uint32_t size;    // subtree node count (this node + descendants)
  uint16_t arity;   // child count (0 for a variable leaf)
  uint16_t _pad;
} WfNode;

// ====================================================================
// == Discrimination tree (DSBaum) ====================================
// ====================================================================
//
// A perfect discrimination tree over the variable-normalized rule LHS
// flatterms.  Edges are labelled by a flatterm symbol read in pre-order;
// a CTR edge consumes one subject node, a variable (star) edge consumes
// (skips) a whole subject subterm.  WM stores per-symbol successor slots
// in a node (BK_Nachf[Symbol]); we use a child / right-sibling chain in
// a flat node pool addressed by index (realloc-stable), the same shape
// thvm's AtpRiNode uses.  A leaf carries the rule index.
//
// `sym` on an edge is either WF_VAR_BIT|0 (the single star edge -- WM
// rebinds old variables by re-walking subject subterms, but for a
// LEFT-LINEAR or repeat-checked match we treat every rule-LHS variable
// occurrence after the first specially; see wf_descend) or a CTR label.

typedef struct {
  uint32_t sym;        // edge label into THIS node (CTR label, or star)
  uint32_t child;      // first child node index (WF_NIL = none)
  uint32_t sibling;    // next sibling node index (WF_NIL = none)
  uint32_t rule_head;  // leaf rule-record list head (WF_NIL = inner node)
} WfTreeNode;

typedef struct {
  uint32_t rule;       // index into the rule store
  uint32_t next;       // next rec at this leaf (WF_NIL = end)
} WfTreeRec;

#define WF_NIL       0xFFFFFFFFu
#define WF_STAR_SYM  (WF_VAR_BIT | 0u)   // canonical star edge label

// One stored oriented rule: flat lhs (pattern) + flat rhs (template).
typedef struct {
  WfNode  *lhs;  uint32_t llen;
  WfNode  *rhs;  uint32_t rlen;
} WfRule;

typedef struct {
  WfTreeNode *nodes;  uint32_t n_nodes, cap_nodes;
  WfTreeRec  *recs;   uint32_t n_recs,  cap_recs;
  uint32_t    root;
  // rule store
  WfRule     *rules;  uint32_t n_rules, cap_rules;
  // retrieval instrumentation
  uint64_t    q_queries, q_candidates, q_nodevisits, q_matchcalls;
} WfTree;

// ---- encode: IC term -> flat pre-order array (boundary, one walk) ----
// Caller supplies the IC accessors via the WfEnc callback table so this
// header has no IC dependency; the bench/engine pass thin wrappers.

// Term handles are the engine's 64-bit IC handles (the tag lives in the
// high bits), so the boundary uses uint64_t -- truncating to 32 bits
// would destroy the tag.
typedef uint64_t WfTermH;

typedef struct {
  // Return 1 if `t` is a variable; write its id to *id.  Else it is a
  // constructor: write its label to *label and arity to *arity.
  int (*is_var)(WfTermH t, uint32_t *id);
  uint32_t (*ctr_label)(WfTermH t);
  uint32_t (*ctr_arity)(WfTermH t);
  WfTermH (*ctr_child)(WfTermH t, uint32_t i);   // i-th child Term handle
} WfEnc;

static uint32_t wf_encode_rec(WfTermH t, WfNode *out, uint32_t *pos,
                              const WfEnc *e, uint32_t cap) {
  uint32_t self = *pos;
  if (*pos >= cap) return 0;
  (*pos)++;
  uint32_t vid;
  if (e->is_var(t, &vid)) {
    out[self].sym = WF_VAR_BIT | vid;
    out[self].size = 1u;
    out[self].arity = 0u;
    return 1u;
  }
  uint32_t lab = e->ctr_label(t);
  uint32_t n   = e->ctr_arity(t);
  uint32_t sz  = 1u;
  for (uint32_t i = 0; i < n; i++) {
    sz += wf_encode_rec(e->ctr_child(t, i), out, pos, e, cap);
  }
  out[self].sym = lab;
  out[self].size = sz;
  out[self].arity = (uint16_t)n;
  return sz;
}

// Returns the node count written (0 on overflow / empty).
static uint32_t wf_encode(WfTermH t, WfNode *out, uint32_t cap,
                          const WfEnc *e) {
  uint32_t pos = 0u;
  wf_encode_rec(t, out, &pos, e, cap);
  return pos;
}

// ---- tree build ------------------------------------------------------

static uint32_t wf_node_new(WfTree *ix, uint32_t sym) {
  if (ix->n_nodes == ix->cap_nodes) {
    uint32_t cap = ix->cap_nodes ? ix->cap_nodes * 2u : 1024u;
    ix->nodes = (WfTreeNode *)realloc(ix->nodes, cap * sizeof(WfTreeNode));
    ix->cap_nodes = cap;
  }
  uint32_t i = ix->n_nodes++;
  ix->nodes[i].sym = sym;
  ix->nodes[i].child = WF_NIL;
  ix->nodes[i].sibling = WF_NIL;
  ix->nodes[i].rule_head = WF_NIL;
  return i;
}

static uint32_t wf_rec_new(WfTree *ix, uint32_t rule) {
  if (ix->n_recs == ix->cap_recs) {
    uint32_t cap = ix->cap_recs ? ix->cap_recs * 2u : 1024u;
    ix->recs = (WfTreeRec *)realloc(ix->recs, cap * sizeof(WfTreeRec));
    ix->cap_recs = cap;
  }
  uint32_t i = ix->n_recs++;
  ix->recs[i].rule = rule;
  ix->recs[i].next = WF_NIL;
  return i;
}

// Pre-order edge label for the pattern node: a CTR keeps its label; ANY
// variable becomes the single star edge (WM's perfect tree branches on
// function symbol vs "a variable here"; repeat-variable consistency is
// re-checked at the leaf, exactly as MO_RegelGefunden's EsFolgtSubstitutVon).
static uint32_t wf_pat_edge(uint32_t sym) {
  return WF_IS_VAR(sym) ? WF_STAR_SYM : sym;
}

// Find or create the child of `node` reached by edge `sym`.
static uint32_t wf_child_for(WfTree *ix, uint32_t node, uint32_t sym) {
  uint32_t c = ix->nodes[node].child;
  uint32_t prev = WF_NIL;
  while (c != WF_NIL) {
    if (ix->nodes[c].sym == sym) return c;
    prev = c;
    c = ix->nodes[c].sibling;
  }
  uint32_t nn = wf_node_new(ix, sym);
  // re-read node pointer after possible realloc
  if (prev == WF_NIL) ix->nodes[node].child = nn;
  else                ix->nodes[prev].sibling = nn;
  return nn;
}

// Insert a stored rule (already in ix->rules[rule]) into the tree by its
// flat LHS pre-order edge string.  Mirrors BO_ObjektEinfuegen.
static void wf_tree_insert(WfTree *ix, uint32_t rule) {
  const WfNode *pat = ix->rules[rule].lhs;
  uint32_t plen = ix->rules[rule].llen;
  if (plen == 0u) return;        // un-encodable (over-deep) rule: absent
  uint32_t node = ix->root;
  for (uint32_t i = 0; i < plen; i++) {
    node = wf_child_for(ix, node, wf_pat_edge(pat[i].sym));
  }
  uint32_t r = wf_rec_new(ix, rule);
  ix->recs[r].next = ix->nodes[node].rule_head;
  ix->nodes[node].rule_head = r;
}

static void wf_tree_init(WfTree *ix) {
  memset(ix, 0, sizeof *ix);
  ix->root = WF_NIL;
}

// Build the tree from a rule array.  `rules` already point at caller-
// owned flat lhs/rhs buffers.
static void wf_tree_build(WfTree *ix, WfRule *rules, uint32_t n_rules) {
  ix->rules = rules;
  ix->n_rules = n_rules;
  ix->cap_rules = n_rules;
  ix->root = wf_node_new(ix, 0u);
  for (uint32_t r = 0; r < n_rules; r++) wf_tree_insert(ix, r);
}

static void wf_tree_free(WfTree *ix) {
  free(ix->nodes);
  free(ix->recs);
}

// ====================================================================
// == Retrieval: MO_RegelGefunden over the flatterm =================
// ====================================================================
//
// Descend the tree in lockstep with the subject subtree rooted at `sp`.
// At each tree node we try, in order: the CTR edge equal to the subject
// node's symbol (consume one node), then the star edge (bind/skip the
// whole subject subtree).  We backtrack via an explicit stack so that
// after a CTR descent dead-ends we still try the star edge -- this is
// MO_RegelGefunden's Backtrack1/tryNVar interplay.  At a leaf we confirm
// repeat-variable consistency (EsFolgtSubstitutVon) and read off the
// substitution as subject (offset,len) slices.
//
// Returns the matched rule index (and fills bind[]) or WF_NIL.

typedef struct { uint32_t off, len; } WfBind;   // len 0 = unbound

// star binding occurrences recorded during descent, in rule-variable
// order: the i-th star edge taken binds the i-th *encountered* variable.
// We record (subject offset,len); repeat-variable equality is enforced
// by re-encountering the SAME tree star edge id during insert is not
// tracked, so we enforce it the WM way: the leaf's flat lhs is walked
// against the subject with the recorded bindings.

typedef struct {
  uint32_t node;     // the star-edge child to take on backtrack
  uint32_t sp;       // subject position to skip-from when taking it
} WfBT;

// Confirm the matched leaf rule against the subject with full repeat-
// variable consistency, writing final bindings.  Walks the rule lhs and
// subject in lockstep (MatchOperationen.c:621 leave-loop), exactly the
// flat match the IC engine does.  Returns 1 on match.
static int wf_leaf_confirm(const WfNode *pat, uint32_t plen,
                           const WfNode *subj, uint32_t sp, WfBind *bind) {
  for (uint32_t v = 0; v < WF_MAX_VARS; v++) bind[v].len = 0u;
  uint32_t pi = 0u, si = sp;
  while (pi < plen) {
    uint32_t psym = pat[pi].sym;
    if (WF_IS_VAR(psym)) {
      uint32_t id = WF_VAR_ID(psym);
      uint32_t slen = subj[si].size;
      if (id >= WF_MAX_VARS) return 0;
      if (bind[id].len == 0u) { bind[id].off = si; bind[id].len = slen; }
      else {
        if (bind[id].len != slen) return 0;
        for (uint32_t k = 0; k < slen; k++)
          if (subj[bind[id].off + k].sym != subj[si + k].sym) return 0;
      }
      pi += 1u; si += slen;
    } else {
      if (WF_IS_VAR(subj[si].sym) || subj[si].sym != psym) return 0;
      pi += 1u; si += 1u;
    }
  }
  return 1u;
}

// Descend the tree from position `sp`, exploring EVERY tree path that
// matches the subject prefix (CTR edge then star edge at each node),
// and return the globally LOWEST rule index whose LHS confirms.  The
// lowest-index policy mirrors thvm's IC rewrite step (rewrite/_.c
// thvm_rewrite_step picks the first applicable rule by index at the
// leftmost-outermost redex), so the WM-FPA normal form is byte-identical
// even on a non-confluent (mid-completion) rule set.  A single tree path
// can be shared by many rules differing only in repeat-variable
// structure (f(x,x) vs f(x,y) both descend star,star); confirming at the
// leaf disambiguates.  We do NOT early-return at the first matching leaf:
// a lower-index rule may live on a different (e.g. more CTR-specific)
// path that DFS reaches later.
static uint32_t wf_descend(WfTree *ix, const WfNode *subj, uint32_t sp,
                           WfBind *bind, uint32_t *out_rule) {
  ix->q_queries++;
  WfBT stack[256];
  uint32_t top = 0u;
  uint32_t node = ix->root;
  uint32_t si   = sp;
  uint32_t best = WF_NIL;

  for (;;) {
    ix->q_nodevisits++;
    if (ix->nodes[node].rule_head != WF_NIL) {
      // leaf: confirm each rec, keeping the lowest confirming rule index.
      for (uint32_t r = ix->nodes[node].rule_head; r != WF_NIL;
           r = ix->recs[r].next) {
        ix->q_candidates++;
        uint32_t rule = ix->recs[r].rule;
        if (best != WF_NIL && rule >= best) continue;   // can't improve
        ix->q_matchcalls++;
        WfBind tb[WF_MAX_VARS];
        if (wf_leaf_confirm(ix->rules[rule].lhs, ix->rules[rule].llen,
                            subj, sp, tb)) {
          best = rule;
          memcpy(bind, tb, sizeof(WfBind) * WF_MAX_VARS);
        }
      }
      // leaf consumed: backtrack to explore other paths.
    } else {
      uint32_t subj_sym = subj[si].sym;
      uint32_t ctr_child = WF_NIL, star_child = WF_NIL;
      for (uint32_t c = ix->nodes[node].child; c != WF_NIL;
           c = ix->nodes[c].sibling) {
        uint32_t es = ix->nodes[c].sym;
        if (es == WF_STAR_SYM) star_child = c;
        else if (!WF_IS_VAR(subj_sym) && es == subj_sym) ctr_child = c;
      }
      if (ctr_child != WF_NIL) {
        if (star_child != WF_NIL && top < 256u) {
          stack[top].node = star_child;
          stack[top].sp = si;
          top++;
        }
        node = ctr_child;
        si += 1u;
        continue;
      }
      if (star_child != WF_NIL) {
        node = star_child;
        si += subj[si].size;
        continue;
      }
      // dead-end
    }
    // backtrack
    if (top == 0u) {
      if (best != WF_NIL) { *out_rule = best; return 1u; }
      return 0u;
    }
    top--;
    node = stack[top].node;
    si = stack[top].sp;
    si += subj[si].size;
  }
}

// ====================================================================
// == Substitution + splice (flatterm subst, no IC alloc) =============
// ====================================================================

static void wf_fixup(WfNode *a, uint32_t base, uint32_t end) {
  for (uint32_t ip = end; ip > base; ) {
    ip--;
    if (WF_IS_VAR(a[ip].sym)) continue;
    uint32_t sz = 1u, c = ip + 1u;
    for (uint32_t k = 0; k < a[ip].arity; k++) { sz += a[c].size; c += a[c].size; }
    a[ip].size = sz;
  }
}

// Emit rhs with bound variable slices spliced in, into out[*op].
static void wf_emit_subst(const WfNode *rhs, uint32_t rlen,
                          const WfNode *subj, const WfBind *bind,
                          WfNode *out, uint32_t *op) {
  for (uint32_t ri = 0; ri < rlen; ri++) {
    uint32_t sym = rhs[ri].sym;
    if (WF_IS_VAR(sym)) {
      uint32_t id = WF_VAR_ID(sym);
      uint32_t boff = bind[id].off, blen = bind[id].len;
      for (uint32_t k = 0; k < blen; k++) out[(*op)++] = subj[boff + k];
    } else {
      out[*op].sym = sym; out[*op].arity = rhs[ri].arity; (*op)++;
    }
  }
}

// ====================================================================
// == Normalize over the tree (NFBildung.c:533 / 615) =================
// ====================================================================
//
// WM's path-walking normalizers (BL_NormalformInnermost1 / mixmost) all
// drive redex retrieval through MO_RegelGefunden on the DSBaum; they
// differ only in the ORDER positions are visited.  thvm's IC normalize
// (rewrite/_.c thvm_rewrite_step) is leftmost-OUTERMOST, lowest-index
// rule, one redex per step, iterated to fixpoint.  To make the WM-FPA
// normal form BYTE-IDENTICAL to thvm's even on a non-confluent (mid-
// completion) rule set, we replicate exactly that visit order: a single
// pre-order scan (parent before children, left sibling before right ==
// leftmost-outermost) picks the first reducible position, and wf_descend
// returns the lowest-index applicable rule there.  On a confluent
// terminating R this is also WM's mixmost normal form (strategy-
// independent); the differential harness asserts equality on every
// subject regardless.  Operates on a flat term in `a` (length *alen);
// `b` is ping-pong scratch for each splice.

// One leftmost-outermost rewrite step: scan pre-order, fire the lowest-
// index rule at the first reducible position.  Returns 0 (no redex) when
// the term is in normal form.
// Length of the rule's substituted RHS under the recorded bindings (so
// the splice's total output length can be checked against the arena cap
// BEFORE any write -- a rewrite can grow the term, e.g. a -> f(b,c)).
static uint32_t wf_subst_len(const WfNode *rhs, uint32_t rlen,
                             const WfBind *bind) {
  uint32_t len = 0u;
  for (uint32_t ri = 0; ri < rlen; ri++) {
    uint32_t sym = rhs[ri].sym;
    len += WF_IS_VAR(sym) ? bind[WF_VAR_ID(sym)].len : 1u;
  }
  return len;
}

// Returns 1 on a fired rewrite, 0 on normal form, 2 on arena overflow
// (the rewritten term would exceed `out_cap`; caller must fall back).
static int wf_step(WfTree *ix, const WfNode *subj, uint32_t slen,
                   WfNode *out, uint32_t out_cap, uint32_t *outlen) {
  for (uint32_t p = 0; p < slen; p++) {
    if (WF_IS_VAR(subj[p].sym)) continue;
    WfBind bind[WF_MAX_VARS];
    uint32_t rule;
    if (wf_descend(ix, subj, p, bind, &rule)) {
      uint32_t skip = subj[p].size;
      uint32_t sublen = wf_subst_len(ix->rules[rule].rhs,
                                     ix->rules[rule].rlen, bind);
      uint32_t total = (slen - skip) + sublen;   // prefix+suffix unchanged
      if (total > out_cap) return 2;             // would overflow arena
      uint32_t op = 0u;
      for (uint32_t i = 0; i < p; i++) out[op++] = subj[i];
      wf_emit_subst(ix->rules[rule].rhs, ix->rules[rule].rlen,
                    subj, bind, out, &op);
      for (uint32_t i = p + skip; i < slen; i++) out[op++] = subj[i];
      wf_fixup(out, 0u, op);
      *outlen = op;
      return 1u;
    }
  }
  return 0u;
}

// Normalize to fixpoint within an arena of `cap` nodes; *result points
// at the arena holding the NF.  *overflow is set to 1 if a rewrite would
// exceed the arena (the partial term is returned and the caller should
// fall back to the unbounded path); 0 otherwise.
static uint32_t wf_normalize(WfTree *ix, WfNode *a, uint32_t alen,
                             WfNode *b, uint32_t cap, uint32_t step_cap,
                             WfNode **result, int *overflow) {
  WfNode *cur = a, *nxt = b;
  *overflow = 0;
  for (uint32_t i = 0; i < step_cap; i++) {
    uint32_t nlen = 0u;
    int st = wf_step(ix, cur, alen, nxt, cap, &nlen);
    if (st == 0) { *result = cur; return alen; }
    if (st == 2) { *result = cur; *overflow = 1; return alen; }
    WfNode *tmp = cur; cur = nxt; nxt = tmp;
    alen = nlen;
  }
  *result = cur;
  return alen;
}

#endif  // WMFPA_H
