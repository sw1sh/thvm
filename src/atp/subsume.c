// --- WM flatterm-faithful subsumption (firstdiv-19) -----------------------
// thvm's atp_eq_subsumes_pair above uses tree matching (thvm_match), which is
// the CORRECT subsumption.  WM's MO_TermpaarSubsummiertZweites
// (MatchOperationen.c:1452) is NOT correct: it binds a pattern variable to the
// xM slot named by an APPEARANCE COUNTER (SubstErgaenzenSubsumption :1445,
// xMletztes++) but unbound-checks / reads it back by the variable's OWN symbol
// (VarUngebunden / SubstitutVon).  For a pattern whose variables are not
// numbered in first-appearance order (the Gegenrichtung of commutativity,
// `x2*x1`), those two index spaces CROSS: the pattern's later variable stays
// "unbound" into the second side and binds to a whole compound with no
// consistency recheck -- an OVER-EAGER match.  That quirk is exactly why WM
// removes axiom2 `x*x = x*(y*(y*y))` the moment commutativity enters, then
// re-derives it later; reproducing WM's CP-selection sequence byte-for-byte
// requires reproducing the quirk.  EsFolgtSubstitutVon's flatterm-prefix test
// (:169) reduces to structural equality for complete-subterm bindings (a
// complete preorder subterm is self-delimiting), so kbo_eq stands in for it.
#define ATP_WMFLAT_MAX 1024u
typedef struct {
  Term node[ATP_WMFLAT_MAX];
  u32  end[ATP_WMFLAT_MAX];        // exclusive preorder end of subtree at i
  // Representation-neutral columns (filled by BOTH flatteners): the
  // phase's symbol tests and the EsFolgt subtree equality run on
  // these, so a span row and a Term row of the same content are
  // indistinguishable to the matcher.
  u32  sym[ATP_WMFLAT_MAX];        // ctr label, or var id when is_var
  u8   is_var[ATP_WMFLAT_MAX];
  u32  n;
  u8   overflow;
} AtpWmFlat;

static void atp_wmflat_preorder(Term t, AtpWmFlat *f) {
  if (f->n >= ATP_WMFLAT_MAX) { f->overflow = 1u; return; }
  u32 self = f->n++;
  f->node[self] = t;
  if (term_tag(t) == TAG_FVR) {
    f->sym[self]    = term_ext(t);
    f->is_var[self] = 1u;
  } else if (term_tag(t) == TAG_CTR) {
    f->sym[self]    = term_ext(t);
    f->is_var[self] = 0u;
    u32 k = term_ctr_n(t);
    for (u32 c = 0; c < k; c++) atp_wmflat_preorder(term_ctr_at(t, c), f);
  } else {
    // TAG_NUM etc.: a leaf with a namespaced code (never collides with
    // ctr labels or var ids in the same column semantics as before --
    // the old node-Term kbo_eq treated distinct NUMs as unequal, and
    // the symbol test compared ext/arity of CTRs only).
    f->sym[self]    = 0x40000000u ^ (u32)term_val(t);
    f->is_var[self] = 0u;
  }
  f->end[self] = f->n;
}

// Span sibling: identical columns off the mirror cells (a span's
// preorder IS its cell order); node[] carries 0 -- nothing reads it
// once the phase runs on the neutral columns.
static void atp_wmflat_preorder_ft(const AtpFtCell *c, AtpWmFlat *f) {
  if (f->n >= ATP_WMFLAT_MAX) { f->overflow = 1u; return; }
  u32 self = f->n++;
  f->node[self] = 0;
  if ((c->sym & WF_VAR_BIT) != 0u) {
    f->sym[self]    = c->sym & ~WF_VAR_BIT;
    f->is_var[self] = 1u;
  } else if (c->sym == ATPFT_NUM_MARKER && c->arity == 2u) {
    const AtpFtCell *dt_cell  = c->next;
    const AtpFtCell *raw_cell = dt_cell->end->next;
    (void)dt_cell;
    f->sym[self]    = 0x40000000u ^ ft_ctr_sym(raw_cell);
    f->is_var[self] = 0u;
  } else {
    f->sym[self]    = c->sym;
    f->is_var[self] = 0u;
    const AtpFtCell *kid = c->next;
    for (u32 i = 0; i < (u32)c->arity; i++) {
      atp_wmflat_preorder_ft(kid, f);
      kid = kid->end->next;
    }
  }
  f->end[self] = f->n;
}

// Neutral subtree equality: the (sym, is_var) sequences of two flat
// ranges -- exactly the verdict kbo_eq gave on the old node Terms.
static u8 atp_wmflat_range_eq(const AtpWmFlat *a, u32 ai,
                              const AtpWmFlat *b, u32 bi) {
  u32 an = a->end[ai] - ai;
  u32 bn = b->end[bi] - bi;
  if (an != bn) return 0u;
  for (u32 k = 0; k < an; k++) {
    if (a->sym[ai + k] != b->sym[bi + k] ||
        a->is_var[ai + k] != b->is_var[bi + k])
      return 0u;
  }
  return 1u;
}

