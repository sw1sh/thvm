// tools/bench_flatcore.c -- flatterm inner-loop spike + A/B microbench.
//
// A MEASURED SPIKE (de-risk step) for a purpose-built saturation inner
// loop that keeps terms in a flatterm representation throughout, so
// match / rewrite-normalize / KBO-compare all run on contiguous
// pre-order node arrays with NO IC-graph traversal, NO per-op
// substitution allocation, NO GC churn.
//
// This file is STANDALONE: it includes src/thvm.c (so it can drive a
// real AndAssociativity / Sheffer-stroke completion to harvest the
// REAL derived rule set + subject corpus), but the flat core itself
// (FlatNode / fc_*) touches the IC term API only ONCE per term, at the
// encode boundary. The default engine is untouched.
//
// Build:  make bin/bench_flatcore
// Run:    bin/bench_flatcore [warmup_steps] [n_ops]
//           warmup_steps = completion steps used to harvest rules+corpus
//                          (default 1200; ~ the proof's mid-game depth)
//           n_ops        = inner-loop ops timed each way (default 2000000)
//
// The two paths compute the IDENTICAL operation -- normalize a subject
// to fixpoint against the ORIENTED rule set (leftmost-outermost, lowest-
// index rule, one redex/step; the WM/Twee/E inner loop) then KBO-orient
// the result against the subject -- and the bench ASSERTS they agree
// (normal form structurally + KBO verdict) on every input. A divergence
// aborts: the flat core would be wrong.
//
// Reference IC path: thvm_rewrite_normalize (rewrite/_.c) + thvm_kbo
// (kbo/_.c, Loechner linear KBO ported from Waldmeister ORD/KBO.c).
// Flat path: fc_normalize + fc_kbo_compare below.

#include "../src/thvm.c"
#include <time.h>

// nand = the single binary symbol; CTR label 1 (matches the harness in
// tests/test_atp_wolfram_bench.c). Sheffer signature: {nand, vars}.
#define L_NAND 1u

static Term nand2(Term x, Term y) {
  Term c[2] = { x, y };
  return term_new_ctr(L_NAND, c, 2);
}
static Term fv(u32 id) { return term_new_fvr(id); }
static Term axiom_lhs(void) {
  Term a = fv(0), b = fv(1), c = fv(2);
  return nand2(nand2(nand2(a, b), c),
               nand2(a, nand2(nand2(a, c), a)));
}

// ====================================================================
// == Flat core ========================================================
// ====================================================================
//
// FlatNode layout (one contiguous pre-order array per term). Each node
// carries inline metadata sufficient to make every hot op cheap:
//
//   sym    : function symbol label (CTR) for a constructor node, OR the
//            variable marker FC_VAR_BIT|var_id for a variable leaf. One
//            field disambiguates leaf-vs-internal AND identifies the
//            symbol -- a single compare in match / KBO top-symbol.
//   size   : subtree node count (this node + all descendants). Gives
//            O(1) "skip this subterm" navigation: node i's children
//            occupy [i+1 .. i+size), and the sibling of a subtree at
//            position p is at p + size[p]. No recursion, no child-base
//            reads -- this is Waldmeister's `Ende` skip pointer
//            (sources/TermOperationen.h:65, the TermzellenT `Ende`
//            field that always points one past the subterm) realised
//            as a count so it survives array splicing.
//   weight : precomputed total KBO weight of THIS whole subterm
//            (Sum of symbol weights, var_weight per variable). Kills the
//            per-compare weight recompute: vortest reads two cached
//            roots; on a tie the lex walk reads cached child weights.
//
// arity is implicit (nand is the only symbol => binary; vars are
// leaves) but we keep a generic walk via `size` so the core is not
// hard-wired to a fixed arity. The bench signature is binary `nand`.
//
// Encoding (fc_encode) is the ONLY place the IC term API is touched,
// once per term. Match/subst/normalize/KBO never call heap_read.

#define FC_VAR_BIT  0x80000000u            // sym high bit set => variable
#define FC_IS_VAR(s) (((s) & FC_VAR_BIT) != 0u)
#define FC_VAR_ID(s) ((s) & ~FC_VAR_BIT)
#define FC_MAX_NODES 8192u                 // per-term node cap (subjects
                                           // in the corpus stay well under)
#define FC_MAX_VARS  64u                   // matches REWRITE/KBO_MAX_VAR

