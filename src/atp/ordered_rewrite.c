#if defined(ATP_ORDERED_REWRITE) || defined(ATP_MNF)
// === variable-occurrence helpers ====================================
// Shared by 9c ordered rewriting (variable-safe rewrite directions)
// and the Milestone-10 MNF search (variable-safe backward steps).

// Does variable id `id` occur in `t`?
static int atp_term_has_var(Term t, u32 id) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  int   hit = 0;
  for (;;) {
    switch (term_tag(t)) {
      case TAG_FVR:
        hit = (term_ext(t) == id);
        break;
      case TAG_CTR: {
        u32 n = term_ctr_n(t);
        while (nst + n > cst) {
          st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
        }
        for (u32 i = n; i-- > 0u;) {
          st[nst++] = term_ctr_at(t, i);
        }
        break;
      }
      default:
        break;
    }
    if (hit || nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return hit;
}
// Every variable of `a` also occurs in `b`?  A rewrite direction is
// variable-safe only when the result side's variables are contained
// in the matched side's -- else the rewrite would introduce variables.
static int atp_vars_contained(Term a, Term b) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  int   all = 1;
  for (;;) {
    switch (term_tag(a)) {
      case TAG_FVR:
        all = atp_term_has_var(b, term_ext(a));
        break;
      case TAG_CTR: {
        u32 n = term_ctr_n(a);
        while (nst + n > cst) {
          st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
        }
        for (u32 i = n; i-- > 0u;) {
          st[nst++] = term_ctr_at(a, i);
        }
        break;
      }
      default:
        break;
    }
    if (!all || nst == 0u) {
      break;
    }
    a = st[--nst];
  }
  term_stack_release(st, st_inl);
  return all;
}
#endif /* ATP_ORDERED_REWRITE || ATP_MNF */

#ifdef ATP_ORDERED_REWRITE
// === 9c-foundation: ordered rewriting ===============================
//
// Proper unfailing-completion rewriting.  The KBO_UN both-ways hack
// stored an unorientable equation u=v as two looping rules u->v and
// v->u.  Here an equation is stored once and the rewrite step tries
// every rule in BOTH directions, applying a direction only when the
// result strictly decreases the redex in the reduction order.  An
// oriented rule l->r (l > r, hence l.sigma > r.sigma for every sigma)
// fires forward only; an unorientable equation fires whichever
// direction is decreasing for the instance at hand.  Every rewrite
// strictly descends a well-founded order, so normalization terminates.

// One ordered rewrite at the top of `t`.
//
// An oriented rule (lhs[i] > rhs[i]) is decreasing for every instance,
// so it fires forward with NO order check and NO discarded `repl` --
// the same cost as the plain rewriter.  Only an unorientable equation
// pays the both-directions order-gated path: each direction is tried
// only when variable-safe and applied only when it strictly decreases
// `t`.  This fast path matters: without it, building a `repl` per
// failed order-check on every rule churns the heap catastrophically.
// Set by the mixed normalize loop: after the indexed (orientable)
// fixpoint, oriented rules cannot fire, so the linear step skips them
// and only tries the unorientable equations.
static u8 g_atp_skip_oriented = 0u;

// Forward decl -- defined with the FVI helpers near
// thvm_atp_orient_and_add.
static Term atp_grounded_instance(AtpState *s, Term t, Term lhs,
                                  Term min_const);

// === WM free-variable-instance ordered rewriting ====================
//
// Waldmeister rewrites with an unorientable equation even when the
// replacement side has EXTRA free variables: `GleichungsrichtungPasst`
// (INF/MatchOperationen.c:923-940) matches one direction, then fires
// with `TP_RechteSeiteUnfrei` -- the instance whose extension
// variables are bound to the minimal constant (`RechtsUnfreiErzeugen`,
// INF/RUndEVerwaltung.c:366-397) -- under the usual strict-decrease
// gate.  Soundness: to[extras -> c_min] = from is an instance of the
// universally quantified equation, so replacing sigma(from) by
// sigma(to[extras -> c_min]) is plain equational reasoning;
// termination stays with the per-rewrite KBO_GT gate.  WM gates the
// mechanism on a ground-total reduction order (`FreieVariablenOK`,
// INF/MatchOperationen.c:247: ORD_OrdnungTotal = SO_PraezedenzTotal
// for kbo/lpo, FALSE for every other ordering); mirror that by
// allowing KBO/LPO -- total precedence by KboConfig/LpoConfig
// contract -- and refusing WPO/RPO.
//
// Can direction from -> to fire at all?  Insertion-time filter for
// the unorientable-faces index.
static int atp_unorient_dir_usable(AtpState *s, Term from, Term to) {
  return atp_vars_contained(to, from)
      || (s->wpo == NULL && s->rpo == NULL);
}
// Replacement template for direction from -> to: `to` itself when the
// direction introduces no variables, otherwise the grounded WM
// free-variable instance (extras bound to s->min_const), or 0 when
// the direction is unusable (non-ground-total order).
static Term atp_unorient_template(AtpState *s, Term from, Term to) {
  if (atp_vars_contained(to, from)) return to;
  if (s->wpo != NULL || s->rpo != NULL) return 0;
  return atp_grounded_instance(s, to, from, s->min_const);
}

// Snapshot-array orientation memo: a normalize against a rule array
// OTHER than the live set (interreduce's new-rule slice, backward
// demodulation's snapshot) has no r_orient cache, so
// atp_ordered_try_top recomputed atp_compare(lhs[i], rhs[i]) at EVERY
// position of every subject -- the entire kbo_compare_core presence in
// the LAT138-1 / RNG021-6 profiles.  The verdict is a pure function of
// the two term VALUES under the run's fixed ordering, so memoize it on
// the (lhs, rhs) handle pair.  Direct-mapped; entries are stamped by
// the gc epoch (a collection can recycle handles) and by an init
// generation (one process can build many engines with different
// orderings -- test harnesses do), so a stale hit is impossible and
// every returned verdict is exactly the atp_compare the scan ran.
enum { ATP_ORIENT_MEMO_N = 2048 };
typedef struct { Term l, r; u64 epoch; u64 gen; u8 gt; u8 valid; } AtpOrientMemoEnt;
static AtpOrientMemoEnt g_atp_orient_memo[ATP_ORIENT_MEMO_N];

