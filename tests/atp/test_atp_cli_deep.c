// test_atp_cli_deep.c - FindProof's proof-output walks on deep terms,
// pinned to the recursion they replaced.
//
// The --proof emission prints, hashes and rewrites the search's derived
// terms: the term printers and hashes of tools/atp_cli/szs.c, and the
// one-step rewrites of tools/proof_check.c that its bridge search and
// replay check run.  Each recursed once per term level (a rewrite level
// held a 4 KB result set); they keep an explicit stack now.  The
// recursive originals are kept here, verbatim, as references: on a few
// thousand random terms each walk must print the same text, fail at the
// same byte, hash the same and allocate the same heap words in the same
// order, and a term 100,000 levels deep must go through, on a 1 MB
// thread stack (tests/atp/deep_stack.h) so a regression to recursion fails
// whatever `ulimit -s` the machine has.  The test includes FindProof's
// translation unit without its entry point.

#define THVM_ATP_CLI_NO_MAIN 1
#include "../../tools/thvm_atp_cli.c"

#include "../test.h"
#include "deep_stack.h"

// ---- References: the recursive walks, as FindProof had them --------------

// tools/atp_cli/szs.c:146
static int ref_cli_term_has_flat_sym(const PrProblem *pr, Term t) {
  if (t == 0 || term_tag(t) != TAG_CTR) return 0;
  if (cli_is_flat_label(pr, term_ext(t))) return 1;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++)
    if (ref_cli_term_has_flat_sym(pr, term_ctr_at(t, i))) return 1;
  return 0;
}

// tools/atp_cli/szs.c:155
static int ref_cli_term_has_label(Term t, u32 l) {
  if (t == 0 || term_tag(t) != TAG_CTR) return 0;
  if (term_ext(t) == l) return 1;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++)
    if (ref_cli_term_has_label(term_ctr_at(t, i), l)) return 1;
  return 0;
}

// tools/atp_cli/szs.c:181
static void ref_cli_print_term(FILE *fp, const PrProblem *pr, Term t) {
  if (t == 0) { fputs("_", fp); return; }
  u32 tag = term_tag(t);
  if (tag == TAG_FVR) {
    u32 v = term_ext(t);
    if (pr != NULL && v < pr->n_vars) fputs(pr->vars[v], fp);
    else fprintf(fp, "%s%u", cli_var_prefix(pr), v);
    return;
  }
  if (tag == TAG_CTR) {
    u32 lab = term_ext(t);
    u32 n = term_ctr_n(t);
    if (pr != NULL && lab >= PR_LABEL_BASE && lab - PR_LABEL_BASE < pr->n_syms)
      fputs(pr->syms[lab - PR_LABEL_BASE].name, fp);
    else fprintf(fp, "f%u", lab);
    if (n > 0) {
      fputc('(', fp);
      for (u32 i = 0; i < n; i++) {
        if (i > 0) fputc(',', fp);
        ref_cli_print_term(fp, pr, term_ctr_at(t, i));
      }
      fputc(')', fp);
    }
    return;
  }
  fprintf(fp, "?t%u", tag);
}

