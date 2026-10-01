// ===================================================================
// SZS + proof output
// ===================================================================

// Competition-clean output: in --szs/--proof mode the bench TU's
// diagnostic chatter is rerouted to stderr and the SZS lines go to the
// preserved real stdout via this stream.
static FILE *g_cli_out = NULL;
static int  g_cli_tptp_mode = 0;
static int  g_cli_want_proof = 0;
static int  g_cli_want_szs = 0;
static int  g_cli_szs_printed = 0;
static const char *g_cli_problem = NULL;
// Wall deadline (atp_now_us) for the proof emission's expensive passes
// (re-derivation, deep citation scans, bridge searches); 0 = none.
static u64 g_cli_emit_deadline_us = 0u;
static int cli_emit_late(void) {
  return g_cli_emit_deadline_us != 0u && atp_now_us() >= g_cli_emit_deadline_us;
}
// A whole-trace scan (bridge successors, parent rescue, deep citation
// resolve) for ONE record or step stops after this long, or at the
// emission deadline -- a record none of them can reach otherwise costs
// seconds per scan on a deep trace.
#define CLI_SCAN_US 1000000ull
static int cli_scan_over(u64 t0, u32 i) {
  if ((i & 1023u) != 0u) return 0;
  return cli_emit_late() || atp_now_us() - t0 >= CLI_SCAN_US;
}

// Fail-closed emission: a proof block is printed only when every step in
// it is derived from what it cites (see cli_emit_trace and
// cli_emit_goal_chain) and some goal reaches $false; otherwise the
// status stands alone and one stderr line names the first step that
// is not.  A missing proof costs a solution; an underived step would
// read as an unsound prover.
static u32  g_cli_n_unjust = 0u;
static u32  g_cli_n_refuted = 0u;
static char g_cli_unjust_what[96];

static void cli_unjustified(const char *name, const char *why) {
  if (g_cli_n_unjust++ == 0u)
    snprintf(g_cli_unjust_what, sizeof g_cli_unjust_what, "%s %s", name, why);
}

// 1 when no completeness-breaking pruning is active AND the goal is
// ground, so a saturation (QUEUE_EMPTY) soundly reports Satisfiable;
// else GaveUp.  An existential goal (variables in the negative unit)
// is decided by BUDGETED narrowing, so its saturation witnesses
// nothing.
static int cli_strategy_complete(void) {
  const char *mw = getenv("THVM_ATP_MAXW");
  // Twee --max-term-size: a per-side hard CP drop, lossy exactly like the
  // -mw sum cap, so a saturation under it certifies nothing (GaveUp, never
  // Satisfiable) -- the dropped CPs could have refuted.
  const char *mts = getenv("THVM_ATP_MAX_TERM_SIZE");
  const char *am = getenv("THVM_ATP_AUTO_MAXW");
  const char *rr = getenv("THVM_ATP_RANDOM_RATIO");
  // Encompassment demodulation drops CPs joined via skolem-ground steps
  // without Twee's simplerThan redundancy discipline, so a saturation
  // under it certifies nothing about the full problem: GaveUp, never
  // Satisfiable.
  const char *en = getenv("THVM_ATP_ENCOMPASS");
  // Simpler-join carries the simplerThan discipline but not the
  // below-peak connectedness side condition of Twee's step3, so its
  // CP drops are not exactly completeness-preserving either: GaveUp,
  // never Satisfiable.
  const char *sj = getenv("THVM_ATP_SIMPLER_JOIN");
  // Pop-time ground-joinability case-split: the discard is a redundancy
  // claim (ground-joinable CPs are redundant), completeness-preserving
  // in theory -- but the implementation is young, and an over-discard
  // would make a saturation certify nothing.  Conservative: GaveUp,
  // never Satisfiable, until the mechanism has corpus history.
  const char *pgj = getenv("THVM_ATP_POP_GJ");
  // Without unfailing CP formation (use_unfailing_cp, armed by the WM
  // preset; THVM_ATP_UNFAILING overrides either way) an unorientable
  // equation contributes NO ordered overlaps, so a QUEUE_EMPTY that
  // leaves one in the final rule store certifies nothing -- the missing
  // overlaps could refute.  COL002-5's S-combinator axiom is inherently
  // KBO-unorientable (the unfolding direction duplicates a variable,
  // the folding direction loses the weight-tie lexicographic compare)
  // and the bare tgt slice "saturated" in 8 steps / 0 CPs on the
  // unsatisfiable problem, claiming Satisfiable.
  const char *wmp = getenv("THVM_ATP_WALDMEISTER");
  const char *uf  = getenv("THVM_ATP_UNFAILING");
  u8 unfailing = (uf != NULL && uf[0] != '\0')
                   ? (uf[0] != '0')
                   : (wmp != NULL && wmp[0] != '\0' && wmp[0] != '0');
  if (!unfailing && g_atp_bench_unorient > 0) return 0;
  return !g_cli_goal_existential && !g_cli_sine_applied && !g_cli_horn_applied
      && !g_cli_equals_applied && !g_cli_fof_applied
      && (mw == NULL || *mw == '0') && (am == NULL || *am == '0')
      && (mts == NULL || *mts == '0')
      && (rr == NULL || *rr == '0') && (en == NULL || *en == '0')
      && (sj == NULL || *sj == '0') && (pgj == NULL || *pgj == '0');
}

// --- term printing (names from the parsed .pr) ---------------------

// Collision-free name prefix for variable ids past the .pr's declared
// set.  Derived equations regularly hold MORE distinct variables than
// the input declares, and the old fallback ("X%u") collided with the
// declared X1..Xn names: id n printed "Xn" -- the DECLARED name of id
// n-1 -- so two distinct variables emitted as one, the record claimed
// a strictly stronger equation, and every later step citing it failed
// syntactic replay (GRP706-1's 23-abstain class; the checker's deep
// sessions were re-deriving what the citation would have settled).
// Scan the declared names once per problem and pick the first prefix
// no declared "<prefix><digits>" name uses.
static const char *cli_var_prefix(const PrProblem *pr) {
  static const PrProblem *cached_pr = NULL;
  static const char *cached_prefix = "X";
  if (pr == cached_pr) return cached_prefix;
  static const char *cands[] = { "X", "Y", "Z", "U", "V", "W", "XV" };
  cached_pr = pr;
  cached_prefix = "XV";
  for (u32 c = 0; c < sizeof cands / sizeof cands[0]; c++) {
    u8 clash = 0u;
    size_t plen = strlen(cands[c]);
    for (u32 v = 0; pr != NULL && v < pr->n_vars && !clash; v++) {
      const char *nm = pr->vars[v];
      if (strncmp(nm, cands[c], plen) != 0) continue;
      const char *d = nm + plen;
      if (*d == '\0') continue;
      while (*d >= '0' && *d <= '9') d++;
      if (*d == '\0') clash = 1u;        // declared "<prefix><digits>"
    }
    if (!clash) { cached_prefix = cands[c]; break; }
  }
  return cached_prefix;
}

// Does the term mention a definitional fresh symbol (pr_flatten_goal's
// sFlat* constants or pr_tgt_transform's sF* functions)?  Such an
// axiom leaf is an introduced definition, not a problem clause, and
// must be sourced as introduced(definition) so a derivation verifier
// (GDV) does not try to match it against the problem's formulae.
// Decided by the recorded fresh-label RANGE, immune to user symbols
// that happen to spell sF*/sFlat*.
static int cli_is_flat_label(const PrProblem *pr, u32 l) {
  if (pr == NULL || pr->n_fresh_syms == 0u || l < PR_LABEL_BASE) return 0;
  u32 si = l - PR_LABEL_BASE;
  return si >= pr->fresh_sym_base &&
         si < pr->fresh_sym_base + pr->n_fresh_syms;
}

static int cli_term_has_flat_sym(const PrProblem *pr, Term t) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  int   hit = 0;
  for (;;) {
    if (t != 0 && term_tag(t) == TAG_CTR) {
      if (cli_is_flat_label(pr, term_ext(t))) {
        hit = 1;
        break;
      }
      u32 n = term_ctr_n(t);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) {
        st[nst++] = term_ctr_at(t, i);
      }
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return hit;
}

static int cli_term_has_label(Term t, u32 l) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  int   hit = 0;
  for (;;) {
    if (t != 0 && term_tag(t) == TAG_CTR) {
      if (term_ext(t) == l) {
        hit = 1;
        break;
      }
      u32 n = term_ctr_n(t);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) {
        st[nst++] = term_ctr_at(t, i);
      }
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return hit;
}

// The fresh symbol a definitional axiom introduces: `d` is a fresh
// label applied to pairwise-distinct variables and `body` is free of
// that label.  Returns the label, 0 when `d` has no definition shape.
static u32 cli_def_label(const PrProblem *pr, Term d, Term body) {
  if (d == 0 || term_tag(d) != TAG_CTR || !cli_is_flat_label(pr, term_ext(d)))
    return 0u;
  u32 n = term_ctr_n(d);
  for (u32 i = 0; i < n; i++) {
    Term a = term_ctr_at(d, i);
    if (term_tag(a) != TAG_FVR) return 0u;
    for (u32 j = 0; j < i; j++)
      if (term_ext(term_ctr_at(d, j)) == term_ext(a)) return 0u;
  }
  if (cli_term_has_label(body, term_ext(d))) return 0u;
  return term_ext(d);
}

