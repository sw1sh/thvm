// tools/bench_wmfpa.c -- A/B microbench of the faithful Waldmeister-FPA
// normalization substrate (src/wmfpa/wmfpa.h: flatterm rep + DSBaum
// discrimination tree + NormalformInnermost retrieval) vs thvm's IC
// normalize, on the REAL harvested AndAssociativity / Sheffer oriented
// rule set + critical-pair subject corpus.
//
// Unlike tools/bench_flatcore.c (which used a LINEAR O(n_rules) scan and
// measured ~1.10x), WM-FPA retrieves redexes by descending a perfect
// discrimination tree built from the rule LHS in lockstep with the
// subject flatterm (MatchOperationen.c:565 MO_RegelGefunden) -- O(term
// depth) per query, independent of |R|.  This is the actual lever.
//
// Asserts WM-FPA normal form == thvm IC normal form on every subject
// (differential, zero mismatches required) before any timing is shown.
// Reports the IC/WM-FPA wall-time ratio at several rule-set sizes
// (100/300/600) so we see how it scales with |R|.
//
// Build: make bin/bench_wmfpa
// Run:   bin/bench_wmfpa [warmup_steps] [n_ops]

#include "../src/thvm.c"
#include "../src/wmfpa/wmfpa.h"
#include <time.h>

#define L_NAND 1u

static Term nand2(Term x, Term y) { Term c[2] = { x, y }; return term_new_ctr(L_NAND, c, 2); }
static Term fv(u32 id) { return term_new_fvr(id); }
static Term axiom_lhs(void) {
  Term a = fv(0), b = fv(1), c = fv(2);
  return nand2(nand2(nand2(a, b), c), nand2(a, nand2(nand2(a, c), a)));
}

// ---- WfEnc adapter: IC Term handle -> wmfpa flatterm ----------------
static int   wf_is_var(WfTermH t, u32 *id) {
  if (term_tag((Term)t) == TAG_FVR) { *id = term_ext((Term)t); return 1; }
  return 0;
}
static u32   wf_ctr_label(WfTermH t) { return term_ext((Term)t); }
static u32   wf_ctr_arity(WfTermH t) { return term_ctr_n((Term)t); }
static WfTermH wf_ctr_child(WfTermH t, u32 i) { return (WfTermH)term_ctr_at((Term)t, i); }
static const WfEnc g_enc = { wf_is_var, wf_ctr_label, wf_ctr_arity, wf_ctr_child };

#define WF_BENCH_MAXNODES 8192u
#define MAX_RULES   4096u
#define MAX_CORPUS  4096u
#define RULE_BUF    1024u
#define ARENA       (WF_BENCH_MAXNODES + 64u)

static WfNode g_rule_lhs[MAX_RULES][RULE_BUF];
static WfNode g_rule_rhs[MAX_RULES][RULE_BUF];
static WfRule g_rules[MAX_RULES];
static u32    g_n_rules = 0u;

static Term   g_ic_lhs[MAX_RULES];
static Term   g_ic_rhs[MAX_RULES];

static Term   g_ic_corpus[MAX_CORPUS];
static WfNode g_corpus_buf[MAX_CORPUS][512];
static u32    g_corpus_len[MAX_CORPUS];
static u32    g_n_corpus = 0u;

// run the A/B at a given rule-set size (first `nr` oriented rules).
static void run_ab(u32 nr, u64 n_ops) {
  if (nr > g_n_rules) nr = g_n_rules;
  WfTree tree; wf_tree_init(&tree);
  wf_tree_build(&tree, g_rules, nr);

  static WfNode arena_a[ARENA], arena_b[ARENA];
  u32 mism = 0u;
  const u32 STEP_CAP = 256u;

  // correctness gate at this size: WM-FPA NF == IC NF on every subject.
  for (u32 i = 0; i < g_n_corpus; i++) {
    Term ic_nf = thvm_rewrite_normalize(g_ic_corpus[i], g_ic_lhs, g_ic_rhs, nr, STEP_CAP);
    u32 alen = g_corpus_len[i];
    for (u32 k = 0; k < alen; k++) arena_a[k] = g_corpus_buf[i][k];
    WfNode *nfp = NULL;
    int ovf = 0;
    u32 nflen = wf_normalize(&tree, arena_a, alen, arena_b, ARENA, STEP_CAP, &nfp, &ovf);
    // encode IC NF for structural compare
    static WfNode icbuf[ARENA];
    u32 iclen = wf_encode((WfTermH)ic_nf, icbuf, ARENA, &g_enc);
    u8 eq = (iclen == nflen);
    for (u32 k = 0; eq && k < nflen; k++) if (icbuf[k].sym != nfp[k].sym) eq = 0;
    if (!eq) {
      if (mism < 8u) {
        char b1[1024]; atp_pretty_term(g_ic_corpus[i], b1, sizeof b1);
        printf("  MISMATCH nr=%u i=%u iclen=%u wflen=%u subj=%s\n",
               nr, i, iclen, nflen, b1);
      }
      mism++;
    }
  }
  if (mism) {
    printf("[nr=%u] NF-IDENTITY FAILED: %u/%u mismatches\n", nr, mism, g_n_corpus);
    wf_tree_free(&tree);
    return;
  }

  // timed A/B
  volatile u64 ic_sink = 0, wf_sink = 0;
  clock_t t0 = clock();
  for (u64 op = 0; op < n_ops; op++) {
    u32 i = (u32)(op % g_n_corpus);
    Term nf = thvm_rewrite_normalize(g_ic_corpus[i], g_ic_lhs, g_ic_rhs, nr, STEP_CAP);
    ic_sink += (u64)term_ctr_n(nf);
  }
  double ic_secs = (double)(clock() - t0) / CLOCKS_PER_SEC;

  t0 = clock();
  for (u64 op = 0; op < n_ops; op++) {
    u32 i = (u32)(op % g_n_corpus);
    u32 alen = g_corpus_len[i];
    for (u32 k = 0; k < alen; k++) arena_a[k] = g_corpus_buf[i][k];
    WfNode *nfp = NULL;
    int ovf = 0;
    u32 nflen = wf_normalize(&tree, arena_a, alen, arena_b, ARENA, STEP_CAP, &nfp, &ovf);
    wf_sink += nflen;
  }
  double wf_secs = (double)(clock() - t0) / CLOCKS_PER_SEC;

  printf("[nr=%-4u] IC %.3fs (%.0f/s)  WM-FPA %.3fs (%.0f/s)  RATIO %.2fx"
         "  | tree nodes=%u  cand/query=%.2f  nodevisits/query=%.2f  (NF-identity OK, sink=%llu/%llu)\n",
         nr, ic_secs, n_ops / ic_secs, wf_secs, n_ops / wf_secs, ic_secs / wf_secs,
         tree.n_nodes,
         tree.q_queries ? (double)tree.q_candidates / tree.q_queries : 0.0,
         tree.q_queries ? (double)tree.q_nodevisits / tree.q_queries : 0.0,
         (unsigned long long)ic_sink, (unsigned long long)wf_sink);
  wf_tree_free(&tree);
}