// tools/atp_cli/szs.c:212
static void ref_cli_sprint_term(char *buf, size_t cap, size_t *o, int *fail,
                                const PrProblem *pr, Term t) {
  char nm[TP_NAME_LEN + 32];
  if (*fail) return;
  if (t == 0) { nm[0] = '_'; nm[1] = '\0'; }
  else if (term_tag(t) == TAG_FVR) {
    u32 v = term_ext(t);
    if (pr != NULL && v < pr->n_vars) snprintf(nm, sizeof nm, "%s", pr->vars[v]);
    else snprintf(nm, sizeof nm, "%s%u", cli_var_prefix(pr), v);
  } else if (term_tag(t) == TAG_CTR) {
    u32 lab = term_ext(t);
    if (pr != NULL && lab >= PR_LABEL_BASE && lab - PR_LABEL_BASE < pr->n_syms)
      snprintf(nm, sizeof nm, "%s", pr->syms[lab - PR_LABEL_BASE].name);
    else snprintf(nm, sizeof nm, "f%u", lab);
  } else {
    *fail = 1;
    return;
  }
  size_t l = strlen(nm);
  if (*o + l + 2 >= cap) { *fail = 1; return; }
  memcpy(buf + *o, nm, l);
  *o += l;
  if (t != 0 && term_tag(t) == TAG_CTR && term_ctr_n(t) > 0) {
    u32 n = term_ctr_n(t);
    buf[(*o)++] = '(';
    for (u32 i = 0; i < n && !*fail; i++) {
      if (i > 0) {
        if (*o + 1 >= cap) { *fail = 1; return; }
        buf[(*o)++] = ',';
      }
      ref_cli_sprint_term(buf, cap, o, fail, pr, term_ctr_at(t, i));
    }
    if (*o + 1 >= cap) { *fail = 1; return; }
    buf[(*o)++] = ')';
  }
  buf[*o] = '\0';
}

// tools/atp_cli/szs.c:325
static u64 ref_cli_ct_hash_walk(Term t, int *vmap, u32 *vn, u64 h) {
  u8 tg = term_tag(t);
  if (tg == TAG_FVR) {
    u32 id = term_ext(t);
    u32 c = id;
    if (id < 256u) {
      if (vmap[id] < 0) vmap[id] = (int)(*vn)++;
      c = (u32)vmap[id];
    }
    return (h ^ (0x9E3779B97F4A7C15ull + c)) * 0x100000001B3ull;
  }
  if (tg == TAG_CTR) {
    u32 n = term_ctr_n(t);
    h = (h ^ ((u64)term_ext(t) * 2654435761ull + n)) * 0x100000001B3ull;
    for (u32 i = 0; i < n; i++)
      h = ref_cli_ct_hash_walk(term_ctr_at(t, i), vmap, vn, h);
    return h;
  }
  return (h ^ (u64)t) * 0x100000001B3ull;
}

// tools/proof_check.c:1262
static void ref_pc_rewrite_all(Term t, Term l, Term r, PcRwSet *acc) {
  RewriteSubst sub = {{0}};
  if (thvm_match(l, t, &sub)) pc_rw_add(acc, thvm_subst_apply(r, &sub));
  if (term_tag(t) == TAG_CTR) {
    u32 n = term_ctr_n(t);
    if (n > 8u) return;
    Term kids[8];
    for (u32 i = 0; i < n; i++) kids[i] = term_ctr_at(t, i);
    for (u32 i = 0; i < n; i++) {
      PcRwSet inner;
      inner.n = 0;
      ref_pc_rewrite_all(kids[i], l, r, &inner);
      for (int k = 0; k < inner.n; k++) {
        Term save = kids[i];
        kids[i] = inner.out[k];
        pc_rw_add(acc, term_new_ctr(term_ext(t), kids, n));
        kids[i] = save;
      }
    }
  }
}

// ---- Terms ----------------------------------------------------------------

// Symbols e/0, inv/1, mult/2, op3/3 and the fresh definitional sF1/1 and
// sFlat2/0; labels 40 and 41 are outside the signature (printed f40,
// f41).  Variables X1, Y and Zed; ids from 3 print with the prefix.
enum {
  CD_E     = PR_LABEL_BASE + 0u,
  CD_INV   = PR_LABEL_BASE + 1u,
  CD_MULT  = PR_LABEL_BASE + 2u,
  CD_OP3   = PR_LABEL_BASE + 3u,
  CD_SF1   = PR_LABEL_BASE + 4u,
  CD_SFLAT = PR_LABEL_BASE + 5u,
  CD_C40   = 40u,
  CD_F41   = 41u
};

static PrProblem g_cd_pr;