// One side of MO_TermpaarSubsummiertZweites: walk pattern preorder against the
// subject preorder.  `subst`/`setf` (indexed 0..maxslot) and `*counter` PERSIST
// across the left and right calls (WM resets only once, at MO entry).
// Bindings are (flat, index) pairs into the SUBJECT flats -- with the
// EsFolgt equality decided on the representation-neutral (sym, is_var)
// columns, a Term subject and a span subject match identically.
typedef struct {
  const AtpWmFlat *f;
  u32 i;
} WmfBind;
static u8 atp_wmflat_phase(const AtpWmFlat *pat, const AtpWmFlat *sub,
                           WmfBind *subst, u8 *setf, u32 *counter,
                           u32 maxslot) {
  u32 i = 0, j = 0;
  while (i < pat->n) {
    if (j >= sub->n) return 0;           // subject exhausted, pattern not
    if (pat->is_var[i]) {
      u32 v = pat->sym[i];               // READ slot = variable's own symbol
      if (v >= maxslot) return 0;
      if (!setf[v]) {                    // VarUngebunden(v)
        u32 slot = *counter;             // WRITE slot = appearance counter
        if (slot >= maxslot) return 0;
        (*counter)++;
        subst[slot].f = sub;
        subst[slot].i = j;
        setf[slot] = 1u;                 // the counter-vs-symbol CROSS
      } else if (!atp_wmflat_range_eq(subst[v].f, subst[v].i, sub, j)) {
        return 0;                        // EsFolgt == structural equality
      }
      i = pat->end[i];                   // pattern var leaf -> next symbol
      j = sub->end[j];                   // subject -> skip whole subterm
    } else {                             // function symbol: must agree
      if (sub->is_var[j] || sub->sym[j] != pat->sym[i]) return 0;
      // Arity must agree (the old walk compared term_ctr_n).  On the
      // flat, arity = number of DIRECT children; equal (sym, extent)
      // prefixes guarantee it only if extents match at every level --
      // compare direct-child counts explicitly.
      {
        u32 pc = 0, sc = 0;
        for (u32 k = i + 1u; k < pat->end[i]; k = pat->end[k]) pc++;
        for (u32 k = j + 1u; k < sub->end[j]; k = sub->end[k]) sc++;
        if (pc != sc) return 0;
      }
      i++;                               // descend (advance one symbol each)
      j++;
    }
  }
  return 1;
}

// Flatten one operand from whichever representation the caller holds:
// the span when given (mirrors, lazy pairs), else the Term.
static void atp_wmflat_fill(Term t, const AtpFtCell *span, AtpWmFlat *f) {
  f->n = 0u;
  f->overflow = 0u;
  if (span != NULL) atp_wmflat_preorder_ft(span, f);
  else              atp_wmflat_preorder(t, f);
}

static u8 atp_wmflat_mo_x(Term pl, const AtpFtCell *pl_ft,
                          Term pr, const AtpFtCell *pr_ft,
                          Term sl, const AtpFtCell *sl_ft,
                          Term sr, const AtpFtCell *sr_ft) {
  AtpWmFlat fpl, fpr, fsl, fsr;
  atp_wmflat_fill(pl, pl_ft, &fpl);
  atp_wmflat_fill(pr, pr_ft, &fpr);
  atp_wmflat_fill(sl, sl_ft, &fsl);
  atp_wmflat_fill(sr, sr_ft, &fsr);
  if (fpl.overflow || fpr.overflow || fsl.overflow || fsr.overflow) return 0;
  enum { WMFLAT_SLOTS = 64u };
  WmfBind subst[WMFLAT_SLOTS];
  u8      setf[WMFLAT_SLOTS];
  for (u32 t = 0; t < WMFLAT_SLOTS; t++) setf[t] = 0u;
  u32 counter = 0u;
  if (!atp_wmflat_phase(&fpl, &fsl, subst, setf, &counter, WMFLAT_SLOTS))
    return 0;
  if (!atp_wmflat_phase(&fpr, &fsr, subst, setf, &counter, WMFLAT_SLOTS))
    return 0;
  return 1;
}

static u8 atp_wmflat_mo(Term pl, Term pr, Term sl, Term sr) {
  return atp_wmflat_mo_x(pl, NULL, pr, NULL, sl, NULL, sr, NULL);
}

// WM SubsumptionBody (Subsumption.c:67) over the counter-cross matcher.  Tries
// both PATTERN orientations (Subsumption.c:108-109); thvm orients unorientable
// equations arbitrarily, so both SUBJECT orientations are covered too (WM picks
// the one distinguished direction -- TP_RichtungAusgezeichnet -- which thvm's
// storage may not match).  Peels the subject pair into its unique differing
// subterm, like atp_eq_subsumes_pair.
static u8 atp_wm_flat_subsumes_pair(Term p_lhs, Term p_rhs, Term lhs, Term rhs) {
  for (;;) {
    if (atp_wmflat_mo(p_lhs, p_rhs, lhs, rhs)) return 1;
    if (atp_wmflat_mo(p_rhs, p_lhs, lhs, rhs)) return 1;
    if (atp_wmflat_mo(p_lhs, p_rhs, rhs, lhs)) return 1;
    if (atp_wmflat_mo(p_rhs, p_lhs, rhs, lhs)) return 1;
    if (term_tag(lhs) != TAG_CTR || term_tag(rhs) != TAG_CTR) return 0;
    if (term_ext(lhs) != term_ext(rhs)) return 0;
    u32 n = term_ctr_n(lhs);
    if (term_ctr_n(rhs) != n) return 0;
    u32 diff = n;
    for (u32 i = 0; i < n; i++) {
      if (!kbo_eq(term_ctr_at(lhs, i), term_ctr_at(rhs, i))) {
        if (diff != n) return 0;
        diff = i;
      }
    }
    if (diff == n) return 0;
    lhs = term_ctr_at(lhs, diff);
    rhs = term_ctr_at(rhs, diff);
  }
}