typedef struct {
  u32 sym;      // CTR label, or FC_VAR_BIT|var_id
  u32 size;     // subtree node count (this node + descendants)
  i32 weight;   // precomputed KBO weight of this subterm
  u16 arity;    // child count (0 for variable leaves) -- lets fixup
                // recompute size/weight after a splice without knowing
                // the signature, and lets KBO lex-recurse generically
  u16 _pad;
} FlatNode;

typedef struct {
  FlatNode *node;
  u32       len;
  u32       cap;
} FlatTerm;

// Shared per-batch read-only KBO tables (mirror KboConfig). For the
// Sheffer signature: weights[nand]=1, var_weight=1, identity precedence.
typedef struct {
  const u32 *weights;
  const u32 *precedence;
  u32        n_labels;
  i32        var_weight;
} FcKbo;

// --- encode: IC term -> flat pre-order array (one-time, at boundary) --
//
// Recursive ONLY here (one walk per term, at the encode boundary, not in
// the hot loop). Writes nodes in pre-order; back-patches `size`; folds
// `weight` bottom-up.
static u32 fc_encode_rec(Term t, FlatNode *out, u32 *pos, const FcKbo *kb) {
  u32 self = *pos;
  if (*pos >= FC_MAX_NODES) { return 0; }
  (*pos)++;
  switch (term_tag(t)) {
    case TAG_FVR: {
      u32 id = term_ext(t);
      out[self].sym    = FC_VAR_BIT | id;
      out[self].size   = 1u;
      out[self].weight = kb->var_weight;
      out[self].arity  = 0u;
      return 1u;
    }
    case TAG_CTR: {
      u32 lab = term_ext(t);
      u32 n   = term_ctr_n(t);
      i32 w   = (i32)((lab < kb->n_labels) ? kb->weights[lab] : 0u);
      u32 sz  = 1u;
      for (u32 i = 0; i < n; i++) {
        u32 child_root = *pos;
        u32 csz = fc_encode_rec(term_ctr_at(t, i), out, pos, kb);
        sz += csz;
        w  += out[child_root].weight;
      }
      out[self].sym    = lab;
      out[self].size   = sz;
      out[self].weight = w;
      out[self].arity  = (u16)n;
      return sz;
    }
    default:
      // Sheffer corpus is CTR+FVR only; anything else is a bug.
      out[self].sym    = 0u;
      out[self].size   = 1u;
      out[self].weight = 0;
      out[self].arity  = 0u;
      return 1u;
  }
}

static void fc_encode(Term t, FlatTerm *ft, const FcKbo *kb) {
  u32 pos = 0u;
  fc_encode_rec(t, ft->node, &pos, kb);
  ft->len = pos;
}

// --- match: flat pattern vs flat subject subterm -----------------------
//
// Linear single-pass match of pattern array `pat` against the subject
// subtree rooted at subject position `sp`. Bindings record subject
// (offset,len) slices -- no copies. A variable seen twice must bind to a
// structurally identical subject slice (consistency via memcmp on the
// FlatNode slice, since sym+size fully determine structure). Returns 1 on
// match. Mirrors thvm_match (rewrite/_.c:27) exactly: FVR binds/checks,
// CTR requires same sym + descends.
typedef struct {
  u32 off;     // subject offset of the bound slice (0 = unbound sentinel
               // is invalid since subjects are nonempty -> use len==0)
  u32 len;     // slice length in nodes; 0 = unbound
} FcBind;

static u8 fc_slice_eq(const FlatNode *a, u32 ao, const FlatNode *b, u32 bo,
                      u32 n) {
  for (u32 i = 0; i < n; i++) {
    if (a[ao + i].sym != b[bo + i].sym) return 0;
    // size is implied by sym-sequence; comparing sym across the whole
    // slice is sufficient and is what kbo_eq's structural compare does.
  }
  return 1;
}

