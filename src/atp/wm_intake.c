// Waldmeister loader-level axiom canonicalization + intake semantics.
//
// WM does NOT process the initial equations in specification order.
// Its spec loader (SO_InitAposteriori, WASIC/SymbolOperationen.c:348,
// called with the `-symbnorm` default TRUE, RUN/Parameter.c:458/:1453)
// runs SN_SpezifikationAuswerten(TRUE) (WASIC/SpezNormierung.c:758-791):
//
//   1. SymboldatenSammeln (:239-355): per-symbol statistics over the
//      WHOLE spec (equations + conclusions, original sides):
//      occurrence counts, a prime-power position code, two combined-
//      context families, and a parent-context "final" score.
//   2. SymbolReihenfolgeFestlegen (:487-511): stable bubble sort of
//      the signature by SymbolVergleich (:375-391) -- an 11-key
//      lexicographic chain over those statistics -- yielding each
//      symbol's canonical position (PositionVon).
//   3. LRSortieren (:536-612): per equation, compare the two sides
//      with LRSortierenRek (:517-534, variables as jokers, head
//      symbols by SymbolVergleich, then argument-wise left to right)
//      and SWAP so the canonically-smaller side is stored first.
//   4. GleichungenVariablenNormieren (:636-664): per equation,
//      renumber variables in first-occurrence order over (sorted lhs,
//      rhs) -- codes -1, -2, ...
//   5. GleichungenSortieren (:692-752): stable bubble sort of the
//      equation LIST by LRSymbSortierenRek (:666-689) on the sorted
//      LHS, ties broken on the RHS; variables compare by normalized
//      code (a LATER first occurrence is Kleiner), var < non-var,
//      non-vars by canonical symbol position, then argument-wise.
//
// The sorted list is what reaches the engine: GleichungenUndZielePuffern
// (ANA/PhilMarlow.c:69-94) buffers PM_Gleichungen from the sorted
// arrays, HK_GleichungenUndZieleHolen (INF/Hauptkomponenten.c:171-191)
// feeds them in that order to KPV_CPinitial (INF/KPVerwaltung.c:419-433)
// -> recentCPinsert(History = Initial) -> C_Classify
// (CLAS/NewClassification.c:315-377), where the `-clas` default
// "initial=ultimate" (RUN/Parameter.c:165-167) maps Initial to
// Act_ultimate: w1 = minimalWeight() = INT32_MIN (general.h:122) and
// w2 = ++CPNr (:326) stamped in SORTED order.  Net effect: every
// axiom pops before any derived CP, in canonical-sort FIFO order.
//
// thvm port: the engine buffers nothing -- axioms are already queued
// as CPs when the first thvm_atp_step runs.  atp_wm_intake_canonicalize
// (hooked there, gated on use_wm_intake_order) recomputes WM's
// canonical order from the queued TRACE_AXIOM rows + the registered
// goal conjuncts (= WM's conclusions), PERMUTES the axiom slots'
// cp_seq stamps into that order, and tags them ultimate (the
// atp_cp_before comparator orders ultimate CPs by cp_seq alone --
// WM's w1 = INT32_MIN makes the (w1, w2) key degenerate to w2).  The
// stored pairs are NOT side-swapped: orientation recomputes the
// direction at pop, and the side order only feeds the sort keys here
// (computed on the swapped pair exactly as WM stores it).
//
// Both intake surfaces (the bench .pr loader and the WL TFindProof
// encoder) reach the queue through thvm_atp_add_equation, so this one
// engine-level hook canonicalizes both identically.

#define ATP_WMI_MAX_VARS 64u

