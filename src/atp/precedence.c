// atp/precedence.c -- algebraic-structure detection + precedence
// generation for the IC-native ATP.
//
// Ported from Waldmeister's analysis stack:
//   * `PhilMarlow` ("Erkennung algebraischer Strukturen" = recognition
//     of algebraic structures), waldmeister/sources/ANA/PhilMarlow.c.
//     `ACSymboleSuchen` ("search for AC symbols", line 111) and
//     `DistributivgesetzeSuchen` ("search for distributive laws",
//     line 145) walk the equation set and tag operators.
//   * The structural detection predicates in
//     waldmeister/sources/WASIC/TermOperationen.c:
//       `TO_IstKommutativitaet` ("is-commutativity", line 1221),
//       `TO_IstAssoziativitaet` ("is-associativity", line 1207),
//       `TO_IstDistribution` ("is-distributivity", line 1251).
//   * The `Sinai` law table (waldmeister/include/Sinai.h, "Tafel1"):
//     `Idempotenz` (idempotence, line 63), `Linksidentitaet` /
//     `Rechtsidentitaet` (left / right identity, lines 56-57),
//     `Linksinverses` / `Rechtsinverses` (left / right inverse,
//     lines 58-59).
//   * `Praezedenzgenerator` (precedence generator),
//     waldmeister/sources/ANA/Praezedenzgenerator.c: `FuchsPraezedenz`
//     (line 292) orders equation symbols by descending arity --
//     "1-stellig > 2-stellig > ... > n-stellig > Konstanten" -- and
//     hoists definitionally-distinguished symbols to the top.
//
// Waldmeister's predicates run on terms that have first been
// "variable-normalized" (variablennormiert) left-to-right; thvm's
// parser already assigns FVR ids in occurrence order, so the same
// fixed positional patterns apply directly.

// === structural detection predicates ================================
//
// Each takes a variable-normalized equation (l, r) of TAG_CTR /
// TAG_FVR terms.  A function symbol is a TAG_CTR; its label is
// term_ext, its arity term_ctr_n.  A variable is a TAG_FVR; its id
// is term_ext.

static u8 prec_is_fvr(Term t, u32 id) {
  return term_tag(t) == TAG_FVR && term_ext(t) == id;
}

// True iff t is a binary CTR with the given label.
static u8 prec_is_binop(Term t, u32 *label_out) {
  if (term_tag(t) != TAG_CTR || term_ctr_n(t) != 2) return 0;
  if (label_out) *label_out = term_ext(t);
  return 1;
}

// Commutativity: f(x0,x1) = f(x1,x0).  Port of
// `TO_IstKommutativitaet` (TermOperationen.c:1221).
static u8 prec_is_commutativity(Term l, Term r, u32 *f_out) {
  u32 fl, fr;
  if (!prec_is_binop(l, &fl) || !prec_is_binop(r, &fr)) return 0;
  if (fl != fr) return 0;
  if (!prec_is_fvr(term_ctr_at(l, 0), 0)) return 0;
  if (!prec_is_fvr(term_ctr_at(l, 1), 1)) return 0;
  if (!prec_is_fvr(term_ctr_at(r, 0), 1)) return 0;
  if (!prec_is_fvr(term_ctr_at(r, 1), 0)) return 0;
  if (f_out) *f_out = fl;
  return 1;
}

// One direction of associativity: a = f(f(x0,x1),x2), b =
// f(x0,f(x1,x2)).  Helper for `prec_is_associativity`.
static u8 prec_assoc_dir(Term a, Term b, u32 f) {
  // a == f(f(x0,x1),x2)
  if (term_tag(a) != TAG_CTR || term_ext(a) != f || term_ctr_n(a) != 2)
    return 0;
  Term a0 = term_ctr_at(a, 0);
  if (term_tag(a0) != TAG_CTR || term_ext(a0) != f || term_ctr_n(a0) != 2)
    return 0;
  if (!prec_is_fvr(term_ctr_at(a0, 0), 0)) return 0;
  if (!prec_is_fvr(term_ctr_at(a0, 1), 1)) return 0;
  if (!prec_is_fvr(term_ctr_at(a, 1), 2)) return 0;
  // b == f(x0,f(x1,x2))
  if (term_tag(b) != TAG_CTR || term_ext(b) != f || term_ctr_n(b) != 2)
    return 0;
  if (!prec_is_fvr(term_ctr_at(b, 0), 0)) return 0;
  Term b1 = term_ctr_at(b, 1);
  if (term_tag(b1) != TAG_CTR || term_ext(b1) != f || term_ctr_n(b1) != 2)
    return 0;
  if (!prec_is_fvr(term_ctr_at(b1, 0), 1)) return 0;
  if (!prec_is_fvr(term_ctr_at(b1, 1), 2)) return 0;
  return 1;
}