static u8 fc_match(const FlatNode *pat, u32 plen,
                   const FlatNode *subj, u32 sp,
                   FcBind *bind) {
  // Walk pattern and subject in lockstep pre-order.
  u32 pi = 0u;     // pattern index
  u32 si = sp;     // subject index
  while (pi < plen) {
    u32 psym = pat[pi].sym;
    if (FC_IS_VAR(psym)) {
      u32 id  = FC_VAR_ID(psym);
      u32 slen = subj[si].size;          // whole subject subtree at si
      if (id >= FC_MAX_VARS) return 0;
      if (bind[id].len == 0u) {
        bind[id].off = si;
        bind[id].len = slen;
      } else {
        if (bind[id].len != slen) return 0;
        if (!fc_slice_eq(subj, bind[id].off, subj, si, slen)) return 0;
      }
      pi += 1u;        // variable leaf is one pattern node
      si += slen;      // skip the matched subject subtree (O(1))
    } else {
      // constructor: symbols must agree; descend in lockstep.
      if (FC_IS_VAR(subj[si].sym)) return 0;
      if (subj[si].sym != psym)    return 0;
      pi += 1u;
      si += 1u;
    }
  }
  return 1u;
}

// own (non-recursive) KBO weight of one node from its symbol.
static inline i32 fc_node_self_weight(u32 sym, const FcKbo *kb) {
  if (FC_IS_VAR(sym)) return kb->var_weight;
  return (i32)((sym < kb->n_labels) ? kb->weights[sym] : 0u);
}

// --- subst+splice: build the substituted RHS via flat slice copies ----
//
// Emit the substituted rhs into `out` at `*op`, splicing bound subject
// slices in for variables. Constructor template nodes are copied with a
// placeholder size/weight; a single bottom-up fixup pass over the emitted
// region then sets correct size+weight (vars copied verbatim are already
// correct). No allocation: out is a caller arena.
static void fc_emit_subst(const FlatNode *rhs, u32 rlen,
                          const FlatNode *subj, const FcBind *bind,
                          FlatNode *out, u32 *op) {
  for (u32 ri = 0; ri < rlen; ri++) {
    u32 sym = rhs[ri].sym;
    if (FC_IS_VAR(sym)) {
      u32 id   = FC_VAR_ID(sym);
      u32 boff = bind[id].off;
      u32 blen = bind[id].len;
      for (u32 k = 0; k < blen; k++) out[(*op)++] = subj[boff + k];
    } else {
      out[*op].sym   = sym;             // size/weight set by fc_fixup
      out[*op].arity = rhs[ri].arity;
      (*op)++;
    }
  }
}

// Recompute size + weight bottom-up over the emitted region [base,end).
// Right-to-left so a constructor's children are finalized before it.
// Children of node i occupy contiguous subtrees starting at i+1; hop
// `arity` of them via their (already-correct) sizes. Var slices copied
// verbatim are already correct and skipped.
static void fc_fixup(FlatNode *a, u32 base, u32 end, const FcKbo *kb) {
  for (u32 ip = end; ip > base; ) {
    ip--;
    if (FC_IS_VAR(a[ip].sym)) continue;
    u32 sz = 1u;
    i32 w  = fc_node_self_weight(a[ip].sym, kb);
    u32 c  = ip + 1u;
    for (u32 k = 0; k < a[ip].arity; k++) {
      sz += a[c].size;
      w  += a[c].weight;
      c  += a[c].size;
    }
    a[ip].size   = sz;
    a[ip].weight = w;
  }
}

// --- one normalize step: leftmost-outermost, lowest-index rule --------
//
// Mirrors thvm_rewrite_step (rewrite/_.c:102) EXACTLY: try every rule at
// the top, then descend children left-to-right, first redex wins -- which
// in a pre-order array IS the lowest-position node (parent before child,
// left sibling before right). So a single pre-order scan with size-skip
// finds the same redex the IC recursion does. On a hit we build the
// rewritten whole term into `out`: copy the unchanged prefix [0,p),
// emit the substituted rhs, copy the unchanged suffix [p+size, len), then
// fixup the spine weights/sizes from the splice point up to the root.
//
// Rules are flat arrays lhs[r]/rhs[r] with lengths llen[r]/rlen[r].
// Returns 1 and writes the rewritten term to (out,*outlen) on a fire; 0
// (no redex) otherwise.
typedef struct {
  const FlatNode *lhs; u32 llen;
  const FlatNode *rhs; u32 rlen;
} FcRule;