static void cli_print_term(FILE *fp, const PrProblem *pr, Term t) {
  // One frame per open argument list: the CTR and the argument `i` being
  // printed.  A proof's terms can be as deep as the search made them.
  typedef struct { Term t; u32 i; u32 n; } CliPrintFrame;
  CliPrintFrame  fr_inl[ATP_WALK_INL];
  CliPrintFrame *fr  = fr_inl;
  u32            nfr = 0u;
  u32            cfr = ATP_WALK_INL;
  for (;;) {
    u32 tag = (t == 0) ? 0u : term_tag(t);
    if (t == 0) {
      fputs("_", fp);
    } else if (tag == TAG_FVR) {
      u32 v = term_ext(t);
      if (pr != NULL && v < pr->n_vars) {
        fputs(pr->vars[v], fp);
      } else {
        fprintf(fp, "%s%u", cli_var_prefix(pr), v);
      }
    } else if (tag == TAG_CTR) {
      u32 lab = term_ext(t);
      u32 n = term_ctr_n(t);
      if (pr != NULL && lab >= PR_LABEL_BASE && lab - PR_LABEL_BASE < pr->n_syms) {
        fputs(pr->syms[lab - PR_LABEL_BASE].name, fp);
      } else {
        fprintf(fp, "f%u", lab);
      }
      if (n > 0) {
        fputc('(', fp);
        if (nfr == cfr) {
          fr = (CliPrintFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].t = t;
        fr[nfr].i = 0u;
        fr[nfr].n = n;
        nfr++;
        t = term_ctr_at(t, 0);
        continue;
      }
    } else {
      fprintf(fp, "?t%u", tag);
    }
    u8 next = 0u;
    while (nfr > 0u) {
      CliPrintFrame *f = &fr[nfr - 1u];
      f->i++;
      if (f->i < f->n) {
        fputc(',', fp);
        t    = term_ctr_at(f->t, f->i);
        next = 1u;
        break;
      }
      fputc(')', fp);
      nfr--;
    }
    if (!next) {
      break;
    }
  }
  term_stack_release(fr, fr_inl);
}

// Buffer variant of cli_print_term: the provenance leaf-match key.
// Overflow sets *fail (the key then matches nothing and the leaf
// falls back to the legacy file() citation).
static void cli_sprint_term(char *buf, size_t cap, size_t *o, int *fail,
                            const PrProblem *pr, Term t) {
  // One frame per open argument list (`i` the argument being printed),
  // with the recursion's failure behaviour: a node that does not fit sets
  // *fail and ends; a list closes with ')' after its last argument or
  // after a failure below it when the bracket still fits, and a separator
  // that does not fit ends the list without one.
  typedef struct { Term t; u32 i; u32 n; } CliSprintFrame;
  CliSprintFrame  fr_inl[ATP_WALK_INL];
  CliSprintFrame *fr  = fr_inl;
  u32             nfr = 0u;
  u32             cfr = ATP_WALK_INL;
  char nm[TP_NAME_LEN + 32];
  if (*fail) {
    return;
  }
  for (;;) {
    u8 ok = 1u;
    if (t == 0) {
      nm[0] = '_';
      nm[1] = '\0';
    } else if (term_tag(t) == TAG_FVR) {
      u32 v = term_ext(t);
      if (pr != NULL && v < pr->n_vars) {
        snprintf(nm, sizeof nm, "%s", pr->vars[v]);
      } else {
        snprintf(nm, sizeof nm, "%s%u", cli_var_prefix(pr), v);
      }
    } else if (term_tag(t) == TAG_CTR) {
      u32 lab = term_ext(t);
      if (pr != NULL && lab >= PR_LABEL_BASE && lab - PR_LABEL_BASE < pr->n_syms) {
        snprintf(nm, sizeof nm, "%s", pr->syms[lab - PR_LABEL_BASE].name);
      } else {
        snprintf(nm, sizeof nm, "f%u", lab);
      }
    } else {
      *fail = 1;
      ok = 0u;
    }
    if (ok) {
      size_t l = strlen(nm);
      if (*o + l + 2 >= cap) {
        *fail = 1;
        ok = 0u;
      } else {
        memcpy(buf + *o, nm, l);
        *o += l;
        if (t != 0 && term_tag(t) == TAG_CTR && term_ctr_n(t) > 0) {
          buf[(*o)++] = '(';
          if (nfr == cfr) {
            fr = (CliSprintFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
          }
          fr[nfr].t = t;
          fr[nfr].i = 0u;
          fr[nfr].n = term_ctr_n(t);
          nfr++;
          t = term_ctr_at(t, 0);
          continue;
        }
        buf[*o] = '\0';
      }
    }
    // The node is done: close the lists it ends.
    u8 next = 0u;
    while (nfr > 0u) {
      CliSprintFrame *f = &fr[nfr - 1u];
      f->i++;
      if (f->i < f->n && !*fail) {
        if (*o + 1 >= cap) {
          *fail = 1;
          nfr--;
          continue;
        }
        buf[(*o)++] = ',';
        t    = term_ctr_at(f->t, f->i);
        next = 1u;
        break;
      }
      nfr--;
      if (*o + 1 >= cap) {
        *fail = 1;
        continue;
      }
      buf[(*o)++] = ')';
      buf[*o] = '\0';
    }
    if (!next) {
      break;
    }
  }
  term_stack_release(fr, fr_inl);
}

// Provenance lookup for one (lhs, rhs) unit: prints the conversion
// chain to fp and fills src with the citation tail.  NULL when the
// unit carries no provenance (legacy file() path).
static const char *cli_prov_source(FILE *fp, const PrProblem *pr,
                                   Term lhs, Term rhs, int neg,
                                   char *src, size_t cap) {
  if (g_prov_nleaf == 0) return NULL;
  static char lb[4 * TP_SIDE_LEN], rb[4 * TP_SIDE_LEN];
  size_t lo = 0, ro = 0;
  int fail = 0;
  cli_sprint_term(lb, sizeof lb, &lo, &fail, pr, lhs);
  cli_sprint_term(rb, sizeof rb, &ro, &fail, pr, rhs);
  if (fail) return NULL;
  const ProvLeaf *pl = prov_leaf_find(lb, rb, neg);
  if (pl == NULL) return NULL;
  if (!prov_src_citation(fp, pl->kind, pl->ix, src, cap)) return NULL;
  return src;
}

// --- derivation closure over the completion trace ------------------

// Forward decl (bridge search uses the pair loader defined below).
static u8 cli_tp_cached(AtpState *s, u32 ti, Term *l, Term *r);

// Treatment-compression bridge (post-mortem Track C): an ORIENT trace
// record stores the equation AFTER normalization by the then-current
// rules but cites only its pre-treatment parent, so the emitted record
// claims a conclusion its one listed parent does not yield and the
// checker abstains (GRP660-10's t10852 class).  At emission, search
// for the ONE-step rewrite (any trace equation, either face, grounded
// r2 template included) that takes the parent pair to the recorded
// pair (modulo orientation + renaming) and emit it as an interposed
// rw record.  Honest by construction: the interposed equation is
// itself a trace record with its own emitted derivation, and the step
// is re-derived here.  A record the search cannot bridge is emitted
// as before (abstain, never a fabricated citation).
typedef struct { u32 eq; Term l, r; } CliBridgeStep;

static u8 cli_pair_eq_mod(Term a1, Term b1, Term a2, Term b2) {
  return pc_pair_alpha_eq(a1, b1, a2, b2);
}

// (l, r) is a substitution instance of (pl, pr), either orientation.
static u8 cli_pair_instance(Term pl, Term pr, Term l, Term r) {
  RewriteSubst sub = {{0}};
  if (thvm_match(pl, l, &sub) && thvm_match(pr, r, &sub)) return 1u;
  RewriteSubst sub2 = {{0}};
  return (u8)(thvm_match(pl, r, &sub2) && thvm_match(pr, l, &sub2));
}

// The bridge search enumerates one-rewrite successors of a pair over
// every earlier trace equation -- a fanout that exhausted from-space
// mid-emission when the candidate Terms were kept (the truncated-
// proof death).  So the scan keeps NO Terms: each candidate is
// reduced to {equation, side, candidate-index, canonical pair hash}
// under a per-equation heap checkpoint/rollback, and only the finally
// chosen picks are re-derived (deterministically, from eq/side/k).
typedef struct { u32 eq; u32 k; u64 h; u8 side; } CliBridgeMeta;

static void cli_bridge_cands(AtpState *s, Term st, Term u, Term v,
                             PcRwSet *acc) {
  acc->n = 0;
  pc_rewrite_all(st, u, v, acc);
  pc_rewrite_all(st, v, u, acc);
  Term tu = atp_unorient_template(s, u, v);
  if (tu != 0 && !kbo_eq(tu, v)) pc_rewrite_all(st, u, tu, acc);
  Term tv = atp_unorient_template(s, v, u);
  if (tv != 0 && !kbo_eq(tv, u)) pc_rewrite_all(st, v, tv, acc);
}

// Allocation-free canonical pair hash: structural walk with variables
// numbered by first occurrence across the pair, minimized over the
// two side orders (mirrors pc_pair_alpha_eq's equivalence; hash
// matches are RE-VERIFIED with the real compare, so a collision can
// only cost a retry, never a wrong bridge).
static u64 cli_ct_hash_walk(Term t, int *vmap, u32 *vn, u64 h) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  for (;;) {
    u8 tg = term_tag(t);
    if (tg == TAG_FVR) {
      u32 id = term_ext(t);
      u32 c = id;
      if (id < 256u) {
        if (vmap[id] < 0) {
          vmap[id] = (int)(*vn)++;
        }
        c = (u32)vmap[id];
      }
      h = (h ^ (0x9E3779B97F4A7C15ull + c)) * 0x100000001B3ull;
    } else if (tg == TAG_CTR) {
      u32 n = term_ctr_n(t);
      h = (h ^ ((u64)term_ext(t) * 2654435761ull + n)) * 0x100000001B3ull;
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) {
        st[nst++] = term_ctr_at(t, i);
      }
    } else {
      h = (h ^ (u64)t) * 0x100000001B3ull;
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return h;
}

static u64 cli_pair_canon_hash(Term l, Term r) {
  u64 best = ~0ull;
  for (int o = 0; o < 2; o++) {
    int vmap[256];
    for (int i = 0; i < 256; i++) vmap[i] = -1;
    u32 vn = 0;
    u64 h = 0xCBF29CE484222325ull;
    h = cli_ct_hash_walk(o ? r : l, vmap, &vn, h);
    h = (h ^ 0x9E37ull) * 0x100000001B3ull;
    h = cli_ct_hash_walk(o ? l : r, vmap, &vn, h);
    if (h < best) best = h;
  }
  return best;
}

#define CLI_BRIDGE_SUCC_CAP 16384u
#define CLI_BRIDGE_HEADROOM (1ull << 18)

// Hash-scan all one-rewrite successors of (pl, pr); Terms are rolled
// back per equation.  Bails (returns count so far) on low heap
// headroom or a GC (rollback then unsafe).
static u32 cli_bridge_scan(AtpState *s, Term pl, Term pr, u32 below,
                           CliBridgeMeta *out, u32 cap) {
  u32 n = 0u;
  u32 scan = (below < s->n_trace) ? below : s->n_trace;
  if (scan > (1u << 20)) return 0u;   // unbounded traces: skip
  u64 t0 = atp_now_us();
  for (u32 ti = 0; ti < scan && n < cap; ti++) {
    Term u = 0, v = 0;
    if (cli_scan_over(t0, ti)) break;
    if (gc_from_end() - HEAP_NEXT < CLI_BRIDGE_HEADROOM) break;
    u64 hp = thvm_atp_heap_checkpoint();
    u64 g0 = gc_count();
    if (!cli_tp_cached(s, ti, &u, &v)) continue;
    for (u32 side = 0; side < 2u && n < cap; side++) {
      Term st = side ? pr : pl;
      PcRwSet acc;
      cli_bridge_cands(s, st, u, v, &acc);
      for (int k = 0; k < acc.n && n < cap; k++) {
        out[n].eq = ti;
        out[n].k = (u32)k;
        out[n].side = (u8)side;
        out[n].h = side ? cli_pair_canon_hash(pl, acc.out[k])
                        : cli_pair_canon_hash(acc.out[k], pr);
        n++;
      }
    }
    if (gc_count() != g0) break;   // relocation: stop scanning
    thvm_atp_heap_reset(hp);
  }
  return n;
}

// Re-derive one successor pair from its pick (deterministic).
static u8 cli_bridge_apply(AtpState *s, Term pl, Term pr,
                           u32 eq, u8 side, u32 k, Term *ol, Term *or_) {
  Term u = 0, v = 0;
  if (!cli_tp_cached(s, eq, &u, &v)) return 0u;
  Term st = side ? pr : pl;
  PcRwSet acc;
  cli_bridge_cands(s, st, u, v, &acc);
  if ((int)k >= acc.n) return 0u;
  *ol = side ? pl : acc.out[k];
  *or_ = side ? acc.out[k] : pr;
  return 1u;
}

// Bridge (pl, pr) -> (tl, tr) in one or two rewrites.  Two-step uses
// meet-in-the-middle: successors of the parent x successors of the
// target (rewriting by an equation is symmetric, so a target
// successor read backward is a valid forward step; the grounded-
// template face reversed is still an instance of the cited equation).
// Fills steps[] forward from the parent; returns the step count
// (0 = no bridge).
static u32 cli_bridge_search(AtpState *s, Term pl, Term pr,
                             Term tl, Term tr, u32 below,
                             CliBridgeStep *steps) {
  if (cli_emit_late()) return 0u;
  static CliBridgeMeta A[CLI_BRIDGE_SUCC_CAP];
  static CliBridgeMeta B[CLI_BRIDGE_SUCC_CAP];
  u32 na = cli_bridge_scan(s, pl, pr, below, A, CLI_BRIDGE_SUCC_CAP);
  u64 th = cli_pair_canon_hash(tl, tr);
  // 1-step: hash match against the target, verified by re-derivation.
  for (u32 i = 0; i < na; i++) {
    if (A[i].h != th) continue;
    Term ml = 0, mr = 0;
    u64 hp = thvm_atp_heap_checkpoint();
    u64 g0 = gc_count();
    u8 ok = cli_bridge_apply(s, pl, pr, A[i].eq, A[i].side, A[i].k,
                             &ml, &mr) &&
            cli_pair_eq_mod(ml, mr, tl, tr);
    if (ok) {
      steps[0].eq = A[i].eq;
      steps[0].l = ml;
      steps[0].r = mr;
      return 1u;
    }
    if (gc_count() == g0) thvm_atp_heap_reset(hp);
  }
  // 2-step meet-in-the-middle on hashes; verify each collision pair.
  u32 nb = cli_bridge_scan(s, tl, tr, below, B, CLI_BRIDGE_SUCC_CAP);
  u32 tries = 0u;
  for (u32 i = 0; i < na && tries < 64u; i++) {
    for (u32 j = 0; j < nb && tries < 64u; j++) {
      if (A[i].h != B[j].h) continue;
      tries++;
      Term al = 0, ar = 0, bl = 0, br2 = 0;
      u64 hp = thvm_atp_heap_checkpoint();
      u64 g0 = gc_count();
      u8 ok = cli_bridge_apply(s, pl, pr, A[i].eq, A[i].side, A[i].k,
                               &al, &ar) &&
              cli_bridge_apply(s, tl, tr, B[j].eq, B[j].side, B[j].k,
                               &bl, &br2) &&
              cli_pair_eq_mod(al, ar, bl, br2);
      if (ok) {
        steps[0].eq = A[i].eq;
        steps[0].l = al;
        steps[0].r = ar;
        steps[1].eq = B[j].eq;
        steps[1].l = tl;
        steps[1].r = tr;
        return 2u;
      }
      if (gc_count() == g0) thvm_atp_heap_reset(hp);
    }
  }
  return 0u;
}

// Parent rescue: when no bridge from the RECORDED parent reaches the
// record's pair, the recorded index may be a mislabel (the LAT171
// class: an ESET-internal index written where a trace id was read).
// Search every EARLIER trace entry for one whose pair equals the
// record's (pure relabel) or reaches it in ONE rewrite; the found
// citation is verified by re-derivation, so a wrong rescue is
// impossible -- only a missing one.  Returns the rescued parent id
// (and fills steps/n_steps with the connecting rewrite) or
// ATP_TRACE_NONE.
static int cmp_u64(const void *a, const void *b) {
  u64 x = *(const u64 *)a, y = *(const u64 *)b;
  return (x < y) ? -1 : (x > y) ? 1 : 0;
}

static u32 cli_bridge_rescue(AtpState *s, Term tl, Term tr, u32 below,
                             CliBridgeStep *steps, u32 *n_steps) {
  if (cli_emit_late()) return ATP_TRACE_NONE;
  static CliBridgeMeta B[CLI_BRIDGE_SUCC_CAP];
  static u64 bh[CLI_BRIDGE_SUCC_CAP];
  u64 th = cli_pair_canon_hash(tl, tr);
  u32 nb = cli_bridge_scan(s, tl, tr, below, B, CLI_BRIDGE_SUCC_CAP);
  for (u32 j = 0; j < nb; j++) bh[j] = B[j].h;
  qsort(bh, nb, sizeof(u64), cmp_u64);
  u32 scan = (below < s->n_trace) ? below : s->n_trace;
  if (scan > (1u << 20)) return ATP_TRACE_NONE;
  u64 t0 = atp_now_us();
  for (u32 e = 0; e < scan; e++) {
    if (cli_scan_over(t0, e)) break;
    if (gc_from_end() - HEAP_NEXT < CLI_BRIDGE_HEADROOM) break;
    u64 hp = thvm_atp_heap_checkpoint();
    u64 g0 = gc_count();
    Term el = 0, er = 0;
    if (!cli_tp_cached(s, e, &el, &er)) continue;
    u64 eh = cli_pair_canon_hash(el, er);
    if (eh == th && cli_pair_eq_mod(el, er, tl, tr)) {
      if (gc_count() == g0) thvm_atp_heap_reset(hp);
      *n_steps = 0u;   // pure relabel: cite e directly
      return e;
    }
    if (nb > 0u && bsearch(&eh, bh, nb, sizeof(u64), cmp_u64) != NULL) {
      // One rewrite of the TARGET reaches e's pair; read backward,
      // e's pair reaches the target by the same equation.  Find the
      // matching B pick and emit the target pair as the bridge step.
      for (u32 j = 0; j < nb; j++) {
        if (B[j].h != eh) continue;
        Term bl = 0, br2 = 0;
        if (cli_bridge_apply(s, tl, tr, B[j].eq, B[j].side, B[j].k,
                             &bl, &br2) &&
            cli_pair_eq_mod(bl, br2, el, er)) {
          steps[0].eq = B[j].eq;
          steps[0].l = tl;
          steps[0].r = tr;
          *n_steps = 1u;
          return e;
        }
      }
    }
    if (gc_count() == g0) thvm_atp_heap_reset(hp);
  }
  return ATP_TRACE_NONE;
}

static void cli_emit_trace(FILE *fp, AtpState *s, const PrProblem *pr,
                           u32 ti, u8 *printed);

// Treatment chain of record ti (the search recorded none): replay it
// (thvm_atp_rederive), print the cited equations' derivations, then one
// rw record per rewrite, t<ti>n<k>.  On success `last` names the final
// record, which the entry's own record cites.  The chain buffer is per
// call: the cited derivations recurse through here.
#define CLI_RD_MAX 512u
static int cli_emit_rederived(FILE *fp, AtpState *s, const PrProblem *pr,
                              u32 ti, u8 *printed, char *last, size_t cap) {
  AtpRederiveStep *rd =
      (AtpRederiveStep *)malloc(CLI_RD_MAX * sizeof(AtpRederiveStep));
  if (rd == NULL) return 0;
  u32 n = thvm_atp_rederive(s, ti, rd, CLI_RD_MAX);
  if (n == 0u) { free(rd); return 0; }
  for (u32 k = 0; k < n; k++) cli_emit_trace(fp, s, pr, rd[k].eq, printed);
  Term e = s->trace[ti];
  char prev[32];
  snprintf(prev, sizeof prev, "t%u", (u32)term_val(term_ctr_at(e, 0)));
  for (u32 k = 0; k < n; k++) {
    fprintf(fp, "cnf(t%un%u, plain, ", ti, k);
    cli_print_term(fp, pr, rd[k].l);
    fputs(" = ", fp);
    cli_print_term(fp, pr, rd[k].r);
    fprintf(fp, ", inference(rw, [status(thm)], [%s, t%u])).\n",
            prev, rd[k].eq);
    snprintf(prev, sizeof prev, "t%un%u", ti, k);
  }
  snprintf(last, cap, "%s", prev);
  free(rd);
  return 1;
}

// Trace id of the input definition introducing each fresh label
// (ATP_TRACE_NONE: the definition never entered): the identity the
// bench recorded at the engine feed (PrProblem.sym_def_trace), else
// the first definition-shaped axiom (built once per emission).
static u32 *g_cli_def_trace = NULL;
static const AtpState *g_cli_def_trace_s = NULL;

static u32 cli_def_trace_of(AtpState *s, const PrProblem *pr, u32 lab) {
  if (cli_is_flat_label(pr, lab)) {
    u32 fed = pr->sym_def_trace[lab - PR_LABEL_BASE];
    if (fed != ATP_TRACE_NONE && fed < s->n_trace) return fed;
  }
  u32 n = pr->n_fresh_syms;
  if (g_cli_def_trace_s != s) {
    free(g_cli_def_trace);
    g_cli_def_trace = (u32 *)malloc((size_t)(n ? n : 1u) * sizeof(u32));
    g_cli_def_trace_s = s;
    if (g_cli_def_trace == NULL) return ATP_TRACE_NONE;
    for (u32 k = 0; k < n; k++) g_cli_def_trace[k] = ATP_TRACE_NONE;
    for (u32 ti = 0; ti < s->n_trace; ti++) {
      Term e = s->trace[ti];
      if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 4u ||
          term_ext(e) != TRACE_AXIOM) continue;
      Term l = 0, r = 0;
      if (!cli_tp_cached(s, ti, &l, &r)) continue;
      u32 d = cli_def_label(pr, l, r);
      if (d == 0u) d = cli_def_label(pr, r, l);
      if (d == 0u) continue;
      u32 k = d - PR_LABEL_BASE - pr->fresh_sym_base;
      if (k < n && g_cli_def_trace[k] == ATP_TRACE_NONE) g_cli_def_trace[k] = ti;
    }
  }
  if (g_cli_def_trace == NULL || !cli_is_flat_label(pr, lab)) return ATP_TRACE_NONE;
  return g_cli_def_trace[lab - PR_LABEL_BASE - pr->fresh_sym_base];
}

// --- tgt-fold presentation --------------------------------------------
// Under --tgt-fold the definitions are born inter-reduced (sF1 =
// complement(sF0)) and the goal / conjecture units are folded in place
// (sF22 != n1).  Printed as-is they state nothing the problem gives,
// and a definition body over another fresh symbol reads as a second
// introduction of that symbol.  Each such record is printed as its RAW
// form instead -- every fresh application expanded to its definition
// body, i.e. a problem formula or a definition over problem symbols --
// followed by single-rewrite FOLD steps, each citing the definition it
// folds by, ending at the engine's record.

// The pair of definition record `ti` split into head (fresh symbol
// applied to its variables) and body.
static u8 cli_def_split(AtpState *s, const PrProblem *pr, u32 ti,
                        Term *head, Term *body) {
  Term l = 0, r = 0;
  if (ti == ATP_TRACE_NONE || !cli_tp_cached(s, ti, &l, &r)) return 0u;
  if (cli_def_label(pr, l, r) != 0u) { *head = l; *body = r; return 1u; }
  if (cli_def_label(pr, r, l) != 0u) { *head = r; *body = l; return 1u; }
  return 0u;
}

// `t` with every fresh-symbol application replaced by its definition's
// raw body; 0 when some fresh symbol has no definition record.
static Term cli_unfold(AtpState *s, const PrProblem *pr, Term t, u32 depth) {
  if (t == 0 || term_tag(t) != TAG_CTR) return t;
  u32 n = term_ctr_n(t);
  if (n > REWRITE_MAX_ARITY || depth > 256u) return 0;
  Term kids[REWRITE_MAX_ARITY];
  for (u32 i = 0; i < n; i++) {
    kids[i] = cli_unfold(s, pr, term_ctr_at(t, i), depth + 1u);
    if (kids[i] == 0) return 0;
  }
  Term u = term_new_ctr(term_ext(t), n ? kids : NULL, n);
  if (!cli_is_flat_label(pr, term_ext(t))) return u;
  Term head = 0, body = 0;
  if (!cli_def_split(s, pr, cli_def_trace_of(s, pr, term_ext(t)), &head, &body))
    return 0;
  RewriteSubst sub = {{0}};
  if (!thvm_match(head, u, &sub)) return 0;
  return cli_unfold(s, pr, thvm_subst_apply(body, &sub), depth + 1u);
}

// One innermost-leftmost fold: a subterm that is an instance of some
// definition body becomes the definition head.  0 when none applies.
static Term cli_fold_once(AtpState *s, const PrProblem *pr, Term t, u32 *def) {
  if (t == 0 || term_tag(t) != TAG_CTR) return 0;
  u32 n = term_ctr_n(t);
  if (n == 0u || n > REWRITE_MAX_ARITY) return 0;
  for (u32 i = 0; i < n; i++) {
    Term c = cli_fold_once(s, pr, term_ctr_at(t, i), def);
    if (c == 0) continue;
    Term kids[REWRITE_MAX_ARITY];
    for (u32 j = 0; j < n; j++) kids[j] = (j == i) ? c : term_ctr_at(t, j);
    return term_new_ctr(term_ext(t), kids, n);
  }
  for (u32 k = 0; k < pr->n_fresh_syms; k++) {
    u32 lab = PR_LABEL_BASE + pr->fresh_sym_base + k;
    u32 ti = cli_def_trace_of(s, pr, lab);
    Term head = 0, body = 0;
    if (!cli_def_split(s, pr, ti, &head, &body)) continue;
    if (term_tag(body) != TAG_CTR || term_ext(body) != term_ext(t)) continue;
    RewriteSubst sub = {{0}};
    if (!thvm_match(body, t, &sub)) continue;
    *def = ti;
    return thvm_subst_apply(head, &sub);
  }
  return 0;
}

#define CLI_FOLD_MAX 256u
typedef struct { Term l, r; u32 def; } CliFoldStep;

// Raw form of (tl, tr) plus the fold steps back to it.  1 when the
// chain reaches (tl, tr) exactly with at least one step; every cited
// definition is emitted before returning.  `head` (1 = left, 2 =
// right, 0 = none) marks a definition's own head, kept as-is.
static int cli_fold_chain(FILE *fp, AtpState *s, const PrProblem *pr,
                          Term tl, Term tr, int head, Term *rl, Term *rr,
                          CliFoldStep *st, u32 *nst, u8 *printed) {
  *nst = 0u;
  Term ul = (head == 1) ? tl : cli_unfold(s, pr, tl, 0u);
  Term ur = (head == 2) ? tr : cli_unfold(s, pr, tr, 0u);
  if (ul == 0 || ur == 0) return 0;
  if ((head != 1 && cli_term_has_flat_sym(pr, ul)) ||
      (head != 2 && cli_term_has_flat_sym(pr, ur))) return 0;
  Term cl = ul, cr = ur;
  // A definition's own head stays: fold only the side that differs.
  while (!(kbo_eq(cl, tl) && kbo_eq(cr, tr))) {
    if (*nst >= CLI_FOLD_MAX) return 0;
    u32 def = ATP_TRACE_NONE;
    Term nl = cl, nr = cr;
    if (!kbo_eq(cl, tl)) nl = cli_fold_once(s, pr, cl, &def);
    else nr = cli_fold_once(s, pr, cr, &def);
    if (nl == 0 || nr == 0 || def == ATP_TRACE_NONE) return 0;
    cl = nl;
    cr = nr;
    st[*nst].l = cl;
    st[*nst].r = cr;
    st[*nst].def = def;
    (*nst)++;
  }
  if (*nst == 0u) return 0;
  for (u32 k = 0; k < *nst; k++) cli_emit_trace(fp, s, pr, st[k].def, printed);
  *rl = ul;
  *rr = ur;
  return 1;
}

// Print the fold steps after the raw record `first` (records
// <base>f<k>, the last one named `last`), `op` = "=" or "!=".
static void cli_print_fold(FILE *fp, const PrProblem *pr, const char *base,
                           const char *first, const char *last,
                           const char *op, const CliFoldStep *st, u32 nst) {
  char prev[64];
  snprintf(prev, sizeof prev, "%s", first);
  for (u32 k = 0; k < nst; k++) {
    char nm[64];
    if (k + 1u == nst) snprintf(nm, sizeof nm, "%s", last);
    else snprintf(nm, sizeof nm, "%sf%u", base, k);
    fprintf(fp, "cnf(%s, plain, ", nm);
    cli_print_term(fp, pr, st[k].l);
    fprintf(fp, " %s ", op);
    cli_print_term(fp, pr, st[k].r);
    fprintf(fp, ", inference(rw, [status(thm)], [%s, t%u])).\n", prev, st[k].def);
    snprintf(prev, sizeof prev, "%s", nm);
  }
}

// Emit the trace entry `ti` (and, recursively, its parents) as cnf()
// lines; printed[] de-duplicates.  Names: t<ti>.
static void cli_emit_trace(FILE *fp, AtpState *s, const PrProblem *pr,
                           u32 ti, u8 *printed) {
  if (ti == ATP_TRACE_NONE || ti >= s->n_trace || printed[ti]) return;
  printed[ti] = 1u;
  Term e = s->trace[ti];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 4u) {
    char nm[32];
    snprintf(nm, sizeof nm, "t%u", ti);
    cli_unjustified(nm, "(unreadable record)");
    return;
  }
  u32 reason = term_ext(e);
  u32 pa = (u32)term_val(term_ctr_at(e, 0));
  u32 pb = (u32)term_val(term_ctr_at(e, 1));
  Term lhs = term_ctr_at(e, 2);
  Term rhs = term_ctr_at(e, 3);
  // Under the off-heap packed trace (TracePack) a non-cone entry's pair
  // slots hold NUM sentinels; load the real pair from the packed store.
  if (term_tag(lhs) == TAG_NUM && term_tag(rhs) == TAG_NUM) {
    Term ll = 0, rr = 0;
    if (atp_trace_eq_load(s, ti, &ll, &rr)) { lhs = ll; rhs = rr; }
  }
  if (term_tag(lhs) == TAG_NUM || term_tag(rhs) == TAG_NUM) {
    char nm[32];
    snprintf(nm, sizeof nm, "t%u", ti);
    cli_unjustified(nm, "(no stored equation)");
  }
  cli_emit_trace(fp, s, pr, pa, printed);
  cli_emit_trace(fp, s, pr, pb, printed);
  // Treatment-compression bridge (see cli_bridge_one): an orient
  // record whose pair is NOT its parent's (modulo orientation and
  // renaming) was normalized during treatment; interpose the found
  // rewrite so the printed record follows from its citations.
  char pa_name[32];
  pa_name[0] = '\0';
  // Justified: an input leaf, a critical pair of its two cited parents
  // (the engine stores the raw overlap), or an ORIENT / SIMPLIFY / FVI
  // record that restates its parent (an FVI record: an instance of it)
  // or is reached from its citation by the re-derived treatment chain, a
  // verified one- or two-rewrite bridge, or a verified parent rescue.
  u8 derived = 1u;
  if (reason == TRACE_ORIENT || reason == TRACE_SIMPLIFY ||
      reason == TRACE_FVI) {
    Term ppl = 0, ppr = 0;
    derived = 0u;
    if (pa != ATP_TRACE_NONE && pa < s->n_trace && pb == ATP_TRACE_NONE &&
        cli_tp_cached(s, pa, &ppl, &ppr)) {
      if (cli_pair_eq_mod(ppl, ppr, lhs, rhs) ||
          (reason == TRACE_FVI && cli_pair_instance(ppl, ppr, lhs, rhs))) {
        derived = 1u;
      } else if (cli_emit_rederived(fp, s, pr, ti, printed, pa_name,
                                    sizeof pa_name)) {
        // Treatment chain re-derived by replay: pa_name is its last record.
        derived = 1u;
      } else if (reason != TRACE_FVI) {
        CliBridgeStep br[2];
        u32 nbr = cli_bridge_search(s, ppl, ppr, lhs, rhs, ti, br);
        u32 cite = pa;
        if (nbr == 0u) {
          // No path from the recorded parent: parent rescue (mislabel /
          // deeper treatment); an earlier entry reaching the pair in
          // <= 1 verified step replaces the citation.
          u32 rp = cli_bridge_rescue(s, lhs, rhs, ti, br, &nbr);
          if (rp != ATP_TRACE_NONE) {
            cite = rp;
            derived = 1u;
            cli_emit_trace(fp, s, pr, rp, printed);
            if (nbr == 0u) snprintf(pa_name, sizeof pa_name, "t%u", rp);
          }
        }
        if (nbr > 0u) {
          derived = 1u;
          snprintf(pa_name, sizeof pa_name, "t%u", cite);
          for (u32 k = 0; k < nbr; k++) {
            cli_emit_trace(fp, s, pr, br[k].eq, printed);
            fprintf(fp, "cnf(t%ub%u, plain, ", ti, k);
            cli_print_term(fp, pr, br[k].l);
            fputs(" = ", fp);
            cli_print_term(fp, pr, br[k].r);
            fprintf(fp, ", inference(rw, [status(thm)], [%s, t%u])).\n",
                    pa_name, br[k].eq);
            snprintf(pa_name, sizeof pa_name, "t%ub%u", ti, k);
          }
        }
      }
    }
  } else if (reason == TRACE_CP) {
    derived = (u8)(pa < s->n_trace && pb < s->n_trace);
  } else if (reason != TRACE_AXIOM) {
    derived = 0u;
  }
  if (!derived) {
    char nm[32];
    snprintf(nm, sizeof nm, "t%u", ti);
    cli_unjustified(nm, reason == TRACE_ORIENT   ? "(orient)" :
                        reason == TRACE_SIMPLIFY ? "(simplify)" :
                        reason == TRACE_FVI      ? "(free-variable instance)" :
                        reason == TRACE_CP       ? "(critical pair)" :
                                                   "(unknown inference)");
  }
  // FOF-path axiom leaf: resolve the conversion lineage FIRST -- the
  // chain records it cites must precede this record in the output.
  const char *prov_src = NULL;
  char srcbuf[1024];
  if (reason == TRACE_AXIOM)
    prov_src = cli_prov_source(fp, pr, lhs, rhs, 0, srcbuf, sizeof srcbuf);
  u32 defl = 0u;
  if (reason == TRACE_AXIOM && prov_src == NULL &&
      (cli_term_has_flat_sym(pr, lhs) || cli_term_has_flat_sym(pr, rhs))) {
    // Folded record (see cli_fold_chain): raw leaf, then the fold steps.
    Term rl = 0, rr = 0;
    // Heap-allocated: cli_fold_chain recurses (through the definitions
    // it cites) into other records' fold chains.
    CliFoldStep *fst = (CliFoldStep *)malloc(CLI_FOLD_MAX * sizeof(CliFoldStep));
    u32 nfst = 0u;
    int body_fresh = 1, head = 0;
    defl = cli_def_label(pr, lhs, rhs);
    if (defl != 0u) { body_fresh = cli_term_has_flat_sym(pr, rhs); head = 1; }
    else if ((defl = cli_def_label(pr, rhs, lhs)) != 0u) {
      body_fresh = cli_term_has_flat_sym(pr, lhs);
      head = 2;
    }
    // Definition SHAPE is not identity: a folded conjecture unit can
    // read sF89(X1,X2) = b.  Only the record the feed tied to the label
    // defines it; any other is a folded axiom (raw leaf + fold steps).
    if (defl != 0u && cli_def_trace_of(s, pr, defl) != ti) {
      defl = 0u;
      body_fresh = 1;
      head = 0;
    }
    if (fst != NULL && body_fresh &&
        cli_fold_chain(fp, s, pr, lhs, rhs, head, &rl, &rr, fst, &nfst, printed)) {
      u32 rdef = cli_def_label(pr, rl, rr);
      if (rdef == 0u) rdef = cli_def_label(pr, rr, rl);
      const char *rsrc = NULL;
      char rbuf[1024];
      if (rdef == 0u) rsrc = cli_prov_source(fp, pr, rl, rr, 0, rbuf, sizeof rbuf);
      fprintf(fp, "cnf(t%ur, axiom, ", ti);
      cli_print_term(fp, pr, rl);
      fputs(" = ", fp);
      cli_print_term(fp, pr, rr);
      if (rdef != 0u)
        fprintf(fp, ", introduced(definition, [new_symbols(definition, [%s])])).\n",
                pr->syms[rdef - PR_LABEL_BASE].name);
      else if (rsrc != NULL)
        fputs(rsrc, fp);
      else
        fprintf(fp, ", file('%s')).\n", g_cli_problem);
      char base[32], first[32], last[32];
      snprintf(base, sizeof base, "t%u", ti);
      snprintf(first, sizeof first, "t%ur", ti);
      snprintf(last, sizeof last, "t%u", ti);
      cli_print_fold(fp, pr, base, first, last, "=", fst, nfst);
      free(fst);
      return;
    }
    free(fst);
  }
  fprintf(fp, "cnf(t%u, %s, ", ti,
          (reason == TRACE_AXIOM && prov_src == NULL) ? "axiom" : "plain");
  cli_print_term(fp, pr, lhs);
  fputs(" = ", fp);
  cli_print_term(fp, pr, rhs);
  if (reason == TRACE_AXIOM) {
    // Leaf clause: a problem axiom cites the problem file so GDV can
    // match it and complete leaf verification; a goal-flattening
    // definition (fresh sFlat constant = subterm) is an introduced
    // definitional extension, not a problem clause, and is sourced as
    // such, declaring the ONE symbol it introduces.  An equationalized
    // FOF clause cites its conversion chain (J13: leaves must be
    // formulae from the problem).
    if (prov_src != NULL)
      fputs(prov_src, fp);
    else if (defl != 0u)
      fprintf(fp, ", introduced(definition, [new_symbols(definition, [%s])])).\n",
              pr->syms[defl - PR_LABEL_BASE].name);
    else if (cli_term_has_flat_sym(pr, lhs) || cli_term_has_flat_sym(pr, rhs))
      fputs(", introduced(definition)).\n", fp);
    else
      fprintf(fp, ", file('%s')).\n", g_cli_problem);
  } else {
    const char *rule =
        (reason == TRACE_CP)      ? "cp" :
        (reason == TRACE_ORIENT)  ? "orient" :
        (reason == TRACE_SIMPLIFY)? "rw" : "step";
    fprintf(fp, ", inference(%s, [status(thm)], [", rule);
    int firstp = 1;
    if (pa_name[0] != '\0') { fprintf(fp, "%s", pa_name); firstp = 0; }
    else if (pa != ATP_TRACE_NONE && pa < s->n_trace) { fprintf(fp, "t%u", pa); firstp = 0; }
    if (pb != ATP_TRACE_NONE && pb < s->n_trace) { fprintf(fp, "%st%u", firstp ? "" : ", ", pb); }
    fputs("])).\n", fp);
  }
}