// Associativity: f(f(x0,x1),x2) = f(x0,f(x1,x2)) (either side
// flattened).  Port of `TO_IstAssoziativitaet`
// (TermOperationen.c:1207).
static u8 prec_is_associativity(Term l, Term r, u32 *f_out) {
  u32 fl;
  if (!prec_is_binop(l, &fl)) {
    if (!prec_is_binop(r, &fl)) return 0;
  }
  if (prec_assoc_dir(l, r, fl) || prec_assoc_dir(r, l, fl)) {
    if (f_out) *f_out = fl;
    return 1;
  }
  return 0;
}

// Idempotence: f(x0,x0) = x0.  Port of the Sinai `Idempotenz`
// pattern "F(x,x) = x" (Sinai.h:63).
static u8 prec_is_idempotence(Term l, Term r, u32 *f_out) {
  u32 fl;
  if (!prec_is_binop(l, &fl)) return 0;
  if (!prec_is_fvr(term_ctr_at(l, 0), 0)) return 0;
  if (!prec_is_fvr(term_ctr_at(l, 1), 0)) return 0;
  if (!prec_is_fvr(r, 0)) return 0;
  if (f_out) *f_out = fl;
  return 1;
}

// Left / right identity: f(e,x0) = x0 or f(x0,e) = x0, where `e`
// is a nullary CTR (a constant).  Port of the Sinai
// `Linksidentitaet` / `Rechtsidentitaet` patterns "+(0,x) = x" /
// "+(x,0) = x" (Sinai.h:56-57).  `side` is 0 for left, 1 for right.
static u8 prec_is_identity(Term l, Term r, u32 side,
                           u32 *f_out, u32 *e_out) {
  u32 fl;
  if (!prec_is_binop(l, &fl)) return 0;
  if (!prec_is_fvr(r, 0)) return 0;
  Term unit = term_ctr_at(l, side);   // the `e` slot
  Term var  = term_ctr_at(l, 1 - side);
  if (!prec_is_fvr(var, 0)) return 0;
  if (term_tag(unit) != TAG_CTR || term_ctr_n(unit) != 0) return 0;
  if (f_out) *f_out = fl;
  if (e_out) *e_out = term_ext(unit);
  return 1;
}

// Left / right inverse: f(i(x0),x0) = e or f(x0,i(x0)) = e, where
// `i` is a unary CTR and `e` a nullary CTR.  Port of the Sinai
// `Linksinverses` / `Rechtsinverses` patterns "+(-(x),x) = 0" /
// "+(x,-(x)) = 0" (Sinai.h:58-59).  `side` is 0 for left, 1 right.
static u8 prec_is_inverse(Term l, Term r, u32 side,
                          u32 *f_out, u32 *i_out, u32 *e_out) {
  u32 fl;
  if (!prec_is_binop(l, &fl)) return 0;
  if (term_tag(r) != TAG_CTR || term_ctr_n(r) != 0) return 0;
  Term inv  = term_ctr_at(l, side);       // the i(x0) slot
  Term var  = term_ctr_at(l, 1 - side);
  if (!prec_is_fvr(var, 0)) return 0;
  if (term_tag(inv) != TAG_CTR || term_ctr_n(inv) != 1) return 0;
  if (!prec_is_fvr(term_ctr_at(inv, 0), 0)) return 0;
  if (f_out) *f_out = fl;
  if (i_out) *i_out = term_ext(inv);
  if (e_out) *e_out = term_ext(r);
  return 1;
}