static void cd_pr_init(void) {
  static const char *names[6] = { "e", "inv", "mult", "op3", "sF1", "sFlat2" };
  static const char *vars[3]  = { "X1", "Y", "Zed" };
  memset(&g_cd_pr, 0, sizeof g_cd_pr);
  for (u32 i = 0; i < 6u; i++) {
    snprintf(g_cd_pr.syms[i].name, PR_NAME_LEN, "%s", names[i]);
  }
  g_cd_pr.syms[1].arity = 1u;
  g_cd_pr.syms[2].arity = 2u;
  g_cd_pr.syms[3].arity = 3u;
  g_cd_pr.syms[4].arity = 1u;
  g_cd_pr.n_syms = 6u;
  for (u32 i = 0; i < 3u; i++) {
    snprintf(g_cd_pr.vars[i], PR_NAME_LEN, "%s", vars[i]);
  }
  g_cd_pr.n_vars         = 3u;
  g_cd_pr.fresh_sym_base = 4u;
  g_cd_pr.n_fresh_syms   = 2u;
}

static u32 cd_rng(u32 *seed) {
  u32 x = *seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *seed = x ? x : 0xdeadbeefu;
  return *seed;
}

// `g_cd_odd` admits null (0) arguments and NUM atoms, which the printers
// render as "_" and "?t<tag>" and the buffer printer refuses.
static u8 g_cd_odd = 1u;

static Term cd_leaf(u32 *seed) {
  u32 r = cd_rng(seed);
  u32 k = (r >> 4) % 32u;
  if (k == 0u && g_cd_odd) {
    return term_new(0, TAG_NUM, 0u, (r >> 9) & 0xffu);
  }
  if (k == 1u && g_cd_odd) {
    return 0;
  }
  if (k < 4u) {
    return term_new_fvr(300u + (r >> 9) % 3u);
  }
  if (k < 14u) {
    return term_new_fvr((r >> 9) % 6u);
  }
  if (k < 20u) {
    return term_new_ctr(CD_E, NULL, 0u);
  }
  if (k < 25u) {
    return term_new_ctr(CD_SFLAT, NULL, 0u);
  }
  return term_new_ctr(CD_C40, NULL, 0u);
}

static Term cd_term(u32 *seed, u32 depth, u32 *budget) {
  u32 r = cd_rng(seed);
  if (depth == 0u || *budget == 0u || (r & 7u) < 2u) {
    if (*budget > 0u) {
      (*budget)--;
    }
    return cd_leaf(seed);
  }
  (*budget)--;
  static const u32 labs[5] = { CD_INV, CD_MULT, CD_OP3, CD_SF1, CD_F41 };
  static const u32 ars[5]  = { 1u, 2u, 3u, 1u, 2u };
  u32  w = (r >> 4) % 5u;
  Term kids[3];
  for (u32 i = 0; i < ars[w]; i++) {
    kids[i] = cd_term(seed, depth - 1u, budget);
  }
  return term_new_ctr(labs[w], kids, ars[w]);
}

static Term cd_random(u32 *seed, u32 size) {
  u32 budget = size;
  return cd_term(seed, 12u, &budget);
}

// lab^depth(bottom).
static Term cd_chain(u32 depth, u32 lab, Term bottom) {
  Term t = bottom;
  for (u32 i = 0; i < depth; i++) {
    t = term_new_ctr(lab, &t, 1u);
  }
  return t;
}

// mult(x, mult(x, ... mult(x, bottom))), `depth` deep.
static Term cd_comb(u32 depth, Term x, Term bottom) {
  Term t = bottom;
  for (u32 i = 0; i < depth; i++) {
    Term kids[2] = { x, t };
    t = term_new_ctr(CD_MULT, kids, 2u);
  }
  return t;
}

// A random subterm of `t`.
static Term cd_pick(u32 *seed, Term t) {
  while (t != 0 && term_tag(t) == TAG_CTR && term_ctr_n(t) > 0u &&
         (cd_rng(seed) & 3u) != 0u) {
    t = term_ctr_at(t, cd_rng(seed) % term_ctr_n(t));
  }
  return t;
}