// Resolve a chain step's rule citation by REPLAY: the recorded index is
// wrong for unorientable-equation steps (the FT normalize hands the
// proofsteps fill an ESET-internal index, which the emitter would map
// through RULE slots -- the LAT171-1 mislabeled-citation bug).  Try the
// recorded slot first, then scan every slot for the equation that
// actually reproduces before -> after in one application (either
// direction, any position).  Returns the resolved slot or n_rules.
// Returns the resolved TRACE id (dead rules live only in the trace --
// an interreduced-away equation is absent from the final slots but its
// derivation record remains), or ATP_TRACE_NONE.
static u8 cli_trace_pair(AtpState *s, u32 ti, Term *l, Term *r) {
  if (ti == ATP_TRACE_NONE || ti >= s->n_trace) return 0;
  Term e = s->trace[ti];
  if (term_tag(e) != TAG_CTR || term_ctr_n(e) < 4u) return 0;
  *l = term_ctr_at(e, 2);
  *r = term_ctr_at(e, 3);
  if (term_tag(*l) == TAG_NUM && term_tag(*r) == TAG_NUM) {
    Term ll = 0, rr = 0;
    if (!atp_trace_eq_load(s, ti, &ll, &rr)) return 0;
    *l = ll;
    *r = rr;
  }
  return (*l != 0 && *r != 0);
}
static u8 cli_step_replays(AtpState *s, const AtpProofStep *st,
                           Term l, Term r) {
  PcRwSet acc;
  acc.n = 0;
  pc_rewrite_all(st->before, l, r, &acc);
  pc_rewrite_all(st->before, r, l, &acc);
  // r2 grounded-template faces: the MNF front (and the WM normalizers)
  // apply an equation face whose replacement carries variables outside
  // the matched side by grounding the extras to the minimal signature
  // constant.  The plain faces above leave those extras as variables
  // and never reproduce such a step, so it resolved to NO citation.
  // Applying the grounded template is still applying an instance of
  // the cited equation -- sound to cite.
  Term tl = atp_unorient_template(s, l, r);
  if (tl != 0 && !kbo_eq(tl, r)) pc_rewrite_all(st->before, l, tl, &acc);
  Term tr = atp_unorient_template(s, r, l);
  if (tr != 0 && !kbo_eq(tr, l)) pc_rewrite_all(st->before, r, tr, &acc);
  for (int k = 0; k < acc.n; k++) {
    if (kbo_eq(acc.out[k], st->after)) return 1;
  }
  return 0;
}
// Trace-pair cache: decode each trace entry's equation at most once per
// emission (the deep resolver's midpoint search was re-loading and
// re-scanning per candidate -- the quadratic blowout that truncated
// emission under the prove timeout).
#define CLI_TP_CACHE_MAX 4096
static struct { Term l, r; u8 ok, loaded; } g_cli_tp[CLI_TP_CACHE_MAX];
static u32 g_cli_tp_n = 0;
static void cli_tp_cache_reset(AtpState *s) {
  g_cli_tp_n = (s->n_trace < CLI_TP_CACHE_MAX) ? s->n_trace : CLI_TP_CACHE_MAX;
  // Eager load: the deep resolver's heap rollbacks must never reclaim a
  // cached pair's Terms, so decode everything BEFORE any checkpoint.
  for (u32 i = 0; i < g_cli_tp_n; i++) {
    g_cli_tp[i].ok = cli_trace_pair(s, i, &g_cli_tp[i].l, &g_cli_tp[i].r);
    g_cli_tp[i].loaded = 1;
  }
}
static u8 cli_tp_cached(AtpState *s, u32 ti, Term *l, Term *r) {
  if (ti >= g_cli_tp_n) return cli_trace_pair(s, ti, l, r);
  if (!g_cli_tp[ti].loaded) {
    g_cli_tp[ti].ok = cli_trace_pair(s, ti, &g_cli_tp[ti].l, &g_cli_tp[ti].r);
    g_cli_tp[ti].loaded = 1;
  }
  if (!g_cli_tp[ti].ok) return 0;
  *l = g_cli_tp[ti].l;
  *r = g_cli_tp[ti].r;
  return 1;
}