// One direction of (left) distributivity: a = f(x0,g(x1,x2)),
// b = g(f(x0,x1),f(x0,x2)).  Port of one branch of
// `TO_IstDistribution` (TermOperationen.c:1251).
static u8 prec_distrib_dir(Term a, Term b, u32 *f_out, u32 *g_out) {
  u32 f, g;
  if (!prec_is_binop(a, &f)) return 0;
  if (!prec_is_fvr(term_ctr_at(a, 0), 0)) return 0;
  Term inner = term_ctr_at(a, 1);
  if (!prec_is_binop(inner, &g)) return 0;
  if (!prec_is_fvr(term_ctr_at(inner, 0), 1)) return 0;
  if (!prec_is_fvr(term_ctr_at(inner, 1), 2)) return 0;
  if (f == g) return 0;
  // b == g(f(x0,x1), f(x0,x2))
  if (term_tag(b) != TAG_CTR || term_ext(b) != g || term_ctr_n(b) != 2)
    return 0;
  Term b0 = term_ctr_at(b, 0), b1 = term_ctr_at(b, 1);
  if (term_tag(b0) != TAG_CTR || term_ext(b0) != f || term_ctr_n(b0) != 2)
    return 0;
  if (term_tag(b1) != TAG_CTR || term_ext(b1) != f || term_ctr_n(b1) != 2)
    return 0;
  if (!prec_is_fvr(term_ctr_at(b0, 0), 0)) return 0;
  if (!prec_is_fvr(term_ctr_at(b0, 1), 1)) return 0;
  if (!prec_is_fvr(term_ctr_at(b1, 0), 0)) return 0;
  if (!prec_is_fvr(term_ctr_at(b1, 1), 2)) return 0;
  if (f_out) *f_out = f;
  if (g_out) *g_out = g;
  return 1;
}

// Distributivity: f distributes over g.  Detects f(x0,g(x1,x2)) =
// g(f(x0,x1),f(x0,x2)) and its anti-distribution mirror -- a
// merge of `TO_IstDistribution` + `TO_IstAntiDistribution`
// (TermOperationen.c:1251 / 1265): which side carries the nested
// `g` does not matter for tagging f's `distributes_over`.
static u8 prec_is_distributivity(Term l, Term r, u32 *f_out, u32 *g_out) {
  return prec_distrib_dir(l, r, f_out, g_out)
      || prec_distrib_dir(r, l, f_out, g_out);
}

// === analysis pass ===================================================
//
// Port of `PM_SpezifikationAnalysieren` ("analyze the
// specification", PhilMarlow.c:1420)'s `ACSymboleSuchen` +
// `DistributivgesetzeSuchen` core: walk the equation set once,
// tag each operator.  Waldmeister keeps the C/A flags separate
// and only sets `IstAC` when both fire for the same symbol; we
// keep the finer-grained per-property booleans.

fn void atp_analyze_axioms(const Term *lhs, const Term *rhs, u32 n_eqns,
                           AtpSymProps *out, u32 n_labels) {
  for (u32 i = 0; i < n_labels; i++) {
    out[i].seen             = 0;
    out[i].arity            = 0;
    out[i].is_commutative   = 0;
    out[i].is_associative   = 0;
    out[i].is_idempotent    = 0;
    out[i].has_left_unit    = 0;
    out[i].has_right_unit   = 0;
    out[i].has_inverse      = 0;
    out[i].is_unit_symbol   = 0;
    out[i].is_inverse_symbol= 0;
    out[i].distributes      = 0;
    out[i].distributes_over = 0;
  }

  for (u32 e = 0; e < n_eqns; e++) {
    Term l = lhs[e], r = rhs[e];

    // Record arity / occurrence for every CTR in both sides.
    Term stack[256];
    u32  sp = 0;
    if (sp < 256) stack[sp++] = l;
    if (sp < 256) stack[sp++] = r;
    while (sp > 0) {
      Term t = stack[--sp];
      if (term_tag(t) == TAG_CTR) {
        u32 lab = term_ext(t);
        u32 n   = term_ctr_n(t);
        if (lab < n_labels) {
          out[lab].seen  = 1;
          out[lab].arity = n;
        }
        for (u32 c = 0; c < n && sp < 256; c++) {
          stack[sp++] = term_ctr_at(t, c);
        }
      }
    }

    // Property tagging.  Each predicate tries both orientations
    // internally; commutativity and associativity are symmetric.
    u32 f, g, i_sym, e_sym;
    if (prec_is_commutativity(l, r, &f) && f < n_labels) {
      out[f].is_commutative = 1;
    }
    if (prec_is_associativity(l, r, &f) && f < n_labels) {
      out[f].is_associative = 1;
    }
    if (prec_is_idempotence(l, r, &f) && f < n_labels) {
      out[f].is_idempotent = 1;
    }
    // Identity: try the equation in both orientations so the
    // `= x` side may sit left or right.
    for (u32 swap = 0; swap < 2; swap++) {
      Term a = swap ? r : l, b = swap ? l : r;
      if (prec_is_identity(a, b, 0, &f, &e_sym) && f < n_labels) {
        out[f].has_left_unit = 1;
        if (e_sym < n_labels) out[e_sym].is_unit_symbol = 1;
      }
      if (prec_is_identity(a, b, 1, &f, &e_sym) && f < n_labels) {
        out[f].has_right_unit = 1;
        if (e_sym < n_labels) out[e_sym].is_unit_symbol = 1;
      }
      if (prec_is_inverse(a, b, 0, &f, &i_sym, &e_sym) && f < n_labels) {
        out[f].has_inverse = 1;
        if (i_sym < n_labels) out[i_sym].is_inverse_symbol = 1;
        if (e_sym < n_labels) out[e_sym].is_unit_symbol = 1;
      }
      if (prec_is_inverse(a, b, 1, &f, &i_sym, &e_sym) && f < n_labels) {
        out[f].has_inverse = 1;
        if (i_sym < n_labels) out[i_sym].is_inverse_symbol = 1;
        if (e_sym < n_labels) out[e_sym].is_unit_symbol = 1;
      }
    }
    if (prec_is_distributivity(l, r, &f, &g) && f < n_labels) {
      out[f].distributes      = 1;
      out[f].distributes_over = g;
    }
  }
}