// --- per-symbol statistics (kodierung_typ = uint32_t in WM; all
// --- arithmetic wraps mod 2^32 exactly like WM's) ------------------
typedef struct {
  u32 arity   [WALD_MAX_SYMBOLS];  // Stelligkeit
  u32 occ_eq  [WALD_MAX_SYMBOLS];  // AnzVorkGleichungen
  u32 occ_goal[WALD_MAX_SYMBOLS];  // AnzVorkZiele
  u32 pos_eq  [WALD_MAX_SYMBOLS];  // PositionenGleichungen
  u32 pos_goal[WALD_MAX_SYMBOLS];  // PositionenZiele
  u32 kk_eq      [WALD_MAX_SYMBOLS];  // KombiKontextGleichungen
  u32 kk_eq_one  [WALD_MAX_SYMBOLS];  // ...GleichungenEinzeln
  u32 kk_eq_opp  [WALD_MAX_SYMBOLS];  // ...GleichungenEinzelnGegenueber
  u32 kk_goal    [WALD_MAX_SYMBOLS];  // KombiKontextZiele
  u32 kk_goal_one[WALD_MAX_SYMBOLS];  // ...ZieleEinzeln
  u32 kk_goal_opp[WALD_MAX_SYMBOLS];  // ...ZieleEinzelnGegenueber
  u32 finale  [WALD_MAX_SYMBOLS];  // FinaleBewertung
  u8  present [WALD_MAX_SYMBOLS];
  u32 pos_of  [WALD_MAX_SYMBOLS];  // PositionVon (canonical rank)
} AtpWmiStats;

// VorkommensDatenAusTermSammeln (SpezNormierung.c:118-134): count
// function-symbol occurrences; also the one walk that registers
// presence + arity for the symbol table.
static void atp_wmi_occ(AtpWmiStats *st, Term t, u32 *field) {
  if (term_tag(t) != TAG_CTR) return;
  u32 f = term_ext(t);
  u32 n = term_ctr_n(t);
  if (f < WALD_MAX_SYMBOLS) {
    field[f] += 1u;
    st->present[f] = 1u;
    st->arity[f]   = n;
  }
  for (u32 i = 0; i < n; i++) atp_wmi_occ(st, term_ctr_at(t, i), field);
}

// PositionsDatenAusTermSammeln (:136-153): each occurrence adds the
// product over its root path of Primzahlen[depth]^(1-based arg index);
// the walk truncates below depth 15 (out of primes).  WM computes the
// power via libm pow cast to uint32_t; an exact u64 integer power,
// truncated the same way, matches it for every value pow represents
// exactly (all spec-sized terms).
static const u32 atp_wmi_primes[15] =
  {2u,3u,5u,7u,11u,13u,17u,19u,23u,29u,31u,37u,41u,43u,47u};

static u32 atp_wmi_prime_pow(u32 p, u32 e) {
  u64 v = 1u;
  for (u32 k = 0; k < e; k++) v *= (u64)p;
  return (u32)v;
}

static void atp_wmi_pos(AtpWmiStats *st, Term t, u32 *field,
                        u32 position, u32 depth) {
  if (depth >= 15u) return;
  if (term_tag(t) != TAG_CTR) return;
  u32 n = term_ctr_n(t);
  for (u32 i = n; i > 0u; i--) {
    atp_wmi_pos(st, term_ctr_at(t, i - 1u), field,
                position * atp_wmi_prime_pow(atp_wmi_primes[depth], i),
                depth + 1u);
  }
  u32 f = term_ext(t);
  if (f < WALD_MAX_SYMBOLS) field[f] += position;
}

// TermKontext (:155-172): accumulating context walk.  A variable is
// context+1; a function node adds its occurrence + position data, then
// folds its arguments RIGHT to LEFT, each child receiving the running
// context and its RETURN added on top (superlinear by design).
static u32 atp_wmi_ctx(const AtpWmiStats *st, Term t, u32 context) {
  if (term_tag(t) != TAG_CTR) return context + 1u;
  u32 f = term_ext(t);
  if (f < WALD_MAX_SYMBOLS) {
    context += st->occ_eq[f] + st->occ_goal[f]
             + st->pos_eq[f] + st->pos_goal[f];
  }
  for (u32 i = term_ctr_n(t); i > 0u; i--) {
    context += atp_wmi_ctx(st, term_ctr_at(t, i - 1u), context);
  }
  return context;
}

