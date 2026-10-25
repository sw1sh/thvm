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

// Vampire `sp=reverse_frequency`: opposite direction of
// atp_occurrence_precedence (which matches Vampire `sp=frequency` /
// E InvFreqRank).  Common symbols outrank rare ones -- frequent
// operators sit highest in the precedence, so they remain on the LHS
// side of rules where the rewrite goes toward rarer-symbol RHSs.
//
// Empirically Vampire's most common winning precedence on UEQ targets
// (122 wins in tools/baselines/vampire_raw/).  See docs/ATP/vampire_port.md.
fn u32 atp_reverse_frequency_precedence(const Term *lhs, const Term *rhs,
                                        u32 n_eqns, u32 n_labels, u32 *prec) {
  if (n_labels > WALD_MAX_SYMBOLS) n_labels = WALD_MAX_SYMBOLS;
  u32 count[WALD_MAX_SYMBOLS];
  for (u32 i = 0; i < n_labels; i++) { count[i] = 0; prec[i] = 0; }
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
  // Ascending score order -> assign ranks 1..N (rarest = rank 1,
  // most common = rank N).  Result: common symbols outrank rare ones,
  // inverse of atp_occurrence_precedence.
  for (u32 i = 0; i < n_seen; i++) {
    prec[order[i]] = i + 1u;
  }
  return n_seen;
}

// === goal-inclusive frequency census (Vampire sp= / KBO weight port) ==
//
// The TPTP CLI path resurrects Vampire's `sp=` symbol precedence (and,
// for weighted-frequency, its KBO symbol weights) on thvm's own
// completion engine.  Unlike the axiom-only atp_occurrence_precedence /
// atp_reverse_frequency_precedence above, the census here folds in the
// goal equation too: Vampire computes a symbol's usageCnt over the WHOLE
// input -- axioms AND the (negated) conjecture -- before building the
// ordering (Kernel/Signature usageCnt, consumed in Kernel/Ordering.cpp
// PrecedenceOrdering::functionPrecedences).  Not capped at
// WALD_MAX_SYMBOLS: these run once at problem intake over the full .pr
// label range (post tgt/tgt-fold), so the scratch tallies are malloc'd.

// Walk one term, tallying CTR-label occurrences into `count`.
static void prec_occ_walk(Term seed, u32 *count, u32 n_labels) {
  Term stack[256];
  u32 sp = 0;
  stack[sp++] = seed;
  while (sp > 0) {
    Term t = stack[--sp];
    if (term_tag(t) == TAG_CTR) {
      u32 lab = term_ext(t);
      if (lab < n_labels) count[lab]++;
      u32 n = term_ctr_n(t);
      for (u32 c = 0; c < n && sp < 256; c++) stack[sp++] = term_ctr_at(t, c);
    }
  }
}

// Occurrence census over every axiom side plus the goal sides (the goal
// is folded in only when at least one side is a real term, mirroring
// pr_wm_derive_precedence's guard).  Returns 0 on OOM, 1 otherwise.
static int prec_occ_census_g(const Term *lhs, const Term *rhs, u32 n_eqns,
                             Term goal_l, Term goal_r,
                             u32 *count, u32 n_labels) {
  for (u32 i = 0; i < n_labels; i++) count[i] = 0;
  for (u32 e = 0; e < n_eqns; e++) {
    prec_occ_walk(lhs[e], count, n_labels);
    prec_occ_walk(rhs[e], count, n_labels);
  }
  if (term_tag(goal_l) == TAG_CTR || term_tag(goal_r) == TAG_CTR) {
    prec_occ_walk(goal_l, count, n_labels);
    prec_occ_walk(goal_r, count, n_labels);
  }
  return 1;
}