// === precedence generation ===========================================
//
// Port of `Praezedenzgenerator`'s `FuchsPraezedenz` syntactic
// ordering rule (Praezedenzgenerator.c:292, comment block at
// line 306): equation symbols are ranked "1-stellig > 2-stellig
// > ... > n-stellig > Konstanten" -- by ascending arity from the
// top -- with definitionally-distinguished symbols pulled higher.
//
// We map that to a numeric score per label (higher = greater
// symbol).  KBO / LPO orient a rule lhs -> rhs when lhs > rhs;
// for completion convergence the "heavier" operator should sit
// HIGH so nested terms over it reduce toward simpler symbols.
// The score layers, most-significant first:
//
//   1. The inverse operator `i` ranks highest -- pushing
//      `i(i(x))` toward `x` (Sinai's Group precedence puts the
//      unary inverse above the binary product, Sinai.h:117).
//   2. A distributor `*` ranks above what it distributes over
//      `+`, so `*(x,+(y,z))` rewrites toward sums of products
//      (Tafel3 Ring precedence "*+", Sinai.h:243).
//   3. Higher-arity function symbols rank above lower-arity ones;
//      constants (arity 0) rank lowest -- the `FuchsPraezedenz`
//      arity ladder.
//   4. AC operators (commutative AND associative) get a small
//      demotion within their arity band so a plain operator at
//      the same arity outranks them, mirroring Waldmeister's
//      preference to keep AC symbols low.
//   5. The unit constant `e` ranks above ordinary constants.
//   6. Label index breaks remaining ties (stable, deterministic).

fn u32 atp_generate_precedence(const AtpSymProps *props, u32 n_labels,
                               u32 *prec) {
  // Build a sortable score per seen label; unseen labels get 0.
  // The score is composed so a plain integer comparison realizes
  // the layered ordering above.
  u64 score[WALD_MAX_SYMBOLS];
  u32 order[WALD_MAX_SYMBOLS];
  u32 n_seen = 0;
  u32 cap = n_labels < WALD_MAX_SYMBOLS ? n_labels : WALD_MAX_SYMBOLS;

  for (u32 l = 0; l < cap; l++) {
    prec[l] = 0;
    if (!props[l].seen) continue;
    const AtpSymProps *p = &props[l];

    u64 s = 0;
    // Layer 1: inverse operator on top.
    s |= (u64)(p->is_inverse_symbol ? 1u : 0u) << 40;
    // Layer 2: distributor above the operator it distributes over.
    s |= (u64)(p->distributes ? 1u : 0u) << 36;
    // Layer 3: arity ladder (cap at 15 so it fits 4 bits).
    u32 ar = p->arity > 15u ? 15u : p->arity;
    s |= (u64)ar << 28;
    // Layer 4: AC demotion within the arity band.
    u8 is_ac = p->is_commutative && p->is_associative;
    s |= (u64)(is_ac ? 0u : 1u) << 24;
    // Layer 5: unit constant above ordinary constants.
    s |= (u64)(p->is_unit_symbol ? 1u : 0u) << 20;
    // Layer 6: label index as the stable tie-breaker.
    s |= (u64)(l & 0xFFFFFu);

    score[n_seen] = s;
    order[n_seen] = l;
    n_seen++;
  }

  // Insertion sort `order` by ascending score; small n.
  for (u32 i = 1; i < n_seen; i++) {
    u64 sk = score[i];
    u32 ok = order[i];
    u32 j = i;
    while (j > 0 && score[j - 1] > sk) {
      score[j] = score[j - 1];
      order[j] = order[j - 1];
      j--;
    }
    score[j] = sk;
    order[j] = ok;
  }

  // Assign ranks 1..n_seen in sorted order (rank 0 stays the
  // "unseen" sentinel, matching the bench harness's +1 shift).
  for (u32 i = 0; i < n_seen; i++) {
    prec[order[i]] = i + 1;
  }
  return n_seen;
}