static u8 fc_step(const FlatNode *subj, u32 slen,
                  const FcRule *rules, u32 n_rules,
                  FlatNode *out, u32 *outlen, const FcKbo *kb) {
  for (u32 p = 0; p < slen; p++) {
    // try each rule at position p, lowest index first.
    for (u32 r = 0; r < n_rules; r++) {
      // quick reject: a constructor pattern's top symbol must match the
      // subject node's symbol (the discrimination tree's first cut).
      const FlatNode *pl = rules[r].lhs;
      if (!FC_IS_VAR(pl[0].sym) && pl[0].sym != subj[p].sym) continue;
      FcBind bind[FC_MAX_VARS];
      for (u32 v = 0; v < FC_MAX_VARS; v++) bind[v].len = 0u;
      if (!fc_match(pl, rules[r].llen, subj, p, &bind[0])) continue;
      // fire: rebuild whole term with rhs[r] substituted at p.
      u32 op = 0u;
      for (u32 i = 0; i < p; i++) out[op++] = subj[i];
      fc_emit_subst(rules[r].rhs, rules[r].rlen, subj, bind, out, &op);
      u32 skip = subj[p].size;
      for (u32 i = p + skip; i < slen; i++) out[op++] = subj[i];
      // one bottom-up pass fixes the spliced region AND every ancestor
      // whose size/weight shifted (term is small; this is the same span
      // the IC path's term_new_ctr rebuild walks).
      fc_fixup(out, 0u, op, kb);
      *outlen = op;
      return 1u;
    }
  }
  return 0u;
}

// normalize to fixpoint, ping-ponging between two arenas (pointer swap,
// no copy-back). Returns the final node count; *result points at whichever
// arena holds the normal form.
static u32 fc_normalize(FlatNode *a, u32 alen,
                        const FcRule *rules, u32 n_rules,
                        FlatNode *b, u32 step_cap, const FcKbo *kb,
                        FlatNode **result) {
  FlatNode *cur = a, *nxt = b;
  for (u32 i = 0; i < step_cap; i++) {
    u32 nlen = 0u;
    if (!fc_step(cur, alen, rules, n_rules, nxt, &nlen, kb)) {
      *result = cur;
      return alen;
    }
    FlatNode *tmp = cur; cur = nxt; nxt = tmp;
    alen = nlen;
  }
  *result = cur;
  return alen;
}

// --- KBO compare on flat arrays (Loechner / Baader-Nipkow) ------------
//
// Identical decision to thvm_kbo (kbo/_.c) / thvm_kbo_naive: weight
// balance + variable-occurrence balance over the divergent subterms, then
// top-symbol precedence, then lexicographic recursion on a tie. Uses the
// CACHED root weights for the dominant weight test (the whole point of the
// inline metadata) and per-variable balance gathered by one pass over the
// two subtrees. Returns KBO_EQ/GT/LT/UN.

// structural equality of two flat subtrees (sym sequence over the slices;
// sizes follow from syms). Mirrors kbo_eq.
static u8 fc_eq(const FlatNode *a, u32 ap, const FlatNode *b, u32 bp) {
  u32 n = a[ap].size;
  if (n != b[bp].size) return 0;
  return fc_slice_eq(a, ap, b, bp, n);
}

// gather count(s,v)-count(t,v) into bal[], touched ids into touched[],
// over the DIVERGENT subterms only (Waldmeister Vortest): walk s and t in
// lockstep; identical subtrees cancel and are skipped via size. *wb gets
// weight(s)-weight(t) using cached subtree weights at the divergence
// points. Returns 1 iff the two subtrees are structurally identical.
static int fc_diff(const FlatNode *s, u32 sp, const FlatNode *t, u32 tp,
                   i64 *wb, int *bal, u32 *touched, u32 *n_touched) {
  // identical slice: contributes nothing.
  if (s[sp].size == t[tp].size && fc_slice_eq(s, sp, t, tp, s[sp].size))
    return 1;
  if (s[sp].sym == t[tp].sym && !FC_IS_VAR(s[sp].sym)) {
    // same constructor, diverge below: recurse over children in lockstep.
    u32 sc = sp + 1u, tc = tp + 1u;
    int ident = 1;
    for (u32 k = 0; k < s[sp].arity; k++) {
      ident &= fc_diff(s, sc, t, tc, wb, bal, touched, n_touched);
      sc += s[sc].size;
      tc += t[tc].size;
    }
    return ident;
  }
  // genuine divergence: s wholly +, t wholly -.
  // weight balance from cached roots.
  *wb += (i64)s[sp].weight - (i64)t[tp].weight;
  // variable balance: walk each subtree's variable leaves.
  for (u32 i = 0; i < s[sp].size; i++) {
    if (FC_IS_VAR(s[sp + i].sym)) {
      u32 id = FC_VAR_ID(s[sp + i].sym);
      if (id < FC_MAX_VARS) { if (bal[id] == 0) touched[(*n_touched)++] = id; bal[id] += 1; }
    }
  }
  for (u32 i = 0; i < t[tp].size; i++) {
    if (FC_IS_VAR(t[tp + i].sym)) {
      u32 id = FC_VAR_ID(t[tp + i].sym);
      if (id < FC_MAX_VARS) { if (bal[id] == 0) touched[(*n_touched)++] = id; bal[id] -= 1; }
    }
  }
  return 0;
}

