// === ClasHeuristics: advanced CP-weight term measures ===============
// Helpers feeding the non-default `AtpCpWeightMode` weight modes --
// ports of measures from Waldmeister's `ClasHeuristics`,
// `ClasFunctions` and `Unifikation1` ("unification 1") modules.

// Term depth: a leaf (FVR / atom / nullary CTR) has depth 0; a
// CTR has 1 + max child depth.  Used by the unification-measure
// weight, mirroring Waldmeister's `TO_Termtiefe` ("term depth").
static u32 atp_term_depth(Term t) {
  // `d` per open CTR: the deepest child so far.
  typedef struct { u64 base; u32 i; u32 n; u32 d; } DepthFrame;
  DepthFrame  fr_inl[ATP_WALK_INL];
  DepthFrame *fr  = fr_inl;
  u32         nfr = 0u;
  u32         cfr = ATP_WALK_INL;
  u32         v   = 0u;
  for (;;) {
    u32 n = (term_tag(t) == TAG_CTR) ? term_ctr_n(t) : 0u;
    if (n > 0u) {
      if (nfr == cfr) {
        fr = (DepthFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
      }
      fr[nfr].base = term_val(t);
      fr[nfr].i    = 0u;
      fr[nfr].n    = n;
      fr[nfr].d    = 0u;
      nfr++;
    } else {
      v = 0u;
      while (nfr > 0u) {
        DepthFrame *f = &fr[nfr - 1u];
        if (v > f->d) f->d = v;
        if (++f->i < f->n) break;
        v = f->d + 1u;
        nfr--;
      }
      if (nfr == 0u) {
        break;
      }
    }
    DepthFrame *f = &fr[nfr - 1u];
    t = heap_read(f->base + 1u + (u64)f->i);
  }
  term_stack_release(fr, fr_inl);
  return v;
}

// Does variable `var_id` (a TAG_FVR id) occur anywhere in `t`?
// The occur-check used by the unification-measure weight.
static int atp_var_occurs(Term t, u32 var_id) {
  return atp_term_has_var(t, var_id);
}

// Depth-weighted term-disagreement count -- a port of Waldmeister's
// `U1_Unifikationsmass` ("unification measure"; sources/INF/
// Unifikation1.c).  Walks `a` and `b` in parallel: equal top
// symbols recurse into children; a variable on either side that
// fails the occur-check binds for free (cost 0) and otherwise costs
// `2*d`; a function-symbol clash costs `d`.  `d` is the working
// depth, clamped at 1 as the recursion descends -- mirroring
// Waldmeister's `if (!(--d)) d = 1;` step-down.  Result is 0 iff
// the terms are syntactically unifiable.
static u32 atp_unif_measure_rec(Term a, Term b, u32 d) {
  // Pairs still to measure, each with its working depth; the masses add
  // up in any order (u32 wraparound included).
  typedef struct { Term a; Term b; u32 d; } UnifPair;
  UnifPair  st_inl[ATP_WALK_INL];
  UnifPair *st   = st_inl;
  u32       nst  = 0u;
  u32       cst  = ATP_WALK_INL;
  u32       mass = 0u;
  for (;;) {
    u8 ta = term_tag(a), tb = term_tag(b);
    if (ta == TAG_CTR && tb == TAG_CTR
        && term_ext(a) == term_ext(b)
        && term_ctr_n(a) == term_ctr_n(b)) {
      u32 nd = d > 1 ? d - 1 : 1;
      u32 n = term_ctr_n(a);
      while (nst + n > cst) {
        st = (UnifPair *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) {
        st[nst].a = term_ctr_at(a, i);
        st[nst].b = term_ctr_at(b, i);
        st[nst].d = nd;
        nst++;
      }
    } else if (ta == TAG_FVR) {
      mass += atp_var_occurs(b, term_ext(a)) ? 2u * d : 0u;
    } else if (tb == TAG_FVR) {
      mass += atp_var_occurs(a, term_ext(b)) ? 2u * d : 0u;
    } else {
      mass += d;   // function-symbol (or non-CTR) clash
    }
    if (nst == 0u) {
      break;
    }
    nst--;
    a = st[nst].a;
    b = st[nst].b;
    d = st[nst].d;
  }
  term_stack_release(st, st_inl);
  return mass;
}

static u32 atp_unif_measure(Term lhs, Term rhs) {
  u32 dl = atp_term_depth(lhs);
  u32 dr = atp_term_depth(rhs);
  u32 d  = dl > dr ? dl : dr;
  if (d == 0) d = 1;
  return atp_unif_measure_rec(lhs, rhs, d);
}

// Per-term KBO weight feeding the ORD mode.  Ports Waldmeister's
// `CF_Phi_KBO` ("KBO weight"; sources/CLAS/ClasFunctions.c): sum
// the active signature's per-symbol weights
// (`cfg->weights[label]` for a function symbol, `cfg->var_weight`
// for a variable).  m8's KBO module only exposes the single-pass
// differential balance, so the standalone single-term walk is
// inlined here.  With no KboConfig attached the raw symbol count
// is the fallback.
//
// GT / MIX / MIX2 / UNIF / MAX use CF_Phi (CP-weight table) not
// CF_Phi_KBO; per WM ClasHeuristics.c that's atp_symbol_count
// here (CP table defaults to 1 for all symbols).
static u32 atp_kbo_weight(AtpState *s, Term t) {
  const KboConfig *cfg = (s != NULL) ? s->kbo : NULL;
  if (cfg == NULL) return atp_symbol_count(t);
  // Served from the per-term KBO weight memo (thvm_kbo_term_weight): the
  // CP-weight heuristics weigh both faces of every critical pair, so the
  // same terms are summed repeatedly within an epoch.  Byte-identical to
  // the inlined recursion (same cfg weights), just memoized.
  return (u32)thvm_kbo_term_weight(cfg, t);
}

// CP-weight base for one weight mode -- ports of the `CH_*Weight`
// functions in Waldmeister's `ClasHeuristics` module.  Mode
// ATP_CP_WEIGHT_ADD reproduces the pre-port symbol-count sum.
static u32 atp_cp_weight_base(AtpState *s, Term lhs, Term rhs, u32 mode) {
  switch (mode) {
    case ATP_CP_WEIGHT_MAX: {
      // CH_MaxWeight: max(|lhs|, |rhs|).
      u32 wl = atp_symbol_count(lhs), wr = atp_symbol_count(rhs);
      return wl > wr ? wl : wr;
    }
    case ATP_CP_WEIGHT_ORD: {
      // CH_OrdWeight: KBO-weight sum (CF_Phi_KBO over both sides).
      return atp_kbo_weight(s, lhs) + atp_kbo_weight(s, rhs);
    }
    case ATP_CP_WEIGHT_GT: {
      // CH_GtWeight: ordering-directed -- the greater side's
      // weight when the CP orients, the sum otherwise.
      // CF_Phi (CP-weight table, default symbol-count), not KBO.
      u32 wl = atp_symbol_count(lhs), wr = atp_symbol_count(rhs);
      KboCmp c = atp_compare(s, lhs, rhs);
      return (c == KBO_GT) ? wl
           : (c == KBO_LT) ? wr
           : wl + wr;
    }
    case ATP_CP_WEIGHT_MIX: {
      // CH_MixWeight: (wl+wr)*g + g + (wl+wr), g = GtWeight value.
      // WM faithfully: side weights are CF_Phi (CP-weight table,
      // default symbol-count per andassoc.pr) NOT CF_Phi_KBO; only
      // the orient discriminator uses ordering.
      u32 wl = atp_symbol_count(lhs), wr = atp_symbol_count(rhs);
      KboCmp c = atp_compare(s, lhs, rhs);
      u32 g = (c == KBO_GT) ? wl
            : (c == KBO_LT) ? wr
            : wl + wr;
      u32 sum = wl + wr;
      return sum * g + g + sum;
    }
    case ATP_CP_WEIGHT_MIX2: {
      // CH_MixWeight2: g*10 + (wl+wr).  Same CP-weight base as MIX.
      u32 wl = atp_symbol_count(lhs), wr = atp_symbol_count(rhs);
      KboCmp c = atp_compare(s, lhs, rhs);
      u32 g = (c == KBO_GT) ? wl
            : (c == KBO_LT) ? wr
            : wl + wr;
      return g * 10u + (wl + wr);
    }
    case ATP_CP_WEIGHT_UNIF: {
      // CH_Unifikationsmass: (wl+wr) * unification-measure.
      // CF_Phi (CP-weight table, default symbol-count), not KBO.
      u32 wl = atp_symbol_count(lhs), wr = atp_symbol_count(rhs);
      return (wl + wr) * atp_unif_measure(lhs, rhs);
    }
    case ATP_CP_WEIGHT_TWEE: {
      // Twee.CP.score (src/Twee/CP.hs:240).  Asymmetric:
      //   lhsweight * size(larger) + rhsweight * size(smaller)
      //   + depthweight * depth
      // Twee defaults: lhsweight=4, rhsweight=1, depthweight=2.
      // The "larger side" is treated as the peak the CP must
      // reduce away, so making the SMALLER side cheap is what
      // accelerates the search.
      //
      // Twee's per-term size also folds in a shared-subterm
      // discount (src/Twee/CP.hs:259-261): cfg_dupcost=1,
      // cfg_dupfactor=0 -- when a subtree has already been seen in
      // the same termScore walk, count it as 1 instead of its full
      // size and do not recurse.  atp_symbol_count_dedup mirrors
      // that with a per-call Term-id hash set.  Integer-arithmetic
      // version (we approximate Twee's varweight 6/7 == 1).
      // Twee-exact per-term size (varweight 6/7, compound-only dedup),
      // scaled *7 so it stays integer; the depth term below is *14
      // (= 2 * 7) to preserve Twee's 2*depth proportion.
      u32 m = atp_twee_term_score(lhs);
      u32 n = atp_twee_term_score(rhs);
      u32 large = m > n ? m : n;
      u32 small = m + n - large;
      // Twee's `depth` is the CP FORMATION depth (getDepth), NOT the
      // syntactic term depth.  On flattened problems the flat-names keep
      // terms shallow (term-depth ~1-2, a useless constant), so the old
      // atp_term_depth gave no derivation penalty and thvm explored the
      // deep-derivation space Twee prunes.  g_cp_formation_depth carries
      // max(parent rule depths)+1, set by atp_push_cps_traced.
      u32 d = g_cp_formation_depth;
      return atp_twee_score_combine(large, small, d);
    }
    case ATP_CP_WEIGHT_STAGGERED: {
      // E StaggeredWeight (HEURISTICS/che_varweights.c::
      // StaggeredWeightCompute).  Lazily compute max axiom weight on
      // first call (one pass over rules; the result is reused for
      // the rest of the run).  Bucket the CP into integer stagger
      // groups via base / stagger_limit.
      u32 base = atp_symbol_count(lhs) + atp_symbol_count(rhs);
      u32 max_aw = 0;
      for (u32 i = 0; i < s->n_rules; i++) {
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        u32 wi = atp_rule_symbol_count(s, i, 0u) +
                 atp_rule_symbol_count(s, i, 1u);
#else
        u32 wi = atp_symbol_count(s->lhs[i]) + atp_symbol_count(s->rhs[i]);
#endif
        if (wi > max_aw) max_aw = wi;
      }
      u32 stagger_limit = (max_aw / 2u);
      if (stagger_limit < 1u) stagger_limit = 1u;
      return base / stagger_limit;
    }
    case ATP_CP_WEIGHT_RELLEVEL: {
      // E RelevanceLevelWeight (HEURISTICS/che_funweights.c:610) +
      // init_relevance_vector.  N-level port: per-symbol BFS distance
      // from the conjecture through the axiom co-occurrence graph,
      // capped at ATP_REL_LEVEL_MAX = 8.  Per-CTR-node weight is
      //   1 + sym_level[label]   (cap at REMOTE penalty = MAX + 2)
      // so level 0 contributes 1, level 1 contributes 2, ..., remote
      // (255) contributes ATP_REL_LEVEL_MAX + 2 = 10.  Variable
      // nodes weight 1.  Sum over both sides.  Level array is
      // precomputed at thvm_atp_set_goal time so this walk is O(|CP|).
      Term stack[256];
      u32 sp = 0;
      if (sp < 256) stack[sp++] = lhs;
      if (sp < 256) stack[sp++] = rhs;
      u32 total = 0;
      while (sp > 0) {
        Term cur = stack[--sp];
        if (term_tag(cur) == TAG_CTR) {
          u32 lab = term_ext(cur);
          u32 level;
          if (lab < WALD_MAX_SYMBOLS &&
              s->sym_level[lab] != ATP_REL_LEVEL_REMOTE) {
            level = s->sym_level[lab];
          } else {
            level = ATP_REL_LEVEL_MAX + 1u;  // remote penalty
          }
          total += 1u + level;
          u32 n = term_ctr_n(cur);
          for (u32 i = 0; i < n && sp < 256; i++) {
            stack[sp++] = term_ctr_at(cur, i);
          }
        } else {
          total += 1u;              // FVR / others: weight 1
        }
      }
      return total;
    }
    case ATP_CP_WEIGHT_DIVERSITY: {
      // E DiversityWeight (HEURISTICS/che_diversityweight.c:161).
      // Iterative walk of both sides; we tally:
      //   base       -- total CTR/FVR node count
      //   f_distinct -- # of distinct CTR head labels (popcount of
      //                 the label-seen bitmask)
      //   v_distinct -- # of distinct FVR ids (popcount of the FVR-seen
      //                 bitmask)
      //   weight = base + f_distinct + v_distinct
      // Linear E-defaults shape (fdiff1=1, fdiff2=0, vdiff1=1, vdiff2=0).
      // Labels and FVR ids are tracked in u64 bitmasks; WALD_MAX_SYMBOLS
      // is 64 so the f-mask covers the whole label space.  FVR ids are
      // capped at 64 since thvm_normalize_vars renumbers each rule into
      // 0..REWRITE_MAX_VAR-1 (== 64).
      u64 f_mask = 0;
      u64 v_mask = 0;
      u32 base = 0;
      Term stack[256];
      u32 sp = 0;
      if (sp < 256) stack[sp++] = lhs;
      if (sp < 256) stack[sp++] = rhs;
      while (sp > 0) {
        Term cur = stack[--sp];
        if (term_tag(cur) == TAG_CTR) {
          u32 lab = term_ext(cur);
          if (lab < 64u) f_mask |= ((u64)1 << lab);
          base++;
          u32 n = term_ctr_n(cur);
          for (u32 i = 0; i < n && sp < 256; i++) {
            stack[sp++] = term_ctr_at(cur, i);
          }
        } else if (term_tag(cur) == TAG_FVR) {
          u32 vid = term_ext(cur);
          if (vid < 64u) v_mask |= ((u64)1 << vid);
          base++;
        } else {
          base++;
        }
      }
      // Portable popcount via Kernighan's bit-clearing loop.
      u32 f_distinct = 0; { u64 m = f_mask; while (m) { f_distinct++; m &= m - 1u; } }
      u32 v_distinct = 0; { u64 m = v_mask; while (m) { v_distinct++; m &= m - 1u; } }
      return base + f_distinct + v_distinct;
    }
    case ATP_CP_WEIGHT_CONJSYM: {
      // E ConjectureSymbolWeight (HEURISTICS/che_funweights.c:550).
      // Walk both sides.  A node whose head symbol appears in the
      // conjecture gets weight 1 (E's conj_fweight=1); a node whose
      // head does not appear gets weight 4 (E's fweight=4, the
      // 4x non-conjecture penalty E defaults to).  Variables count
      // as 1 (E's vweight=1).  Sum over both sides.
      //   conj_count = # CTR nodes whose symbol is in conj_sym_mask
      //   off_count  = # CTR nodes whose symbol is NOT in mask
      //   var_count  = # FVR nodes
      //   weight = conj_count + 4*off_count + var_count
      // When no goal is set (conj_sym_mask == 0), every label misses
      // and the formula degenerates to 4*ctr_count + var_count -- a
      // monotonic alias for ADD, so the mode is still well-defined
      // in pure-completion runs.
      u64 mask = s->conj_sym_mask;
      Term stack[256];
      u32 sp = 0;
      if (sp < 256) stack[sp++] = lhs;
      if (sp < 256) stack[sp++] = rhs;
      u32 conj_count = 0, off_count = 0, var_count = 0;
      while (sp > 0) {
        Term cur = stack[--sp];
        if (term_tag(cur) == TAG_CTR) {
          u32 lab = term_ext(cur);
          if (lab < 64u && (mask & ((u64)1 << lab))) conj_count++;
          else                                       off_count++;
          u32 n = term_ctr_n(cur);
          for (u32 i = 0; i < n && sp < 256; i++) {
            stack[sp++] = term_ctr_at(cur, i);
          }
        } else if (term_tag(cur) == TAG_FVR) {
          var_count++;
        }
      }
      return conj_count + 4u * off_count + var_count;
    }
    case ATP_CP_WEIGHT_WMQ: {
      // Waldmeister -auto (Automodus/StdS) CP measure: weight = S*S + S - 1,
      // S = symcount(l) + symcount(r).  Derived exactly from wmcli -a4 verbose
      // weights on WolframAxioms/OrAssociativity (S=14->209, 26->701, 30->929).
      // Total-size-only (not l/r split): same-S CPs share a bucket -> cp_seq
      // FIFO age tie-break decides, matching WM's best-first.  See thvm.h.
      u64 s = (u64)atp_symbol_count(lhs) + (u64)atp_symbol_count(rhs);
      u64 w = s * s + s - 1u;
      return (w > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (u32)w;
    }
    case ATP_CP_WEIGHT_WMQ2: {
      // Waldmeister -auto CP measure, DERIVED from the full wmcli emission
      // trace (3780 CP/weight pairs): weight = b*b + 2*b + a*(b+1), a = min
      // side symbol count, b = max side symbol count.  Fits 96.5% of WM's real
      // weights exactly; WMQ (S*S+S-1) is only the a==1 collapser special case.
      u64 sl = (u64)atp_symbol_count(lhs);
      u64 sr = (u64)atp_symbol_count(rhs);
      u64 a = sl < sr ? sl : sr;
      u64 b = sl < sr ? sr : sl;
      u64 w = b * b + 2u * b + a * (b + 1u);
      return (w > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (u32)w;
    }
    case ATP_CP_WEIGHT_ADD:
    default:
      // CH_AddWeight: symbol-count sum -- the pre-port default.
      return atp_symbol_count(lhs) + atp_symbol_count(rhs);
  }
}

// === Waldmeister goal-directed CP selection (Clas_CP_Goal.c) ========
// Weight a critical pair by how its two sides structurally match the
// goal (the CPinGoal classifier).  A CP side that is a generalization
// of -- i.e. matches into -- a goal subterm "covers" that subterm.
//   Doppelmatch  : both CP sides match goal subterms under ONE
//                  consistent substitution.  Weight = the goal residual
//                  phi(goal) - covered (minimized over match positions).
//   Einfachmatch : one CP side matches.  Weight = residual x SingleMatchFactor.
//   Nullmatch    : neither.  Weight = the CP's own size x NoMatchFactor.
// A goal-resembling CP thus scores small and is selected first,
// steering completion at the goal -- Waldmeister's key to fast proofs.
// Factors are Waldmeister's -auto mnfanalysis=1000.2000.10 overrides
// (Clas_CP_Goal.c:237-239), NOT the 5/50/4 source DEFAULTS (:242-246):
// FEQ runs the Automodus override, so the large factors strongly separate
// the match levels (Doppel << Einfach << Null) as WM's goal pick requires.
// THVM_ATP_GOAL_FACTORS=def restores the 5/50 defaults for the legacy path.
#define ATP_GOAL_SINGLE_FACTOR atp_goal_single_factor()
#define ATP_GOAL_NONE_FACTOR   atp_goal_none_factor()
static u32 atp_goal_single_factor(void) {
  static int v = -1;
  if (v < 0) { const char *e = getenv("THVM_ATP_GOAL_FACTORS");
               v = (e && e[0] == 'd') ? 5 : 1000; }
  return (u32)v;
}
static u32 atp_goal_none_factor(void) {
  static int v = -1;
  if (v < 0) { const char *e = getenv("THVM_ATP_GOAL_FACTORS");
               v = (e && e[0] == 'd') ? 50 : 2000; }
  return (u32)v;
}

// Best coverage of `pat` matched into `subj` or any subterm, extending
// the (consistent) substitution `base`: returns the largest matched
// subterm's symbol count, 0 if none.  `pat` must be a CTR -- a bare
// variable generalizes everything and carries no goal signal (the
// MinStruct spirit of Clas_CP_Goal).
static u32 atp_goal_match_cov(Term pat, Term subj, RewriteSubst base) {
  if (term_tag(pat) != TAG_CTR) return 0u;
  // Every subterm of `subj`, preorder, each matched against `pat` from
  // the same `base`.
  Term  st_inl[ATP_WALK_INL];
  Term *st   = st_inl;
  u32   nst  = 0u;
  u32   cst  = ATP_WALK_INL;
  u32   best = 0u;
  for (;;) {
    RewriteSubst t = base;
    if (thvm_match(pat, subj, &t)) {
      u32 c = atp_symbol_count(subj);
      if (c > best) best = c;
    }
    if (term_tag(subj) == TAG_CTR) {
      u32 n = term_ctr_n(subj);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) st[nst++] = term_ctr_at(subj, i);
    }
    if (nst == 0u) {
      break;
    }
    subj = st[--nst];
  }
  term_stack_release(st, st_inl);
  return best;
}

// Best (cov_cl + cov_cr) for a Doppelmatch: cl matches a subterm of sx
// (substitution sigma), cr matches a subterm of gy extending sigma.
// Walks every cl-match position so the cr-match is gated by a
// consistent sigma, maximizing total coverage (= minimal residual).
static u32 atp_goal_doppel(Term cl, Term cr, Term sx, Term gy,
                           RewriteSubst base) {
  // Every subterm of `sx`, preorder, tried as the cl-match position.
  Term  st_inl[ATP_WALK_INL];
  Term *st   = st_inl;
  u32   nst  = 0u;
  u32   cst  = ATP_WALK_INL;
  u32   best = 0u;
  for (;;) {
    if (term_tag(cl) == TAG_CTR) {
      RewriteSubst t1 = base;
      if (thvm_match(cl, sx, &t1)) {
        u32 cov_a = atp_symbol_count(sx);
        u32 cov_b = atp_goal_match_cov(cr, gy, t1);
        if (cov_b && cov_a + cov_b > best) best = cov_a + cov_b;
      }
    }
    if (term_tag(sx) == TAG_CTR) {
      u32 n = term_ctr_n(sx);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) st[nst++] = term_ctr_at(sx, i);
    }
    if (nst == 0u) {
      break;
    }
    sx = st[--nst];
  }
  term_stack_release(st, st_inl);
  return best;
}

// CPinGoal weight of CP (cl, cr) against the conjecture.  No goal
// (completion mode) -> 0.  See the block comment above.
// --- Structural goal-relevance selection (use_goal_subterm) ----------
//
// A genuinely distinct goal-directed CP metric from SOS (symbol-level:
// on a single-operator Sheffer signature EVERY CP touches the one
// operator, so SOS is a no-op) and from CPinGoal/atp_goal_weight
// (whole-side match coverage, an expensive per-CP unify).  Here we
// reward a CP by the SIZE of the largest GROUND subterm it shares
// (structurally) with the goal's ground terms -- i.e. CPs that operate
// on the goal's own substructure surface first, biasing lemma
// derivation toward the goal without a match/unify per CP.  Sound: only
// the heap key changes; no CP is dropped, completeness preserved.
#define ATP_GOAL_SUBTERM_FACTOR 4u
static void atp_goal_sub_collect_rec(AtpState *s, Term t) {
  // Preorder over the CTR subterms of `t` (explicit stack).
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  for (;;) {
    if (term_tag(t) == TAG_CTR) {
      if (atp_term_is_ground(t)) {
        u32 sz = atp_symbol_count(t);
        if (sz >= 2u && s->goal_sub_n < 128u) {   // skip bare constants
          u64 h = atp_term_struct_hash(t);
          u8 dup = 0u;
          for (u32 i = 0; i < s->goal_sub_n; i++)
            if (s->goal_sub_hash[i] == h) { dup = 1u; break; }
          if (!dup) {
            s->goal_sub_hash[s->goal_sub_n] = h;
            s->goal_sub_size[s->goal_sub_n] = (u16)(sz > 0xffffu ? 0xffffu : sz);
            s->goal_sub_n++;
          }
        }
      }
      u32 n = term_ctr_n(t);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) st[nst++] = term_ctr_at(t, i);
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
}
static void atp_goal_sub_ensure(AtpState *s) {
  if (s->goal_sub_ready || s->goal_lhs == 0) return;
  s->goal_sub_n = 0u;
  atp_goal_sub_collect_rec(s, s->goal_lhs);
  atp_goal_sub_collect_rec(s, s->goal_rhs);
  s->goal_sub_ready = 1u;
}
static u32 atp_goal_subterm_overlap_rec(const AtpState *s, Term t, u32 best) {
  // Preorder over the CTR subterms of `t`, threading `best`.
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  for (;;) {
    if (term_tag(t) == TAG_CTR) {
      if (atp_term_is_ground(t)) {
        u32 sz = atp_symbol_count(t);
        if (sz > best) {
          u64 h = atp_term_struct_hash(t);
          for (u32 i = 0; i < s->goal_sub_n; i++)
            if (s->goal_sub_hash[i] == h) { best = sz; break; }
        }
      }
      u32 n = term_ctr_n(t);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) st[nst++] = term_ctr_at(t, i);
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return best;
}

// CPinGoal weight against an explicit goal pair (gl, gr): the goal's
// normal form, or the goal itself before one exists.
static u32 atp_goal_weight_vs(Term gl, Term gr, Term cl, Term cr) {
  u32 phi_g = atp_symbol_count(gl) + atp_symbol_count(gr);
  RewriteSubst e = {{0}};
  // Doppelmatch over both pairings (cl->gl & cr->gr, cl->gr & cr->gl).
  u32 d1 = atp_goal_doppel(cl, cr, gl, gr, e);
  u32 d2 = atp_goal_doppel(cl, cr, gr, gl, e);
  u32 dcov = d1 > d2 ? d1 : d2;
  if (dcov) return (dcov < phi_g) ? (phi_g - dcov) : 0u;
  // Einfachmatch: best single coverage of either CP side into either
  // goal side.
  u32 ec = atp_goal_match_cov(cl, gl, e);
  { u32 x = atp_goal_match_cov(cl, gr, e); if (x > ec) ec = x; }
  { u32 x = atp_goal_match_cov(cr, gl, e); if (x > ec) ec = x; }
  { u32 x = atp_goal_match_cov(cr, gr, e); if (x > ec) ec = x; }
  if (ec) {
    u32 res = (ec < phi_g) ? (phi_g - ec) : 0u;
    return res * ATP_GOAL_SINGLE_FACTOR;
  }
  // Nullmatch.
  return (atp_symbol_count(cl) + atp_symbol_count(cr)) * ATP_GOAL_NONE_FACTOR;
}

static u32 atp_goal_weight(const AtpState *s, Term cl, Term cr) {
  if (s == NULL || s->goal_lhs == 0) return 0u;
  Term gl = s->goal_lhs_nf ? s->goal_lhs_nf : s->goal_lhs;
  Term gr = s->goal_rhs_nf ? s->goal_rhs_nf : s->goal_rhs;
  return atp_goal_weight_vs(gl, gr, cl, cr);
}
// 8.8: priority weight for a CP.  Default `--add` heuristic is
// the symbol-count sum.  When `s->use_mix_heuristic` is set, add
// a penalty for CPs that fail to orient cleanly (KBO_UN or
// KBO_EQ) -- mirrors Waldmeister's `--mix` heuristic in
// `ClasHeuristics.c`.  The penalty (`MIX_UNORIENTED_PENALTY`)
// is conservative; experiments may want to tune it.  Under
// -DATP_GOAL_HEURISTIC a bounded goal-directed penalty is then
// added (Waldmeister lever 1, above).
//
// `s->cp_weight_mode` selects among the ported `ClasHeuristics`
// weight functions (see the `AtpCpWeightMode` enum); the engine
// default is ATP_CP_WEIGHT_GT.  Selecting ATP_CP_WEIGHT_ADD with
// use_mix_heuristic unset and no goal makes this function the
// bare symbol-count sum.
#define MIX_UNORIENTED_PENALTY 4u

// Forward-decl: atp_cp_priority_sized uses this; the helper is defined
// next to thvm_atp_set_use_sos.
static int atp_term_touches_goal(const AtpState *s, Term t);
// CP-insert weigh comparator: same VERDICT as atp_compare, with the
// plain-KBO case served memo-free (thvm_kbo_nomemo) below a node-count
// threshold.  The GT/MIX/MIX2 weigh compares the two faces of a FRESH
// CP exactly once -- new cells, never re-compared -- so the persistent
// weight memo's hashing (kbo_leaf_hash / kbo_memo_combine over every
// cold node) costs more than the single-pass walk it replaces; the
// runtime profile put the memo machinery at ~70% of the weigh's KBO
// time.  The memo is a pure cache (THVM_KBO_NO_WCACHE is the standing
// A/B witness), so the verdict -- and the engine trajectory -- is
// byte-identical for ANY threshold.  Big CPs (>= the threshold) keep
// the memo path: their subtrees repeat internally and across the
// orient gates, where the cache does pay.  Every non-plain-KBO route
// (AC mask, WPO/RPO/LPO, opt-in flat) falls back to atp_compare.
// THVM_ATP_KBO_NOMEMO_MAX overrides the threshold (0 = disabled).
static KboCmp atp_weigh_compare(AtpState *s, Term lhs, Term rhs, u32 nodes) {
  // Fused CP weigh: the pack walk already encoded both faces + the
  // whole-pair weight/var balances (acp_pack_fused).  Verdict-identical
  // to the thvm_kbo_nomemo route below on the renumbered pair; the
  // fused gate (atp_cp_push_fuse_ok) admits only the plain-KBO route,
  // so this branch never bypasses an AC/WPO/RPO/LPO ordering.
  if (g_acp_fuse.valid) return acp_fuse_kbo(s->kbo);
  static i64 nomemo_max = -1;
  if (nomemo_max < 0) {
    const char *e = getenv("THVM_ATP_KBO_NOMEMO_MAX");
    nomemo_max = (e != NULL) ? (i64)strtoul(e, NULL, 10) : (i64)0xffffffffu;
  }
  if (s != NULL && (i64)nodes <= nomemo_max &&
      s->kbo != NULL && s->wpo == NULL && s->rpo == NULL && s->lpo == NULL) {
#ifdef THVM_ATP_AC
    if (thvm_atp_get_ac_mask() != 0ull) return atp_compare(s, lhs, rhs);
#endif
    return thvm_kbo_nomemo(lhs, rhs, s->kbo);
  }
  return atp_compare(s, lhs, rhs);
}

// Per-side variant of atp_cp_weight_base for the modes that consume
// the two sides' symbol counts (MAX/GT/MIX/MIX2/UNIF): identical
// formulas, with the counts supplied by the caller (acp_pack counts
// exactly one per node, the same value atp_symbol_count returns) so
// the two per-side re-walks are skipped.  Every other mode falls back
// to the general path.
static u32 atp_cp_weight_base_counts(AtpState *s, Term lhs, Term rhs,
                                     u32 mode, u32 wl, u32 wr) {
  switch (mode) {
    case ATP_CP_WEIGHT_MAX:
      return wl > wr ? wl : wr;
    case ATP_CP_WEIGHT_GT: {
      KboCmp c = atp_weigh_compare(s, lhs, rhs, wl + wr);
      return (c == KBO_GT) ? wl
           : (c == KBO_LT) ? wr
           : wl + wr;
    }
    case ATP_CP_WEIGHT_MIX: {
      KboCmp c = atp_weigh_compare(s, lhs, rhs, wl + wr);
      u32 g = (c == KBO_GT) ? wl
            : (c == KBO_LT) ? wr
            : wl + wr;
      u32 sum = wl + wr;
      return sum * g + g + sum;
    }
    case ATP_CP_WEIGHT_MIX2: {
      KboCmp c = atp_weigh_compare(s, lhs, rhs, wl + wr);
      u32 g = (c == KBO_GT) ? wl
            : (c == KBO_LT) ? wr
            : wl + wr;
      return g * 10u + (wl + wr);
    }
    case ATP_CP_WEIGHT_UNIF:
      return (wl + wr) * atp_unif_measure(lhs, rhs);
    default:
      return atp_cp_weight_base(s, lhs, rhs, mode);
  }
}

// ATP_CP_WEIGHT_TWEE base for a FUSED push (g_acp_fuse.valid): the
// per-side scores come from the fuse state when the pack walk stashed
// them (ftp_pack_fused, the treated FT-direct route), from a
// materialize-and-score of the treated stash when the pack's size
// guard declined (twee_valid == 0 on an over-256-node side), and from
// the classic renumber+score of the REAL Term pair otherwise (the
// acp_pack_fused Term route arrives un-renumbered here -- the clone
// renumber reproduces the classic pipeline's nv-then-score exactly,
// including the Term-identity shared-subterm discounts of the raw
// >= 50 class, whose images this route still builds as Terms).
// THVM_ATP_TWEE_FUSE_CHECK=1 certifies the stashed-score arm per push
// against the materialized classic computation and aborts on any
// mismatch.
static u32 atp_cp_twee_fused_base(Term lhs, Term rhs) {
  u32 m, n;
  u8 have = 0u;
  if (g_acp_fuse.twee_valid) {
    m = g_acp_fuse.twee_l;
    n = g_acp_fuse.twee_r;
    have = 1u;
#ifdef THVM_ATPFT_NORM
    static int twee_check = -1;
    if (twee_check < 0) twee_check = atp_env_on("THVM_ATP_TWEE_FUSE_CHECK");
    if (twee_check && g_vcp_treat.armed &&
        g_vcp_treat.fl != NULL && g_vcp_treat.fr != NULL) {
      Term cl = ft_to_term(g_vcp_treat.fl);
      Term cr = ft_to_term(g_vcp_treat.fr);
      thvm_normalize_vars(&cl, &cr);
      u32 rm = atp_twee_term_score(cl);
      u32 rn = atp_twee_term_score(cr);
      if (rm != m || rn != n) {
        fprintf(stderr,
                "ATP_TWEE_FUSE_CHECK MISMATCH lhs=%u/%u rhs=%u/%u "
                "(fl_n=%u fr_n=%u)\n",
                m, rm, n, rn, g_acp_fuse.fl_n, g_acp_fuse.fr_n);
        abort();
      }
    }
#endif
  }
#ifdef THVM_ATPFT_NORM
  if (!have && g_vcp_treat.armed &&
      g_vcp_treat.fl != NULL && g_vcp_treat.fr != NULL) {
    // Treated FT-direct push whose pack declined the plain count (size
    // guard): the Terms here are routed-push sentinels, so materialize
    // the stash and score classically -- ft_to_term is fresh/unshared,
    // exactly the classic write-back image.
    m = atp_twee_term_score(ft_to_term(g_vcp_treat.fl));
    n = atp_twee_term_score(ft_to_term(g_vcp_treat.fr));
    have = 1u;
  }
#endif
  if (!have) {
    // Term-route fused push (raw >= 50 class + anomaly fallbacks): the
    // pair is REAL but un-renumbered.  Clone-renumber and score -- the
    // exact classic computation (nv output is isomorphic, and the
    // score is deterministic on the shared-DAG shape).
    Term cl = lhs, cr = rhs;
    thvm_normalize_vars(&cl, &cr);
    m = atp_twee_term_score(cl);
    n = atp_twee_term_score(cr);
  }
  u32 large = m > n ? m : n;
  u32 small = m + n - large;
  return atp_twee_score_combine(large, small, g_cp_formation_depth);
}

// Priority weight for a CP whose symbol-count sum is already known
// (`base`) -- e.g. counted for free during acp_pack.  Identical
// verdict to atp_cp_priority; only the redundant size walk is
// skipped.  The precomputed `base` is the ADD-mode value; a
// non-default cp_weight_mode recomputes the base from the terms.
// `lhs_nodes` (0xffffffff = unknown) is the LHS-only node count from
// acp_pack_ex; when known, the per-side weight modes reuse it (and
// base - lhs_nodes) instead of re-walking the two sides.
// LADR clause-weight penalties (ladr/weight.c): nest counts the
// parent/child pairs sharing a symbol under an arity <= 2 parent; depth
// is the term depth with variables and constants at 0.
static u32 atp_ladr_nest(Term t) {
  // Every CTR node adds its same-symbol children (when at most binary);
  // the counts sum in any order.
  Term  st_inl[ATP_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = ATP_WALK_INL;
  u32   c   = 0u;
  for (;;) {
    if (term_tag(t) == TAG_CTR) {
      u32 n = term_ctr_n(t);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = 0; i < n; i++) {
        Term ch = term_ctr_at(t, i);
        if (n <= 2u && term_tag(ch) == TAG_CTR && term_ext(ch) == term_ext(t)) c++;
        st[nst++] = ch;
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
static u32 atp_ladr_depth(Term t) {
  // `d` per open CTR: the deepest child so far; leaves are depth 0.
  typedef struct { u64 base; u32 i; u32 n; u32 d; } DepthFrame;
  DepthFrame  fr_inl[ATP_WALK_INL];
  DepthFrame *fr  = fr_inl;
  u32         nfr = 0u;
  u32         cfr = ATP_WALK_INL;
  u32         v   = 0u;
  for (;;) {
    u32 n = (term_tag(t) == TAG_CTR) ? term_ctr_n(t) : 0u;
    if (n > 0u) {
      if (nfr == cfr) {
        fr = (DepthFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
      }
      fr[nfr].base = term_val(t);
      fr[nfr].i    = 0u;
      fr[nfr].n    = n;
      fr[nfr].d    = 0u;
      nfr++;
    } else {
      v = 0u;
      while (nfr > 0u) {
        DepthFrame *f = &fr[nfr - 1u];
        if (v > f->d) f->d = v;
        if (++f->i < f->n) break;
        v = f->d + 1u;
        nfr--;
      }
      if (nfr == 0u) {
        break;
      }
    }
    DepthFrame *f = &fr[nfr - 1u];
    t = heap_read(f->base + 1u + (u64)f->i);
  }
  term_stack_release(fr, fr_inl);
  return v;
}
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// Span twins for the treated push route, whose Term args are zero
// sentinels (the CP content is the treatment's NF spans).  A NUM marker
// subtree is one leaf node, as its Term is.
static u8 atp_ladr_leaf_ft(const AtpFtCell *c) {
  return (u8)((c->sym & WF_VAR_BIT) != 0u || c->arity == 0u ||
              c->sym == ATPFT_NUM_MARKER);
}
static u32 atp_ladr_nest_ft(const AtpFtCell *t) {
  // One linear pass over the span's preorder: a non-leaf cell adds its
  // same-symbol children (when at most binary); the cells a leaf hides
  // (a NUM marker's two fields) are skipped with it.
  const AtpFtCell *stop = t->end->next;
  u32 c = 0u;
  for (const AtpFtCell *p = t; p != stop;) {
    if (atp_ladr_leaf_ft(p)) {
      p = p->end->next;
      continue;
    }
    const AtpFtCell *ch = p->next;
    for (u32 i = 0; i < p->arity; i++) {
      if (p->arity <= 2u && (ch->sym & WF_VAR_BIT) == 0u && ch->sym == p->sym) c++;
      ch = ch->end->next;
    }
    p = p->next;
  }
  return c;
}
static u32 atp_ladr_depth_ft(const AtpFtCell *t) {
  // `d` per open non-leaf cell: the deepest child so far.
  typedef struct { const AtpFtCell *child; u32 left; u32 d; } DepthFrame;
  DepthFrame  fr_inl[ATP_WALK_INL];
  DepthFrame *fr  = fr_inl;
  u32         nfr = 0u;
  u32         cfr = ATP_WALK_INL;
  u32         v   = 0u;
  for (;;) {
    if (!atp_ladr_leaf_ft(t)) {
      if (nfr == cfr) {
        fr = (DepthFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
      }
      fr[nfr].child = t->next;
      fr[nfr].left  = t->arity;
      fr[nfr].d     = 0u;
      nfr++;
    } else {
      v = 0u;
      while (nfr > 0u) {
        DepthFrame *f = &fr[nfr - 1u];
        if (v > f->d) f->d = v;
        if (f->left > 0u) break;
        v = f->d + 1u;
        nfr--;
      }
      if (nfr == 0u) {
        break;
      }
    }
    DepthFrame *f = &fr[nfr - 1u];
    t        = f->child;
    f->child = t->end->next;
    f->left--;
  }
  term_stack_release(fr, fr_inl);
  return v;
}
#endif

// The LADR clause-weight penalty of a CP (0 when both penalties are
// off): nest_pen * nest + depth_pen * (1 + max side depth).  Real Terms
// score directly; the treated push route (zero-sentinel Terms) scores
// its NF spans.
static u32 atp_ladr_penalty(const AtpState *s, Term lhs, Term rhs) {
  if (s == NULL || (s->ladr_nest_pen | s->ladr_depth_pen) == 0u) return 0u;
  if (lhs != 0 && rhs != 0 &&
      term_tag(lhs) != TAG_NUM && term_tag(rhs) != TAG_NUM) {
    u32 dl = atp_ladr_depth(lhs), dr = atp_ladr_depth(rhs);
    return s->ladr_nest_pen * (atp_ladr_nest(lhs) + atp_ladr_nest(rhs)) +
           s->ladr_depth_pen * (1u + (dl > dr ? dl : dr));
  }
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  if (g_vcp_treat.armed && g_vcp_treat.fl != NULL && g_vcp_treat.fr != NULL) {
    const AtpFtCell *fl = g_vcp_treat.fl, *fr = g_vcp_treat.fr;
    u32 dl = atp_ladr_depth_ft(fl), dr = atp_ladr_depth_ft(fr);
    return s->ladr_nest_pen * (atp_ladr_nest_ft(fl) + atp_ladr_nest_ft(fr)) +
           s->ladr_depth_pen * (1u + (dl > dr ? dl : dr));
  }
#endif
  return 0u;
}

static u32 atp_cp_priority_sized_ex(AtpState *s, Term lhs, Term rhs,
                                    u32 base, u32 lhs_nodes) {
  // ifeq guard discipline (see AtpState.ifeq_syms): scale by
  // penalty^(undecided guards) when armed.  tgt goal-targeted scale
  // (see atp_tgt_weight_scale): multiplicative goal-symbol bias when
  // armed, applied BEFORE the ifeq scale (both multiplicative, so the
  // order only matters at the saturation ceiling).  ifeq guard-
  // DECIDING boost (see atp_ifeq_decide_scale) applied LAST, same
  // reasoning -- a guard-deciding CP with zero undecided guards of its
  // own (the common case: `g = true` has no ifeq subterm to penalize)
  // reaches the boost at its tgt/penalty-scaled base unchanged, so the
  // three disciplines compose without fighting in practice.  Applied
  // on every path out of this function via the wrapper below;
  // sentinel/zero Term args (the FT-routed pushes) count 0 guards / 0
  // symbols / no registry hit here and get their exact FT-span
  // treatment in atp_cp_heap_insert_packed instead.
  u8 ifeq_arm   = (s != NULL && s->use_ifeq_guard) ? 1u : 0u;
  u8 decide_arm = (s != NULL && s->use_ifeq_decide) ? 1u : 0u;
  u8 tgt_arm    = atp_tgt_armed(s);
#define ATP_IFEQ_RET(w)                                                   \
    do {                                                                  \
      u32 w_ret_ = (w);                                                   \
      if (tgt_arm) w_ret_ = atp_tgt_weight_scale(s, lhs, rhs, w_ret_);    \
      if (ifeq_arm) w_ret_ = atp_ifeq_weight_scale(s, lhs, rhs, w_ret_);  \
      if (decide_arm) w_ret_ = atp_ifeq_decide_scale(s, lhs, rhs, w_ret_);\
      return w_ret_;                                                     \
    } while (0)
  // Goal-directed mode (Waldmeister CPinGoal): weight by structural
  // match to the goal.  Opt-in via cp_weight_mode so completion-mode
  // and the other weights are unaffected.
  if (s != NULL && s->cp_weight_mode == ATP_CP_WEIGHT_GOAL &&
      s->goal_lhs != 0) {
    ATP_IFEQ_RET(atp_goal_weight(s, lhs, rhs));
  }
  u32 mode = (s != NULL) ? s->cp_weight_mode : ATP_CP_WEIGHT_ADD;
  if (mode == ATP_CP_WEIGHT_TWEE && g_acp_fuse.valid) {
    // Fused push: consume the pack walk's stashed per-side scores (or
    // their exact fallbacks) -- the routed FT pushes carry sentinel
    // Terms, so the generic recompute below would score garbage.
    base = atp_cp_twee_fused_base(lhs, rhs);
  } else if (mode != ATP_CP_WEIGHT_ADD) {
    // The caller's `base` is the ADD-mode symbol-count sum; for any
    // other mode it must be recomputed from the CP terms.
    base = (lhs_nodes != 0xffffffffu && lhs_nodes <= base)
             ? atp_cp_weight_base_counts(s, lhs, rhs, mode,
                                         lhs_nodes, base - lhs_nodes)
             : atp_cp_weight_base(s, lhs, rhs, mode);
  }
  // The equation literal: depth 1 + max side depth (literals_depth).
  base += atp_ladr_penalty(s, lhs, rhs);
  if (s != NULL && s->use_mix_heuristic) {
    KboCmp c = atp_compare(s, lhs, rhs);
    if (c != KBO_GT && c != KBO_LT) {
      // KBO_EQ / KBO_UN -- penalize.
      base += MIX_UNORIENTED_PENALTY;
    }
  }
  // Set-of-Support bonus: a CP that shares any symbol with the goal
  // gets its priority halved, surfacing it earlier in the min-heap.
  // Sound -- nothing dropped, only ordering perturbed.  Mirrors
  // Vampire's --sos / E-prover's -S sos in spirit; tailored for the
  // equational-completion variant where the "support set" is symbols
  // (the goal isn't a separate clause set in our engine).
  if (s != NULL && s->use_sos && s->goal_lhs != 0) {
    if (atp_term_touches_goal(s, lhs) || atp_term_touches_goal(s, rhs)) {
      base = base >> 1;   // halve -> sort earlier
    }
  }
  // Structural goal-relevance: discount by the largest ground subterm
  // shared with the goal (see atp_goal_subterm_overlap_rec).
  if (s != NULL && s->use_goal_subterm && s->goal_lhs != 0) {
    atp_goal_sub_ensure(s);
    u32 ov = atp_goal_subterm_overlap_rec(s, lhs, 0u);
    ov = atp_goal_subterm_overlap_rec(s, rhs, ov);
    if (ov > 0u) {
      u32 disc = ov * ATP_GOAL_SUBTERM_FACTOR;
      base = (disc < base) ? (base - disc) : 1u;
    }
  }
  ATP_IFEQ_RET(base);
#undef ATP_IFEQ_RET
}
static u32 atp_cp_priority_sized(AtpState *s, Term lhs, Term rhs, u32 base) {
  return atp_cp_priority_sized_ex(s, lhs, rhs, base, 0xffffffffu);
}
// Learned CP-selection scorer (ENIGMA-style).  Logistic-regression
// weights trained on the labelled corpus exported by THVM_ATP_CP_DATASET
// (per-selected-CP features, labelled by trace-DAG reachability from the
// goal-closing step over the 83 provable AxiomaticTheory notable
// theorems; held-out test AUC ~0.85).  score = W.features + B, in the
// RAW feature space (the standardization is folded into W,B).  A higher
// score means more proof-relevant, so it maps to a LOWER heap priority
// (selected sooner).  Completeness is preserved by select_cp's periodic
// FIFO (CPdimension) pick, which fires regardless of the weight mode.
static const float ATP_LEARNED_W[ATP_CP_FEATURE_DIM] = {
  0.003216f, -0.271657f, 0.460614f, -0.096104f, 0.003216f, 0.042247f,
  0.003216f, -0.000402f, -0.005740f, -0.156174f, -0.023514f, 1.121586f,
  1.999360f, -0.012683f};
static const float ATP_LEARNED_B = -1.598045f;

// Runtime-loadable model (default inactive -> baked-in logreg above).
// Process-global: a WL-trained model is pushed once via
// thvm_atp_set_learned_scorer and reused by every subsequent
// ATP_CP_WEIGHT_LEARNED run, mirroring the abort-hook / dataset-env
// globals this bridge already uses.
static AtpLearnedScorer g_atp_learned;
static int g_atp_learned_active = 0;

// Forward pass: standardized features -> raw logit (no sigmoid head).
static float atp_learned_forward(const AtpLearnedScorer *m, const float *feat) {
  float z[ATP_CP_FEATURE_DIM];
  for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) {
    z[i] = (feat[i] - m->mean[i]) * m->inv_std[i];
  }
  if (m->kind == ATP_LEARNED_LINEAR) {
    float s = m->b2;
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) s += m->w1[i] * z[i];
    return s;
  }
  // ATP_LEARNED_MLP: one ReLU hidden layer, single linear output.
  float s = m->b2;
  for (u32 j = 0; j < m->hidden; j++) {
    const float *wrow = m->w1 + (size_t)j * ATP_CP_FEATURE_DIM;
    float h = m->b1[j];
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) h += wrow[i] * z[i];
    if (h < 0.0f) h = 0.0f;
    s += m->w2[j] * h;
  }
  return s;
}

fn int thvm_atp_set_learned_scorer(const double *blob, u32 len) {
  if (blob == NULL || len < 30u) return 0;
  u32 kind   = (u32)blob[0];
  u32 hidden = (u32)blob[1];
  if (kind != ATP_LEARNED_LINEAR && kind != ATP_LEARNED_MLP) return 0;
  u32 want = (kind == ATP_LEARNED_LINEAR)
               ? 45u                              // 2 + 14 + 14 + (14 + 1)
               : 31u + 16u * hidden;              // 2 + 14 + 14 + H*14 + H + H + 1
  if (kind == ATP_LEARNED_MLP &&
      (hidden == 0u || hidden > ATP_LEARNED_MAX_HIDDEN)) {
    return 0;
  }
  if (len != want) return 0;

  AtpLearnedScorer m;
  m.kind   = (u8)kind;
  m.hidden = (kind == ATP_LEARNED_LINEAR) ? 0u : hidden;
  u32 p = 2u;
  for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) m.mean[i]    = (float)blob[p++];
  for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) m.inv_std[i] = (float)blob[p++];
  if (kind == ATP_LEARNED_LINEAR) {
    for (u32 i = 0; i < ATP_CP_FEATURE_DIM; i++) m.w1[i] = (float)blob[p++];
    m.b2 = (float)blob[p++];
  } else {
    for (u32 i = 0; i < hidden * ATP_CP_FEATURE_DIM; i++) m.w1[i] = (float)blob[p++];
    for (u32 i = 0; i < hidden; i++) m.b1[i] = (float)blob[p++];
    for (u32 i = 0; i < hidden; i++) m.w2[i] = (float)blob[p++];
    m.b2 = (float)blob[p++];
  }
  g_atp_learned        = m;
  g_atp_learned_active = 1;
  return 1;
}

fn void thvm_atp_clear_learned_scorer(void) {
  g_atp_learned_active = 0;
}