// Deep citation resolve (a full trace scan per unresolved step): on in
// --proof runs, THVM_ATP_PROOF_RESOLVE_DEEP=0/1 overrides.
static int cli_resolve_deep(void) {
  static int deep = -1;
  if (deep < 0) {
    const char *e = getenv("THVM_ATP_PROOF_RESOLVE_DEEP");
    deep = (e != NULL) ? (e[0] != '0' && e[0] != '\0') : g_cli_want_proof;
  }
  return deep;
}

static u32 cli_resolve_step_trace(AtpState *s, const AtpProofStep *st) {
  if (st->before == 0 || st->after == 0) return ATP_TRACE_NONE;
  // Recorded slot first (correct for oriented-rule steps).  A lazy
  // slot (TAG_NUM sentinel, live only as its FT mirror) materializes
  // through atp_rule_term -- skipping it left every FT-lane step
  // citing NOTHING (the COL006-5 g0_13 abstain).
  if (st->rule < s->n_rules && s->r_trace != NULL) {
    Term l = s->lhs[st->rule], r = s->rhs[st->rule];
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    if (term_tag(l) == TAG_NUM || term_tag(r) == TAG_NUM) {
      l = atp_rule_term(s, st->rule, 0u);
      r = atp_rule_term(s, st->rule, 1u);
    }
#endif
    if (term_tag(l) != TAG_NUM && term_tag(r) != TAG_NUM &&
        cli_step_replays(s, st, l, r)) {
      return s->r_trace[st->rule];
    }
  }
  // Recorded TRACE id next: MNF-extracted steps (and the mode-1
  // normalize decomposition) store the citation as a trace index, not
  // a rule slot.  Verified by replay like every other resolution, so
  // a slot-labelled step can never mis-cite through this arm -- but
  // without it, an MNF step whose rule was interreduced away resolved
  // to NOTHING (rw record with no equation parent: unverifiable).
  if (st->rule < s->n_trace) {
    Term l = 0, r = 0;
    if (cli_tp_cached(s, st->rule, &l, &r) &&
        cli_step_replays(s, st, l, r)) {
      return st->rule;
    }
  }
  // Live slots next (cheap; covers mis-indexed eset steps whose
  // equation is still alive).  Lazy slots materialize (see above).
  for (u32 rr = 0; rr < s->n_rules; rr++) {
    Term l = s->lhs[rr], r = s->rhs[rr];
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    if (term_tag(l) == TAG_NUM || term_tag(r) == TAG_NUM) {
      l = atp_rule_term(s, rr, 0u);
      r = atp_rule_term(s, rr, 1u);
    }
#endif
    if (term_tag(l) == TAG_NUM || term_tag(r) == TAG_NUM) continue;
    if (cli_step_replays(s, st, l, r)) {
      return (s->r_trace != NULL) ? s->r_trace[rr] : ATP_TRACE_NONE;
    }
  }
  // Full trace scan: the applied equation was interreduced away.
  // Costly (replay per trace entry) -- opt-in, the deep resolve pass.
  if (!cli_resolve_deep() || cli_emit_late()) return ATP_TRACE_NONE;
  u64 t0 = atp_now_us();
  for (u32 ti = 0; ti < s->n_trace; ti++) {
    if (cli_scan_over(t0, ti)) break;
    Term l = 0, r = 0;
    if (!cli_tp_cached(s, ti, &l, &r)) continue;
    if (cli_step_replays(s, st, l, r)) return ti;
  }
  return ATP_TRACE_NONE;
}