static KboCmp fc_kbo_rec(const FlatNode *s, u32 sp, const FlatNode *t, u32 tp,
                         const FcKbo *kb) {
  i64 wb = 0;
  int bal[FC_MAX_VARS]; u32 touched[FC_MAX_VARS]; u32 nt = 0u;
  for (u32 i = 0; i < FC_MAX_VARS; i++) bal[i] = 0;
  if (fc_diff(s, sp, t, tp, &wb, bal, touched, &nt)) return KBO_EQ;

  u8 s_dom = 1, t_dom = 1;
  for (u32 i = 0; i < nt; i++) {
    int v = bal[touched[i]];
    if (v < 0) s_dom = 0;
    if (v > 0) t_dom = 0;
  }
  if (!s_dom && !t_dom) return KBO_UN;
  if (wb > 0) return s_dom ? KBO_GT : KBO_UN;
  if (wb < 0) return t_dom ? KBO_LT : KBO_UN;

  // weights equal: compare top symbols.
  u8 s_var = FC_IS_VAR(s[sp].sym), t_var = FC_IS_VAR(t[tp].sym);
  if (s_var || t_var) return KBO_UN;
  u32 ps = (s[sp].sym < kb->n_labels) ? kb->precedence[s[sp].sym] : 0u;
  u32 pt = (t[tp].sym < kb->n_labels) ? kb->precedence[t[tp].sym] : 0u;
  if (ps > pt) return s_dom ? KBO_GT : KBO_UN;
  if (ps < pt) return t_dom ? KBO_LT : KBO_UN;
  if (s[sp].arity != t[tp].arity) return KBO_UN;
  u32 sc = sp + 1u, tc = tp + 1u;
  for (u32 k = 0; k < s[sp].arity; k++) {
    KboCmp c = fc_kbo_rec(s, sc, t, tc, kb);
    if (c == KBO_GT) return s_dom ? KBO_GT : KBO_UN;
    if (c == KBO_LT) return t_dom ? KBO_LT : KBO_UN;
    if (c == KBO_UN) return KBO_UN;
    sc += s[sc].size;
    tc += t[tc].size;
  }
  return KBO_EQ;
}

static KboCmp fc_kbo_compare(const FlatTerm *s, const FlatTerm *t,
                             const FcKbo *kb) {
  return fc_kbo_rec(s->node, 0u, t->node, 0u, kb);
}

// ====================================================================
// == Harness: harvest the REAL Sheffer rule set + subject corpus =====
// ====================================================================

#define FC_MAX_RULES   4096u
#define FC_MAX_CORPUS  4096u
#define FC_ARENA       (FC_MAX_NODES + 64u)

// flat-encoded rule store.
#define FC_RULE_BUF 1024u
static FlatNode g_rule_lhs[FC_MAX_RULES][FC_RULE_BUF];
static FlatNode g_rule_rhs[FC_MAX_RULES][FC_RULE_BUF];
static FcRule   g_frules[FC_MAX_RULES];
static u32      g_n_frules = 0u;

// IC rule store (parallel arrays for thvm_rewrite_normalize).
static Term g_ic_lhs[FC_MAX_RULES];
static Term g_ic_rhs[FC_MAX_RULES];

// subject corpus: both IC terms and their flat encodings.
static Term      g_ic_corpus[FC_MAX_CORPUS];
static FlatTerm  g_fc_corpus[FC_MAX_CORPUS];
static FlatNode  g_fc_corpus_buf[FC_MAX_CORPUS][512];
static u32       g_n_corpus = 0u;

static void encode_into(Term t, FlatNode *buf, FlatTerm *ft, const FcKbo *kb) {
  ft->node = buf; ft->cap = 512u; ft->len = 0u;
  fc_encode(t, ft, kb);
}