// `t` with some leaves turned into variables x0..x2 (shallow terms only).
static Term cd_generalize(u32 *seed, Term t) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) == 0u) {
    return (cd_rng(seed) & 1u) ? term_new_fvr(cd_rng(seed) % 3u) : t;
  }
  u32  n = term_ctr_n(t);
  Term kids[3];
  for (u32 i = 0; i < n; i++) {
    kids[i] = cd_generalize(seed, term_ctr_at(t, i));
  }
  return term_new_ctr(term_ext(t), kids, n);
}

// A CTR word pointing into [lo, hi) moved to the same offset from `to`.
static Term cd_reloc(Term w, u64 lo, u64 hi, u64 to) {
  if (term_tag(w) == TAG_CTR && term_val(w) >= lo && term_val(w) < hi) {
    return (w & ~VAL_MASK) | ((term_val(w) - lo + to) & VAL_MASK);
  }
  return w;
}

// Heap words [a0, a1) and [b0, b1) are the same allocation sequence.
static int cd_heap_same(u64 a0, u64 a1, u64 b0, u64 b1) {
  if (a1 - a0 != b1 - b0) {
    return 0;
  }
  for (u64 k = 0; k < a1 - a0; k++) {
    if (cd_reloc(heap_read(a0 + k), a0, a1, b0) != heap_read(b0 + k)) {
      return 0;
    }
  }
  return 1;
}

// The text `print` writes for `t`; malloc'd, *len its length.
static char *cd_capture(void (*print)(FILE *, const PrProblem *, Term),
                        const PrProblem *pr, Term t, size_t *len) {
  char  *buf = NULL;
  size_t n   = 0;
  FILE  *ms  = open_memstream(&buf, &n);
  if (ms == NULL) {
    *len = 0;
    return NULL;
  }
  print(ms, pr, t);
  fclose(ms);
  *len = n;
  return buf;
}

// Both buffer printers from offset `o0` into `cap` bytes prefilled alike:
// the same bytes, end offset and failure flag.
static int cd_sprint_same(const PrProblem *pr, Term t, size_t cap, size_t o0,
                          int fail0) {
  char  *a  = (char *)malloc(cap + 1u);
  char  *b  = (char *)malloc(cap + 1u);
  memset(a, 0x5a, cap + 1u);
  memset(b, 0x5a, cap + 1u);
  size_t oa = o0, ob = o0;
  int    fa = fail0, fb = fail0;
  ref_cli_sprint_term(a, cap, &oa, &fa, pr, t);
  cli_sprint_term(b, cap, &ob, &fb, pr, t);
  int same = (oa == ob && fa == fb && memcmp(a, b, cap + 1u) == 0);
  free(a);
  free(b);
  return same;
}

// ---- The printers, hashes and label tests of tools/atp_cli/szs.c --------

enum { CD_N = 3000 };