static u8 atp_orient_memo_gt(AtpState *s, Term l, Term r) {
  u64 h = ((u64)l * 0x9e3779b97f4a7c15ull) ^ ((u64)r * 0xff51afd7ed558ccdull);
  AtpOrientMemoEnt *e = &g_atp_orient_memo[(u32)(h >> 48) & (ATP_ORIENT_MEMO_N - 1u)];
  if (e->valid && e->epoch == g_atp_gc_epoch &&
      e->gen == g_atp_orient_memo_gen && e->l == l && e->r == r) {
    return e->gt;
  }
  u8 gt = (u8)(atp_compare(s, l, r) == KBO_GT);
  e->l = l;
  e->r = r;
  e->epoch = g_atp_gc_epoch;
  e->gen   = g_atp_orient_memo_gen;
  e->gt = gt;
  e->valid = 1u;
  return gt;
}

static u32 atp_symbol_count(Term t);
// Term twin of ftn_lex_dep_dirs (ft_norm.c): the LADR lex-dependent
// demodulator directions of an unorientable equation (bit0 l -> r,
// bit1 r -> l), 0 when it is not a lex-dependent demodulator.
static u8 atp_lex_dep_dirs(const AtpState *s, Term l, Term r) {
  u32 nl = atp_symbol_count(l), nr = atp_symbol_count(r);
  if (nl != nr || nl + nr + 1u > s->lex_dep_lim) return 0u;
  u8 d = 0u;
  if (term_tag(l) != TAG_FVR && atp_vars_contained(r, l)) d |= 1u;
  if (term_tag(r) != TAG_FVR && atp_vars_contained(l, r)) d |= 2u;
  return d;
}

static Term atp_ordered_try_top(AtpState *s, Term t,
                                const Term *lhs, const Term *rhs,
                                u32 n_rules, u8 *fired) {
  // A rule's orientation is fixed at creation; for the live rule set it
  // is cached in s->r_orient.  Recomputing it here -- a full KBO compare
  // per rule, per rewrite position -- was ~70% of the completion wall.
  // A custom rule array (interreduction's 2-rule set) is not the live
  // set, so it falls back to the direct compare.
  const u8 *orient = (lhs == s->lhs && rhs == s->rhs) ? s->r_orient : NULL;
  // Top-symbol fast-fail: a CTR pattern cannot match a non-CTR subject
  // (FVR / atom).  Saves the function-call + 16-binding stack-init cost
  // of thvm_match on the common non-matching case.
  u32 t_tag = term_tag(t);
  u32 t_sym = (t_tag == TAG_CTR) ? term_ext(t) : 0u;
  u32 t_n   = (t_tag == TAG_CTR) ? term_ctr_n(t) : 0u;
  // WM per-position redex priority (BL_RegelOderGleichungAngewendet,
  // NF/NFBildung.c:503-531; path-based NFB_ variant :219-238): at this
  // position the rule tree (Regelbaum) is consulted BEFORE the equation
  // tree (Gleichungsbaum) -- an unorientable equation fires only when
  // NO oriented rule matches here.  Pass 1: oriented rules, index order.
  if (!g_atp_skip_oriented) {   // skip flag: already at indexed fixpoint
    for (u32 i = 0; i < n_rules; i++) {
      u8 oriented = orient ? orient[i]
                           : atp_orient_memo_gt(s, lhs[i], rhs[i]);
      if (!oriented) continue;
      // Top-symbol pre-filter: rule lhs must agree on tag, label, arity
      // for thvm_match to succeed.  FVR pattern can match anything --
      // skip the filter then.  Eliminates the recursive entry on the
      // hot fail-fast path; the descent into children fires only on
      // genuine top matches.
      Term li = lhs[i];
      if (term_tag(li) == TAG_CTR) {
        if (t_tag != TAG_CTR || term_ext(li) != t_sym ||
            term_ctr_n(li) != t_n) continue;
      }
      // oriented rule -- forward only, no order check, no waste.
      RewriteSubst subst = {{0}};
      if (atp_match_maybe_ac(li, t, &subst)) {
        *fired = 1;
        return thvm_subst_apply(rhs[i], &subst);
      }
#ifdef THVM_ATP_AC
      // Bachmair-Plaisted extended rewriting: for an oriented AC-top
      // rule l -> r whose LHS doesn't match the whole goal as-is, try
      // the extended form l_ext = f(l, z) -> f(r, z).  z absorbs the
      // leftover AC leaves so a rule like m(x, x) -> x fires on a
      // goal m(a, m(a, b)) (AC-flat {a, a, b}) -- matches the
      // embedded {a, a} sub-multiset, returns the rebuilt term with
      // the remaining {b} intact.  Closes the AC subset-match gap
      // surfaced by tests/test_atp_ac_bench's bool-idem-embed.
      {
        u64 acm = thvm_atp_get_ac_mask();
        if (acm != 0ull
            && term_tag(li) == TAG_CTR
            && term_ext(li) < 64u
            && ((acm >> term_ext(li)) & 1ull) != 0ull
            && t_tag == TAG_CTR
            && term_ext(t) == term_ext(li)) {
          AtpAcInfo ac = { .ac_mask = acm };
          Term ext_li = 0, ext_ri = 0;
          if (atp_ac_extend_rule(li, rhs[i], &ac, &ext_li, &ext_ri)) {
            RewriteSubst esubst = {{0}};
            if (atp_match_maybe_ac(ext_li, t, &esubst)) {
              *fired = 1;
              return thvm_subst_apply(ext_ri, &esubst);
            }
          }
        }
      }
#endif
    }
  }
  // Pass 2: unorientable equations -- reached only when no oriented
  // rule matched at this position.
  for (u32 i = 0; i < n_rules; i++) {
    u8 oriented = orient ? orient[i]
                         : atp_orient_memo_gt(s, lhs[i], rhs[i]);
    if (oriented) continue;
    // Unorientable equation -- both directions, order-gated.  Match
    // FIRST, then pick the replacement template: at most positions the
    // rule has no redex, so the cheap fail-fast thvm_match avoids the
    // full-term atp_unorient_template walk (vars-containment is a
    // measured hot leaf), and the template does not depend on the
    // redex.  A direction whose replacement side carries extension
    // variables fires with the grounded WM free-variable instance.
    Term li = lhs[i];
    Term ri = rhs[i];
    u8 ld = 3u;
    if (s->lex_dep_lim != 0u) {
      ld = atp_lex_dep_dirs(s, li, ri);
      if (ld == 0u) continue;
    }
    if (!(ld & 1u) ||
        (term_tag(li) == TAG_CTR &&
         (t_tag != TAG_CTR || term_ext(li) != t_sym ||
          term_ctr_n(li) != t_n))) {
      // l side cannot match -- skip both directions' l->r path; the
      // r->l path may still apply if its top (term_tag(ri)) matches.
    } else {
      RewriteSubst subst = {{0}};
      if (atp_match_maybe_ac(li, t, &subst)) {             // l -> r
        Term tmpl = atp_unorient_template(s, li, ri);
        if (tmpl != 0) {
          Term repl = thvm_subst_apply(tmpl, &subst);
          if (atp_compare(s, t, repl) == KBO_GT) { *fired = 1; return repl; }
        }
      }
    }
    if (!(ld & 2u) ||
        (term_tag(ri) == TAG_CTR &&
         (t_tag != TAG_CTR || term_ext(ri) != t_sym ||
          term_ctr_n(ri) != t_n))) {
      continue;
    }
    {
      RewriteSubst subst = {{0}};
      if (atp_match_maybe_ac(ri, t, &subst)) {             // r -> l
        Term tmpl = atp_unorient_template(s, ri, li);
        if (tmpl != 0) {
          Term repl = thvm_subst_apply(tmpl, &subst);
          if (atp_compare(s, t, repl) == KBO_GT) { *fired = 1; return repl; }
        }
      }
    }
  }
  *fired = 0;
  return t;
}