// The goal record goal_<id>: the negated conjecture as the problem
// states it (a FOF goal cites its encoding lineage).  A tgt-folded goal
// prints its raw form goal_<id>r and the fold steps ending at goal_<id>.
static void cli_emit_goal_leaf(FILE *fp, AtpState *s, const PrProblem *pr,
                               Term gl, Term gr, u32 goal_id, u8 *printed) {
  Term pl = gl, pr_ = gr;
  CliFoldStep *fst = (CliFoldStep *)malloc(CLI_FOLD_MAX * sizeof(CliFoldStep));
  u32 nfst = 0u;
  Term rl = 0, rr = 0;
  if (fst != NULL &&
      (cli_term_has_flat_sym(pr, gl) || cli_term_has_flat_sym(pr, gr)) &&
      cli_fold_chain(fp, s, pr, gl, gr, 0, &rl, &rr, fst, &nfst, printed)) {
    pl = rl;
    pr_ = rr;
  } else {
    nfst = 0u;
  }
  const char *goal_src = NULL;
  char gsrcbuf[1024];
  goal_src = cli_prov_source(fp, pr, pl, pr_, 1, gsrcbuf, sizeof gsrcbuf);
  if (nfst > 0u) fprintf(fp, "cnf(goal_%ur, negated_conjecture, ", goal_id);
  else fprintf(fp, "cnf(goal_%u, negated_conjecture, ", goal_id);
  cli_print_term(fp, pr, pl);
  fputs(" != ", fp);
  cli_print_term(fp, pr, pr_);
  if (goal_src != NULL)
    fputs(goal_src, fp);
  else
    fprintf(fp, ", file('%s')).\n", g_cli_problem);
  if (nfst > 0u) {
    char base[32], first[32], last[32];
    snprintf(base, sizeof base, "goal_%u", goal_id);
    snprintf(first, sizeof first, "goal_%ur", goal_id);
    snprintf(last, sizeof last, "goal_%u", goal_id);
    cli_print_fold(fp, pr, base, first, last, "!=", fst, nfst);
  }
  free(fst);
}