// WM-FAITHFUL distinguished-direction variant of atp_wm_flat_subsumes_pair: the
// new-equation E-set subsumption test WM actually runs (GMSubsummierenMitGleichung,
// INF/Interreduktion.c:251-279).  WM gates the subject on TP_RichtungAusgezeichnet
// (Interreduktion.c:261), so the OLD equation is tested ONLY in its distinguished
// (stored) orientation -- never the reversed face.  SS_TermpaarSubsummiertTermpaar
// (Subsumption.c:104-110) still tries BOTH PATTERN (new-equation) orientations
// against that one fixed subject.  This is exactly atp_wm_flat_subsumes_pair MINUS
// the two subject-swapped combos (rhs,lhs) -- the over-match that the general 4-way
// path adds because thvm stores one arbitrary orientation per equation and tries
// the subject reversed too.  thvm's stored order matches WM's distinguished order
// wherever it matters (the same CP-normalization fixes both), so dropping the two
// subject-reversal attempts is the faithful test.  This retains the Meredith eqn-12
// content `a.((a.b).c) = a.(c.(b.a))` (and its sibling) that the 3-var-swap
// `x.(y.z) = (z.y).x` subsumes only in eqn-12's reversed orientation -- WM keeps
// both, so thvm now keeps them too, with NO content gate.
// WM SS_TermpaarSubsummiertTermpaar, DIRECT tries only (no SubsumptionBody
// descent): the two pattern orientations of the NEW equation against the
// OLD equation's WM-distinguished stored direction.  Behavioral ground
// truth (instrumented wmcli, WM_SUBDUMP/WM_SUBDUMP3):
//   * Sheffer OrAssociativity: commutativity's E-add subsumes E1 and E4 --
//     BOTH are DIRECT slot-semantics matches (the MO-ENTRY trace shows the
//     top-level pair accepted within two EsFolgt calls, no descent).
//   * Meredith OrAssociativity: 37M probe calls, ZERO subsumptions in the
//     whole run -- in particular uid-139 `x0.(x1.x2)=(x2.x1).x0` must NOT
//     subsume E11/E12 (whose descent-peeled pair WOULD match; thvm's
//     descent-bearing variants over-killed them, orphaning the kept
//     #178/#179 CPs -- the fact-178 divergence).
// So the faithful E-set sweep test = direct-only.  The descent-bearing
// variants above remain for the pop-time -ks "s" stage (SS_Termpaar-
// SubsummiertVonGM), which is a different WM call site.
static u8 atp_wm_flat_subsumes_pair_direct(Term p_lhs, Term p_rhs,
                                           Term lhs, Term rhs) {
  if (atp_wmflat_mo(p_lhs, p_rhs, lhs, rhs)) return 1;
  if (atp_wmflat_mo(p_rhs, p_lhs, lhs, rhs)) return 1;
  return 0;
}

// Span-capable sibling: any operand may arrive as a span (mirrors for
// the subjects, the lazy pair for the patterns); the neutral flats
// make the verdict representation-blind.
static u8 atp_wm_flat_subsumes_pair_direct_x(
    Term p_lhs, const AtpFtCell *p_lhs_ft,
    Term p_rhs, const AtpFtCell *p_rhs_ft,
    Term lhs, const AtpFtCell *lhs_ft,
    Term rhs, const AtpFtCell *rhs_ft) {
  if (atp_wmflat_mo_x(p_lhs, p_lhs_ft, p_rhs, p_rhs_ft,
                      lhs, lhs_ft, rhs, rhs_ft)) return 1;
  if (atp_wmflat_mo_x(p_rhs, p_rhs_ft, p_lhs, p_lhs_ft,
                      lhs, lhs_ft, rhs, rhs_ft)) return 1;
  return 0;
}

static u8 atp_wm_flat_subsumes_pair_distdir(Term p_lhs, Term p_rhs,
                                            Term lhs, Term rhs) {
  for (;;) {
    if (atp_wmflat_mo(p_lhs, p_rhs, lhs, rhs)) return 1;
    if (atp_wmflat_mo(p_rhs, p_lhs, lhs, rhs)) return 1;
    if (term_tag(lhs) != TAG_CTR || term_tag(rhs) != TAG_CTR) return 0;
    if (term_ext(lhs) != term_ext(rhs)) return 0;
    u32 n = term_ctr_n(lhs);
    if (term_ctr_n(rhs) != n) return 0;
    u32 diff = n;
    for (u32 i = 0; i < n; i++) {
      if (!kbo_eq(term_ctr_at(lhs, i), term_ctr_at(rhs, i))) {
        if (diff != n) return 0;
        diff = i;
      }
    }
    if (diff == n) return 0;
    lhs = term_ctr_at(lhs, diff);
    rhs = term_ctr_at(rhs, diff);
  }
}

// WM -ks "s" pop-time E-subsumption test (SS_TermpaarSubsummiertVonGM,
// INF/Subsumption.c:91-104).  Returns 1 if `(lhs, rhs)` is subsumed by
// a live unorientable equation under the SubsumptionBody semantics
// above.  Both orientations of each equation are tried, mirroring WM's
// Gleichung/Antigleichung twin storage in the Gleichungsbaum
// (RUndEVerwaltung.c:407-470: both directions are separately indexed,
// so MO_SubsummierendeGleichungGefunden sees each equation twice).
// Rules (r_orient[k] == 1) never participate -- WM consults
// RE_Gleichungsbaum only.  Caller guarantees lhs != rhs (the joined
// drop ran) and compare(lhs, rhs) == KBO_UN (WM gates the stage on
// Unvergleichbar, KPVerwaltung.c:667).
static u8 atp_pop_eq_subsumed(AtpState *s, Term lhs, Term rhs) {
  if (s->n_unorient == 0u) return 0;       // RE_GleichungsmengeLeer
  for (u32 k = 0; k < s->n_rules; k++) {
    if (s->r_orient[k]) continue;                    // E only, never R
    if (s->r_dead != NULL && s->r_dead[k]) continue;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // Sentinel pattern rows materialize (bounded by the live E-set)
    // so a subsumption is never missed behind a lazy slot.
    (void)atp_rule_term(s, k, 0u);
    (void)atp_rule_term(s, k, 1u);
#endif
    if (atp_eq_subsumes_pair(s->lhs[k], s->rhs[k], lhs, rhs)) {
      static int sub_trace = -1;
      if (sub_trace < 0) sub_trace = atp_env_on("THVM_ATP_POP_TRACE");
      if (sub_trace) {
        fprintf(stderr, "POPSUBR pick=%u subsumer_slot=%u orient=%u lhs=",
                s->cp_select_count, k, s->r_orient[k]);
        atp_dbg_print_term(stderr, s->lhs[k]); fputs(" rhs=", stderr);
        atp_dbg_print_term(stderr, s->rhs[k]);
        fputs(" query_lhs=", stderr); atp_dbg_print_term(stderr, lhs);
        fputs(" query_rhs=", stderr); atp_dbg_print_term(stderr, rhs);
        fputc('\n', stderr);
      }
      return 1;
    }
  }
  return 0;
}