static void pin_szs(u32 *seed) {
  TEST_BEGIN("atp_cli_deep/pin-has-flat-sym-label");
  {
    u32 ok = 0, hits = 0;
    static const u32 labs[9] = { CD_E, CD_INV, CD_MULT, CD_OP3, CD_SF1,
                                 CD_SFLAT, CD_C40, CD_F41, 99u };
    for (u32 i = 0; i < CD_N; i++) {
      Term t = cd_random(seed, 40u);
      u32  l = labs[cd_rng(seed) % 9u];
      int  r = ref_cli_term_has_flat_sym(&g_cd_pr, t);
      if (r == cli_term_has_flat_sym(&g_cd_pr, t) &&
          ref_cli_term_has_flat_sym(NULL, t) == cli_term_has_flat_sym(NULL, t) &&
          ref_cli_term_has_label(t, l) == cli_term_has_label(t, l)) {
        ok++;
      }
      hits += (u32)r;
    }
    CHECK_EQ(ok, (u32)CD_N);
    CHECK(hits > 0u);
    CHECK(hits < (u32)CD_N);
  }

  TEST_BEGIN("atp_cli_deep/pin-print-term");
  {
    u32 ok = 0;
    for (u32 i = 0; i < CD_N; i++) {
      Term t = cd_random(seed, 40u);
      const PrProblem *pr = (i & 1u) ? &g_cd_pr : NULL;
      size_t la = 0, lb = 0;
      char  *a = cd_capture(ref_cli_print_term, pr, t, &la);
      char  *b = cd_capture(cli_print_term, pr, t, &lb);
      if (a != NULL && b != NULL && la == lb && memcmp(a, b, la) == 0) {
        ok++;
      }
      free(a);
      free(b);
    }
    CHECK_EQ(ok, (u32)CD_N);
  }

  TEST_BEGIN("atp_cli_deep/pin-sprint-term");
  {
    u32 ok = 0, fits = 0, fails = 0;
    for (u32 i = 0; i < CD_N; i++) {
      Term t = cd_random(seed, 40u);
      const PrProblem *pr = (i & 1u) ? &g_cd_pr : NULL;
      size_t len = 0;
      char  *txt = cd_capture(ref_cli_print_term, pr, t, &len);
      free(txt);
      // Room to spare, the exact fit and below, every length short of it,
      // a starting offset, and a call that starts failed.
      u32 same = 0u, runs = 0u;
      same += (u32)cd_sprint_same(pr, t, len + 64u, 0u, 0);
      runs++;
      for (size_t c = (len > 4u) ? len - 4u : 0u; c <= len + 3u; c++) {
        same += (u32)cd_sprint_same(pr, t, c, 0u, 0);
        runs++;
      }
      for (u32 k = 0; k < 6u; k++) {
        size_t cap = cd_rng(seed) % (len + 4u);
        size_t o0  = (cap > 0u) ? cd_rng(seed) % ((cap < 4u) ? cap : 4u) : 0u;
        same += (u32)cd_sprint_same(pr, t, cap, o0, 0);
        runs++;
      }
      same += (u32)cd_sprint_same(pr, t, len + 64u, 0u, 1);
      runs++;
      if (same == runs) {
        ok++;
      }
      char  *buf  = (char *)malloc(len + 64u);
      size_t o    = 0u;
      int    fail = 0;
      cli_sprint_term(buf, len + 64u, &o, &fail, pr, t);
      fits  += (u32)(fail == 0);
      fails += (u32)(fail != 0);
      free(buf);
    }
    CHECK_EQ(ok, (u32)CD_N);
    CHECK(fits > 0u);
    CHECK(fails > 0u);
  }

  TEST_BEGIN("atp_cli_deep/pin-ct-hash-walk");
  {
    u32 ok = 0;
    for (u32 i = 0; i < CD_N; i++) {
      Term t = cd_random(seed, 40u);
      Term u = cd_random(seed, 40u);
      int  va[256], vb[256];
      for (u32 v = 0; v < 256u; v++) {
        va[v] = -1;
        vb[v] = -1;
      }
      u32 na = 0u, nb = 0u;
      u64 h0 = ((u64)cd_rng(seed) << 32) | cd_rng(seed);
      u64 ha = ref_cli_ct_hash_walk(t, va, &na, h0);
      u64 hb = cli_ct_hash_walk(t, vb, &nb, h0);
      ha = ref_cli_ct_hash_walk(u, va, &na, ha);
      hb = cli_ct_hash_walk(u, vb, &nb, hb);
      if (ha == hb && na == nb && memcmp(va, vb, sizeof va) == 0) {
        ok++;
      }
    }
    CHECK_EQ(ok, (u32)CD_N);
  }
}