// TermKontext2 (:174-187): same recursion over the combined-context
// totals instead of the occurrence/position data.
static u32 atp_wmi_ctx2(const AtpWmiStats *st, Term t, u32 context) {
  if (term_tag(t) != TAG_CTR) return context + 1u;
  u32 f = term_ext(t);
  if (f < WALD_MAX_SYMBOLS) context += st->kk_eq[f] + st->kk_goal[f];
  for (u32 i = term_ctr_n(t); i > 0u; i--) {
    context += atp_wmi_ctx2(st, term_ctr_at(t, i - 1u), context);
  }
  return context;
}

// FinaleSymbolBewertung (:189-204): every symbol accumulates its
// direct parent term's TermKontext2 score; the root gets 0.
static void atp_wmi_finale(AtpWmiStats *st, Term t, u32 vater) {
  if (term_tag(t) != TAG_CTR) return;
  u32 f = term_ext(t);
  if (f < WALD_MAX_SYMBOLS) st->finale[f] += vater;
  u32 v2 = atp_wmi_ctx2(st, t, 0u);
  for (u32 i = term_ctr_n(t); i > 0u; i--) {
    atp_wmi_finale(st, term_ctr_at(t, i - 1u), v2);
  }
}

// KontextBewertungAusfuehren (:206-230) for one equation/goal list:
// per pair, GesKonL/GesKonR = TermKontext of each ORIGINAL side; a
// symbol present in a side books the pair total into kk and its own
// side's score into kk_one (BOTH sides book if it appears in both);
// a symbol present in exactly one side books the OTHER side's score
// into kk_opp.
static void atp_wmi_kombi(AtpWmiStats *st, const Term *ls, const Term *rs,
                          u32 n, u32 *kk, u32 *kk_one, u32 *kk_opp) {
  for (u32 e = 0; e < n; e++) {
    u32 kl = atp_wmi_ctx(st, ls[e], 0u);
    u32 kr = atp_wmi_ctx(st, rs[e], 0u);
    u64 lm = 0u, rm = 0u;
    atp_collect_symbols(ls[e], &lm);
    atp_collect_symbols(rs[e], &rm);
    for (u32 f = 0; f < WALD_MAX_SYMBOLS; f++) {
      u64 bit = (u64)1u << f;
      if (rm & bit) { kk[f] += kr + kl; kk_one[f] += kr; }
      if (lm & bit) { kk[f] += kr + kl; kk_one[f] += kl; }
      if ((rm & bit) && !(lm & bit)) kk_opp[f] += kl;
      if ((lm & bit) && !(rm & bit)) kk_opp[f] += kr;
    }
  }
}

// SymbolVergleich (:375-391): the 11-key chain.  Returns -1 Kleiner,
// 0 Gleich, +1 Groesser.  Key 1 is a MAX comparison (MORE equation
// occurrences sorts EARLIER); every other key is MIN.
#define ATP_WMI_KEY_MIN(field)                                          \
  if (st->field[f] != st->field[g])                                     \
    return st->field[f] > st->field[g] ? 1 : -1;
static int atp_wmi_symcmp(const AtpWmiStats *st, u32 f, u32 g) {
  if (st->occ_eq[f] != st->occ_eq[g])             // DatenVergleichMax
    return st->occ_eq[f] < st->occ_eq[g] ? 1 : -1;
  ATP_WMI_KEY_MIN(arity)
  ATP_WMI_KEY_MIN(pos_eq)
  ATP_WMI_KEY_MIN(pos_goal)
  ATP_WMI_KEY_MIN(kk_eq_one)
  ATP_WMI_KEY_MIN(kk_eq_opp)
  ATP_WMI_KEY_MIN(kk_eq)
  ATP_WMI_KEY_MIN(kk_goal_one)
  ATP_WMI_KEY_MIN(kk_goal_opp)
  ATP_WMI_KEY_MIN(kk_goal)
  ATP_WMI_KEY_MIN(finale)
  return 0;
}
#undef ATP_WMI_KEY_MIN