// Vampire `sp=frequency` (common_high=0) / `sp=reverse_frequency`
// (common_high=1), goal-inclusive.  Direction is the ONLY difference
// from atp_occurrence_precedence / atp_reverse_frequency_precedence
// besides the goal fold-in and the uncapped label range.
//
// Vampire (Kernel/Ordering.cpp): FreqComparator forward sorts higher
// usageCnt earlier, then functionPrecedences[aux[i]] = i, so the most
// frequent symbol gets precedence 0 (LOWEST) and the rarest the highest
// -- that is `sp=frequency`.  `sp=reverse_frequency` (FreqComparator
// revert) flips it: most frequent gets the highest precedence.
//
// prec[label] = rank (1..n_seen; higher = greater symbol), 0 = unseen.
// Returns n_seen, or 0 on OOM (prec left untouched on OOM).
fn u32 atp_frequency_precedence_g(const Term *lhs, const Term *rhs, u32 n_eqns,
                                  Term goal_l, Term goal_r, u32 n_labels,
                                  u32 *prec, int common_high) {
  if (n_labels == 0u) return 0u;
  u32 *count = (u32 *)malloc(sizeof(u32) * (size_t)n_labels);
  u64 *score = (u64 *)malloc(sizeof(u64) * (size_t)n_labels);
  u32 *order = (u32 *)malloc(sizeof(u32) * (size_t)n_labels);
  if (count == NULL || score == NULL || order == NULL) {
    free(count); free(score); free(order); return 0u;
  }
  prec_occ_census_g(lhs, rhs, n_eqns, goal_l, goal_r, count, n_labels);
  for (u32 i = 0; i < n_labels; i++) prec[i] = 0u;
  u32 n_seen = 0;
  for (u32 l = 0; l < n_labels; l++) {
    if (count[l] == 0u) continue;
    score[n_seen] = ((u64)count[l] << 32) | (u64)l;   // count high, label low
    order[n_seen] = l;
    n_seen++;
  }
  // Insertion sort `order` by ASCENDING score (count, then label id).
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
  // Ascending order = rarest first.  common_high: rank 1..N (rare low,
  // common high) = reverse_frequency.  else: rank N..1 (rare high,
  // common low) = frequency.
  for (u32 i = 0; i < n_seen; i++)
    prec[order[i]] = common_high ? (i + 1u) : (n_seen - i);
  free(count); free(score); free(order);
  return n_seen;
}

// Vampire symbol precedence (Kernel/Ordering.cpp
// sortAuxBySymbolPrecedence + BoostWrapper), the `sp=` schemes of the
// UEQ schedule plus `spb=goal`.  Every declared label in [base, n_labels)
// is sorted ascending under the scheme's comparator chain and ranked
// 1..N (N = greatest); labels below `base` keep their value, and the
// caller lifts the ranked band above them (atp_precedence_floor_reserved).
// Chains:
//   arity              higher arity greater
//   reverse-arity      lower arity greater
//   unary-first        unary greatest, then arity, then frequency
//   const-max          constants greatest, then arity
//   const-min          constants least, then reverse arity
//   frequency          rarer greater (Vampire's default sp)
//   reverse-frequency  commoner greater
//   unary-frequency    unary greatest, then frequency
//   const-frequency    constants least, then frequency
//   occurrence         tie-break only
//   ladr               Prover9's LADR lex_order (lex_compare_arity_0213:
//                      constants < binary < unary < ternary..., fewer
//                      axiom occurrences greater) + inverse_order
// Frequency = occurrence count over the axioms and the goal (Shell/
// Property.cpp scanForInterpreted -> incUsageCnt).  The final tie-break
// is the label id; Vampire's is the symbol id (first-occurrence order),
// which the alphabetized .pr signature does not preserve.  boost_goal
// (spb=goal): goal-occurring symbols outrank all others, the scheme
// deciding inside each band.  Returns the number of ranked labels, 0
// on OOM or an unknown scheme.
typedef struct {
  const u32 *arity, *count, *goal;
  int scheme, boost_goal;
} AtpVspCtx;

static int atp_vsp_icmp(u32 a, u32 b) { return (a > b) - (a < b); }