fn u32 atp_auto_precedence(const Term *lhs, const Term *rhs, u32 n_eqns,
                           u32 n_labels, u32 *prec) {
  AtpSymProps props[WALD_MAX_SYMBOLS];
  u32 cap = n_labels < WALD_MAX_SYMBOLS ? n_labels : WALD_MAX_SYMBOLS;
  atp_analyze_axioms(lhs, rhs, n_eqns, props, cap);
  return atp_generate_precedence(props, cap, prec);
}

// === occurrence-frequency precedence ================================
//
// Vampire's `sp=occurrence` / E's `-G InvFreqRank`: rank symbols by
// ASCENDING occurrence count in the axiom set -- rare symbols get the
// highest rank, common symbols the lowest.  Intuition: rewriting toward
// frequent symbols often makes the term set smaller, while a rule
// rewriting AWAY from rare symbols tends to terminate faster.
//
// Mirrors atp_auto_precedence's contract: `prec[label]` is the rank
// (1..n_seen, 0 = unseen).  Returns n_seen.  Unlike auto_precedence
// this ignores structural detection -- it's a pure frequency count.
fn u32 atp_occurrence_precedence(const Term *lhs, const Term *rhs, u32 n_eqns,
                                 u32 n_labels, u32 *prec) {
  if (n_labels > WALD_MAX_SYMBOLS) n_labels = WALD_MAX_SYMBOLS;
  u32 count[WALD_MAX_SYMBOLS];
  for (u32 i = 0; i < n_labels; i++) { count[i] = 0; prec[i] = 0; }
  // Walk each axiom side, count CTR labels.  Recursion bounded by
  // term depth (no manual stack needed for reasonable axioms; large
  // arities would blow the call stack but axiom terms are small).
  Term stack[256];
  for (u32 e = 0; e < n_eqns; e++) {
    Term seeds[2] = { lhs[e], rhs[e] };
    for (u32 s = 0; s < 2; s++) {
      u32 sp = 0;
      stack[sp++] = seeds[s];
      while (sp > 0) {
        Term t = stack[--sp];
        if (term_tag(t) == TAG_CTR) {
          u32 lab = term_ext(t);
          if (lab < n_labels) count[lab]++;
          u32 n = term_ctr_n(t);
          for (u32 c = 0; c < n && sp < 256; c++) {
            stack[sp++] = term_ctr_at(t, c);
          }
        }
      }
    }
  }
  // Sort the seen labels by ASCENDING count, then assign ranks
  // (rare = high rank).  Score: pack count into high bits, label into
  // low bits so equal counts tie-break by label index (stable).
  u64 score[WALD_MAX_SYMBOLS];
  u32 order[WALD_MAX_SYMBOLS];
  u32 n_seen = 0;
  for (u32 l = 0; l < n_labels; l++) {
    if (count[l] == 0) continue;
    score[n_seen] = ((u64)count[l] << 32) | (u64)l;
    order[n_seen] = l;
    n_seen++;
  }
  for (u32 i = 1; i < n_seen; i++) {
    u64 sk = score[i];
    u32 ok = order[i];
    u32 j = i;
    while (j > 0 && score[j - 1] > sk) {
      score[j] = score[j - 1];
      order[j] = order[j - 1];
      j--;
    }
    score[j] = sk;
    order[j] = ok;
  }
  // Ascending score order -> assign ranks N..1 (most common = rank 1,
  // rarest = rank N).  Result: rare symbols outrank common ones.
  for (u32 i = 0; i < n_seen; i++) {
    prec[order[i]] = n_seen - i;
  }
  return n_seen;
}