#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
// The pop-subsume FT route is exact only for the plain-KBO order
// config -- atp_compare dispatches col3 / AC / WPO / RPO / LPO first,
// and only the final thvm_kbo has the certified FT sibling
// (thvm_kbo_ft = the identical Loechner decision over the flat
// encode, the same verdict the FT normalize order gates rely on).
static u8 atp_order_is_plain_kbo(const AtpState *s) {
  if (s->col3_apply != 0u) return 0;
#ifdef THVM_ATP_AC
  if (thvm_atp_get_ac_mask() != 0ull) return 0;
#endif
  if (s->wpo != NULL || s->rpo != NULL || s->lpo != NULL) return 0;
  return 1;
}

// Order-independent node-symbol fingerprint equality -- a NECESSARY
// condition for every atp_cp_perm_subsumed positive (both the
// AC-dispensable route, whose atp_ac_eq gate preserves the symbol
// multiset by construction, and the mask-0 binary-swap recursion,
// which only permutes children).  The pop treatment uses a MISMATCH
// as proof the perm gate returns 0, unlocking the FT pop-subsume
// verdict without materializing Terms.  Commutative accumulators
// (count / keyed sum / keyed xor) so any permutation fingerprints
// identically.
static u8 ft_sym_multiset_eq(const AtpFtCell *a, const AtpFtCell *b) {
  u64 acc[2][3] = {{0, 0, 0}, {0, 0, 0}};
  const AtpFtCell *span[2] = { a, b };
  for (u32 t = 0; t < 2u; t++) {
    const AtpFtCell *e = (span[t]->end != NULL) ? span[t]->end->next : NULL;
    for (const AtpFtCell *p = span[t]; p != e; p = p->next) {
      u64 h = (u64)p->sym * 0x9E3779B97F4A7C15ull;
      acc[t][0]++;
      acc[t][1] += h;
      acc[t][2] ^= h * ((h >> 32) | 1ull);
    }
  }
  return acc[0][0] == acc[1][0] && acc[0][1] == acc[1][1] &&
         acc[0][2] == acc[1][2];
}

// FT sibling of atp_eq_subsumes_pair: the identical SubsumptionBody
// walk on cells.  ft_match threads bindings across the two side
// matches exactly like thvm_match's RewriteSubst (Stage-5 verdict
// parity), ft_eq stands in for kbo_eq, and the unique-differing-child
// descent follows the span sibling chain.  A TAG_NUM pair maps to
// marker cells; the Term walk stops at non-CTR tags, so the marker
// sym stops the descent here identically.
static u8 atp_eq_subsumes_pair_ft(const AtpFtCell *p_lhs,
                                  const AtpFtCell *p_rhs,
                                  const AtpFtCell *lhs,
                                  const AtpFtCell *rhs) {
  AtpFtSubst subst;
  for (;;) {
    memset(&subst, 0, sizeof(subst));
    if (ft_match(p_lhs, lhs, &subst) && ft_match(p_rhs, rhs, &subst)) {
      return 1;
    }
    memset(&subst, 0, sizeof(subst));
    if (ft_match(p_lhs, rhs, &subst) && ft_match(p_rhs, lhs, &subst)) {
      return 1;
    }
    if (ft_is_var(lhs) || ft_is_var(rhs)) return 0;
    if (ft_ctr_sym(lhs) != ft_ctr_sym(rhs)) return 0;
    if (ft_ctr_sym(lhs) == ATPFT_NUM_MARKER) return 0;  // TAG_NUM stop
    u32 n = (u32)lhs->arity;
    if ((u32)rhs->arity != n) return 0;
    const AtpFtCell *cl = lhs->next, *cr = rhs->next;
    const AtpFtCell *dl = NULL, *dr = NULL;
    u32 ndiff = 0u;
    for (u32 i = 0; i < n; i++) {
      if (!ft_eq(cl, cr)) {
        if (ndiff != 0u) return 0;        // a second differing child
        ndiff = 1u;
        dl = cl;
        dr = cr;
      }
      cl = cl->end->next;
      cr = cr->end->next;
    }
    if (ndiff == 0u) return 0;            // identical pair: unreachable
    lhs = dl;
    rhs = dr;
  }
}