// < 0 when label a ranks below label b.
static int atp_vsp_cmp(const AtpVspCtx *c, u32 a, u32 b) {
  if (c->boost_goal) {
    int ga = c->goal[a] != 0u, gb = c->goal[b] != 0u;
    if (ga != gb) return ga ? 1 : -1;
  }
  u32 ara = c->arity[a], arb = c->arity[b];
  int r = 0;
  switch (c->scheme) {
    case ATP_VSP_ARITY:
      r = atp_vsp_icmp(ara, arb);
      break;
    case ATP_VSP_REVERSE_ARITY:
      r = atp_vsp_icmp(arb, ara);
      break;
    case ATP_VSP_UNARY_FIRST:
      if ((ara == 1u) != (arb == 1u)) return (ara == 1u) ? 1 : -1;
      r = atp_vsp_icmp(ara, arb);
      if (r == 0) r = atp_vsp_icmp(c->count[b], c->count[a]);
      break;
    case ATP_VSP_CONST_MAX:
      if ((ara == 0u) != (arb == 0u)) return (ara == 0u) ? 1 : -1;
      r = atp_vsp_icmp(ara, arb);
      break;
    case ATP_VSP_CONST_MIN:
      if ((ara == 0u) != (arb == 0u)) return (ara == 0u) ? -1 : 1;
      r = atp_vsp_icmp(arb, ara);
      break;
    case ATP_VSP_FREQUENCY:
      r = atp_vsp_icmp(c->count[b], c->count[a]);
      break;
    case ATP_VSP_REVERSE_FREQUENCY:
      r = atp_vsp_icmp(c->count[a], c->count[b]);
      break;
    case ATP_VSP_UNARY_FREQUENCY:
      if ((ara == 1u) != (arb == 1u)) return (ara == 1u) ? 1 : -1;
      r = atp_vsp_icmp(c->count[b], c->count[a]);
      break;
    case ATP_VSP_CONST_FREQUENCY:
      if ((ara == 0u) != (arb == 0u)) return (ara == 0u) ? -1 : 1;
      r = atp_vsp_icmp(c->count[b], c->count[a]);
      break;
    case ATP_VSP_LADR: {
      // LADR lex_compare_arity_0213: constants < binary < unary <
      // ternary < ...; fewer occurrences greater; name order last (the
      // alphabetized .pr labels ARE strcmp order).
      u32 ka = (ara == 0u) ? 0u : (ara == 2u) ? 1u : (ara == 1u) ? 2u : ara;
      u32 kb = (arb == 0u) ? 0u : (arb == 2u) ? 1u : (arb == 1u) ? 2u : arb;
      r = atp_vsp_icmp(ka, kb);
      if (r == 0) r = atp_vsp_icmp(c->count[b], c->count[a]);
      break;
    }
    default:
      break;
  }
  return (r != 0) ? r : atp_vsp_icmp(a, b);
}

// LADR inverse_order (provers.src/utilities.c): every inverse axiom
// f2(f1(x), x) = c or f2(x, f1(x)) = c (c a constant) moves its binary
// f2, then its unary f1, to just above the constants, keeping their
// relative lex order: constants < f2's < f1's < the rest.  `order`
// holds the n ranked labels, least first; rewritten in place.
static int ladr_inverse_axiom(Term a, Term b, const u32 *arity,
                              u32 *f2, u32 *f1) {
  if (term_tag(a) == TAG_CTR && term_ctr_n(a) == 0u) { Term t = a; a = b; b = t; }
  if (term_tag(b) != TAG_CTR || term_ctr_n(b) != 0u) return 0;
  if (term_tag(a) != TAG_CTR || term_ctr_n(a) != 2u) return 0;
  Term a0 = term_ctr_at(a, 0), a1 = term_ctr_at(a, 1);
  if (term_tag(a0) == TAG_FVR) { Term t = a0; a0 = a1; a1 = t; }
  if (term_tag(a1) != TAG_FVR) return 0;
  if (term_tag(a0) != TAG_CTR || term_ctr_n(a0) != 1u) return 0;
  Term x = term_ctr_at(a0, 0);
  if (term_tag(x) != TAG_FVR || term_ext(x) != term_ext(a1)) return 0;
  *f2 = term_ext(a);
  *f1 = term_ext(a0);
  (void)arity;
  return 1;
}

