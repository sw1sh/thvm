static u8 atp_eq_is_mono(const AtpState *s, u32 i) {
#ifdef ATP_VAR_NORM
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Mirror-sourced variant test: renumber the SWAPPED pair on span
  // clones and compare against the mirrors -- bit-identical verdict by
  // the lockstep invariant (guarded by the var cliff where the FT and
  // Term renumbers coincide); no rule-Term content read.
  if (s->lhs_ft[i] != NULL && s->rhs_ft[i] != NULL &&
      ft_distinct_var_count(s->lhs_ft[i], s->rhs_ft[i]) <
          REWRITE_MAX_VAR) {
    AtpFt *a = (AtpFt *)s->ft_arena_ptr;
    AtpFtCell *cl = ft_clone_span(a, s->rhs_ft[i], 0);
    AtpFtCell *cr = ft_clone_span(a, s->lhs_ft[i], 0);
    ft_normalize_vars_pair(cl, cr);
    u8 mono = (u8)(ft_eq(cl, s->lhs_ft[i]) && ft_eq(cr, s->rhs_ft[i]));
    ft_free_span(a, cl, cl->end);
    ft_free_span(a, cr, cr->end);
    return mono;
  }
#endif
  Term rl = s->rhs[i];
  Term rr = s->lhs[i];
  thvm_normalize_vars(&rl, &rr);
  return kbo_eq(rl, s->lhs[i]) && kbo_eq(rr, s->rhs[i]);
#else
  // Without canonical variable renumbering the stored pair is not in
  // BO_TermpaarNormieren form, so the variant test cannot be decided
  // syntactically; keep both faces (the pre-port behavior).
  (void)s; (void)i;
  return 0u;
#endif
}

// Subterm of `t` at a CriticalPair-recorded overlap position.
static Term atp_cp_pos_subterm(Term t, const u8 *pos, u8 len) {
  for (u8 d = 0; d < len; d++) {
    if (term_tag(t) != TAG_CTR) return 0;
    if (pos[d] >= term_ctr_n(t)) return 0;
    t = term_ctr_at(t, pos[d]);
  }
  return t;
}

// WM CP-formation ordering gate (Unifikation1.c KPActionGR :1394-1401,
// KPActionRG :1404-1411, KPActionGG :1414-1421): a CP candidate is
// discarded at FORMATION -- before it is numbered, weighed, or queued
// -- when an equation parent's rewrite step at the peak is strictly
// UPHILL on the unified instance, because ordered rewriting can never
// perform that step, so the overlap is not a peak of the ordered
// system (the unfailing-completion extended-CP condition).
//
//   outer (Vater) test, equation i:  discard if
//       sigma(rhs_i) > sigma(lhs_i)   ==  cp.rhs > cp.peak
//     (WM: TermGroesserUnif(KPLinks, Ueberlappung))
//   inner (Mutter) test, equation j:  discard if
//       sigma(rj) > sigma(lj)
//     read back as the subterms of cp.lhs / cp.peak at the overlap
//     position (WM: TermGroesserUnif(KPRechtsInnen, UeberlappungInnen);
//     "Vergleich innen ... ausreichend" -- by monotonicity of the
//     reduction order the inner comparison decides the whole term).
//
// Rules are never tested (KPActionRR has no ordering test): an
// oriented rule is downhill on every instance by stability.  WM's
// ORD_TermGroesserUnif is the SAME function as the reduction-order
// test (Ordnungen.c:273-283 sets UnifTestfkt = RedTestfkt), i.e.
// thvm's atp_compare.  Filters buf[lo, hi) in place; returns new hi.
//
// The compared pair is var-NORMALIZED first (one bijective renaming
// across both sides -- KBO/LPO verdicts are renaming-invariant, so
// the verdict equals WM's on its own alpha-variant).  The raw overlap
// terms carry j-side ids >= CP_RENAME_OFFSET, which sit ABOVE the KBO
// balance caps (kbo/_.c KBO_MAX_VAR oracle range, u8 wmemo var
// profile): comparing them raw both mis-verdicts the var condition
// AND seeds the address-keyed weight memo with entries whose ids
// alias -- the dense renumber keeps every id inside the caps.
static u32 atp_cp_order_gate(AtpState *s, CriticalPair *buf,
                             u32 lo, u32 hi, u8 outer_eq, u8 inner_eq) {
  if (!outer_eq && !inner_eq) {
    if (g_cptr_active)
      for (u32 k = lo; k < hi; k++) atp_cptr_cp("ORDGATE pass(no-eq)", &buf[k]);
    return hi;
  }
  u32 w = lo;
  for (u32 k = lo; k < hi; k++) {
    Term peak = buf[k].peak;
    u8 drop = 0u;
    u8 drop_outer = 0u;
    if (peak != 0) {
      // Since the CP-emission side fix (4df4f2ab) the built pair is WM's
      // (KPLinks, KPRechts) = (sigma(host.rhs), reduct): the outer
      // KPActionGR test TermGroesserUnif(KPLinks, Ueberlappung) reads
      // cp.LHS, and the inner KPActionRG test's KPRechtsInnen -- the
      // spliced sigma(rin) at the overlap position -- lives inside
      // cp.RHS (the reduct carries the position coordinates).
      if (outer_eq) {
        Term a = buf[k].lhs, b = peak;
        thvm_normalize_vars(&a, &b);
        if (atp_compare(s, a, b) == KBO_GT) drop = drop_outer = 1u;
      }
      if (!drop && inner_eq) {
        Term in_new = atp_cp_pos_subterm(buf[k].rhs,
                                         buf[k].pos, buf[k].pos_len);
        Term in_old = atp_cp_pos_subterm(peak, buf[k].pos, buf[k].pos_len);
        if (in_new != 0 && in_old != 0) {
          thvm_normalize_vars(&in_new, &in_old);
          if (atp_compare(s, in_new, in_old) == KBO_GT) drop = 1u;
        }
      }
    }
    if (g_cptr_active) {
      char tag[96];
      snprintf(tag, sizeof tag, "ORDGATE %s(outer_eq=%u inner_eq=%u%s)",
               drop ? "DROP" : "pass", outer_eq, inner_eq,
               drop ? (drop_outer ? " by=outer" : " by=inner") : "");
      atp_cptr_cp(tag, &buf[k]);
    }
    if (!drop) {
      if (w != k) buf[w] = buf[k];
      w++;
    }
  }
  return w;
}