// Emit one goal's closing rewrite chain (steps from
// thvm_atp_proof_extract) plus the derivations of every rule cited.
static void cli_emit_goal_chain(FILE *fp, AtpState *s, const PrProblem *pr,
                                Term gl, Term gr, u32 goal_id,
                                const AtpProofStep *steps, u32 n_steps,
                                u8 *printed) {
  static u32 resolved[ATP_PROOF_MAX_STEPS];
  static u32 resolved2[ATP_PROOF_MAX_STEPS];   // 2nd id of a split step
  static Term midpoint[ATP_PROOF_MAX_STEPS];   // its intermediate term
  cli_tp_cache_reset(s);
  for (u32 k = 0; k < n_steps; k++) {
    resolved[k]  = cli_resolve_step_trace(s, &steps[k]);
    resolved2[k] = ATP_TRACE_NONE;
    midpoint[k]  = 0;
    if (cli_resolve_deep() && resolved[k] == ATP_TRACE_NONE &&
        !cli_emit_late()) {
      // No single recorded equation reproduces before -> after: the
      // recorder coalesced two rewrites into one step.  Find a 2-step
      // decomposition through any midpoint and SPLIT the record at
      // emission so the checker sees single-step inferences.
      const AtpProofStep *st = &steps[k];
      u64 t0 = atp_now_us();
      for (u32 ta = 0; ta < s->n_trace && midpoint[k] == 0; ta++) {
        if (cli_scan_over(t0, ta)) break;
        Term la = 0, ra = 0;
        if (!cli_tp_cached(s, ta, &la, &ra)) continue;
        // The candidate replays allocate heavily (fanout x trace Term
        // builds); without rollback the emission child exhausts
        // from-space and dies mid-write (the 4-record stub).  Nothing
        // survives a failed candidate; a found midpoint skips the
        // rollback so its Term stays live.
        u64 _hp = thvm_atp_heap_checkpoint();
        u64 _gc = gc_count();
        PcRwSet acc;
        acc.n = 0;
        pc_rewrite_all(st->before, la, ra, &acc);
        pc_rewrite_all(st->before, ra, la, &acc);
        for (int m = 0; m < acc.n && midpoint[k] == 0; m++) {
          AtpProofStep half = *st;
          half.before = acc.out[m];
          for (u32 tb = 0; tb < s->n_trace && midpoint[k] == 0; tb++) {
            if (cli_scan_over(t0, tb)) break;
            Term lb = 0, rb = 0;
            if (!cli_tp_cached(s, tb, &lb, &rb)) continue;
            if (cli_step_replays(s, &half, lb, rb)) {
              resolved[k]  = ta;
              resolved2[k] = tb;
              midpoint[k]  = acc.out[m];
            }
          }
        }
        if (midpoint[k] == 0 && gc_count() == _gc) thvm_atp_heap_reset(_hp);
      }
    }
    if (resolved[k] != ATP_TRACE_NONE)
      cli_emit_trace(fp, s, pr, resolved[k], printed);
    if (resolved2[k] != ATP_TRACE_NONE)
      cli_emit_trace(fp, s, pr, resolved2[k], printed);
  }
  // FOF-path goal: cite the encoding lineage of the negative unit
  // (direct goals cite their CNF record; the funnel goal cites every
  // negative clause's record) -- chains print before the record.
  cli_emit_goal_leaf(fp, s, pr, gl, gr, goal_id, printed);
  Term cur_l = gl, cur_r = gr;
  char prev[32];
  snprintf(prev, sizeof prev, "goal_%u", goal_id);
  for (u32 k = 0; k < n_steps; k++) {
    const AtpProofStep *st = &steps[k];
    if (midpoint[k] != 0) {
      // Split record: emit the intermediate disequation first (one
      // application of resolved[k]), then the target via resolved2[k].
      Term mid_l = (st->side == 0u) ? midpoint[k] : cur_l;
      Term mid_r = (st->side == 0u) ? cur_r : midpoint[k];
      fprintf(fp, "cnf(g%u_%um, plain, ", goal_id, k);
      cli_print_term(fp, pr, mid_l);
      fputs(" != ", fp);
      cli_print_term(fp, pr, mid_r);
      fprintf(fp, ", inference(rw, [status(thm)], [%s, t%u])).\n",
              prev, resolved[k]);
      snprintf(prev, sizeof prev, "g%u_%um", goal_id, k);
    }
    if (st->side == 0u) cur_l = st->after; else cur_r = st->after;
    fprintf(fp, "cnf(g%u_%u, plain, ", goal_id, k);
    cli_print_term(fp, pr, cur_l);
    fputs(" != ", fp);
    cli_print_term(fp, pr, cur_r);
    u32 rt = (midpoint[k] != 0) ? resolved2[k] : resolved[k];
    fprintf(fp, ", inference(rw, [status(thm)], [%s", prev);
    if (rt != ATP_TRACE_NONE) fprintf(fp, ", t%u", rt);
    fputs("])).\n", fp);
    snprintf(prev, sizeof prev, "g%u_%u", goal_id, k);
    // A step is derived only by the one-rewrite replay that resolved
    // its equation (cli_resolve_step_trace / the midpoint split).
    if (rt == ATP_TRACE_NONE) cli_unjustified(prev, "(goal rewrite, no equation)");
  }
  fprintf(fp, "cnf(contradiction_%u, plain, $false, "
              "inference(trivial_inequality_removal, [status(thm)], [%s])).\n",
          goal_id, prev);
  if (kbo_eq(cur_l, cur_r)) {
    g_cli_n_refuted++;
  } else {
    char nm[32];
    snprintf(nm, sizeof nm, "contradiction_%u", goal_id);
    cli_unjustified(nm, "(sides not identical)");
  }
}