#ifdef THVM_ATP_AC
// FT transliteration of the atp_gj_perm_valuable shape tests (WM
// PROTECT_3_PERMS): var ids read off cells, binary decompose via the
// span sibling chain.  Same shapes, same id equalities.
static u8 ft_gj_var_id(const AtpFtCell *t, u32 *id) {
  if (!ft_is_var(t)) return 0;
  *id = ft_var_id(t);
  return 1;
}
static u8 ft_gj_bin(const AtpFtCell *t, u32 f,
                    const AtpFtCell **a, const AtpFtCell **b) {
  if (ft_is_var(t) || ft_ctr_sym(t) != f || t->arity != 2u) return 0;
  *a = t->next;
  *b = (*a)->end->next;
  return 1;
}
static u8 ft_gj_shape_rr(const AtpFtCell *t, u32 f, u32 *x, u32 *y, u32 *z) {
  const AtpFtCell *a, *b, *c, *d;
  if (!ft_gj_bin(t, f, &a, &b) || !ft_gj_bin(b, f, &c, &d)) return 0;
  return ft_gj_var_id(a, x) && ft_gj_var_id(c, y) && ft_gj_var_id(d, z);
}
static u8 ft_gj_shape_ll(const AtpFtCell *t, u32 f, u32 *x, u32 *y, u32 *z) {
  const AtpFtCell *a, *b, *c, *d;
  if (!ft_gj_bin(t, f, &a, &b) || !ft_gj_bin(a, f, &c, &d)) return 0;
  return ft_gj_var_id(c, x) && ft_gj_var_id(d, y) && ft_gj_var_id(b, z);
}
static u8 atp_gj_perm_valuable_ft(const AtpFtCell *l, const AtpFtCell *r) {
  if (ft_is_var(l) || l->arity != 2u) return 0;
  u32 f = ft_ctr_sym(l);
  const AtpFtCell *p, *q, *p2, *q2;
  u32 a, b, c, d, x, y, z, u, v, w;
  // C: f(x1, x2) = f(x2, x1)
  if (ft_gj_bin(l, f, &p, &q) && ft_gj_bin(r, f, &p2, &q2) &&
      ft_gj_var_id(p, &a) && ft_gj_var_id(q, &b) &&
      ft_gj_var_id(p2, &c) && ft_gj_var_id(q2, &d) &&
      a != b && c == b && d == a) {
    return 1;
  }
  // A: f(f(x1,x2),x3) = f(x1,f(x2,x3)), either nesting on either side.
  if (ft_gj_shape_ll(l, f, &x, &y, &z) && ft_gj_shape_rr(r, f, &u, &v, &w) &&
      x == u && y == v && z == w && x != y && y != z && x != z) {
    return 1;
  }
  if (ft_gj_shape_rr(l, f, &x, &y, &z) && ft_gj_shape_ll(r, f, &u, &v, &w) &&
      x == u && y == v && z == w && x != y && y != z && x != z) {
    return 1;
  }
  // C': f(x1,f(x2,x3)) = one of the four rotated right-nested perms.
  if (ft_gj_shape_rr(l, f, &x, &y, &z) && ft_gj_shape_rr(r, f, &u, &v, &w) &&
      x != y && y != z && x != z) {
    if ((u == z && v == x && w == y) ||
        (u == y && v == x && w == z) ||
        (u == z && v == y && w == x) ||
        (u == y && v == z && w == x)) {
      return 1;
    }
  }
  return 0;
}

// FT sibling of atp_cp_ac_dispensable + the pop perm gate dispatch
// (mask != 0 is the caller's condition, mirroring the legacy gate):
// ft_eq for the t=t upstream guard, ft_ac_eq for TO_ACGleich, the
// mask test for SO_IstACSymbol, and the shape port above for
// GZ_wertvoll.
static u8 atp_cp_perm_subsumed_ft(const AtpFtCell *lhs,
                                  const AtpFtCell *rhs) {
  AtpAcInfo ac = { .ac_mask = g_atp_perm_subsume_mask };
  if (ft_eq(lhs, rhs)) return 0;
  if (!ft_ac_eq(lhs, rhs, &ac)) return 0;
  if (ft_is_var(lhs) || !ft_ac_eq_is_ac_label(&ac, ft_ctr_sym(lhs))) return 1;
  return !atp_gj_perm_valuable_ft(lhs, rhs);
}
#endif // THVM_ATP_AC

// FT sibling of atp_pop_eq_subsumed: the E-set sweep over the live
// rule MIRRORS (lockstep with the Term sides).  2 = a live equation
// has no mirror (defensive; post-mirror-ensure this cannot happen) --
// the caller falls back to the Term route rather than mis-verdict.
static int atp_pop_eq_subsumed_ft(AtpState *s, const AtpFtCell *lhs,
                                  const AtpFtCell *rhs) {
  if (s->n_unorient == 0u) return 0;       // RE_GleichungsmengeLeer
  for (u32 k = 0; k < s->n_rules; k++) {
    if (s->r_orient[k]) continue;                    // E only, never R
    if (s->r_dead != NULL && s->r_dead[k]) continue;
    if (s->lhs_ft[k] == NULL || s->rhs_ft[k] == NULL) return 2;
    if (atp_eq_subsumes_pair_ft(s->lhs_ft[k], s->rhs_ft[k], lhs, rhs)) {
      static int sub_trace = -1;
      if (sub_trace < 0) sub_trace = atp_env_on("THVM_ATP_POP_TRACE");
      if (sub_trace) {
        Term ql = ft_to_term(lhs), qr = ft_to_term(rhs);
        fprintf(stderr, "POPSUBR pick=%u subsumer_slot=%u orient=%u lhs=",
                s->cp_select_count, k, s->r_orient[k]);
        atp_dbg_print_term(stderr, s->lhs[k]); fputs(" rhs=", stderr);
        atp_dbg_print_term(stderr, s->rhs[k]);
        fputs(" query_lhs=", stderr); atp_dbg_print_term(stderr, ql);
        fputs(" query_rhs=", stderr); atp_dbg_print_term(stderr, qr);
        fputc('\n', stderr);
      }
      return 1;
    }
  }
  return 0;
}
#endif // THVM_ATPFT_NORM && THVM_ATPFT_RULES