// WM -einsstern CP filter (EinsSternUeberlappung, Unifikation1.c:
// 1039-1055 via AnEinsSternIn :1028-1036): keep a CP iff its overlap
// position lies on the "1*" leftmost-argument spine of the overlapped
// LHS.  AnEinsSternIn descends from the root taking the FIRST subterm
// (TO_ErsterTeilterm) until it reaches the overlap position si; success
// means every descent step was into argument 0.  In thvm's CriticalPair
// geometry that is exactly pos[d] == 0 for all d (pos_len == 0 = root,
// trivially on the spine).  WM's existential-goal branch (:1043-1047) is
// inert on the ground-goal corpus.  Filters buf[lo, hi) in place;
// returns the new hi.  No-op (returns hi) when the flag is off.
static int atp_kapur_on(void) {
  static int v = -1;
  if (v < 0) v = getenv("THVM_ATP_KAPUR") ? 1 : 0;
  return v;
}
// WM Kapur prime-superposition filter (Primueberlappung, Unifikation1.c:990;
// NF_TermInnenReduzibel = TermReduzibelR on the tail/proper subterms).  Drop a
// CP whose OVERLAP SUBTERM (peak sigma(li) navigated to the overlap position)
// is reducible at a PROPER (inner) position -- i.e. the unifier bound a
// variable to a reducible term, so reducing sigma(x) first subsumes the CP.
// Completeness-preserving and part of WM's CP set (so thvm's set becomes WM's,
// improving parity while cutting generation volume).  Needs cp.peak populated
// (need_peak forced on under kapur).  Off by default.
// True if peak sigma(li) has a redex OFF the overlap path (a reducible
// sigma(x) binding = WM MGUreduzibel) or, at the overlap subterm, a reducible
// PROPER subterm (Primueberlappung).  li is an irreducible rule LHS, so its
// redexes come only from the substitution (variable positions off the overlap
// path) or the overlap itself (on-path == the CP, allowed).
static Term g_kapur_face = 0;
// WM MGUreduzibel (Unifikation1.c:1000): drop a CP if the unifier bound any
// VARIABLE of the outer face to a reducible term (sigma(x) reducible under R).
// Parallel-walk the outer face and the peak sigma(face); at the face's variable
// positions the peak carries sigma(x) -- check ONLY those (rules-only), so a
// NEW redex at a non-variable position (a genuine other overlap WM keeps) is
// not dropped.  g_kapur_face is the outer face set per-combo before the gate.
static u8 atp_mgu_reducible(AtpState *s, Term face, Term peak) {
  if (face == 0 || peak == 0) return 0u;
  if (term_tag(face) == TAG_FVR) {
    if (term_tag(peak) != TAG_CTR) return 0u;   // sigma(x) a bare var: irred.
    return (u8)(!kbo_eq(atp_rules_only_normalize(s, peak, 4096u), peak));
  }
  if (term_tag(face) == TAG_CTR && term_tag(peak) == TAG_CTR) {
    u32 n = term_ctr_n(face);
    if (term_ctr_n(peak) != n) return 0u;
    for (u32 c = 0; c < n; c++)
      if (atp_mgu_reducible(s, term_ctr_at(face, c), term_ctr_at(peak, c)))
        return 1u;
  }
  return 0u;
}
static u32 atp_kapur_gate(AtpState *s, CriticalPair *buf, u32 lo, u32 hi) {
  if (!atp_kapur_on() || g_kapur_face == 0) return hi;
  u32 w = lo;
  for (u32 k = lo; k < hi; k++) {
    u8 drop = (buf[k].peak != 0) &&
              atp_mgu_reducible(s, g_kapur_face, buf[k].peak);
    if (!drop) {
      if (w != k) buf[w] = buf[k];
      w++;
    } else {
      s->n_cps_dropped_nusfu++;
    }
  }
  return w;
}