int main(int argc, char **argv) {
  thvm_init();
  u32 warmup = (argc > 1) ? (u32)strtoul(argv[1], NULL, 10) : 1600u;
  u64 n_ops  = (argc > 2) ? strtoull(argv[2], NULL, 10) : 400000ull;

  static u32 weights[2]    = { 0u, 1u };
  static u32 precedence[2] = { 0u, 1u };
  KboConfig cfg = { .weights = weights, .precedence = precedence, .n_labels = 2u, .var_weight = 1u };

  AtpState *s = thvm_atp_init(&cfg, 1u << 20);
  thvm_atp_set_selection_ratio(s, 51u);
  thvm_atp_set_use_rhs_interreduce(s, 1u);
  thvm_atp_set_use_unfailing_cp(s, 1u);
  thvm_atp_add_equation(s, axiom_lhs(), fv(2));

  printf("=== bench_wmfpa: harvesting real Sheffer rule set ===\n");
  for (u32 i = 0; i < warmup; i++) if (thvm_atp_step(s) != ATP_RUNNING) break;
  printf("harvested after %u steps: %u rules, ", warmup, s->n_rules);

  for (u32 r = 0; r < s->n_rules && g_n_rules < MAX_RULES; r++) {
    if (!s->r_orient[r]) continue;
    g_ic_lhs[g_n_rules] = s->lhs[r];
    g_ic_rhs[g_n_rules] = s->rhs[r];
    u32 ll = wf_encode((WfTermH)s->lhs[r], g_rule_lhs[g_n_rules], RULE_BUF, &g_enc);
    u32 rl = wf_encode((WfTermH)s->rhs[r], g_rule_rhs[g_n_rules], RULE_BUF, &g_enc);
    g_rules[g_n_rules].lhs = g_rule_lhs[g_n_rules]; g_rules[g_n_rules].llen = ll;
    g_rules[g_n_rules].rhs = g_rule_rhs[g_n_rules]; g_rules[g_n_rules].rlen = rl;
    g_n_rules++;
  }
  printf("%u oriented rules kept.\n", g_n_rules);

  // corpus: fresh critical pairs + queued CPs (the reducible workload).
  {
    static CriticalPair cps[MAX_CORPUS];
    u32 ncp = thvm_critical_pairs(g_ic_lhs, g_ic_rhs, g_n_rules, cps, MAX_CORPUS / 2u);
    for (u32 c = 0; c < ncp && g_n_corpus < MAX_CORPUS; c++) {
      g_ic_corpus[g_n_corpus++] = cps[c].lhs;
      if (g_n_corpus < MAX_CORPUS) g_ic_corpus[g_n_corpus++] = cps[c].rhs;
    }
  }
  for (u32 q = 0; q < s->n_cps && g_n_corpus < MAX_CORPUS; q++) {
    if (s->cp_packed[q] == NULL) continue;
    Term cl = 0, cr = 0; acp_unpack(s->cp_packed[q], &cl, &cr);
    g_ic_corpus[g_n_corpus++] = cl;
    if (g_n_corpus < MAX_CORPUS) g_ic_corpus[g_n_corpus++] = cr;
  }
  u32 kept = 0u;
  for (u32 i = 0; i < g_n_corpus; i++) {
    u32 len = wf_encode((WfTermH)g_ic_corpus[i], g_corpus_buf[kept], 512u, &g_enc);
    if (len == 0u || len > 500u) continue;
    g_ic_corpus[kept] = g_ic_corpus[i];
    g_corpus_len[kept] = len;
    kept++;
  }
  g_n_corpus = kept;
  printf("corpus: %u real subject terms.\n\n", g_n_corpus);
  if (g_n_corpus == 0u || g_n_rules == 0u) { printf("FATAL: empty corpus/ruleset\n"); return 1; }

  u32 sizes[] = { 100u, 300u, 600u, g_n_rules };
  for (u32 si = 0; si < sizeof(sizes)/sizeof(sizes[0]); si++) {
    if (sizes[si] == 0u) continue;
    if (si > 0 && sizes[si] == sizes[si-1]) continue;
    run_ab(sizes[si], n_ops);
  }

  thvm_atp_free(s);
  return 0;
}