static void atp_ladr_inverse_order(const Term *lhs, const Term *rhs,
                                   u32 n_eqns, const u32 *arity,
                                   u32 *order, u32 n) {
  u8 *bin = (u8 *)calloc((size_t)n + 1u, 1u);
  u8 *un  = (u8 *)calloc((size_t)n + 1u, 1u);
  u32 *out = (u32 *)malloc(sizeof(u32) * ((size_t)n + 1u));
  if (bin == NULL || un == NULL || out == NULL) {
    free(bin); free(un); free(out); return;
  }
  u32 hits = 0;
  for (u32 e = 0; e < n_eqns; e++) {
    u32 f2 = 0, f1 = 0;
    if (!ladr_inverse_axiom(lhs[e], rhs[e], arity, &f2, &f1)) continue;
    for (u32 i = 0; i < n; i++) {
      if (order[i] == f2) { bin[i] = 1u; hits++; }
      if (order[i] == f1) { un[i] = 1u; hits++; }
    }
  }
  if (hits > 0u) {
    u32 w = 0, i = 0;
    for (; i < n && arity[order[i]] == 0u; i++)
      if (!bin[i] && !un[i]) out[w++] = order[i];
    for (u32 k = 0; k < n; k++) if (bin[k]) out[w++] = order[k];
    for (u32 k = 0; k < n; k++) if (un[k] && !bin[k]) out[w++] = order[k];
    for (; i < n; i++) if (!bin[i] && !un[i]) out[w++] = order[i];
    for (u32 k = 0; k < n; k++) order[k] = out[k];
  }
  free(bin); free(un); free(out);
}

fn int atp_vampire_sp_scheme(const char *name) {
  static const char *const names[] = {
    "arity", "reverse-arity", "unary-first", "const-max", "const-min",
    "frequency", "reverse-frequency", "unary-frequency", "const-frequency",
    "occurrence", "ladr",
  };
  if (name == NULL) return -1;
  for (int k = 0; k < (int)(sizeof names / sizeof names[0]); k++) {
    if (strcmp(name, names[k]) == 0) return k;
  }
  return -1;
}

fn u32 atp_vampire_precedence_g(const Term *lhs, const Term *rhs, u32 n_eqns,
                                Term goal_l, Term goal_r, u32 base,
                                u32 n_labels, const u32 *arity, int scheme,
                                int boost_goal, u32 *prec) {
  if (n_labels <= base || scheme < 0 || scheme > ATP_VSP_LADR) return 0u;
  u32 *count = (u32 *)calloc((size_t)n_labels, sizeof(u32));
  u32 *goal  = (u32 *)calloc((size_t)n_labels, sizeof(u32));
  u32 *order = (u32 *)malloc(sizeof(u32) * (size_t)n_labels);
  if (count == NULL || goal == NULL || order == NULL) {
    free(count); free(goal); free(order); return 0u;
  }
  // LADR counts the non-negative clauses only (symbol_order ->
  // nonneg_clauses): the negated goal stays out of the census.
  if (scheme == ATP_VSP_LADR)
    prec_occ_census_g(lhs, rhs, n_eqns, 0, 0, count, n_labels);
  else
    prec_occ_census_g(lhs, rhs, n_eqns, goal_l, goal_r, count, n_labels);
  prec_occ_walk(goal_l, goal, n_labels);
  prec_occ_walk(goal_r, goal, n_labels);
  AtpVspCtx c = { arity, count, goal, scheme, boost_goal };
  u32 n = 0;
  for (u32 l = base; l < n_labels; l++) order[n++] = l;
  for (u32 i = 1; i < n; i++) {
    u32 x = order[i];
    u32 j = i;
    while (j > 0 && atp_vsp_cmp(&c, order[j - 1], x) > 0) {
      order[j] = order[j - 1];
      j--;
    }
    order[j] = x;
  }
  if (scheme == ATP_VSP_LADR) atp_ladr_inverse_order(lhs, rhs, n_eqns, arity, order, n);
  for (u32 i = 0; i < n; i++) prec[order[i]] = i + 1u;
  free(count); free(goal); free(order);
  return n;
}