static void deep_szs(void) {
  enum { D = 100000 };
  Term e      = term_new_ctr(CD_E, NULL, 0u);
  Term sflat  = term_new_ctr(CD_SFLAT, NULL, 0u);
  Term x0     = term_new_fvr(0u);
  Term x7     = term_new_fvr(7u);
  Term inv_f  = cd_chain(D, CD_INV, sflat);
  Term inv_e  = cd_chain(D, CD_INV, e);
  Term comb0  = cd_comb(D, x0, e);
  Term comb7  = cd_comb(D, x7, e);
  // The texts: inv(...(sFlat2)...) and mult(X1,...mult(X1,e)...).
  size_t ilen = 4u * D + 6u + D;
  size_t clen = 8u * D + 1u + D;
  char  *iexp = (char *)malloc(ilen + 1u);
  char  *cexp = (char *)malloc(clen + 1u);
  for (u32 i = 0; i < D; i++) {
    memcpy(iexp + 4u * i, "inv(", 4u);
    memcpy(cexp + 8u * i, "mult(X1,", 8u);
    iexp[4u * D + 6u + i] = ')';
    cexp[8u * D + 1u + i] = ')';
  }
  memcpy(iexp + 4u * D, "sFlat2", 6u);
  cexp[8u * D] = 'e';
  iexp[ilen] = '\0';
  cexp[clen] = '\0';

  TEST_BEGIN("atp_cli_deep/deep-has-flat-sym-label");
  {
    CHECK(cli_term_has_flat_sym(&g_cd_pr, inv_f));
    CHECK(!cli_term_has_flat_sym(&g_cd_pr, inv_e));
    CHECK(!cli_term_has_flat_sym(&g_cd_pr, comb0));
    CHECK(cli_term_has_label(inv_f, CD_SFLAT));
    CHECK(!cli_term_has_label(inv_e, CD_SFLAT));
    CHECK(cli_term_has_label(comb0, CD_E));
    CHECK(!cli_term_has_label(comb0, CD_INV));
  }

  TEST_BEGIN("atp_cli_deep/deep-print-term");
  {
    size_t la = 0, lb = 0;
    char  *a = cd_capture(cli_print_term, &g_cd_pr, inv_f, &la);
    char  *b = cd_capture(cli_print_term, &g_cd_pr, comb0, &lb);
    CHECK(a != NULL && la == ilen && memcmp(a, iexp, ilen) == 0);
    CHECK(b != NULL && lb == clen && memcmp(b, cexp, clen) == 0);
    free(a);
    free(b);
  }

  TEST_BEGIN("atp_cli_deep/deep-sprint-term");
  {
    size_t cap  = clen + 2u;
    char  *buf  = (char *)malloc(cap);
    size_t o    = 0u;
    int    fail = 0;
    cli_sprint_term(buf, cap, &o, &fail, &g_cd_pr, comb0);
    CHECK_EQ(fail, 0);
    CHECK_EQ(o, clen);
    CHECK(memcmp(buf, cexp, clen + 1u) == 0);
    // Half the room: the names stop at the first that does not fit and
    // the open lists close with ')' while a byte remains.
    size_t half = 2u * D;
    o    = 0u;
    fail = 0;
    cli_sprint_term(buf, half, &o, &fail, &g_cd_pr, inv_f);
    CHECK_EQ(fail, 1);
    CHECK_EQ(o, half - 1u);
    CHECK_EQ(buf[o], '\0');
    size_t q = 0u;
    while (q < o && buf[q] != ')') {
      q++;
    }
    CHECK(q % 4u == 0u && q + 5u >= half && memcmp(buf, iexp, q) == 0);
    u8 closes = 1u;
    for (size_t k = q; k < o; k++) {
      closes &= (u8)(buf[k] == ')');
    }
    CHECK(closes);
    free(buf);
  }

  TEST_BEGIN("atp_cli_deep/deep-ct-hash-walk");
  {
    const u64 K = 2654435761ull, P = 0x100000001B3ull;
    u64 h0 = 0xCBF29CE484222325ull;
    u64 hi = h0, hc = h0;
    for (u32 i = 0; i < D; i++) {
      hi = (hi ^ ((u64)CD_INV * K + 1u)) * P;
      hc = (hc ^ ((u64)CD_MULT * K + 2u)) * P;
      hc = (hc ^ (0x9E3779B97F4A7C15ull + 0u)) * P;
    }
    hi = (hi ^ ((u64)CD_SFLAT * K)) * P;
    hc = (hc ^ ((u64)CD_E * K)) * P;
    int vmap[256];
    for (u32 v = 0; v < 256u; v++) {
      vmap[v] = -1;
    }
    u32 vn = 0u;
    CHECK_EQ(cli_ct_hash_walk(inv_f, vmap, &vn, h0), hi);
    CHECK_EQ(vn, 0u);
    CHECK_EQ(cli_ct_hash_walk(comb7, vmap, &vn, h0), hc);
    CHECK_EQ(vn, 1u);
    CHECK_EQ(vmap[7], 0);
    CHECK_EQ(cli_pair_canon_hash(comb0, inv_e), cli_pair_canon_hash(comb7, inv_e));
    CHECK(cli_pair_canon_hash(comb0, inv_e) != cli_pair_canon_hash(comb0, inv_f));
  }

  free(iexp);
  free(cexp);
}