// === Narrowing (existential-goal) proof emission ====================
//
// Replays the recorded narrowing derivation (AtpState.nrec; see
// src/atp/narrow.c) from the PRISTINE conjecture pair and emits it as
// an instantiate/rw record chain closing at t != t.  Every term below
// is re-derived here -- the recording only names the equation (trace
// id), the goal side, the redex path and the face -- so a defective
// recording can only fail the replay (marker fallback), never emit an
// unsound record.  Each narrow step splits into its two checkable
// halves: an `instantiate` record (parent instance -- the checker's
// match fast-path) and a `rw` record citing the equation's trace
// derivation.

typedef struct {
  Term l, r;
  u32  trace;                 // cited trace id (rw) or ATP_TRACE_NONE
  u8   is_rw;                 // 1 = rw record, 0 = instantiate record
} CliNrecRec;

// Load one trace equation for replay (no cache: narrow chains are
// short).  Returns 0 when the id is out of range / unloadable.
static u8 cli_nrec_eq(AtpState *s, u32 ti, Term *l, Term *r) {
  return cli_trace_pair(s, ti, l, r);
}

// Replay ONE recorded step onto (cur_l, cur_r), appending records.
// Returns 0 on any mismatch.
static int cli_nrec_replay_step(AtpState *s, const NrStep *st,
                                Term *cur_l, Term *cur_r,
                                CliNrecRec *recs, u32 *nr, u32 cap) {
  u32 pos[ATP_PROOF_MAX_DEPTH];
  for (u8 i = 0; i < st->pos_len; i++) pos[i] = st->pos[i];
  switch (st->kind) {
  case NR_CANON:
    thvm_normalize_vars(cur_l, cur_r);   // alpha only: no record needed
    return 1;
  case NR_CLOSE: {
    if (kbo_eq(*cur_l, *cur_r)) return 1;
    RewriteSubst mg = {{0}};
    if (!thvm_unify(*cur_l, *cur_r, &mg)) return 0;
    *cur_l = thvm_unify_apply(*cur_l, &mg);
    *cur_r = thvm_unify_apply(*cur_r, &mg);
    if (*nr >= cap) return 0;
    recs[*nr].l = *cur_l; recs[*nr].r = *cur_r;
    recs[*nr].trace = ATP_TRACE_NONE; recs[*nr].is_rw = 0u;
    (*nr)++;
    return kbo_eq(*cur_l, *cur_r);
  }
  case NR_NORM: {
    Term tl = 0, tr = 0;
    if (!cli_nrec_eq(s, st->trace, &tl, &tr)) return 0;
    Term pat = st->fwd ? tl : tr;
    Term rep = st->fwd ? tr : tl;
    Term side = st->side ? *cur_r : *cur_l;
    Term sub = cp_subterm_at(side, pos, st->pos_len);
    if (sub == 0) return 0;
    RewriteSubst m = {{0}};
    if (!thvm_match(pat, sub, &m)) return 0;
    Term repl;
    if (atp_vars_contained(rep, pat)) {
      repl = thvm_subst_apply(rep, &m);
    } else {
      // r2 grounded template: extras -> minimal constant, exactly the
      // normalizer's application (still an instance of the equation).
      Term tmpl = atp_unorient_template(s, pat, rep);
      if (tmpl == 0) return 0;
      repl = thvm_subst_apply(tmpl, &m);
    }
    Term nside = cp_replace_at(side, pos, st->pos_len, repl);
    if (nside == 0) return 0;
    if (st->side) *cur_r = nside; else *cur_l = nside;
    if (*nr >= cap) return 0;
    recs[*nr].l = *cur_l; recs[*nr].r = *cur_r;
    recs[*nr].trace = st->trace; recs[*nr].is_rw = 1u;
    (*nr)++;
    return 1;
  }
  case NR_NARROW: {
    Term tl = 0, tr = 0;
    if (!cli_nrec_eq(s, st->trace, &tl, &tr)) return 0;
    Term pat = thvm_rename_vars(st->fwd ? tl : tr, CP_RENAME_OFFSET);
    Term rep = thvm_rename_vars(st->fwd ? tr : tl, CP_RENAME_OFFSET);
    Term side = st->side ? *cur_r : *cur_l;
    Term sub = cp_subterm_at(side, pos, st->pos_len);
    if (sub == 0 || term_tag(sub) == TAG_FVR) return 0;
    RewriteSubst mg = {{0}};
    if (!thvm_unify(sub, pat, &mg)) return 0;
    Term inst_l = thvm_unify_apply(*cur_l, &mg);
    Term inst_r = thvm_unify_apply(*cur_r, &mg);
    if (!kbo_eq(inst_l, *cur_l) || !kbo_eq(inst_r, *cur_r)) {
      if (*nr >= cap) return 0;
      recs[*nr].l = inst_l; recs[*nr].r = inst_r;
      recs[*nr].trace = ATP_TRACE_NONE; recs[*nr].is_rw = 0u;
      (*nr)++;
    }
    Term rep_i = thvm_unify_apply(rep, &mg);
    Term iside = st->side ? inst_r : inst_l;
    Term nside = cp_replace_at(iside, pos, st->pos_len, rep_i);
    if (nside == 0) return 0;
    *cur_l = st->side ? inst_l : nside;
    *cur_r = st->side ? nside : inst_r;
    if (*nr >= cap) return 0;
    recs[*nr].l = *cur_l; recs[*nr].r = *cur_r;
    recs[*nr].trace = st->trace; recs[*nr].is_rw = 1u;
    (*nr)++;
    return 1;
  }
  default:
    return 0;
  }
}

#define CLI_NREC_MAX_RECS 16384u