// Reserved constants below every user symbol.  Waldmeister enters f > c
// for each reserved constant c in {const1, const2} and every symbol f
// numbered below c (NeueFunktionenEinbauen, "install new functions",
// WASIC/SymbolOperationen.c, "symbol operations", :210-216), so both
// sit under the whole signature with const1 > const2, and the grounding
// constant SO_minimaleKonstante (the minimal constant) is the bottom
// constant of that total chain (minimaleKonstanteBestimmen, "determine
// the minimal constant", :112-126).  thvm keeps the two at labels 1 and
// 2, label 1 grounding free variables (s->min_const), and numbers user
// symbols from label 3.  The generators rank user labels among
// themselves: the Vampire ports and --precedence from 1, which ties the
// lowest user symbol with the grounding constant (an incomparable head
// pair under LPO), and the frequency ports leave both reserved labels
// at the unseen rank 0, tied with each other.  This lifts the user
// band, its order and ties unchanged, so that its least value is 3, and
// gives label 1 precedence 1 and label 2 precedence 2.  A precedence
// already in that layout (the .pr ORDERING chain, the
// Waldmeister-derived one) is left as it is.
fn void atp_precedence_floor_reserved(u32 *prec, u32 n_labels) {
  u32 first_user = ATP_RESERVED_LABEL_LAST + 1u;
  u32 lo = 0xFFFFFFFFu;
  for (u32 l = first_user; l < n_labels; l++) {
    if (prec[l] < lo) {
      lo = prec[l];
    }
  }
  if (lo < first_user) {
    u32 lift = first_user - lo;
    for (u32 l = first_user; l < n_labels; l++) {
      prec[l] += lift;
    }
  }
  for (u32 l = ATP_RESERVED_LABEL_FIRST; l <= ATP_RESERVED_LABEL_LAST && l < n_labels; l++) {
    prec[l] = l - ATP_RESERVED_LABEL_FIRST + 1u;
  }
}

// Vampire KBO FREQUENCY weight-generation scheme (Kernel/KBO.cpp
// weightsFromOpts, KboWeightGenerationScheme::FREQUENCY:
// `weight = frequency > 0 ? frequency : 1`).  Goal-inclusive census.
// weights[label] = max(occurrences, 1); every label in [0, n_labels)
// is written (unseen -> 1, matching the .pr default), so a symbol that
// occurs more often weighs more and KBO rewrites toward the rarer
// (lighter) side.  All weights are >= 1 so the ordering stays KBO-
// admissible against a variable weight of 1.
fn void atp_frequency_weights_g(const Term *lhs, const Term *rhs, u32 n_eqns,
                                Term goal_l, Term goal_r, u32 n_labels,
                                u32 *weights) {
  if (n_labels == 0u) return;
  u32 *count = (u32 *)malloc(sizeof(u32) * (size_t)n_labels);
  if (count == NULL) return;
  prec_occ_census_g(lhs, rhs, n_eqns, goal_l, goal_r, count, n_labels);
  for (u32 l = 0; l < n_labels; l++)
    weights[l] = (count[l] > 0u) ? count[l] : 1u;
  free(count);
}

// === KBO weight-generation schemes (Vampire kbo_weight_scheme -kws) ===
//
// The remaining E-inspired schemes from Vampire Kernel/KBO.cpp
// weightsFromOpts, as goal-inclusive ports mirroring
// atp_frequency_weights_g above.  Every scheme yields a weight >= 1 for
// every symbol (variables stay at var_weight 1), so the KBO stays a
// valid, admissible reduction ordering: a weight-generation scheme can
// reshape which superpositions fire but never make an unsound one.
// Symbols that never occur in axioms + goal get weight 1 (they cannot
// appear in any compared term, so ft_order never reads them).

// Walk one term, recording each CTR-label's arity (every occurrence of a
// well-formed symbol shares one arity, so last-writer-wins is exact) and
// tallying occurrences.  count[] / arity[] are indexed by label.
static void prec_arity_walk(Term seed, u32 *count, u32 *arity, u32 n_labels) {
  Term stack[256];
  u32 sp = 0;
  stack[sp++] = seed;
  while (sp > 0) {
    Term t = stack[--sp];
    if (term_tag(t) == TAG_CTR) {
      u32 lab = term_ext(t);
      u32 n = term_ctr_n(t);
      if (lab < n_labels) { count[lab]++; arity[lab] = n; }
      for (u32 c = 0; c < n && sp < 256; c++) stack[sp++] = term_ctr_at(t, c);
    }
  }
}

static void prec_arity_census_g(const Term *lhs, const Term *rhs, u32 n_eqns,
                                Term goal_l, Term goal_r,
                                u32 *count, u32 *arity, u32 n_labels) {
  for (u32 i = 0; i < n_labels; i++) { count[i] = 0u; arity[i] = 0u; }
  for (u32 e = 0; e < n_eqns; e++) {
    prec_arity_walk(lhs[e], count, arity, n_labels);
    prec_arity_walk(rhs[e], count, arity, n_labels);
  }
  if (term_tag(goal_l) == TAG_CTR || term_tag(goal_r) == TAG_CTR) {
    prec_arity_walk(goal_l, count, arity, n_labels);
    prec_arity_walk(goal_r, count, arity, n_labels);
  }
}