// The 2,000-deep comparisons against the recursions, on the reference
// stack.
static void mid_szs(void) {
  Term sflat = term_new_ctr(CD_SFLAT, NULL, 0u);
  Term x0    = term_new_fvr(0u);

  TEST_BEGIN("atp_cli_deep/mid-depth-szs-vs-recursion");
  {
    // 2,000 levels, within the recursion's reach.
    Term t = cd_comb(2000u, x0, cd_chain(2000u, CD_SF1, sflat));
    size_t la = 0, lb = 0;
    char  *a = cd_capture(ref_cli_print_term, &g_cd_pr, t, &la);
    char  *b = cd_capture(cli_print_term, &g_cd_pr, t, &lb);
    CHECK(a != NULL && b != NULL && la == lb && memcmp(a, b, la) == 0);
    free(a);
    free(b);
    u32 same = 0u;
    for (size_t c = 0u; c < la + 8u; c += 997u) {
      same += (u32)cd_sprint_same(&g_cd_pr, t, c, 0u, 0);
    }
    same += (u32)cd_sprint_same(&g_cd_pr, t, la + 2u, 0u, 0);
    CHECK_EQ(same, (u32)((la + 8u + 996u) / 997u) + 1u);
    int va[256], vb[256];
    for (u32 v = 0; v < 256u; v++) {
      va[v] = -1;
      vb[v] = -1;
    }
    u32 na = 0u, nb = 0u;
    CHECK_EQ(ref_cli_ct_hash_walk(t, va, &na, 7u), cli_ct_hash_walk(t, vb, &nb, 7u));
    CHECK_EQ(ref_cli_term_has_flat_sym(&g_cd_pr, t), cli_term_has_flat_sym(&g_cd_pr, t));
  }
}

// ---- The one-step rewrites of tools/proof_check.c ------------------------

// Both rewriters on (t, l -> r) after `pre` results already in the set:
// the same results (moved to the second run's heap) and the same heap
// words in the same order.
static int cd_rewrite_same(Term t, Term l, Term r, int pre, u32 *nres) {
  static PcRwSet a, b;
  a.n = pre;
  b.n = pre;
  for (int k = 0; k < pre; k++) {
    a.out[k] = 0;
    b.out[k] = 0;
  }
  u64 h0 = HEAP_NEXT;
  ref_pc_rewrite_all(t, l, r, &a);
  u64 h1 = HEAP_NEXT;
  pc_rewrite_all(t, l, r, &b);
  u64 h2 = HEAP_NEXT;
  int same = (a.n == b.n && cd_heap_same(h0, h1, h1, h2));
  for (int k = 0; same && k < a.n; k++) {
    same = (cd_reloc(a.out[k], h0, h1, h1) == b.out[k]);
  }
  *nres = (u32)(b.n - pre);
  return same;
}