// One outermost-leftmost ordered rewrite anywhere in `t`: tries the
// top, else descends into TAG_CTR children left-to-right.
static Term atp_ordered_rewrite_step(AtpState *s, Term t,
                                     const Term *lhs, const Term *rhs,
                                     u32 n_rules, u8 *fired) {
  // One frame per open CTR (`i` = the child being stepped): the first
  // child that fires is rebuilt into its parent, which returns fired in
  // turn; a quiet child passes to the next sibling.
  AtpWalkFrame  fr_inl[ATP_WALK_INL];
  AtpWalkFrame *fr  = fr_inl;
  u32           nfr = 0u;
  u32           cfr = ATP_WALK_INL;
  Term          res;
  u8            hit;
  for (;;) {
    // DIAG (THVM_ATP_NORMLOG): trip when a normalize subject grows large, to
    // locate the rule-250 lazy-normalize blowup (grow-per-step vs many-steps).
    {
      static int dl = -1; static u32 mx = 0u;
      if (dl < 0) {
        dl = (getenv("THVM_ATP_NORMLOG") != NULL) ? 1 : 0;
      }
      if (dl) { u32 sz = atp_symbol_count(t);
        if (sz > mx && sz > 5000u) { mx = sz;
          fprintf(stderr, "NORMLOG t_size=%u\n", sz); } }
    }
    hit = 0;
    res = atp_ordered_try_top(s, t, lhs, rhs, n_rules, &hit);
    if (!hit) {
      u32 n = (term_tag(t) == TAG_CTR) ? term_ctr_n(t) : 0u;
      if (n > 0u && n <= REWRITE_MAX_ARITY) {
        if (nfr == cfr) {
          fr = (AtpWalkFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].t    = t;
        fr[nfr].base = term_val(t);
        fr[nfr].i    = 0u;
        fr[nfr].n    = n;
        nfr++;
        t = heap_read(term_val(t) + 1u);
        continue;
      }
      res = t;
    }
    for (;;) {
      if (nfr == 0u) {
        term_stack_release(fr, fr_inl);
        *fired = hit;
        return res;
      }
      AtpWalkFrame *f = &fr[nfr - 1u];
      if (hit) {
        Term children[REWRITE_MAX_ARITY];
        for (u32 j = 0; j < f->n; j++) {
          children[j] = (j == f->i) ? res : heap_read(f->base + 1u + (u64)j);
        }
        res = term_new_ctr(term_ext(f->t), children, f->n);
        nfr--;
        continue;
      }
      f->i++;
      if (f->i < f->n) {
        t = heap_read(f->base + 1u + (u64)f->i);
        break;
      }
      res = f->t;
      nfr--;
    }
  }
}

// Ordered normalization to fixpoint.  step_cap is a safety bound only
// -- ordered rewriting genuinely terminates.
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Forward decl: defined far below (with atp_cp_trivially_joinable).  Needed
// here so the use_mixmost_nf reroute can repopulate the FT mirror on demand.
static inline void atp_ft_mirror_ensure(AtpState *s);
#endif
// Forward decl: defined far below.  Needed here so the lazy (pop-time)
// ground-joinability drop in thvm_atp_step can call it before its
// definition (the push-side caller sits after it).
static int atp_cp_ground_joinable(AtpState *s, Term lhs, Term rhs);
// Forward decl: the Twee Join.hs pop-time ground-joinability case-split
// (defined far below with the gj machinery).  The pop-time keep/drop
// gate in thvm_atp_step calls it before its definition.
static u8 atp_pop_gj_joinable(AtpState *s, Term l, Term r);
static Term atp_rewrite_normalize_ordered(AtpState *s, Term t,
                                          const Term *lhs, const Term *rhs,
                                          u32 n_rules, u32 step_cap) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // use_mixmost_nf (the WM "Waldmeister" preset) demands WM's innermost
  // NormalformInnermost order at EVERY normalize site.  The Term-side
  // scan below is outermost-leftmost (top-first); that order divergence
  // both over-joins critical pairs WM keeps (the goal-closer is lost) and
  // over-generates ones WM joins (the queue explodes to millions).  Route
  // the live-rule-set call through the FT mixmost normalizer -- the same
  // one the goal-check uses -- so the whole engine matches WM byte-for-
  // byte.  Only the live rule set has an FT mirror, so snapshot calls
  // (different lhs/rhs/n_rules) fall through to the Term path unchanged.
  if (s->use_mixmost_nf && lhs == s->lhs && rhs == s->rhs &&
      n_rules == s->n_rules) {
    atp_ft_mirror_ensure(s);
    AtpFt *a = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *fc = atp_rewrite_normalize_ft(s, ft_from_term(a, t, 0),
                                             step_cap);
    return ft_to_term(fc);
  }
#endif
#ifdef ATP_RULE_INDEX
  // When EVERY rule in R is KBO-oriented (lhs > rhs), every forward
  // rewrite is order-decreasing, so the discrimination-tree normalizer
  // is an exact equivalent of the linear ordered scan (same outermost-
  // leftmost redex, same lowest-index rule) and replaces the per-
  // position O(n_rules) thvm_match scan with a tree descent.  An
  // unorientable equation, while present in R, drops to the linear
  // path (where both rewrite directions are order-gated).  `n_unorient`
  // is a live COUNT, not a sticky flag: a transient unorientable rule
  // -- interreduced away or re-oriented after reduction -- restores the
  // indexed path the moment R is orientable again.
  if (lhs == s->lhs && rhs == s->rhs && n_rules == s->n_rules) {
#ifdef THVM_ATP_AC
    // Under a non-zero AC mask, route through the pure linear
    // ordered_rewrite_step path (the outermost fallback below).
    // Reasons:
    //   - The indexed / flatterm / wmfpa fast paths key their
    //     discrim-tree descents on syntactic preorder and would
    //     miss AC-permuted redexes.
    //   - The "mixed loop" path runs atp_rewrite_normalize_indexed
    //     interleaved with ordered_rewrite_step under
    //     g_atp_skip_oriented = 1, which BYPASSES the oriented-
    //     branch AC match in atp_ordered_try_top.
    // So the AC path takes the linear ordered step directly.  The
    // slowdown applies only when ac_mask != 0; default builds and
    // ac_mask = 0 take the original fast paths unchanged.
    if (thvm_atp_get_ac_mask() != 0ull) {
      for (u32 i = 0; i < step_cap; i++) {
        if (atp_norm_deadline_fired(s)) return t;
        u8 fired = 0;
        Term t2 = atp_ordered_rewrite_step(s, t, lhs, rhs, n_rules, &fired);
        if (!fired) break;
        t = t2;
      }
      return t;
    }
#endif
    if (s->n_unorient == 0u) {
      // Iter 133 step 2: keep the subject in flat form when use_flatterm
      // is on.  flatterm_mixed runs indexed_fixpoint + (no-op) unorient
      // step, returning the same NF as the indexed path but avoiding the
      // per-call tree-flatten + rebuild round-trip.
      if (s->use_flatterm)
        return atp_rewrite_normalize_flatterm_mixed(s, t, step_cap);
      // Dormant Term-side WM-FPA path (leftmost-outermost, lowest-slot
      // policy: wf_normalize / wf_descend).  Re-gated from
      // THVM_ATP_WMFPA to THVM_ATP_WMFPA_TERM when that flag was
      // repointed at the WM-policy FT hook (ft_norm.c
      // atp_rules_only_normalize_wf) -- one flag must never arm two
      // different redex policies.
      {
        static int wmfpa_term = -1;
        if (wmfpa_term < 0) wmfpa_term = atp_env_on("THVM_ATP_WMFPA_TERM");
        if (wmfpa_term) return atp_rewrite_normalize_wmfpa(s, t, step_cap);
      }
      return atp_rewrite_normalize_indexed(s, t, step_cap);
    }
    // Opt-in flatterm fast-path: keep the subject flat across both the
    // orientable indexed fixpoint and the unorientable pass (no per-step
    // re-flatten / tree rebuild).  Same normal form as the tree loop
    // below; default OFF so the engine is byte-identical.
    if (s->use_flatterm) {
#if defined(ATP_FLATTERM_SELFCHECK) || defined(ATP_FLATTERM_DIFF)
      // In-engine differential: also run the tree path and check every
      // LIVE-saturation normalize for flat-NF == tree-NF.  Build-only
      // (defeats the speedup).  Under SELFCHECK a mismatch aborts (the
      // build-time soundness invariant); under the DIFF test build it
      // bumps g_atp_ft_diff_mism so a test can CHECK it == 0.  This fires
      // on the REAL subjects saturation reduces -- the deep critical-pair
      // terms the offline random battery never constructs.
      {
        Term ref = atp_rewrite_normalize_flatterm_selfcheck_tree(s, t, step_cap);
        Term got = atp_rewrite_normalize_flatterm_mixed(s, t, step_cap);
        if (!kbo_eq(ref, got)) {
          char ib[2048], rb[2048], gb[2048];
          atp_pretty_term(t, ib, sizeof ib);
          atp_pretty_term(ref, rb, sizeof rb);
          atp_pretty_term(got, gb, sizeof gb);
          fprintf(stderr, "FLATTERM SELFCHECK MISMATCH\n in=%s\n tree=%s\n flat=%s\n",
                  ib, rb, gb);
          fprintf(stderr, " n_rules=%u n_unorient=%u\n", s->n_rules, s->n_unorient);
#ifdef ATP_FLATTERM_SELFCHECK
          for (u32 ri = 0; ri < s->n_rules; ri++) {
            char la[1024], ra[1024];
            atp_pretty_term(s->lhs[ri], la, sizeof la);
            atp_pretty_term(s->rhs[ri], ra, sizeof ra);
            fprintf(stderr, "  R%u%s: %s = %s\n", ri,
                    s->r_orient[ri] ? "" : "(un)", la, ra);
          }
          abort();   // build-time invariant: flatterm NF == tree NF
#else
          // DIFF test build: record the divergence.  Return the FLAT result
          // (got) -- the same value the un-instrumented engine uses -- so
          // saturation follows the EXACT trajectory the live engine does,
          // reaching the same deep critical pairs where the staleness bites.
          // A correct flat path has got == ref, so the trajectory is the
          // tree-correct one and g_atp_ft_diff_mism stays 0.  The test
          // CHECKs g_atp_ft_diff_mism == 0.
          g_atp_ft_diff_mism++;
#endif
        }
        return got;
      }
#endif
      return atp_rewrite_normalize_flatterm_mixed(s, t, step_cap);
    }
    // Mixed rule set: the discrimination tree (orientable rules only)
    // normalizes every orientable rewrite in one fast descent; the few
    // unorientable equations are then applied one outermost, KBO-gated
    // step at a time, re-running the indexed pass between.  After an
    // indexed fixpoint no orientable rule fires, so the linear step
    // finds an unorientable rewrite (or none = done).  Replaces the
    // O(n_rules) linear scan per rewrite that dominated completion on a
    // self-overlapping axiom.
    for (u32 i = 0; i < step_cap; i++) {
      if (atp_norm_deadline_fired(s)) return t;
      // Iterate the indexed orientable fixpoint to convergence -- a single
      // atp_rewrite_normalize_indexed call exits at step_cap regardless of
      // whether the inner loop reached fixpoint, so when the term needs
      // more than step_cap orientable rewrites the caller observed an
      // incomplete NF.  Call again until the term stops changing.  Bounded
      // by the outer step_cap so total orientable work stays
      // O(step_cap * step_cap) -- matches the previous loose bound.
      for (u32 j = 0; j < step_cap; j++) {
        Term t_in = t;
        t = atp_rewrite_normalize_indexed(s, t, step_cap);
        if (kbo_eq(t, t_in)) break;     // true fixpoint
      }
      u8 fired = 0;
      Term t2;
      if (s->use_unorient_index) {
        // Indexed unorientable step: a discrimination-tree retrieval over
        // both faces of every unorientable equation replaces the O(n_rules)
        // linear KBO-gated scan.  Byte-identical redex -> identical NF.
        t2 = atp_unorient_step_indexed(s, t, &fired);
      } else {
        g_atp_skip_oriented = 1u;
        t2 = atp_ordered_rewrite_step(s, t, lhs, rhs, n_rules, &fired);
        g_atp_skip_oriented = 0u;
      }
      if (!fired) break;
      t = t2;
    }
    return t;
  }
#endif
  for (u32 i = 0; i < step_cap; i++) {
    if (atp_norm_deadline_fired(s)) return t;
    u8 fired = 0;
    Term t2 = atp_ordered_rewrite_step(s, t, lhs, rhs, n_rules, &fired);
    if (!fired) break;
    t = t2;
  }
  return t;
}
#endif /* ATP_ORDERED_REWRITE */