static u32 atp_einsstern_gate(AtpState *s, CriticalPair *buf,
                              u32 lo, u32 hi) {
  if (!s->use_einsstern) return hi;
  u32 w = lo;
  for (u32 k = lo; k < hi; k++) {
    u8 on_spine = 1u;
    for (u8 d = 0; d < buf[k].pos_len; d++)
      if (buf[k].pos[d] != 0u) { on_spine = 0u; break; }
    if (on_spine) {
      if (w != k) buf[w] = buf[k];
      w++;
    } else {
      s->n_cps_dropped_einsstern++;
    }
  }
  return w;
}

// WM -nusfu CP filter (NusfUeberlappung, Unifikation1.c:1082-1090 via
// Nusfu :1063-1079): drop a CP whose overlap position lies physically
// BELOW a skolem-function symbol.  Nusfu walks the overlapped LHS and
// for every skolem symbol checks whether the overlap position is in its
// argument subterm; here we walk the recorded overlap path on the
// instantiated peak (the substituted overlapped term) and drop the CP if
// any symbol strictly ABOVE the position (depth 0..pos_len-1) carries a
// registered skolem label.  The skolem registry is empty on every
// ground-goal path, so the gate is a no-op there even when on.  Filters
// buf[lo, hi) in place; returns the new hi.
static u8 atp_label_is_skolem(const AtpState *s, u32 label) {
  for (u32 k = 0; k < s->n_skolem_labels; k++)
    if (s->skolem_labels[k] == label) return 1u;
  return 0u;
}

static u32 atp_nusfu_gate(AtpState *s, CriticalPair *buf, u32 lo, u32 hi) {
  if (!s->use_no_overlap_below_skolem || s->n_skolem_labels == 0u)
    return hi;
  u32 w = lo;
  for (u32 k = lo; k < hi; k++) {
    Term t = buf[k].peak;
    u8 below_skolem = 0u;
    for (u8 d = 0; d < buf[k].pos_len; d++) {
      if (t == 0 || term_tag(t) != TAG_CTR) break;
      if (atp_label_is_skolem(s, term_ext(t))) { below_skolem = 1u; break; }
      if (buf[k].pos[d] >= term_ctr_n(t)) { t = 0; break; }
      t = term_ctr_at(t, buf[k].pos[d]);
    }
    if (!below_skolem) {
      if (w != k) buf[w] = buf[k];
      w++;
    } else {
      s->n_cps_dropped_nusfu++;
    }
  }
  return w;
}

// Apply the per-segment CP-generation gates to a freshly produced
// buffer segment [lo, hi): the KPAction ordering gate (always, intrinsic
// to unfailing CP-gen) then the WM CP filters (einsstern + nusfu, both
// default OFF).  Since the WM filters default OFF and are not in the
// "Waldmeister"* presets, this is byte-identical to the bare order gate
// on every current path; threading them here keeps `combo_end` (the
// emission-order ranker's per-combo boundaries) consistent with the
// post-filter buffer.
static u32 atp_cp_gen_gates(AtpState *s, CriticalPair *buf,
                            u32 lo, u32 hi, u8 outer_eq, u8 inner_eq) {
  hi = atp_cp_order_gate(s, buf, lo, hi, outer_eq, inner_eq);
  hi = atp_einsstern_gate(s, buf, lo, hi);
  hi = atp_nusfu_gate(s, buf, lo, hi);
  hi = atp_kapur_gate(s, buf, lo, hi);
  return hi;
}

// `combo_end` (optional, 4 slots) receives the buffer count after each
// of the four base face-combo passes -- the WM emission-order ranker
// needs to know which (i-face, j-face) combination produced each CP.
// AC-extension CPs (when compiled in) land after combo_end[3]; the
// WM-order path treats them as combo 0.
extern int g_cp_visit_trace;