// Emit the recorded narrowing proof for goal `goal_id`.  Returns 1 on
// a complete, closed replay; 0 leaves the caller's marker path.
static int cli_emit_narrow_proof(FILE *fp, AtpState *s,
                                 const PrProblem *pr, u32 goal_id,
                                 u8 *printed) {
  AtpNrec *q = (AtpNrec *)s->nrec;
  Term gl = s->goal_orig_lhs, gr = s->goal_orig_rhs;
  if (gl == 0 || gr == 0) return 0;
  // Assemble the winning step sequence: the greedy chain is the arena
  // prefix; the queue's is the closing segment's parent lineage.
  static const NrStep *seq[CLI_NREC_MAX_RECS];
  u32 n_seq = 0u;
  if (q != NULL && q->closed) {
    // Lineage: close_parent .. seed, reversed.
    u32 lineage[512];
    u32 ln = 0u;
    u32 cur = q->close_parent;
    while (cur != NR_ROOT && ln < 512u) {
      if (cur == NR_NONE || cur >= q->n_cg) return 0;
      lineage[ln++] = cur;
      cur = q->cg_parent[cur];
    }
    if (cur != NR_ROOT) return 0;   // lineage too deep / broken
    for (u32 i = ln; i > 0u; i--) {
      u32 cg = lineage[i - 1u];
      u32 off = q->cg_off[cg];
      if (off == NR_NONE) return 0;
      for (u32 k = 0; k < q->cg_len[cg] && n_seq < CLI_NREC_MAX_RECS; k++)
        seq[n_seq++] = &q->steps[off + k];
    }
    if (q->close_off == NR_NONE) return 0;
    for (u32 k = 0; k < q->close_len && n_seq < CLI_NREC_MAX_RECS; k++)
      seq[n_seq++] = &q->steps[q->close_off + k];
  } else if (q != NULL && q->greedy) {
    if (q->lost) return 0;
    for (u32 k = 0; k < q->n && n_seq < CLI_NREC_MAX_RECS; k++)
      seq[n_seq++] = &q->steps[k];
  } else if (!(term_tag(gl) == term_tag(gr) && kbo_eq(gl, gr))) {
    return 0;   // nothing recorded and the goal is not trivially closed
  }
  if (n_seq >= CLI_NREC_MAX_RECS) return 0;
  // Replay.
  CliNrecRec *recs =
      (CliNrecRec *)malloc((size_t)(2u * n_seq + 2u) * sizeof(CliNrecRec));
  if (recs == NULL) return 0;
  u32 cap = 2u * n_seq + 2u;
  u32 nrs = 0u;
  Term cur_l = gl, cur_r = gr;
  for (u32 k = 0; k < n_seq; k++) {
    if (!cli_nrec_replay_step(s, seq[k], &cur_l, &cur_r,
                              recs, &nrs, cap)) {
      free(recs);
      return 0;
    }
  }
  if (!kbo_eq(cur_l, cur_r)) { free(recs); return 0; }
  // Emit: cited trace derivations first, then the goal chain.
  for (u32 k = 0; k < nrs; k++) {
    if (recs[k].is_rw && recs[k].trace != ATP_TRACE_NONE)
      cli_emit_trace(fp, s, pr, recs[k].trace, printed);
  }
  cli_emit_goal_leaf(fp, s, pr, gl, gr, goal_id, printed);
  char prev[32];
  snprintf(prev, sizeof prev, "goal_%u", goal_id);
  for (u32 k = 0; k < nrs; k++) {
    fprintf(fp, "cnf(g%u_%u, plain, ", goal_id, k);
    cli_print_term(fp, pr, recs[k].l);
    fputs(" != ", fp);
    cli_print_term(fp, pr, recs[k].r);
    if (recs[k].is_rw && recs[k].trace != ATP_TRACE_NONE)
      fprintf(fp, ", inference(rw, [status(thm)], [%s, t%u])).\n",
              prev, recs[k].trace);
    else
      fprintf(fp, ", inference(instantiate, [status(thm)], [%s])).\n",
              prev);
    snprintf(prev, sizeof prev, "g%u_%u", goal_id, k);
  }
  fprintf(fp, "cnf(contradiction_%u, plain, $false, "
              "inference(trivial_inequality_removal, [status(thm)], [%s])).\n",
          goal_id, prev);
  g_cli_n_refuted++;
  free(recs);
  return 1;
}

// The internal signature differs from the problem's (quoted atoms,
// hashed long names, curried wide functors): proof text is printed
// through cli_proof_write, which maps every record back.
static int cli_proof_renamed(void) { return g_tp_nren > 0 || g_curry_nfn > 0; }

static void cli_proof_write(FILE *out, const char *text, size_t len) {
  if (g_curry_nfn > 0) {
    size_t ul = 0;
    char *u = tp_uncurry_text(text, len, &ul);
    if (u != NULL) {
      tp_unrename_write(out, u, ul);
      free(u);
      return;
    }
  }
  tp_unrename_write(out, text, len);
}

static void cli_proof_hook(AtpState *s) {
  if (!g_cli_want_proof || g_atp_bench_status != (int)ATP_PROVED) return;
  thvm_atp_rederive_deadline(g_cli_emit_deadline_us);
  PrProblem *pr = (PrProblem *)g_atp_bench_pr;
  if (pr == NULL) return;
  static AtpProofStep steps[ATP_PROOF_MAX_STEPS];
  u8 *printed = (u8 *)calloc(s->n_trace, 1u);
  if (printed == NULL) return;
  // The bench's stdout is rerouted to stderr in competition mode; the
  // SZS stream goes to the preserved real stdout (g_cli_out).
  FILE *real_out = (g_cli_out != NULL) ? g_cli_out : stdout;
  if (g_cli_want_szs && !g_cli_szs_printed) {
    // FOF ontology when a conjecture was negated (Theorem); CNF
    // ontology for clausal input (Unsatisfiable).  The status stands
    // whatever the emission below decides.
    char *st_buf = NULL;
    size_t st_len = 0;
    FILE *st_ms = open_memstream(&st_buf, &st_len);
    FILE *st_out = (st_ms != NULL) ? st_ms : real_out;
    fprintf(st_out, "%% SZS status %s for %s\n",
            g_cli_fof_conjecture ? "Theorem" :
            g_cli_tptp_mode      ? "Unsatisfiable" : "Theorem",
            g_cli_problem);
    if (st_ms != NULL) {
      fclose(st_ms);
      if (cli_proof_renamed()) cli_proof_write(real_out, st_buf, st_len);
      else fwrite(st_buf, 1, st_len, real_out);
      free(st_buf);
    }
    fflush(real_out);
    g_cli_szs_printed = 1;
  }
  // The block is built in memory and printed only when every step in it
  // is derived (renamed input identifiers -- quoted atoms, hashed long
  // names -- get the problem's own names back on the way out).
  char *blk_buf = NULL;
  size_t blk_len = 0;
  FILE *out = open_memstream(&blk_buf, &blk_len);
  if (out == NULL) {
    fprintf(stderr, "FindProof: proof withheld: no memory for the proof block\n");
    free(printed);
    return;
  }
  g_cli_n_unjust = 0u;
  g_cli_n_refuted = 0u;
  g_cli_unjust_what[0] = '\0';
  // FOF-path derivations include fof() conversion records, so the
  // output form is a Proof; pure-CNF input keeps CNFRefutation
  // byte-identically.
  const char *szs_form = g_cli_fof_applied ? "Proof" : "CNFRefutation";
  fprintf(out, "%% SZS output start %s for %s\n", szs_form, g_cli_problem);
  // Source the goal terms from the ENGINE's GC-rooted slots, NOT the
  // bench PrProblem's parse-time handles.  The PrProblem is not a GC
  // root, so pr->goals_l[]/pr->goal_l dangle after any mid-search GC:
  // they read back as garbage (typically a nullary cell) whose two
  // sides then look trivially "joined" at zero steps, which is exactly
  // why the extractor returned an empty chain and the hook printed
  // "no single-NF chain extractable" for GC-triggering proofs.
  // s->goals_lhs[]/s->goal_lhs relocate with the heap and are the only
  // valid goal terms at emit time.  A single goal is already the live
  // alias (goal_lhs == goals_lhs[0], MNF join state intact) so it is
  // used directly -- re-setting it would rebuild the goals array and
  // destroy the MNF front the extractor needs.  Multi-goal re-points
  // the alias per conjunct with atp_goals_alias (no array rebuild).
  u32 n_goals = (s->n_goals > 0u) ? s->n_goals
              : ((pr->n_goals > 0u) ? pr->n_goals : 1u);
  // Trace-pair cache: shared by citation resolution, the treatment
  // bridge, and the narrowing replay (cli_emit_goal_chain re-resets;
  // idempotent).
  cli_tp_cache_reset(s);
  for (u32 g = 0; g < n_goals; g++) {
    Term gl, gr;
    // Existential (narrowing-closed) goal: no single-NF chain exists
    // structurally; emit the recorded narrowing derivation instead.
    // Never touch the pr-> handles here (not GC roots) and never
    // thvm_atp_set_goal (it would clear the existential state).
    if (s->goal_existential) {
      if (cli_emit_narrow_proof(out, s, pr, g, printed)) continue;
      fprintf(out, "%% goal %u: no single-NF chain extractable\n", g);
      continue;
    }
    if (s->n_goals > 1u) {
      atp_goals_alias(s, g);
      gl = s->goal_lhs; gr = s->goal_rhs;
    } else if (s->n_goals == 1u) {
      gl = s->goal_lhs; gr = s->goal_rhs;
    } else {
      gl = (pr->n_goals > 0u) ? pr->goals_l[g] : pr->goal_l;
      gr = (pr->n_goals > 0u) ? pr->goals_r[g] : pr->goal_r;
      if (thvm_atp_set_goal(s, gl, gr) == 0u) continue;
    }
    u32 n = thvm_atp_proof_extract(s, steps, ATP_PROOF_MAX_STEPS);
    if (n == 0u) n = thvm_atp_mnf_proof_extract(s, steps, ATP_PROOF_MAX_STEPS);
    if (n == 0u && !(term_tag(gl) == term_tag(gr) && gl == gr)) {
      fprintf(out, "%% goal %u: no single-NF chain extractable\n", g);
      continue;
    }
    cli_emit_goal_chain(out, s, pr, gl, gr, g, steps, n, printed);
  }
  fprintf(out, "%% SZS output end %s for %s\n", szs_form, g_cli_problem);
  fclose(out);
  if (g_cli_n_unjust == 0u && g_cli_n_refuted > 0u) {
    if (cli_proof_renamed()) cli_proof_write(real_out, blk_buf, blk_len);
    else fwrite(blk_buf, 1, blk_len, real_out);
  } else if (g_cli_n_unjust > 0u) {
    fprintf(stderr, "FindProof: proof withheld: %s is not derived from "
            "what it cites\n", g_cli_unjust_what);
  } else {
    fprintf(stderr, "FindProof: proof withheld: no goal reaches $false\n");
  }
  free(blk_buf);
  fflush(real_out);
  free(printed);
  thvm_atp_rederive_end(s);
}