// Arity family (Vampire ARITY / INV_ARITY / ARITY_SQUARED /
// INV_ARITY_SQUARED).  mode 0 = arity + 1, 1 = maxArity - arity + 1,
// 2 = arity*arity + 1, 3 = maxArity*maxArity - arity*arity + 1, where
// maxArity is the greatest arity among occurring symbols.  Each formula
// is >= 1 for every arity in [0, maxArity].
fn void atp_arity_weights_g(const Term *lhs, const Term *rhs, u32 n_eqns,
                            Term goal_l, Term goal_r, u32 n_labels,
                            u32 *weights, int mode) {
  if (n_labels == 0u) return;
  u32 *count = (u32 *)malloc(sizeof(u32) * (size_t)n_labels);
  u32 *arity = (u32 *)malloc(sizeof(u32) * (size_t)n_labels);
  if (count == NULL || arity == NULL) { free(count); free(arity); return; }
  prec_arity_census_g(lhs, rhs, n_eqns, goal_l, goal_r, count, arity,
                      n_labels);
  u32 max_ar = 0u;
  for (u32 l = 0; l < n_labels; l++)
    if (count[l] > 0u && arity[l] > max_ar) max_ar = arity[l];
  for (u32 l = 0; l < n_labels; l++) {
    if (count[l] == 0u) { weights[l] = 1u; continue; }
    u32 a = arity[l];
    switch (mode) {
      case 1:  weights[l] = max_ar - a + 1u; break;
      case 2:  weights[l] = a * a + 1u; break;
      case 3:  weights[l] = max_ar * max_ar - a * a + 1u; break;
      default: weights[l] = a + 1u; break;
    }
  }
  free(count); free(arity);
}

// INV_FREQUENCY (Vampire: max > 0 ? max - freq + 1 : 1).  The RAREST
// symbol weighs the most; the mirror of atp_frequency_weights_g.
fn void atp_inv_frequency_weights_g(const Term *lhs, const Term *rhs,
                                    u32 n_eqns, Term goal_l, Term goal_r,
                                    u32 n_labels, u32 *weights) {
  if (n_labels == 0u) return;
  u32 *count = (u32 *)malloc(sizeof(u32) * (size_t)n_labels);
  if (count == NULL) return;
  prec_occ_census_g(lhs, rhs, n_eqns, goal_l, goal_r, count, n_labels);
  u32 max_f = 0u;
  for (u32 l = 0; l < n_labels; l++) if (count[l] > max_f) max_f = count[l];
  for (u32 l = 0; l < n_labels; l++)
    weights[l] = (count[l] > 0u && max_f > 0u) ? (max_f - count[l] + 1u) : 1u;
  free(count);
}

// PRECEDENCE / INV_PRECEDENCE (Vampire: prec + 1 / maxPrec - prec + 1,
// its rawPrecedence being a 0-based rank).  We read the LIVE precedence
// array (prp.precedence, post --sym-prec) and normalize against the
// minimum occurring precedence, so the lightest-precedence symbol gets
// weight 1 (precedence) or the heaviest gets weight 1 (inv_precedence);
// both stay >= 1.  inverse = 0 -> precedence, 1 -> inv_precedence.
fn void atp_precedence_weights_g(const Term *lhs, const Term *rhs, u32 n_eqns,
                                 Term goal_l, Term goal_r, u32 n_labels,
                                 const u32 *precedence, u32 *weights,
                                 int inverse) {
  if (n_labels == 0u) return;
  u32 *count = (u32 *)malloc(sizeof(u32) * (size_t)n_labels);
  if (count == NULL) return;
  prec_occ_census_g(lhs, rhs, n_eqns, goal_l, goal_r, count, n_labels);
  u32 min_p = 0xFFFFFFFFu, max_p = 0u;
  for (u32 l = 0; l < n_labels; l++) {
    if (count[l] == 0u) continue;
    if (precedence[l] < min_p) min_p = precedence[l];
    if (precedence[l] > max_p) max_p = precedence[l];
  }
  for (u32 l = 0; l < n_labels; l++) {
    if (count[l] == 0u) { weights[l] = 1u; continue; }
    weights[l] = inverse ? (max_p - precedence[l] + 1u)
                         : (precedence[l] - min_p + 1u);
  }
  free(count);
}