// --- WM commutativity-aware E-set subsumption (THVM_ATP_COMM_SUBSUME) ---
// Two terms are equal modulo ONE top-level commutativity swap: either they
// are structurally identical (compared via atp_term_struct_hash, the same
// fingerprint kbo_eq short-circuits on), OR both are 2-ary constructor
// applications of the SAME constructor whose children match when the
// candidate's two children are transposed (a.b == b.a -- a single top swap,
// no deeper / AC enumeration).  Mirrors WM's commutativity-normalised
// Interreduktion, NOT AC saturation.
static u8 atp_term_eq_mod_top_comm(Term a, Term b) {
  if (atp_term_struct_hash(a) == atp_term_struct_hash(b) && kbo_eq(a, b))
    return 1;
  if (term_tag(a) != TAG_CTR || term_tag(b) != TAG_CTR) return 0;
  if (term_ext(a) != term_ext(b)) return 0;
  if (term_ctr_n(a) != 2u || term_ctr_n(b) != 2u) return 0;
  return (u8)(kbo_eq(term_ctr_at(a, 0), term_ctr_at(b, 1)) &&
              kbo_eq(term_ctr_at(a, 1), term_ctr_at(b, 0)));
}

// Is constructor `ctr` live-commutative in the current rule/equation set?
// True iff some LIVE (non-dead) equation slot stores `ctr(V0,V1)=ctr(V1,V0)`
// in EITHER orientation (the commutativity axiom for `ctr`).  Walks the same
// per-slot eset/rule arrays atp_eset_subsume_by_new and atp_pop_eq_subsumed
// iterate (n_rules x lhs/rhs, r_orient/r_dead).
static u8 atp_comm_eq_for_ctr(Term l, Term r, u32 ctr) {
  if (term_tag(l) != TAG_CTR || term_tag(r) != TAG_CTR) return 0;
  if (term_ext(l) != ctr || term_ext(r) != ctr) return 0;
  if (term_ctr_n(l) != 2u || term_ctr_n(r) != 2u) return 0;
  Term l0 = term_ctr_at(l, 0), l1 = term_ctr_at(l, 1);
  Term r0 = term_ctr_at(r, 0), r1 = term_ctr_at(r, 1);
  if (term_tag(l0) != TAG_FVR || term_tag(l1) != TAG_FVR) return 0;
  if (term_tag(r0) != TAG_FVR || term_tag(r1) != TAG_FVR) return 0;
  // f(a,b) = f(b,a) with a != b, a/b matched across the swap.
  if (term_ext(l0) == term_ext(l1)) return 0;
  return (u8)(term_ext(l0) == term_ext(r1) && term_ext(l1) == term_ext(r0));
}

static u8 atp_op_is_live_commutative(AtpState *s, u32 ctr) {
  for (u32 k = 0; k < s->n_rules; k++) {
    if (s->r_dead != NULL && s->r_dead[k]) continue;
    // Either stored orientation expresses commutativity.
    if (atp_comm_eq_for_ctr(s->lhs[k], s->rhs[k], ctr)) return 1;
    if (atp_comm_eq_for_ctr(s->rhs[k], s->lhs[k], ctr)) return 1;
  }
  return 0;
}

// Commutativity-aware pair subsumption: candidate (cl,cr) is subsumed by the
// new equation (rl,rr) when, in EITHER orientation of the candidate, rl
// matches the candidate's lhs EXACTLY (struct hash / kbo_eq) AND rr equals
// the candidate's rhs modulo ONE top-comm swap, AND the candidate's top
// constructor is live-commutative.  This drops soa slot15
// `x.(y.x)=(y.y).x` once eqn-10 `x.(y.x)=x.(y.y)` enters -- same LHS, RHS
// one top-`.`-swap apart, with `.` live-commutative.
static u8 atp_eq_subsumes_pair_mod_top_comm(AtpState *s, Term rl, Term rr,
                                            Term cl, Term cr) {
  // Forward orientation: candidate stored as (cl, cr).
  if (term_tag(cl) == TAG_CTR && term_ctr_n(cl) == 2u &&
      atp_op_is_live_commutative(s, term_ext(cl)) &&
      kbo_eq(rl, cl) && atp_term_eq_mod_top_comm(rr, cr))
    return 1;
  // Swapped orientation: candidate stored as (cr, cl).
  if (term_tag(cr) == TAG_CTR && term_ctr_n(cr) == 2u &&
      atp_op_is_live_commutative(s, term_ext(cr)) &&
      kbo_eq(rl, cr) && atp_term_eq_mod_top_comm(rr, cl))
    return 1;
  return 0;
}