static void pin_pc_rewrite(u32 *seed) {
  TEST_BEGIN("atp_cli_deep/pin-pc-rewrite-all");
  {
    u32 ok = 0, some = 0, capped = 0;
    g_cd_odd = 0u;
    for (u32 i = 0; i < CD_N; i++) {
      // A few large terms under a variable rule: past the 512-result cap.
      u32  big = ((i % 16u) == 0u);
      Term t = cd_random(seed, big ? 900u : 40u);
      Term l, r;
      switch (cd_rng(seed) % 4u) {
        case 0:  l = term_new_fvr(cd_rng(seed) % 3u); break;
        case 1:  l = cd_pick(seed, t); break;
        case 2:  l = cd_generalize(seed, cd_pick(seed, t)); break;
        default: l = cd_random(seed, 6u); break;
      }
      if (big) {
        l = term_new_fvr(0u);
      }
      r = (cd_rng(seed) & 1u) ? cd_random(seed, 6u) : term_new_fvr(cd_rng(seed) % 3u);
      int pre = (int)((cd_rng(seed) & 3u) == 0u ? cd_rng(seed) % (PC_RW_MAX + 1u) : 0u);
      u32 nres = 0u;
      if (cd_rewrite_same(t, l, r, pre, &nres)) {
        ok++;
      }
      some   += (u32)(nres > 0u);
      capped += (u32)(pre + (int)nres == PC_RW_MAX);
    }
    g_cd_odd = 1u;
    CHECK_EQ(ok, (u32)CD_N);
    CHECK(some > 0u);
    CHECK(capped > 0u);
  }
}

static void deep_pc_rewrite(void) {
  enum { D = 100000 };
  Term e     = term_new_ctr(CD_E, NULL, 0u);
  Term sflat = term_new_ctr(CD_SFLAT, NULL, 0u);
  Term inv_e = cd_chain(D, CD_INV, e);

  TEST_BEGIN("atp_cli_deep/deep-pc-rewrite-all");
  {
    static PcRwSet acc;
    // One redex at the bottom: rebuilt through every level.
    acc.n = 0;
    pc_rewrite_all(inv_e, e, sflat, &acc);
    CHECK_EQ(acc.n, 1);
    Term t = acc.out[0];
    u32  d = 0u;
    while (term_tag(t) == TAG_CTR && term_ext(t) == CD_INV && term_ctr_n(t) == 1u) {
      t = term_ctr_at(t, 0);
      d++;
    }
    CHECK_EQ(d, (u32)D);
    CHECK(term_tag(t) == TAG_CTR && term_ext(t) == CD_SFLAT && term_ctr_n(t) == 0u);
    // No redex anywhere.
    Term kids[3] = { e, e, e };
    acc.n = 0;
    pc_rewrite_all(inv_e, term_new_ctr(CD_OP3, kids, 3u), e, &acc);
    CHECK_EQ(acc.n, 0);
    // The binary spine, its redex at the bottom.
    acc.n = 0;
    pc_rewrite_all(cd_comb(D, sflat, e), e, sflat, &acc);
    CHECK_EQ(acc.n, 1);
  }
}

// 600 levels against the recursion (about 4.8 KB of C stack a level),
// on the reference stack.
static void mid_pc_rewrite(void) {
  Term e     = term_new_ctr(CD_E, NULL, 0u);
  Term sflat = term_new_ctr(CD_SFLAT, NULL, 0u);

  TEST_BEGIN("atp_cli_deep/mid-depth-pc-rewrite-vs-recursion");
  {
    // 600 levels (the recursion's 4 KB per level), a variable rule that
    // fires everywhere: the sets of the levels with 512 positions or more
    // below them reach the cap.
    u32 nres = 0u;
    Term t = cd_chain(600u, CD_INV, e);
    CHECK(cd_rewrite_same(t, term_new_fvr(0u), sflat, 0, &nres));
    CHECK_EQ(nres, (u32)PC_RW_MAX);
    CHECK(cd_rewrite_same(cd_comb(600u, sflat, e), e, sflat, 100, &nres));
    CHECK_EQ(nres, 1u);
  }
}

int main(void) {
  thvm_init();
  cd_pr_init();
  u32 seed = 0x3c1a9e57u;
  TEST_BEGIN("atp_cli_deep/negative-control");
  CHECK(deep_stack_catches_recursion());
  pin_szs(&seed);
  deep_stack_call(deep_szs);
  ref_stack_call(mid_szs);
  pin_pc_rewrite(&seed);
  deep_stack_call(deep_pc_rewrite);
  ref_stack_call(mid_pc_rewrite);
  thvm_free();
  TEST_REPORT();
}