static u32 atp_overlap_ij(AtpState *s, u32 i, u32 j,
                          CriticalPair *buf, u32 cap, u32 *combo_end) {
  u32 cnt = 0;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
  // Lazy slots: the Term-side branch below (the only batch former when
  // THVM_ATPFT_UNIFY is not compiled) reads s->lhs/s->rhs raw -- a NUM
  // sentinel renames to a leaf, unifies with nothing, and the pair
  // silently forms zero CPs (sentinel-consumer audit, 2026-07-11; the
  // batch sibling of the single-walk sweep-entry fix).  Materialize the
  // two rules side-exactly from the mirrors; no-op on eager slots.
  if (atp_lazy_rules_on()) {
    (void)atp_rule_term(s, i, 0u);
    (void)atp_rule_term(s, i, 1u);
    (void)atp_rule_term(s, j, 0u);
    (void)atp_rule_term(s, j, 1u);
  }
#endif
  // Gated CP-overlap trace for a target rule pair (THVM_CP_TRACE_I/J): pins a
  // missed superposition by logging per-position thvm_unify pass/fail.
  {
    static int ti = -2, tj = -2;
    if (ti == -2) {
      const char *a = getenv("THVM_CP_TRACE_I");
      const char *b = getenv("THVM_CP_TRACE_J");
      ti = a ? atoi(a) : -1; tj = b ? atoi(b) : -1;
    }
    g_cp_visit_trace = (ti >= 0 &&
                        ((i == (u32)ti && j == (u32)tj) ||
                         (i == (u32)tj && j == (u32)ti))) ? 1 : 0;
    if (g_cp_visit_trace) {
      fprintf(stderr, "CPVIS === overlap i=%u j=%u (i_or=%u j_or=%u) pick=%u ===\n",
              i, j, s->r_orient[i], s->r_orient[j], s->cp_select_count);
      static int dumped = 0;
      if (!dumped) {
        dumped = 1;
        for (u32 k = 0; k < s->n_rules; k++) {
          fprintf(stderr, "CPVIS ALLRULE[%u] lhs=", k);
          atp_dbg_print_term(stderr, s->lhs[k]);
          fprintf(stderr, " rhs="); atp_dbg_print_term(stderr, s->rhs[k]);
          fputc('\n', stderr);
        }
      }
    }
  }
  // Variables of j must be renamed apart from i's -- the SAME offset
  // thvm_critical_pairs_range uses internally (REWRITE_MAX_VAR / 2).
  // One-entry rename cache: the WM batch loop's eTT phase overlaps
  // every OLD fact against the SAME new fact as j, so consecutive
  // calls rename identical source terms.  Keyed on the source handles
  // + the GC epoch (cells neither move nor get reused between
  // collections; a rule-slot mutation swaps the source handle itself),
  // so a hit returns exactly what the rename would rebuild.
  static Term rn_src_l = 0, rn_src_r = 0, rn_lj = 0, rn_rj = 0;
  static u64  rn_epoch = ~0ull;
  Term lj, rj;
  if (rn_epoch == g_atp_gc_epoch && rn_src_l == s->lhs[j] &&
      rn_src_r == s->rhs[j]) {
    lj = rn_lj;
    rj = rn_rj;
  } else {
    lj = thvm_rename_vars(s->lhs[j], REWRITE_MAX_VAR / 2);
    rj = thvm_rename_vars(s->rhs[j], REWRITE_MAX_VAR / 2);
    rn_src_l = s->lhs[j];
    rn_src_r = s->rhs[j];
    rn_lj    = lj;
    rn_rj    = rj;
    rn_epoch = g_atp_gc_epoch;
  }
  Term li = s->lhs[i], ri = s->rhs[i];

  // Face combinations.  i_face in {li (always), ri (if i unorientable)};
  // j as the from->to pair {(lj,rj) always, (rj,lj) if j unorientable}.
  // Mono equations contribute their distinguished face only (WM
  // Monogleichung; see atp_eq_is_mono).
  u8 i_eq = s->use_unfailing_cp && !s->r_orient[i];
  u8 j_eq = s->use_unfailing_cp && !s->r_orient[j];
  u8 i_mono = i_eq ? atp_eq_is_mono(s, i) : 0u;
  u8 j_mono = (i == j) ? i_mono : (j_eq ? atp_eq_is_mono(s, j) : 0u);
  u8 i_un = i_eq && !i_mono;
  u8 j_un = j_eq && !j_mono;
  // cp.peak (sigma(l_i)) is read only by the connectedness gate
  // (:16856), the simpler-join checkOrder/step3 legs (mode 1 treat,
  // ft_norm.c), the equation-parent KPAction ordering (:17076/:17151,
  // i_eq/j_eq), and the nusfu gate when armed with a non-empty skolem
  // registry.  Otherwise the Term cp_visit must NOT spend a third
  // thvm_unify_apply building it -- gate it for this pair.
  g_cp_need_peak = (s->use_connectedness || s->use_simpler_join != 0u ||
                    i_eq || j_eq) || atp_kapur_on() ||
                   (s->use_no_overlap_below_skolem && s->n_skolem_labels != 0u)
                       ? 1u : 0u;

  // Root-overlap ownership (WM Unifikation1.c U1_KPsBildenZuRegel /
  // U1_KPsBildenZuGleichung).  WM forms each root-x-root overlap of a
  // fact pair exactly ONCE: the new fact's toplevel phase walks TT(l)
  // (root included) against OLD tops, while the converse phase walks
  // the top against eTT (PROPER subterms) only.  The saturator visits
  // a pair as both (i, j) and (j, i); the i > j call owns the roots,
  // the i < j call enumerates proper positions only.  Self pair
  // (i == j): WM's dedicated phases give an equation the roots
  // F (l =? l), C (l =? r, stereo only, ONCE -- combo 3's root is C's
  // mirror and is skipped) and G (r =? r); a RULE never forms a root
  // self-overlap (U1_KPsBildenZuRegel passes the rule itself as the
  // toplevel-pass Ausschluss = exclusion object).
  u8 skip1, skip2, skip3, skip4;
  if (i < j) {
    skip1 = skip2 = skip3 = skip4 = 1u;
  } else if (i > j) {
    skip1 = skip2 = skip3 = skip4 = 0u;
  } else {
    skip1 = i_eq ? 0u : 1u;
    skip2 = 0u;
    skip3 = 1u;
    skip4 = 0u;
  }

  // Remove-and-re-derive root-overlap ownership (flat-subsume only).
  // A rule re-derived after E-set subsumption removed an equation of the
  // same shape must NOT re-enumerate the root-x-root overlaps the
  // subsuming rule already formed against the removed original in its
  // ORIGINAL batch -- WM forms each such root overlap exactly once and
  // that ownership stays with the original batch across the remove/
  // re-derive boundary.  When i is re-derived (r_rederive_cut[i] set) and
  // it would own the roots here (i > j) against a rule j born at or before
  // the cutoff, force the noroot variant: proper-position overlaps remain,
  // only the root overlap is suppressed.  This is the sole consumer of
  // r_rederive_cut, which is NONE outside flat-subsume, so no other path
  // is affected.
  //
  // Exception -- partner IS the subsumer (s->r_trace[j] == r_rederive_cut[i],
  // the commutativity rule that caused the cut): its overlap onto the
  // re-derived equation is genuinely new content WM emits in the re-derived
  // fact's own batch, not ownership the original batch resolved.  Keep skip1's
  // forward face so its ROOT plants the subsumer onto the re-derived
  // equation's distinguished side -- the C-shape (x*x = (y*(y*y))*x) WM selects
  // there.  The walk's position-major sweep emits that root overlap after the
  // leading proper-position A-shape, so the C-shape gets WM's batch FIFO age.
  // skip2/skip3/skip4 stay noroot.  Identify the subsumer
  // by trace equality, not by a partner-slot scan or a flat-transposition shape
  // match.
  if (i > j && i < s->n_rules && s->r_rederive_cut != NULL &&
      s->r_rederive_cut[i] != ATP_TRACE_NONE &&
      j < s->n_rules && s->r_trace[j] != ATP_TRACE_NONE &&
      s->r_trace[j] <= s->r_rederive_cut[i]) {
    u8 j_is_subsumer = (s->r_trace[j] == s->r_rederive_cut[i]);
    skip2 = skip3 = skip4 = 1u;
    skip1 = j_is_subsumer ? skip1 : 1u;
    if (atp_rule_trace_on()) {
      fprintf(stderr,
              "  REDERIVE-SKIPROOT i=%u (cut=%u) x j=%u (trace=%u)%s\n",
              i, s->r_rederive_cut[i], j, s->r_trace[j],
              j_is_subsumer ? " [subsumer: skip1 kept]" : "");
    }
  }

  // uid-keyed probe: arm for this pair so the gen gates + push path log.
  u8 _cptr_here = atp_cptr_pair(s, i, j);
  if (_cptr_here) {
    g_cptr_active = 1u;
    fprintf(stderr,
            "CPTR OVERLAP i=%u j=%u uid_i=%u uid_j=%u i_or=%u j_or=%u "
            "i_eq=%u j_eq=%u i_mono=%u j_mono=%u skip=%u%u%u%u pick=%u\n",
            i, j, s->r_uid[i], s->r_uid[j], s->r_orient[i], s->r_orient[j],
            i_eq, j_eq, i_mono, j_mono, skip1, skip2, skip3, skip4,
            s->cp_select_count);
    fprintf(stderr, "CPTR OVERLAP li=");
    atp_dbg_print_term(stderr, li);
    fprintf(stderr, " ri=");
    atp_dbg_print_term(stderr, ri);
    fprintf(stderr, " lj=");
    atp_dbg_print_term(stderr, lj);
    fprintf(stderr, " rj=");
    atp_dbg_print_term(stderr, rj);
    fputc('\n', stderr);
  }

#ifdef THVM_ATPFT_UNIFY
  // FT-native overlap path: when both rules' FT mirrors are populated,
  // the rename + cp_visit work happens on AtpFt cells directly, only
  // converting back to Term for the CriticalPair slot.  The 4 base
  // syntactic-overlap cases above all route through this; AC extensions
  // (gated on THVM_ATP_AC below) stay Term-side because they rebuild
  // Term-shaped extended rule forms.
  // Arm the generation-time --max-term-size guard (ft_cp.c) with the
  // state's queue-time cap when --max-term-size-gen asks for the earlier
  // enforcement point (raw sides, before the push-time normalize); the
  // plain knob keeps its queue-time semantics.
  {
    static int gen = -1;
    if (gen < 0) gen = atp_env_on("THVM_ATP_MAX_TERM_SIZE_GEN");
    thvm_ft_cp_set_max_side(gen ? s->max_cp_side : 0u);
  }
  AtpFt *ft_arena_local = (AtpFt *)s->ft_arena_ptr;
  if (ft_arena_local != NULL
      && s->lhs_ft != NULL && s->rhs_ft != NULL
      && s->lhs_ft[i] != NULL && s->rhs_ft[i] != NULL
      && s->lhs_ft[j] != NULL && s->rhs_ft[j] != NULL) {
    // Rename j into scratch arena once.
    AtpFtCell *lj_r = thvm_ft_rename_vars(ft_arena_local, s->lhs_ft[j],
                                          REWRITE_MAX_VAR / 2, 1);
    AtpFtCell *rj_r = thvm_ft_rename_vars(ft_arena_local, s->rhs_ft[j],
                                          REWRITE_MAX_VAR / 2, 1);
    AtpFtCell *li_ft = s->lhs_ft[i];
    AtpFtCell *ri_ft = s->rhs_ft[i];
    // The KPAction ordering gate reads cp.peak, so equation-parent
    // overlaps need it even when connectedness is off.  The nusfu gate
    // (atp_cp_gen_gates -> atp_nusfu_gate) also reads cp.peak to walk the
    // overlap path for a skolem symbol above it; mirror the Term-side
    // g_cp_need_peak condition (:21430) so an armed nusfu registry still
    // gets a peak here -- else the FT path drops zero and diverges.
    u8 need_peak = (s->use_connectedness || s->use_simpler_join != 0u ||
                    i_eq || j_eq) || atp_kapur_on() ||
                   (s->use_no_overlap_below_skolem && s->n_skolem_labels != 0u)
                       ? 1u : 0u;
    u32 seg = cnt;
    g_kapur_face = s->lhs[i];
    cnt = thvm_critical_pairs_pair_ft(li_ft, ri_ft, lj_r, rj_r,
                                      ft_arena_local, need_peak, skip1,
                                      buf, cap, cnt);
    if (_cptr_here) fprintf(stderr, "CPTR COMBO0 raw=%u\n", cnt - seg);
    cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
    if (combo_end != NULL) combo_end[0] = cnt;
    if (j_un) {
      seg = cnt;
      g_kapur_face = s->lhs[i];
      cnt = thvm_critical_pairs_pair_ft(li_ft, ri_ft, rj_r, lj_r,
                                        ft_arena_local, need_peak, skip2,
                                        buf, cap, cnt);
      if (_cptr_here) fprintf(stderr, "CPTR COMBO1 raw=%u\n", cnt - seg);
      cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
    }
    if (combo_end != NULL) combo_end[1] = cnt;
    if (i_un) {
      seg = cnt;
      g_kapur_face = s->rhs[i];
      cnt = thvm_critical_pairs_pair_ft(ri_ft, li_ft, lj_r, rj_r,
                                        ft_arena_local, need_peak, skip3,
                                        buf, cap, cnt);
      if (_cptr_here) fprintf(stderr, "CPTR COMBO2 raw=%u\n", cnt - seg);
      cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
      if (combo_end != NULL) combo_end[2] = cnt;
      if (j_un) {
        seg = cnt;
        g_kapur_face = s->rhs[i];
        cnt = thvm_critical_pairs_pair_ft(ri_ft, li_ft, rj_r, lj_r,
                                          ft_arena_local, need_peak, skip4,
                                          buf, cap, cnt);
        if (_cptr_here) fprintf(stderr, "CPTR COMBO3 raw=%u\n", cnt - seg);
        cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
      }
    }
    if (combo_end != NULL) {
      if (!i_un) combo_end[2] = cnt;
      combo_end[3] = cnt;
    }
    // Scratch is reset by the caller between overlap pairs (saturation
    // step).  Each scratch-allocated rename + working tree dies on that
    // reset; no per-call cleanup needed here.
  } else
#endif
  {
    u32 (*const pf[2])(Term, Term, Term, Term, CriticalPair *, u32, u32) =
        { thvm_critical_pairs_pair, thvm_critical_pairs_pair_noroot };
    u32 seg = cnt;
    // (i-face li) x (j: lj->rj)  -- the standard overlap.
    cnt = pf[skip1](li, ri, lj, rj, buf, cap, cnt);
    if (_cptr_here) fprintf(stderr, "CPTR COMBO0 raw=%u\n", cnt - seg);
    cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
    if (combo_end != NULL) combo_end[0] = cnt;
    if (j_un) {
      // (i-face li) x (j: rj->lj)
      seg = cnt;
      cnt = pf[skip2](li, ri, rj, lj, buf, cap, cnt);
      if (_cptr_here) fprintf(stderr, "CPTR COMBO1 raw=%u\n", cnt - seg);
      cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
    }
    if (combo_end != NULL) combo_end[1] = cnt;
    if (i_un) {
      // (i-face ri) x (j: lj->rj)
      seg = cnt;
      cnt = pf[skip3](ri, li, lj, rj, buf, cap, cnt);
      if (_cptr_here) fprintf(stderr, "CPTR COMBO2 raw=%u\n", cnt - seg);
      cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
      if (combo_end != NULL) combo_end[2] = cnt;
      if (j_un) {
        // (i-face ri) x (j: rj->lj)
        seg = cnt;
        cnt = pf[skip4](ri, li, rj, lj, buf, cap, cnt);
        if (_cptr_here) fprintf(stderr, "CPTR COMBO3 raw=%u\n", cnt - seg);
        cnt = atp_cp_gen_gates(s, buf, seg, cnt, i_eq, j_eq);
      }
    }
    if (combo_end != NULL) {
      if (!i_un) combo_end[2] = cnt;
      combo_end[3] = cnt;
    }
  }

#ifdef THVM_ATP_AC
  // AC-superposition extension (Bachmair-Plaisted).  NOT routed through
  // atp_cp_order_gate: WM has no AC lane (the gate is a U1 KPAction
  // mechanism), the AC unifier's overlap position is modulo-AC so the
  // recorded path does not address the syntactic subterm the inner test
  // would compare, and the block is inert on every WM path (ac_mask is
  // 0 there).  When both rules
  // have the SAME AC-top symbol, the standard syntactic overlap above
  // misses the merge-position CPs needed for AC-completeness.  The
  // standard fix: replace one rule's LHS with its extended form
  // R+ = f(l, z) → f(r, z) and re-run the overlap.  The matcher's
  // AC mode (Stage 3 atp_match_ac) then surfaces the merge-position
  // unifications.
  //
  // Run only when the engine-global AC mask is non-zero AND both top
  // symbols are the same AC label.  No-op under default builds (the
  // whole block is #ifdef'd) and under runs that haven't registered
  // an AC mask.
  u64 ac_mask = thvm_atp_get_ac_mask();
  if (ac_mask != 0ull
      && term_tag(li) == TAG_CTR
      && term_tag(lj) == TAG_CTR) {
    u32 li_lab = term_ext(li);
    u32 lj_lab = term_ext(lj);
    if (li_lab == lj_lab && li_lab < 64u
        && ((ac_mask >> li_lab) & 1ull) != 0ull) {
      AtpAcInfo ac = { .ac_mask = ac_mask };
      Term ext_li = 0, ext_ri = 0;
      Term ext_lj = 0, ext_rj = 0;
      u8 has_ext_i = atp_ac_extend_rule(li, ri, &ac, &ext_li, &ext_ri);
      u8 has_ext_j = atp_ac_extend_rule(lj, rj, &ac, &ext_lj, &ext_rj);

      // i-extended X j (li becomes f(li, z), z fresh per rename).
      if (has_ext_i) {
        cnt = thvm_critical_pairs_pair(ext_li, ext_ri, lj, rj, buf, cap, cnt);
        if (j_un) {
          cnt = thvm_critical_pairs_pair(ext_li, ext_ri, rj, lj, buf, cap, cnt);
        }
        if (i_un) {
          Term ext_ri2 = 0, ext_li2 = 0;
          if (atp_ac_extend_rule(ri, li, &ac, &ext_ri2, &ext_li2)) {
            cnt = thvm_critical_pairs_pair(ext_ri2, ext_li2, lj, rj, buf, cap, cnt);
            if (j_un) {
              cnt = thvm_critical_pairs_pair(ext_ri2, ext_li2, rj, lj, buf, cap, cnt);
            }
          }
        }
      }

      // i X j-extended (the dual: lj becomes f(lj, z')).  The
      // saturator's atp_overlap_ij(j, i) pass DOES eventually run with
      // i+j swapped, but i-X-jext is a STRUCTURALLY DIFFERENT CP set
      // than j-X-iext when li != lj: position p in the unextended OUTER
      // rule against the extended INNER rule covers merge-positions in
      // the inner that the swapped pass walks OVER (it walks the
      // extended-outer's positions, not the unextended-outer's).
      if (has_ext_j) {
        cnt = thvm_critical_pairs_pair(li, ri, ext_lj, ext_rj, buf, cap, cnt);
        if (j_un) {
          cnt = thvm_critical_pairs_pair(li, ri, ext_rj, ext_lj, buf, cap, cnt);
        }
        if (i_un) {
          cnt = thvm_critical_pairs_pair(ri, li, ext_lj, ext_rj, buf, cap, cnt);
          if (j_un) {
            cnt = thvm_critical_pairs_pair(ri, li, ext_rj, ext_lj, buf, cap, cnt);
          }
        }
      }

      // i-extended X j-extended: needed when neither side's positions
      // surface the AC merge-fix without both extensions in play (the
      // canonical case Bachmair-Plaisted's symmetric variant covers).
      if (has_ext_i && has_ext_j) {
        cnt = thvm_critical_pairs_pair(ext_li, ext_ri, ext_lj, ext_rj, buf, cap, cnt);
        if (j_un) {
          cnt = thvm_critical_pairs_pair(ext_li, ext_ri, ext_rj, ext_lj, buf, cap, cnt);
        }
        if (i_un) {
          cnt = thvm_critical_pairs_pair(ext_ri, ext_li, ext_lj, ext_rj, buf, cap, cnt);
          if (j_un) {
            cnt = thvm_critical_pairs_pair(ext_ri, ext_li, ext_rj, ext_lj, buf, cap, cnt);
          }
        }
      }
    }
  }
#endif

  if (_cptr_here) {
    fprintf(stderr, "CPTR OVERLAP-END i=%u j=%u kept=%u\n", i, j, cnt);
    g_cptr_active = 0u;
  }
  return cnt;
}