// LRSortierenRek (:517-534): side-order comparator, variables as
// jokers (var/var = Gleich, var < non-var), heads by SymbolVergleich,
// then argument-wise left to right.  Gleich heads share an arity
// (SymbolVergleich key 2), so the lhs arity bound is safe.
static int atp_wmi_lr_cmp(const AtpWmiStats *st, Term a, Term b) {
  u8 av = (term_tag(a) != TAG_CTR), bv = (term_tag(b) != TAG_CTR);
  if (av && bv) return 0;
  if (av) return -1;
  if (bv) return 1;
  int c = atp_wmi_symcmp(st, term_ext(a), term_ext(b));
  if (c != 0) return c;
  u32 n = term_ctr_n(a);
  for (u32 i = 0; i < n && c == 0; i++) {
    c = atp_wmi_lr_cmp(st, term_ctr_at(a, i), term_ctr_at(b, i));
  }
  return c;
}

// GleichungenVariablenNormieren (:617-664) reduced to its sort-key
// payload: per equation, each variable's first-occurrence rank over
// (canonical lhs, rhs), walked left to right.  WM assigns code
// -(rank+1); only the rank ORDER feeds the comparator below.
static void atp_wmi_var_ranks(Term t, u32 *map, u32 *next) {
  if (term_tag(t) == TAG_FVR) {
    u32 v = term_ext(t);
    if (v < ATP_WMI_MAX_VARS && map[v] == 0xFFFFFFFFu) map[v] = (*next)++;
    return;
  }
  if (term_tag(t) != TAG_CTR) return;
  u32 n = term_ctr_n(t);
  for (u32 i = 0; i < n; i++) atp_wmi_var_ranks(term_ctr_at(t, i), map, next);
}

// LRSymbSortierenRek (:666-689): equation-sort term comparator over
// variable-normalized terms.  Two variables compare by code -(rank+1)
// -- a LATER first occurrence (-2 < -1) is Kleiner (:673-677); var <
// non-var; non-vars by canonical symbol position; then argument-wise.
static int atp_wmi_eq_term_cmp(const AtpWmiStats *st,
                               Term a, const u32 *amap,
                               Term b, const u32 *bmap) {
  u8 av = (term_tag(a) == TAG_FVR), bv = (term_tag(b) == TAG_FVR);
  if (av && bv) {
    u32 ea = term_ext(a), eb = term_ext(b);
    u32 ra = ea < ATP_WMI_MAX_VARS ? amap[ea] : 0xFFFFFFFEu;
    u32 rb = eb < ATP_WMI_MAX_VARS ? bmap[eb] : 0xFFFFFFFEu;
    if (ra == rb) return 0;
    return ra > rb ? -1 : 1;
  }
  if (av) return -1;
  if (bv) return 1;
  u32 fa = term_ext(a), fb = term_ext(b);
  u32 pa = fa < WALD_MAX_SYMBOLS ? st->pos_of[fa] : fa;
  u32 pb = fb < WALD_MAX_SYMBOLS ? st->pos_of[fb] : fb;
  if (pa != pb) return pa < pb ? -1 : 1;
  u32 n = term_ctr_n(a);
  int c = 0;
  for (u32 i = 0; i < n && c == 0; i++) {
    c = atp_wmi_eq_term_cmp(st, term_ctr_at(a, i), amap,
                            term_ctr_at(b, i), bmap);
  }
  return c;
}