// WM E-set subsumption destroy on new-equation entry
// (GMSubsummierenMitGleichung, INF/Interreduktion.c:251-274; reached
// from IR_InterreduktionLinks :371-373 only when the new fact is an
// EQUATION, BEFORE RE_FaktumEinfuegen inserts it into GM --
// ArbeitsAufnahme, INF/Hauptkomponenten.c:311-312, so it can never
// subsume itself).  Every existing live E-member whose pair the new
// equation subsumes (SS_TermpaarSubsummiertTermpaar at :262, gated on
// the distinguished direction TP_RichtungAusgezeichnet at :261 -- one
// test per stored equation, both pattern orientations inside the
// matcher) is removed from GM and physically destroyed, twin included
// (FinaleKillprozSubsumption, :236-245).  NO requeue, no CP made.
//
// thvm stores ONE slot per equation (no Gleichung/Antigleichung twin
// under ATP_ORDERED_REWRITE), so a single soft-delete covers both
// directions: the bwd-subsume dead-sentinel recipe (sentinel faces
// make thvm_match / thvm_unify return 0, so every rewrite /
// CP-generation site skips the slot; originals go to the save slots
// for proof reconstruction).  n_unorient is NOT decremented --
// matching the bwd-subsume convention, it stays an upper bound on
// live unorientables and dead slots are skipped via r_dead/sentinel.
static void atp_eset_subsume_by_new(AtpState *s, u32 new_i) {
  Term new_lhs = s->lhs[new_i];
  Term new_rhs = s->rhs[new_i];
  Term dead_sentinel = term_new(0, TAG_FVR, 255u, 0);
  static int eset_mdiff = -1;
  if (eset_mdiff < 0) eset_mdiff = atp_env_on("THVM_ATP_ESET_MIRROR_DIFF");
  for (u32 i = 0; i < new_i; i++) {
    if (s->r_orient[i]) continue;   // E only (RE_forGleichungenRobust)
    if (s->r_dead[i]) continue;
    if (eset_mdiff && s->lhs_ft[i] != NULL && s->rhs_ft[i] != NULL) {
      // Mirror-integrity differ: the subject mirror must still be the
      // exact image of the (materialized) rule Terms -- a mismatch
      // means a span-lifetime alias corrupted it (the CPQ hunt).
      Term dl = atp_rule_term(s, i, 0u);
      Term dr = atp_rule_term(s, i, 1u);
      if (!ft_eq_term(s->lhs_ft[i], dl) || !ft_eq_term(s->rhs_ft[i], dr)) {
        fprintf(stderr, "ESET_MIRROR_DIFF: slot %u mirror != Terms\n", i);
        abort();
      }
    }
    u8 subsumed;
    if (s->use_flat_subsume) {
      // WM GMSubsummierenMitGleichung: DIRECT slot-semantics tries only
      // (see atp_wm_flat_subsumes_pair_direct), against the OLD equation's
      // WM-DISTINGUISHED direction (TP_RichtungAusgezeichnet -- thvm's
      // storage may be flipped; dist-correct via the wmo dist flag).
      // THVM_ATP_ESET_DESCENT=1 restores the descent-bearing variants
      // (the historical port) as a bisection kill switch.
      static int eset_descent = -1;
      if (eset_descent < 0)
        eset_descent = getenv("THVM_ATP_ESET_DESCENT") != NULL ? 1 : 0;
      if (eset_descent) {
        subsumed = s->use_eset_distdir
            ? atp_wm_flat_subsumes_pair_distdir(new_lhs, new_rhs,
                                                s->lhs[i], s->rhs[i])
            : atp_wm_flat_subsumes_pair(new_lhs, new_rhs,
                                        s->lhs[i], s->rhs[i]);
      } else {
        // Both subject orientations (thvm's stored side order does not
        // reliably encode WM's Ausgezeichnet direction); still DIRECT-only.
        // The Meredith over-accept needed the DESCENT on top of the swap;
        // direct tries reject it in both orientations (hand-verified),
        // while OA's two genuine kills and Sheffer's E1/E4 all accept
        // directly in one of the four tries.
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
        // Subjects flatten from the MIRRORS -- exact for real rows and
        // the only valid read for lazy sentinels.
        subsumed = atp_wm_flat_subsumes_pair_direct_x(
                       new_lhs, s->lhs_ft[new_i], new_rhs, s->rhs_ft[new_i],
                       s->lhs[i], s->lhs_ft[i], s->rhs[i], s->rhs_ft[i])
                || atp_wm_flat_subsumes_pair_direct_x(
                       new_lhs, s->lhs_ft[new_i], new_rhs, s->rhs_ft[new_i],
                       s->rhs[i], s->rhs_ft[i], s->lhs[i], s->lhs_ft[i]);
#else
        subsumed = atp_wm_flat_subsumes_pair_direct(new_lhs, new_rhs,
                                                    s->lhs[i], s->rhs[i])
                || atp_wm_flat_subsumes_pair_direct(new_lhs, new_rhs,
                                                    s->rhs[i], s->lhs[i]);
#endif
      }
    } else {
      subsumed = atp_eq_subsumes_pair(new_lhs, new_rhs, s->lhs[i], s->rhs[i]);
    }
    // Commutativity-aware widening (THVM_ATP_COMM_SUBSUME, DEFAULT OFF):
    // after the plain test, also drop the candidate when the new equation
    // subsumes it modulo ONE top-comm swap under a live commutativity axiom.
    u8 comm_subsumed = 0;
    if (!subsumed && s->use_comm_subsume &&
        atp_eq_subsumes_pair_mod_top_comm(s, new_lhs, new_rhs,
                                          s->lhs[i], s->rhs[i])) {
      subsumed = 1;
      comm_subsumed = 1;
    }
    if (!subsumed)
      continue;
    s->r_dead_lhs_save[i] = s->lhs[i];   // sentinel rides; save_ft is authoritative
    s->r_dead_rhs_save[i] = s->rhs[i];
    // Remove-and-re-derive root-overlap ownership: save the subsuming
    // equation's SHAPE on this slot.  Its final birth trace is not yet
    // stamped here (the step loop stamps r_trace after orient_and_add
    // returns), so the cutoff is resolved lazily at re-derivation time
    // by locating the live rule of this shape.  When an equation of the
    // removed slot's shape is later re-derived (atp_push_rule), it then
    // suppresses root overlaps against any rule born at or before the
    // subsumer -- WM already enumerated those roots in the subsumer's
    // original batch against this (now-removed) equation.
    s->r_dead_subsumer_lhs[i] = new_lhs;
    s->r_dead_subsumer_rhs[i] = new_rhs;
#if defined(THVM_ATPFT_NORM) && defined(THVM_ATPFT_RULES)
    // Subsumer shape as spans: clones of the new row's mirrors (the
    // Term pair above may be lazy sentinels; the locate in
    // atp_push_rule prefers these when present).
    if (s->lhs_ft[new_i] != NULL && s->rhs_ft[new_i] != NULL) {
      AtpFt *sra = (AtpFt *)s->ft_arena_ptr;
      s->r_dead_subsumer_lhs_ft[i] =
          ft_clone_span(sra, s->lhs_ft[new_i], 0);
      s->r_dead_subsumer_rhs_ft[i] =
          ft_clone_span(sra, s->rhs_ft[new_i], 0);
    }
#endif
    s->lhs[i] = dead_sentinel;
    s->rhs[i] = dead_sentinel;
    s->r_dead[i] = 1;
    // WM destroys the subsumed equation outright, and the removal
    // (RE_GleichungEntfernen, RUndEVerwaltung.c:503) runs
    // KPV_KillParent on the distinguished direction -- the victim's
    // queued CPs die as orphans at pop (selectNonOrphan), consuming
    // no selection.  Mirror via the lazy trace-dead mark.
    if (atp_rule_trace_on()) {
      fprintf(stderr, "  ESET-SUBSUME kill (slot %u, trace %u) lhs=",
              i, s->r_trace[i]);
      atp_dbg_print_term(stderr, s->r_dead_lhs_save[i]);
      fputs(" rhs=", stderr);
      atp_dbg_print_term(stderr, s->r_dead_rhs_save[i]);
      fputc('\n', stderr);
    }
    if (s->use_orphan_murder) atp_trace_mark_dead(s, s->r_trace[i]);
    if (s->r_uid != NULL) atp_uid_mark_dead(s, s->r_uid[i]);
    // WM order mirror: the subsumed equation's faces leave the tree
    // (RE_GleichungEntfernen via FinaleKillprozSubsumption).  Keyed by
    // birth uid like every other remove site -- the mirror never sees
    // trace-space ids (a former r_trace[i] here left the dead faces in
    // the tree and, on numeric collision, evicted a live uid instead).
    if (s->use_emission_order) atp_wmo_remove_trace(s, s->r_uid[i]);
    // The active E set changed: invalidate the IR-normalize cookie
    // and force the rule/unorient index rebuild (a revision delta
    // that exceeds the rule-count delta is never pure-append).
    s->r_revision++;
    s->n_eqs_dropped_eset_subsumed++;
    if (comm_subsumed) s->n_eqs_dropped_comm_subsumed++;
#ifdef THVM_ATPFT_RULES
    s->r_dead_lhs_save_ft[i] = s->lhs_ft[i];
    s->r_dead_rhs_save_ft[i] = s->rhs_ft[i];
    s->lhs_ft[i] = ftnew_var((AtpFt *)s->ft_arena_ptr, 255u, 0);
    s->rhs_ft[i] = ftnew_var((AtpFt *)s->ft_arena_ptr, 255u, 0);
#endif
#ifdef ATP_RULE_INDEX
    s->rule_index_dirty = 1u; s->wmfpa_dirty = 1u;
#endif
  }
}