// Run one overlap pair and push its CPs (the body shared by the indexed
// and unindexed generator loops).
static u32 atp_gen_one(AtpState *s, u32 i, u32 j, CriticalPair *buf) {
  // WM Weggefiltert (Unifikation1.c:967-972 + the new-fact skip
  // :1518/:1588): a sterilized (ground-joinable, GZ_ZSFB_BEHALTEN=1)
  // fact forms no CP as EITHER parent.  The status only ever becomes
  // JOINABLE under the opt-in -gj port (use_bwd_ground_join), so the
  // default path's verdict is always false.
  if (s->r_gj_status[i] == ATP_GJ_ST_JOINABLE ||
      s->r_gj_status[j] == ATP_GJ_ST_JOINABLE) {
    return 0;
  }
  // A passive demodulator (Otter loop, src/atp/otter.c) is not yet
  // active: Vampire generates inferences only with the active set.
  if (atp_otter_passive(s, i) || atp_otter_passive(s, j)) return 0;
  u32 nbuf   = atp_overlap_ij(s, i, j, buf, ATP_CP_BATCH, NULL);
  u32 pushed = atp_push_cps_traced(s, buf, nbuf,
                                   s->r_trace[i], s->r_trace[j], i, j);
  // A single saturation step can out-allocate a whole GC semi-space in
  // raw critical-pair + normalisation scratch.  Collect between overlap
  // pairs -- `buf` is fully processed here, so no in-flight CP needs
  // rooting -- to bound the transient working set instead of crashing.
  atp_pressure_collect(s);
  return pushed;
}


// A flat transposition equation `f(x,y) = f(y,x)` (commutativity): same
// binary label over two distinct vars, args swapped.
static u8 atp_is_flat_transposition(Term lhs, Term rhs) {
  if (term_tag(lhs) != TAG_CTR || term_tag(rhs) != TAG_CTR) return 0u;
  if (term_ext(lhs) != term_ext(rhs)) return 0u;
  if (term_ctr_n(lhs) != 2u || term_ctr_n(rhs) != 2u) return 0u;
  Term a = term_ctr_at(lhs, 0), b = term_ctr_at(lhs, 1);
  Term c = term_ctr_at(rhs, 0), d = term_ctr_at(rhs, 1);
  if (term_tag(a) != TAG_FVR || term_tag(b) != TAG_FVR) return 0u;
  if (term_tag(c) != TAG_FVR || term_tag(d) != TAG_FVR) return 0u;
  if (term_ext(a) == term_ext(b)) return 0u;
  return (u8)(term_ext(a) == term_ext(d) && term_ext(b) == term_ext(c));
}