// The full SpezNormierung pipeline over the queued axiom set + the
// registered goal conjuncts, followed by the Initial = Act_ultimate
// restamp.  Runs once, from thvm_atp_step's first call (gated on
// use_wm_intake_order); every later enqueue is a derived CP and takes
// the normal classification.
static void atp_wm_intake_canonicalize(AtpState *s) {
  if (s == NULL || s->n_cps == 0u) return;

  // 1. Collect the queued TRACE_AXIOM rows (slot + the var-normalized
  //    original pair off the trace entry, children 2/3).
  u32 cap = s->n_cps;
  u32  *slots = (u32 *)malloc(cap * sizeof(u32));
  Term *ls    = (Term *)malloc(cap * sizeof(Term));
  Term *rs    = (Term *)malloc(cap * sizeof(Term));
  Term *cls   = (Term *)malloc(cap * sizeof(Term));  // canonical (LR-ordered) sides
  Term *crs   = (Term *)malloc(cap * sizeof(Term));
  u32 **vmaps = (u32 **)malloc(cap * sizeof(u32 *));
  u32  *order = (u32 *)malloc(cap * sizeof(u32));
  u32  *seqs  = (u32 *)malloc(cap * sizeof(u32));
  if (slots == NULL || ls == NULL || rs == NULL || cls == NULL
      || crs == NULL || vmaps == NULL || order == NULL || seqs == NULL) {
    fprintf(stderr, "atp_wm_intake_canonicalize: malloc failed\n");
    exit(1);
  }
  u32 count = 0u;
  for (u32 i = 0; i < s->n_cps; i++) {
    u32 tr = s->cp_trace[i];
    if (tr == ATP_TRACE_NONE || tr >= s->n_trace) continue;
    Term te = s->trace[tr];
    if (term_tag(te) != TAG_CTR || term_ext(te) != TRACE_AXIOM) continue;
    slots[count] = i;
    ls[count]    = term_ctr_at(te, 2);
    rs[count]    = term_ctr_at(te, 3);
    count++;
  }
  if (count == 0u) goto done;

  // 2. SymboldatenSammeln over equations (= the axioms) + conclusions
  //    (= the goal conjuncts), ORIGINAL sides.
  AtpWmiStats *st = (AtpWmiStats *)calloc(1u, sizeof(AtpWmiStats));
  if (st == NULL) {
    fprintf(stderr, "atp_wm_intake_canonicalize: calloc failed\n");
    exit(1);
  }
  for (u32 g = 0; g < s->n_goals; g++) {
    atp_wmi_occ(st, s->goals_lhs[g], st->occ_goal);
    atp_wmi_occ(st, s->goals_rhs[g], st->occ_goal);
    atp_wmi_pos(st, s->goals_lhs[g], st->pos_goal, 1u, 0u);
    atp_wmi_pos(st, s->goals_rhs[g], st->pos_goal, 1u, 0u);
  }
  for (u32 e = 0; e < count; e++) {
    atp_wmi_occ(st, ls[e], st->occ_eq);
    atp_wmi_occ(st, rs[e], st->occ_eq);
    atp_wmi_pos(st, ls[e], st->pos_eq, 1u, 0u);
    atp_wmi_pos(st, rs[e], st->pos_eq, 1u, 0u);
  }
  atp_wmi_kombi(st, ls, rs, count, st->kk_eq, st->kk_eq_one, st->kk_eq_opp);
  if (s->n_goals > 0u) {
    atp_wmi_kombi(st, s->goals_lhs, s->goals_rhs, s->n_goals,
                  st->kk_goal, st->kk_goal_one, st->kk_goal_opp);
  }
  for (u32 e = 0; e < count; e++) {
    atp_wmi_finale(st, ls[e], 0u);
    atp_wmi_finale(st, rs[e], 0u);
  }
  for (u32 g = 0; g < s->n_goals; g++) {
    atp_wmi_finale(st, s->goals_lhs[g], 0u);
    atp_wmi_finale(st, s->goals_rhs[g], 0u);
  }

  // 3. SymbolReihenfolgeFestlegen: stable bubble sort of the present
  //    symbols (initial order = ascending label, the registration /
  //    SIGNATURE order on both intake surfaces) -> pos_of ranks.
  {
    u32 syms[WALD_MAX_SYMBOLS];
    u32 nsym = 0u;
    for (u32 f = 0; f < WALD_MAX_SYMBOLS; f++) {
      if (st->present[f]) syms[nsym++] = f;
    }
    for (u32 i = 1u; i < nsym; i++) {
      for (u32 j = 0u; j + i < nsym; j++) {
        if (atp_wmi_symcmp(st, syms[j], syms[j + 1u]) > 0) {
          u32 t = syms[j]; syms[j] = syms[j + 1u]; syms[j + 1u] = t;
        }
      }
    }
    for (u32 k = 0; k < nsym; k++) st->pos_of[syms[k]] = k;
  }

  // 4. LRSortieren + GleichungenVariablenNormieren: canonical side
  //    order per axiom (swap iff lhs > rhs under the joker comparator;
  //    Gleich keeps spec order, WM's "Nicht eindeutig" case), then the
  //    per-equation variable first-occurrence ranks on the canonical
  //    pair.  Key-side only -- the STORED pair keeps its input sides.
  for (u32 e = 0; e < count; e++) {
    if (atp_wmi_lr_cmp(st, ls[e], rs[e]) > 0) {
      cls[e] = rs[e]; crs[e] = ls[e];
    } else {
      cls[e] = ls[e]; crs[e] = rs[e];
    }
    vmaps[e] = (u32 *)malloc(ATP_WMI_MAX_VARS * sizeof(u32));
    if (vmaps[e] == NULL) {
      fprintf(stderr, "atp_wm_intake_canonicalize: malloc failed\n");
      exit(1);
    }
    for (u32 v = 0; v < ATP_WMI_MAX_VARS; v++) vmaps[e][v] = 0xFFFFFFFFu;
    u32 next = 0u;
    atp_wmi_var_ranks(cls[e], vmaps[e], &next);
    atp_wmi_var_ranks(crs[e], vmaps[e], &next);
  }

  // 5. GleichungenSortieren: stable bubble sort by canonical LHS,
  //    ties by canonical RHS (swap on strict Groesser only).
  for (u32 e = 0; e < count; e++) order[e] = e;
  for (u32 i = 1u; i < count; i++) {
    for (u32 j = 0u; j + i < count; j++) {
      u32 a = order[j], b = order[j + 1u];
      int c = atp_wmi_eq_term_cmp(st, cls[a], vmaps[a], cls[b], vmaps[b]);
      if (c == 0) {
        c = atp_wmi_eq_term_cmp(st, crs[a], vmaps[a], crs[b], vmaps[b]);
      }
      if (c > 0) { order[j] = b; order[j + 1u] = a; }
    }
  }

  // 6. Intake restamp: the participating slots' existing cp_seq values
  //    are redistributed in canonical order (rank r gets the r-th
  //    smallest), and every axiom is tagged ultimate -- WM's
  //    Initial -> Act_ultimate w1 = INT32_MIN.  atp_cp_before orders
  //    the ultimate class by cp_seq alone, so the axioms pop first, in
  //    canonical-sort FIFO order, exactly like WM's (w1, w2) heap.
  for (u32 e = 0; e < count; e++) seqs[e] = s->cp_seq[slots[e]];
  for (u32 i = 1u; i < count; i++) {       // insertion sort, ascending
    u32 v = seqs[i];
    u32 j = i;
    while (j > 0u && seqs[j - 1u] > v) { seqs[j] = seqs[j - 1u]; j--; }
    seqs[j] = v;
  }
  thvm_atp_set_use_initial_ultimate(s, 1u);
  for (u32 r = 0; r < count; r++) {
    u32 slot = slots[order[r]];
    s->cp_seq[slot] = seqs[r];
    if (s->cp_ultimate != NULL && !s->cp_ultimate[slot]) {
      s->cp_ultimate[slot] = 1u;
      s->n_cps_ultimate++;
    }
  }
#ifdef ATP_FV_INDEX
  // FV-index records key on cp_seq; rebuild after the permutation.
  atp_fv_index_rebuild(s);
#endif
  atp_cp_floyd_only(s);

  for (u32 e = 0; e < count; e++) free(vmaps[e]);
  free(st);
done:
  free(slots); free(ls); free(rs); free(cls); free(crs);
  free(vmaps); free(order); free(seqs);
}