int main(int argc, char **argv) {
  thvm_init();

  u32 warmup = (argc > 1) ? (u32)strtoul(argv[1], NULL, 10) : 1200u;
  u64 n_ops  = (argc > 2) ? strtoull(argv[2], NULL, 10)     : 2000000ull;

  // Sheffer KBO config (one binary nand=label 1; identity precedence;
  // var_weight 1) -- identical to tests/test_atp_wolfram_bench.c.
  static u32 weights[2]    = { 0u, 1u };
  static u32 precedence[2] = { 0u, 1u };
  KboConfig cfg = { .weights = weights, .precedence = precedence,
                    .n_labels = 2u, .var_weight = 1u };
  FcKbo kb = { .weights = weights, .precedence = precedence,
               .n_labels = 2u, .var_weight = 1 };

  // Drive a REAL completion with the corrected Waldmeister preset
  // (SelectionRatio 51, RHS interreduce, unfailing CP) -- the trajectory
  // the AndAssociativity proof follows -- to harvest its derived rules.
  AtpState *s = thvm_atp_init(&cfg, 1u << 20);
  thvm_atp_set_selection_ratio(s, 51u);
  thvm_atp_set_use_rhs_interreduce(s, 1u);
  thvm_atp_set_use_unfailing_cp(s, 1u);
  thvm_atp_add_equation(s, axiom_lhs(), fv(2));

  printf("=== bench_flatcore: harvesting real Sheffer rule set ===\n");
  for (u32 i = 0; i < warmup; i++) {
    AtpStatus st = thvm_atp_step(s);
    if (st != ATP_RUNNING) break;
  }
  printf("harvested after %u steps: %u rules, ", warmup, s->n_rules);

  // Snapshot the ORIENTED rules (the inner-loop rewrites against R). Use
  // only oriented rules so both paths share an unambiguous, terminating
  // rewrite relation (the spike's inner-loop unit).
  for (u32 r = 0; r < s->n_rules && g_n_frules < FC_MAX_RULES; r++) {
    if (!s->r_orient[r]) continue;
    g_ic_lhs[g_n_frules] = s->lhs[r];
    g_ic_rhs[g_n_frules] = s->rhs[r];
    FlatTerm fl = { g_rule_lhs[g_n_frules], FC_RULE_BUF, 0u };
    FlatTerm fr = { g_rule_rhs[g_n_frules], FC_RULE_BUF, 0u };
    fc_encode(s->lhs[r], &fl, &kb);
    fc_encode(s->rhs[r], &fr, &kb);
    g_frules[g_n_frules].lhs  = g_rule_lhs[g_n_frules];
    g_frules[g_n_frules].llen = fl.len;
    g_frules[g_n_frules].rhs  = g_rule_rhs[g_n_frules];
    g_frules[g_n_frules].rlen = fr.len;
    g_n_frules++;
  }
  printf("%u oriented rules kept.\n", g_n_frules);

  // Build the subject corpus from FRESH critical pairs of the harvested
  // rule set: thvm_critical_pairs emits the raw (un-normalized)
  // superposition products -- exactly the terms the completion inner loop
  // normalizes. This is the authentic reducible workload (the queued CPs
  // were already normalized on push, so they under-represent rewriting).
  // We mix in the queued CPs + rule sides too for a representative spread.
  {
    static CriticalPair cps[FC_MAX_CORPUS];
    u32 ncp = thvm_critical_pairs(g_ic_lhs, g_ic_rhs, g_n_frules,
                                  cps, FC_MAX_CORPUS / 2u);
    for (u32 c = 0; c < ncp && g_n_corpus < FC_MAX_CORPUS; c++) {
      g_ic_corpus[g_n_corpus++] = cps[c].lhs;
      if (g_n_corpus < FC_MAX_CORPUS) g_ic_corpus[g_n_corpus++] = cps[c].rhs;
    }
  }
  for (u32 q = 0; q < s->n_cps && g_n_corpus < FC_MAX_CORPUS; q++) {
    if (s->cp_packed[q] == NULL) continue;
    Term cl = 0, cr = 0;
    acp_unpack(s->cp_packed[q], &cl, &cr);
    g_ic_corpus[g_n_corpus++] = cl;
    if (g_n_corpus < FC_MAX_CORPUS) g_ic_corpus[g_n_corpus++] = cr;
  }
  // encode the corpus once.
  u32 kept = 0u;
  for (u32 i = 0; i < g_n_corpus; i++) {
    encode_into(g_ic_corpus[i], g_fc_corpus_buf[kept], &g_fc_corpus[kept], &kb);
    if (g_fc_corpus[kept].len == 0u || g_fc_corpus[kept].len > 500u) continue;
    g_ic_corpus[kept] = g_ic_corpus[i];
    kept++;
  }
  g_n_corpus = kept;
  printf("corpus: %u real subject terms.\n", g_n_corpus);
  if (g_n_corpus == 0u || g_n_frules == 0u) {
    printf("FATAL: empty corpus/ruleset -- raise warmup steps.\n");
    return 1;
  }

  const u32 STEP_CAP = 256u;

  // ---- correctness gate: both paths IDENTICAL on every corpus term ----
  // The inner-loop op = normalize(subject) then KBO(subject, normalform).
  FlatNode arena_a[FC_ARENA], arena_b[FC_ARENA];
  u32 mism = 0u;
  for (u32 i = 0; i < g_n_corpus; i++) {
    // IC path.
    Term ic_nf = thvm_rewrite_normalize(g_ic_corpus[i], g_ic_lhs, g_ic_rhs,
                                        g_n_frules, STEP_CAP);
    KboCmp ic_v = thvm_kbo(g_ic_corpus[i], ic_nf, &cfg);
    // flat path.
    u32 alen = g_fc_corpus[i].len;
    for (u32 k = 0; k < alen; k++) arena_a[k] = g_fc_corpus[i].node[k];
    FlatNode *nfp = NULL;
    u32 nflen = fc_normalize(arena_a, alen, g_frules, g_n_frules,
                             arena_b, STEP_CAP, &kb, &nfp);
    FlatTerm subj = { g_fc_corpus[i].node, 512u, g_fc_corpus[i].len };
    FlatTerm nf   = { nfp, FC_ARENA, nflen };
    KboCmp fc_v = fc_kbo_compare(&subj, &nf, &kb);
    // compare normal forms structurally + verdicts.
    FlatNode ic_nf_flat[FC_ARENA]; FlatTerm icf = { ic_nf_flat, FC_ARENA, 0u };
    fc_encode(ic_nf, &icf, &kb);
    u8 nf_eq = (icf.len == nflen) && fc_slice_eq(icf.node, 0u, nfp, 0u, nflen);
    if (!nf_eq || ic_v != fc_v) {
      if (mism < 8u) {
        char b1[1024]; atp_pretty_term(g_ic_corpus[i], b1, sizeof b1);
        printf("MISMATCH #%u nf_eq=%u ic_v=%d fc_v=%d iclen=%u fclen=%u\n  subj=%s\n",
               i, nf_eq, (int)ic_v, (int)fc_v, icf.len, nflen, b1);
      }
      mism++;
    }
  }
  if (mism) {
    printf("IDENTICAL-RESULTS ASSERTION FAILED: %u / %u mismatches.\n",
           mism, g_n_corpus);
    return 1;
  }
  printf("IDENTICAL-RESULTS: %u / %u corpus terms agree (nf + KBO verdict).\n",
         g_n_corpus, g_n_corpus);

  // ---- workload characterization (diagnostic) ------------------------
  {
    u64 total_redex = 0, total_subj_nodes = 0;
    for (u32 i = 0; i < g_n_corpus; i++) {
      u32 alen = g_fc_corpus[i].len;
      total_subj_nodes += alen;
      for (u32 k = 0; k < alen; k++) arena_a[k] = g_fc_corpus[i].node[k];
      u32 cur = alen, nlen; FlatNode *c = arena_a, *n = arena_b;
      while (fc_step(c, cur, g_frules, g_n_frules, n, &nlen, &kb)) {
        FlatNode *tmp = c; c = n; n = tmp; cur = nlen; total_redex++;
        if (total_redex > 1000000ull) break;
      }
    }
    printf("workload: %llu redexes fired over corpus, %.1f avg subj nodes, %.3f redex/term.\n",
           (unsigned long long)total_redex,
           (double)total_subj_nodes / g_n_corpus,
           (double)total_redex / g_n_corpus);
  }

  // ---- component split (diagnostic): isolate match-scan vs KBO -------
  // The inner-loop op = normalize(subj) + KBO(subj, nf). Time the flat
  // normalize alone vs KBO alone to see which dominates -- this localizes
  // whether the representation or the O(n_rules) match scan is the wall.
  {
    u64 probe_ops = n_ops / 4u; if (probe_ops == 0) probe_ops = 1u;
    clock_t p0 = clock();
    volatile u64 sk = 0;
    for (u64 op = 0; op < probe_ops; op++) {
      u32 i = (u32)(op % g_n_corpus);
      u32 alen = g_fc_corpus[i].len;
      for (u32 k = 0; k < alen; k++) arena_a[k] = g_fc_corpus[i].node[k];
      FlatNode *nfp = NULL;
      u32 nflen = fc_normalize(arena_a, alen, g_frules, g_n_frules,
                               arena_b, 256u, &kb, &nfp);
      sk += nflen;
    }
    double norm_secs = (double)(clock() - p0) / CLOCKS_PER_SEC;
    p0 = clock();
    for (u64 op = 0; op < probe_ops; op++) {
      u32 i = (u32)(op % g_n_corpus);
      FlatTerm a = { g_fc_corpus[i].node, 512u, g_fc_corpus[i].len };
      sk += (u64)fc_kbo_compare(&a, &a, &kb);
    }
    double kbo_secs = (double)(clock() - p0) / CLOCKS_PER_SEC;
    printf("flat split (%llu ops): normalize %.3fs  KBO %.3fs  (sink=%llu)\n",
           (unsigned long long)probe_ops, norm_secs, kbo_secs,
           (unsigned long long)sk);
  }

  // ---- timed A/B: n_ops inner-loop ops each way -----------------------
  volatile u64 ic_sink = 0, fc_sink = 0;

  clock_t t0 = clock();
  for (u64 op = 0; op < n_ops; op++) {
    u32 i = (u32)(op % g_n_corpus);
    Term nf = thvm_rewrite_normalize(g_ic_corpus[i], g_ic_lhs, g_ic_rhs,
                                     g_n_frules, STEP_CAP);
    KboCmp v = thvm_kbo(g_ic_corpus[i], nf, &cfg);
    ic_sink += (u64)v + term_ctr_n(nf);
  }
  double ic_secs = (double)(clock() - t0) / CLOCKS_PER_SEC;

  t0 = clock();
  for (u64 op = 0; op < n_ops; op++) {
    u32 i = (u32)(op % g_n_corpus);
    u32 alen = g_fc_corpus[i].len;
    for (u32 k = 0; k < alen; k++) arena_a[k] = g_fc_corpus[i].node[k];
    FlatNode *nfp = NULL;
    u32 nflen = fc_normalize(arena_a, alen, g_frules, g_n_frules,
                             arena_b, STEP_CAP, &kb, &nfp);
    FlatTerm subj = { g_fc_corpus[i].node, 512u, g_fc_corpus[i].len };
    FlatTerm nf   = { nfp, FC_ARENA, nflen };
    KboCmp v = fc_kbo_compare(&subj, &nf, &kb);
    fc_sink += (u64)v + nflen;
  }
  double fc_secs = (double)(clock() - t0) / CLOCKS_PER_SEC;

  printf("\n=== A/B over %llu inner-loop ops (normalize+KBO-orient) ===\n",
         (unsigned long long)n_ops);
  printf("IC   path: %.3fs  (%.1f ops/s)  sink=%llu\n",
         ic_secs, (double)n_ops / ic_secs, (unsigned long long)ic_sink);
  printf("flat path: %.3fs  (%.1f ops/s)  sink=%llu\n",
         fc_secs, (double)n_ops / fc_secs, (unsigned long long)fc_sink);
  printf("RATIO (IC / flat): %.2fx\n", ic_secs / fc_secs);

  // extrapolation: thvm's measured ~2,800 CPs/s on this workload; the
  // proof needs ~260k CPs/s (Waldmeister, 1601 rules / 14.3s). The inner
  // loop is the broadly-distributed constant factor, so a kx inner-loop
  // speedup maps ~linearly to CPs/s.
  double base_cps = 2800.0;
  double proj_cps = base_cps * (ic_secs / fc_secs);
  printf("\nextrapolation (base ~%.0f CPs/s * ratio):\n", base_cps);
  printf("  projected ~%.0f CPs/s ; target ~260000 CPs/s ; need ~%.0fx\n",
         proj_cps, 260000.0 / base_cps);
  printf("  -> %s the ~90x gap.\n",
         (ic_secs / fc_secs) >= 90.0 ? "CLOSES" : "does NOT close");

  thvm_atp_free(s);
  return 0;
}