// Stage 7.3b: queue subsumption check.  Returns 1 if the candidate
// `(lhs, rhs)` is a substitution instance of some queued CP
// the CP unpacked from `s->cp_packed[k]` -- i.e., there is σ such
// that `(lhs, rhs) = (σ qs[k], σ qt[k])` (or symmetric).
//
// Genuinely orthogonal to 7.1: the queue does not participate in
// `thvm_rewrite_normalize`, so a CP can be queue-subsumed without
// being trivially-joinable (and vice versa).  Used as a real
// filter: the candidate is dropped before reaching the queue.
//
// Cost: O(|queue| * |term|) per candidate; cheap relative to a
// `thvm_rewrite_normalize` because matching has no fixed-point
// loop.
//
// 7d: with -DATP_FV_INDEX the O(n_cps) scan is replaced by an
// FV-index retrieval -- componentwise-dominated FVs are pulled from
// the trie and the SAME two-sided thvm_match runs only on those.
// The FV filter is a sound over-approximation, so the verdict is
// IDENTICAL to the scan, CP for CP.
#if defined(ATP_FV_INDEX)
static u8 atp_cp_queue_subsumed(AtpState *s, Term lhs, Term rhs) {
  if (s->n_cps == 0) return 0;
  return atp_fv_index_query(s->fv_index, lhs, rhs);
}
#else
static u8 atp_cp_queue_subsumed(AtpState *s, Term lhs, Term rhs) {
  for (u32 k = 0; k < s->n_cps; k++) {
    // Deferred-CP slot (cp_packed[k] == NULL): never matched against --
    // WM has no queue-vs-queue subsumption, so its implicit passive set
    // never participates; the FV-index variant gets the same verdict by
    // never indexing implicit slots.
    if (s->cp_packed[k] == NULL) continue;
    // Match the queued CP straight off its packed byte string.
    // Forward: σqs = lhs AND σqt = rhs (one σ).
    RewriteSubst fwd = {{0}};
    if (acp_match_pair(s->cp_packed[k], lhs, rhs, &fwd)) return 1;
    // Symmetric.
    RewriteSubst sym = {{0}};
    if (acp_match_pair(s->cp_packed[k], rhs, lhs, &sym)) return 1;
  }
  return 0;
}
#endif