// Total symbol count: TAG_FVR / atoms count as 1; TAG_CTR counts
// itself + the symbols of its children.  This is the "size" used
// by Waldmeister's `--add` heuristic in `ClasHeuristics.c`
// ("classification heuristics") -- the simplest CP-priority
// function: cheapest-by-size wins.
static u32 atp_symbol_count(Term t) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  u32   c   = 0u;
  for (;;) {
    c++;   // one per node: FVR / atoms / NUM / CTR head
    if (term_tag(t) == TAG_CTR) {
      u32 n = term_ctr_n(t);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = 0; i < n; i++) {
        st[nst++] = term_ctr_at(t, i);
      }
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return c;
}

// Capped variant: accumulate `t`'s node count into *acc, stopping the
// walk as soon as *acc reaches `cap`.  For a threshold test (is the
// count < cap?) the verdict is identical to the full count -- a capped
// result >= cap iff the true count is -- while an over-cap giant pays
// O(cap) instead of O(|t|).
static void atp_symbol_count_upto(Term t, u32 cap, u32 *acc) {
  // Preorder count that stops for good once *acc reaches cap.
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  while (*acc < cap) {
    (*acc)++;
    if (term_tag(t) == TAG_CTR) {
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
}

// Same node count as atp_symbol_count, but with Twee's shared-subterm
// discount (src/Twee/CP.hs:259-261, cfg_dupcost=1, cfg_dupfactor=0):
// each unique Term value counts once at full weight; repeat occurrences
// count 1 and the recursion stops -- so a term f(g(a), g(a)) counts
// {f, g(a), a} + 1 for the second g(a) = 4 instead of 5.  An iterative
// walk with a per-call open-addressed Term hash set (capacity 512 -- a
// CP face larger than ~500 nodes would have walled the saturation long
// before reaching this weight).  An overflow into the 513th distinct
// term falls back to counting that node without dedup.
static u32 atp_symbol_count_dedup(Term t) {
  Term stack[256];
  Term seen[512];
  for (u32 i = 0; i < 512; i++) seen[i] = 0;
  u32 sp = 0;
  if (sp < 256) stack[sp++] = t;
  u32 count = 0;
  while (sp > 0) {
    Term cur = stack[--sp];
    // Probe the seen-set with an inlined splitmix64 finalizer over
    // the Term's u64 representation.
    u64 x = (u64)cur;
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    u32 h = (u32)x & 511u;
    u8 dup = 0;
    for (u32 step = 0; step < 512u; step++) {
      u32 idx = (h + step) & 511u;
      if (seen[idx] == 0) { seen[idx] = cur; break; }
      if (seen[idx] == cur) { dup = 1; break; }
    }
    if (dup) {
      count += 1u;            // cfg_dupcost = 1; do not recurse.
      continue;
    }
    count += 1u;              // this node's own weight.
    if (term_tag(cur) == TAG_CTR) {
      u32 n = term_ctr_n(cur);
      for (u32 i = 0; i < n && sp < 256; i++) {
        stack[sp++] = term_ctr_at(cur, i);
      }
    }
  }
  return count;
}

// Twee's exact per-term size (Twee.CP.termScore, src/Twee/CP.hs:249).
// Differs from atp_symbol_count_dedup in TWO ways that matter for the
// Twee CP score's selection order:
//   1. varweight = 6/7 (a variable is cheaper than a function symbol),
//      not 1.  We scale the whole score by 7 to stay integer: a variable
//      contributes 6, a function symbol / constant 7.
//   2. the shared-subterm discount (cfg_dupcost=1) fires ONLY for
//      COMPOUND subterms (Twee's `len t > 1`), NEVER for a repeated
//      variable or constant -- those are counted in full every time.
//      atp_symbol_count_dedup deduped every repeat, underweighting terms
//      with repeated constants (e.g. the median goal's skolem u/w x3).
// Caller multiplies the depth term by 7 to keep 2*depth's proportion.
static u64 g_twee_dbg_calls = 0, g_twee_dbg_dups = 0, g_twee_dbg_drops = 0;
static u8  g_twee_dbg_ctx = 0;   // 0=other 1=insert-treated 2=insert-raw 3=rebuild/IR
static u64 g_twee_dbg_ctx_calls[4], g_twee_dbg_ctx_dups[4];
static void atp_twee_dbg_report(void) {
  fprintf(stderr, "[twee-score] calls=%llu dups=%llu stack-drops=%llu\n",
          (unsigned long long)g_twee_dbg_calls,
          (unsigned long long)g_twee_dbg_dups,
          (unsigned long long)g_twee_dbg_drops);
  static const char *nm[4] = {"other", "insert-treated", "insert-raw",
                              "rebuild"};
  for (u32 i = 0; i < 4; i++) {
    fprintf(stderr, "[twee-score]   %-14s calls=%llu dups=%llu\n", nm[i],
            (unsigned long long)g_twee_dbg_ctx_calls[i],
            (unsigned long long)g_twee_dbg_ctx_dups[i]);
  }
}
// --- STRUCTURAL shared-subterm discount (THVM_ATP_TWEE_SD) ----------
// Twee's size' dedup is STRUCTURAL: an occurrence of a compound
// subterm costs dupcost=1 when an identical copy occurs anywhere in
// the REMAINING traversal (Twee.CP.hs:259-261, `t isSubtermOfList ts
// || any (t isSubtermOfList) us` -- a deep-subterm containment test).
// The pointer-identity dedup below only fires when equal subterms
// share heap cells, which substitution-materialized CPs (fresh cells
// per sigma image) never do -- so on the FT-direct treated route the
// discount is structurally ABSENT and every CP whose conclusion
// repeats an instantiated condition (the condensed-detachment
// discharge shape: LCL927-10's B-permutation cascade repeats i(Z,Y)
// x3 and the instantiated condition x2) is over-taxed by
// lhsweight * (size(dup) - 1) per lost dup.  Measured on the LCL927
// frontier CP: pri 791 pointer-mode vs 571 structural-mode -- the
// difference between buried-forever and inside the selection band.
// Opt-in knob (--twee-struct-dedup): default OFF keeps every
// existing mode-8 lane byte-identical.
static u8 atp_twee_sd_on(void) {
  static int on = -1;
  if (on < 0) on = atp_env_on("THVM_ATP_TWEE_SD");
  return (u8)on;
}

// Deep structural containment: does needle occur as a subterm of hay?
// Both are TAG_CTR trees; equality is label + arity + children.
static u8 atp_twee_sd_eq(Term a, Term b) {
  typedef struct { Term a; Term b; } SdPair;
  SdPair  st_inl[ATP_WALK_INL];
  SdPair *st  = st_inl;
  u32     nst = 0u;
  u32     cst = ATP_WALK_INL;
  u8      eq  = 1u;
  for (;;) {
    if (a != b) {
      u32 ta = term_tag(a), tb = term_tag(b);
      if (ta != tb) {
        eq = 0u;
      } else if (ta == TAG_FVR || ta == TAG_NUM) {
        eq = (u8)(term_ext(a) == term_ext(b) && term_val(a) == term_val(b));
      } else if (ta != TAG_CTR || term_ext(a) != term_ext(b)) {
        eq = 0u;
      } else {
        u32 n = term_ctr_n(a);
        if (n != term_ctr_n(b)) {
          eq = 0u;
        } else {
          while (nst + n > cst) {
            st = (SdPair *)term_stack_grow(st, st_inl, &cst, sizeof *st);
          }
          for (u32 i = n; i-- > 0u;) {
            st[nst].a = term_ctr_at(a, i);
            st[nst].b = term_ctr_at(b, i);
            nst++;
          }
        }
      }
    }
    if (!eq || nst == 0u) {
      break;
    }
    nst--;
    a = st[nst].a;
    b = st[nst].b;
  }
  term_stack_release(st, st_inl);
  return eq;
}
static u8 atp_twee_sd_contains(Term hay, Term needle) {
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  u8    hit = 0u;
  for (;;) {
    if (atp_twee_sd_eq(hay, needle)) {
      hit = 1u;
      break;
    }
    if (term_tag(hay) == TAG_CTR) {
      u32 n = term_ctr_n(hay);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) {
        st[nst++] = term_ctr_at(hay, i);
      }
    }
    if (nst == 0u) {
      break;
    }
    hay = st[--nst];
  }
  term_stack_release(st, st_inl);
  return hit;
}

// Twee size' with the structural discount, literal port: preorder
// worklist; a compound costs 7 and is NOT expanded when it occurs
// (deeply) in any remaining worklist entry; otherwise costs 7 and
// expands.  Vars cost 6, leaves 7 (the *7 scaling of the caller).
// Worklist order = Twee's (Cons t ts) us: children pushed so the
// leftmost pops first.
static u32 atp_twee_term_score_sd(Term t) {
  Term work[256];
  u32 wn = 0;
  work[wn++] = t;
  u32 score = 0;
  while (wn > 0) {
    Term cur = work[--wn];
    if (term_tag(cur) != TAG_CTR || term_ctr_n(cur) == 0u) {
      score += (term_tag(cur) == TAG_FVR) ? 6u : 7u;
      continue;
    }
    u8 dup = 0;
    for (u32 i = 0; i < wn; i++) {
      if (atp_twee_sd_contains(work[i], cur)) { dup = 1; break; }
    }
    score += 7u;
    if (dup) continue;                    // dupcost=1 (*7), no recurse
    u32 n = term_ctr_n(cur);
    for (u32 i = n; i > 0 && wn < 256; i--) {
      work[wn++] = term_ctr_at(cur, i - 1u);   // leftmost child pops first
    }
  }
  return score;
}

// --- Twee hasEqualsBonus (THVM_ATP_TWEE_EQB) ------------------------
// Twee.CP.hs:262-269 (the equals-bonus case of size'): when the score
// walk reaches f(a, b, ...) where f carries the bonus flag
// (SequentialMain.hs:515-518 hasEqualsBonus = con_bonus;
// SequentialMain.hs:554-556 bonus = labeled ifeq, or labeled equals at
// arity 2 -- the prover's OWN equals/ifeq machinery), a and b are both
// non-variables, and unify(a, b) = sub exists, the term scores
//     min( weight f + size' ws xs             -- the normal expansion
//        , 1 + size' (sub us) (sub (vs:xs)) ) -- the DISCHARGED branch
// (ws = f's argument list, us = the arguments after a and b, vs = the
// remaining siblings, xs = the outer worklist).  The discharged branch
// prices the equation as if the near-solved condition a=b were already
// closed: pay dupcost 1, drop a and b entirely, and continue over the
// REMAINder instantiated with the unifier -- the goal-directed
// accelerant for the equals encoding, where eq(s,t) = efalse CPs with
// unifiable s,t are exactly the almost-proved goal instances.  Note
// the reference drops vs in the normal branch (ws continues into xs,
// not vs:xs) -- ported verbatim, quirk included.
//
// Symbol set: Twee keys the flag on jukebox name LABELS, which only
// its own encoding machinery carries -- file-level ifeq symbols in the
// TPTP -10 encodings are unlabeled and never get the bonus (verified
// against SequentialMain.hs:540-557 + jukebox Name.hs label
// mechanics).  Our analog: the CLI registers the equals-goal
// encoding's fresh eq symbol (tools/atp_cli/def_unfold.c
// tp_equals_goal) and name-keyed ifeq*/equals heads by exact shape --
// see the registration site in tests/atp/test_atp_wolfram_bench.c.  Note
// the 4-ary -10 ifeq guards compare a `>=`-rooted guard against the
// constant true, which NEVER unify, so the bonus is structurally
// inert there (measured on LCL890-10) -- arity-2 eq is the live case.
//
// Opt-in (--twee-eq-bonus): default OFF keeps every lane
// byte-identical.  The bonus scorer embeds the STRUCTURAL dup
// discount (both live in the same size' in the reference), so
// --twee-eq-bonus supersets --twee-struct-dedup.
static u32 g_atp_eqb_syms[16];
static u32 g_atp_eqb_nsyms = 0u;
fn void atp_twee_eqb_reset(void) { g_atp_eqb_nsyms = 0u; }
fn void atp_twee_eqb_mark(u32 sym) {
  for (u32 i = 0; i < g_atp_eqb_nsyms; i++)
    if (g_atp_eqb_syms[i] == sym) return;
  if (g_atp_eqb_nsyms < 16u) g_atp_eqb_syms[g_atp_eqb_nsyms++] = sym;
}
static u8 atp_twee_eqb_has(u32 sym) {
  for (u32 i = 0; i < g_atp_eqb_nsyms; i++)
    if (g_atp_eqb_syms[i] == sym) return 1u;
  return 0u;
}
// Env-only gate: with an EMPTY symbol set the bonus case never fires
// and the scorer degenerates to the structural-discount walk, so
// --twee-eq-bonus supersets --twee-struct-dedup COHERENTLY -- gating
// on a non-empty set instead would flip pre-registration scoring
// (axiom intake) and bonus-free problems back to the CLASSIC pointer
// scorer, forking the trajectory at the intake (measured: LCL927-10
// axiom pri 319 vs 271, pick-3 fork).
static u8 atp_twee_eqb_on(void) {
  static int on = -1;
  if (on < 0) on = atp_env_on("THVM_ATP_TWEE_EQB");
  return (u8)on;
}

#define ATP_EQB_CUR_CAP    256u
#define ATP_EQB_DEPTH_CAP  8u

typedef struct AtpEqbXs {
  const Term *items;            // list order: items[0] is consumed first
  u32 n;
  const struct AtpEqbXs *next;
} AtpEqbXs;

// Recursive size' over (current list as a stack, outer worklist chain).
// cur[ncur-1] is the leftmost (next-consumed) entry.  Recursion depth
// = nested bonus branches only (capped at ATP_EQB_DEPTH_CAP; past the
// cap a bonus node scores as a plain symbol -- deterministic,
// content-only).  Caps mirror the classic scorer's 256-entry stack.
static u32 atp_eqb_score(Term *cur, u32 ncur, const AtpEqbXs *xs, u32 depth) {
  u32 n = 0;
  for (;;) {
    if (ncur == 0u) {
      if (xs == NULL) return n;
      u32 m = xs->n;
      if (m > ATP_EQB_CUR_CAP) m = ATP_EQB_CUR_CAP;
      for (u32 i = 0; i < m; i++) cur[i] = xs->items[m - 1u - i];
      ncur = m;
      xs = xs->next;
      continue;
    }
    Term t = cur[--ncur];
    if (term_tag(t) != TAG_CTR || term_ctr_n(t) == 0u) {
      n += (term_tag(t) == TAG_FVR) ? 6u : 7u;
      continue;
    }
    // (1) structural shared-subterm discount (`t isSubtermOfList ts
    //     || any (t isSubtermOfList) us`, CP.hs:259-261)
    u8 dup = 0;
    for (u32 i = 0; i < ncur && !dup; i++)
      if (atp_twee_sd_contains(cur[i], t)) dup = 1;
    for (const AtpEqbXs *e = xs; e != NULL && !dup; e = e->next)
      for (u32 i = 0; i < e->n && !dup; i++)
        if (atp_twee_sd_contains(e->items[i], t)) dup = 1;
    if (dup) { n += 7u; continue; }       // dupcost=1 (*7), no recurse
    u32 nc = term_ctr_n(t);
    // (2) the equals bonus (CP.hs:262-269)
    if (nc >= 2u && depth < ATP_EQB_DEPTH_CAP &&
        atp_twee_eqb_has(term_ext(t))) {
      Term a = term_ctr_at(t, 0);
      Term b = term_ctr_at(t, 1);
      if (term_tag(a) != TAG_FVR && term_tag(b) != TAG_FVR) {
        RewriteSubst mg;
        memset(&mg, 0, sizeof mg);
        if (thvm_unify(a, b, &mg)) {
          // Branch 1: weight f + f's own arguments, outer worklist
          // kept, current-list siblings dropped (reference verbatim).
          Term c1[ATP_EQB_CUR_CAP];
          u32 n1 = (nc < ATP_EQB_CUR_CAP) ? nc : ATP_EQB_CUR_CAP;
          for (u32 i = 0; i < n1; i++) c1[i] = term_ctr_at(t, n1 - 1u - i);
          u32 s1 = 7u + atp_eqb_score(c1, n1, xs, depth + 1u);
          // Branch 2: dupcost 1, drop a and b, continue over the
          // instantiated remainder: sub(us) then sub(vs) :: sub(xs).
          Term c2[ATP_EQB_CUR_CAP];
          u32 n2 = 0;
          for (u32 i = nc; i-- > 2u;)
            c2[n2++] = thvm_unify_apply(term_ctr_at(t, i), &mg);
          Term vsub[ATP_EQB_CUR_CAP];
          u32 nvs = 0;
          for (u32 i = ncur; i-- > 0u;)
            vsub[nvs++] = thvm_unify_apply(cur[i], &mg);
          Term xbuf[ATP_EQB_DEPTH_CAP][ATP_EQB_CUR_CAP];
          AtpEqbXs xnode[ATP_EQB_DEPTH_CAP + 1u];
          xnode[0].items = vsub;
          xnode[0].n     = nvs;
          xnode[0].next  = NULL;
          u32 nx = 1u;
          AtpEqbXs *tail = &xnode[0];
          for (const AtpEqbXs *e = xs;
               e != NULL && nx <= ATP_EQB_DEPTH_CAP; e = e->next) {
            u32 m = (e->n < ATP_EQB_CUR_CAP) ? e->n : ATP_EQB_CUR_CAP;
            for (u32 i = 0; i < m; i++)
              xbuf[nx - 1u][i] = thvm_unify_apply(e->items[i], &mg);
            xnode[nx].items = xbuf[nx - 1u];
            xnode[nx].n     = m;
            xnode[nx].next  = NULL;
            tail->next = &xnode[nx];
            tail = &xnode[nx];
            nx++;
          }
          u32 s2 = 7u + atp_eqb_score(c2, n2, &xnode[0], depth + 1u);
          return n + ((s1 < s2) ? s1 : s2);
        }
      }
    }
    // (3) plain symbol: weight f, children spliced into the current
    //     list (ConsSym rest = args ++ siblings)
    n += 7u;
    for (u32 i = nc; i-- > 0u && ncur < ATP_EQB_CUR_CAP;)
      cur[ncur++] = term_ctr_at(t, i);
  }
}

static u32 atp_twee_term_score_eqb(Term t) {
  Term cur[ATP_EQB_CUR_CAP];
  cur[0] = t;
  return atp_eqb_score(cur, 1u, NULL, 0u);
}

static u32 atp_twee_term_score(Term t) {
  if (atp_twee_eqb_on()) return atp_twee_term_score_eqb(t);
  if (atp_twee_sd_on()) return atp_twee_term_score_sd(t);
  static int dbg = -1;
  if (dbg < 0) {
    dbg = atp_env_on("THVM_TWEE_SCORE_STATS");
    if (dbg) atexit(atp_twee_dbg_report);
  }
  if (dbg) { g_twee_dbg_calls++; g_twee_dbg_ctx_calls[g_twee_dbg_ctx]++; }
  Term stack[256];
  Term seen[512];
  for (u32 i = 0; i < 512; i++) seen[i] = 0;
  u32 sp = 0;
  if (sp < 256) stack[sp++] = t;
  u32 score = 0;
  while (sp > 0) {
    Term cur = stack[--sp];
    if (term_tag(cur) != TAG_CTR || term_ctr_n(cur) == 0u) {
      // Variable (FVR) -> varweight 6; constant / atom -> weight 7.
      // Neither is ever deduped (Twee dedups only `len t > 1`).
      score += (term_tag(cur) == TAG_FVR) ? 6u : 7u;
      continue;
    }
    // Compound subterm: dupcost=1 (scaled 7) if seen before, no recurse.
    u64 x = (u64)cur;
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    u32 h = (u32)x & 511u;
    u8 dup = 0;
    for (u32 step = 0; step < 512u; step++) {
      u32 idx = (h + step) & 511u;
      if (seen[idx] == 0) { seen[idx] = cur; break; }
      if (seen[idx] == cur) { dup = 1; break; }
    }
    if (dup) {
      score += 7u;   // cfg_dupcost=1 *7, no recurse
      if (dbg) { g_twee_dbg_dups++; g_twee_dbg_ctx_dups[g_twee_dbg_ctx]++; }
      continue;
    }
    score += 7u;                          // the function symbol (weight 1)
    u32 n = term_ctr_n(cur);
    for (u32 i = 0; i < n; i++) {
      if (sp >= 256) { if (dbg) g_twee_dbg_drops++; break; }
      stack[sp++] = term_ctr_at(cur, i);
    }
  }
  return score;
}

// Combine the two per-side Twee term scores and the CP formation depth
// into the final Twee.CP priority (Twee.CP.score, src/Twee/CP.hs:240):
//   lhsweight * size(larger) + rhsweight * size(smaller) + depthweight * depth
// Twee's built-in defaults are lhsweight=4, rhsweight=1, depthweight=2.
// `large`/`small` arrive already *7-scaled (atp_twee_term_score's varweight
// 6/7); the depth coefficient is likewise scaled *7 so depthweight*depth
// stays in the same units, exactly as the historical hardcoded
// `4*large + 1*small + 14*depth`.  Twee's parallel portfolio auto-selects
// per-problem weight configs (LAT138-1 solves under `--lhs-weight 1
// --depth-weight 60`, a much steeper depth penalty that keeps the KB
// derivation shallow and reaches the goal before the ACI CP explosion);
// the env knobs (CLI --cp-lhs-weight / --cp-rhs-weight /
// --cp-depth-weight) expose the same tuning.  Defaults reproduce the
// prior hardcoded formula byte-for-byte, so the WM-parity path is
// unchanged.
static u32 atp_twee_score_combine(u32 large, u32 small, u32 depth) {
  static int lhs_w = -1, rhs_w = -1, depth_w = -1;
  if (lhs_w < 0) {
    const char *e;
    e = getenv("THVM_ATP_TWEE_LHS_W");
    lhs_w   = (e != NULL && e[0] != '\0') ? (int)strtol(e, NULL, 10) : 4;
    e = getenv("THVM_ATP_TWEE_RHS_W");
    rhs_w   = (e != NULL && e[0] != '\0') ? (int)strtol(e, NULL, 10) : 1;
    e = getenv("THVM_ATP_TWEE_DEPTH_W");
    depth_w = (e != NULL && e[0] != '\0') ? (int)strtol(e, NULL, 10) : 2;
    if (lhs_w < 0)   lhs_w   = 4;
    if (rhs_w < 0)   rhs_w   = 1;
    if (depth_w < 0) depth_w = 2;
  }
  return (u32)lhs_w * large + (u32)rhs_w * small + (u32)depth_w * 7u * depth;
}

// (atp_term_depth defined below at line ~4653.)

