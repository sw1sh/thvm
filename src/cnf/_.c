// cnf_at -- collapsed normal-form readback.
//
// Mirrors HVM4's clang/cnf/_.c (single-threaded slice).  Reduces a
// term to WHNF, then recursively walks compound nodes to lift any
// SUP child to the top.  Plain (non-grad) DP projections are
// Levy-opaque under wnf (src/wnf/_.c); cnf is the readback layer
// where their duplication actually happens, so we dispatch DUP-XXX
// directly here for projection terms whose body cnf-result has a
// known WHNF tag (SUP/LAM/CTR/NUM/...).
//
// Lift shape (mirrors HVM4):
//
//   cnf_at(NODE(c_0, ..., c_{i-1}, SUP^L(a, b), c_{i+1}, ..., c_{n-1}))
//   = SUP^L(NODE(c_0_dup, ..., a, ..., c_{n-1}_dup),
//           NODE(c_0_dup, ..., b, ..., c_{n-1}_dup))
//
// Each non-SUP sibling is wrapped in a fresh DUP under label L so
// the two branches share work via the existing dup-cell mechanism.
// ERA/INC propagate; SUP at the head is the lifted result; pure
// (DP-free, SUP-free) terms pass through unchanged.
//
// Single-translation-unit build: loaded after wnf/_.c so cnf_at can
// call wnf().  Forward-declares itself at the top so the recursive
// helper compiles without separate prototypes.

fn Term cnf_at(Term term, u32 depth);

// Wrap a value in a fresh DUP under `lab`, returning the (DP0, DP1)
// pair via out-args.  Mirrors HVM4 term_clone -- the fresh cell sits
// at heap_alloc(1) and carries the original value; both projections
// reference the same loc so a downstream cnf_at on either side will
// re-fire the dup with the cnf-result of the body.
static void cnf_clone(u32 lab, Term val, Term *out_dp0, Term *out_dp1) {
  u64 loc = heap_alloc(1);
  heap_set(loc, val);
  *out_dp0 = term_new(0, TAG_DP0, lab, loc);
  *out_dp1 = term_new(0, TAG_DP1, lab, loc);
}

// Build a SUP^lab(a, b) on a fresh 2-cell block.  Common helper for
// the lift paths below.
static Term cnf_make_sup(u32 lab, Term a, Term b) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, a);
  heap_set(loc + 1, b);
  return term_new(0, TAG_SUP, lab, loc);
}

// CNF a DP0/DP1 projection: walk the cell's cnf-result and dispatch
// the matching DUP-XXX interaction.  Mirrors the apply-phase DP frame
// dispatch wnf used to perform before Phase 1.  Stays stuck (returns
// the original DP) when the body cnf-result doesn't pattern-match a
// known WHNF tag.
static Term cnf_dp(Term dp, u32 depth) {
  u64 loc  = term_val(dp);
  u32 lab  = term_ext(dp);
  u8  side = (term_tag(dp) == TAG_DP0) ? 0 : 1;
  // Already-substituted projection (sibling fired earlier): follow
  // the SUB chain so callers see the resolved value.
  Term cell = heap_read(loc);
  if (term_sub_get(cell)) {
    return cnf_at(term_sub_set(cell, 0), depth);
  }
  // Drive the body to its own CNF before dispatching.  We heap_take
  // here (not heap_read) so heap_subst_cop on the chosen interaction
  // sees the loc as available for the SUB write.  If the body is
  // already in WHNF / CNF, this round-trip is cheap.
  Term body = heap_take(loc);
  Term body_cnf = cnf_at(body, depth);
  switch (term_tag(body_cnf)) {
    case TAG_SUP: {
      // interact_dup_sup bumps ITRS internally (annihilate or commute).
      return cnf_at(interact_dup_sup(lab, loc, side, body_cnf), depth);
    }
    case TAG_ERA: {
      return interact_dup_era(side, loc, body_cnf);
    }
    case TAG_LAM: {
      return cnf_at(interact_dup_lam(lab, loc, side, body_cnf), depth);
    }
    case TAG_BRI: {
      return cnf_at(interact_dup_bri(lab, loc, side, body_cnf), depth);
    }
    case TAG_NUM: {
      return interact_dup_num(side, loc, body_cnf);
    }
    case TAG_ANY: {
      return interact_dup_any(side, loc, body_cnf);
    }
    case TAG_TEN: {
      return interact_dup_ten(side, loc, body_cnf);
    }
    case TAG_CTR: {
      return cnf_at(interact_dup_ctr(lab, loc, side, body_cnf), depth);
    }
    case TAG_UOP: {
      Term r = interact_dup_uop(lab, loc, side, body_cnf);
      if (r == 0) {
        heap_set(loc, body_cnf);
        return dp;
      }
      return cnf_at(r, depth);
    }
    default: {
      // Stuck: the body cnf'd to a tag we can't dispatch (TAG_REF,
      // TAG_ALO, TAG_VAR, TAG_INC, TAG_DP0/DP1, ...).  Restore the
      // body to its slot and propagate the DP up unchanged.
      heap_set(loc, body_cnf);
      return dp;
    }
  }
}

// CNF a LAM by descending into its body and lifting if the body
// cnf-result becomes SUP.  The binder is left intact (we don't
// substitute a de Bruijn placeholder like HVM4's BJV; thvm uses
// VAR-with-loc for binders) -- the body's references back to lam_loc
// continue to resolve normally.
static Term cnf_lam(Term lam, u32 depth) {
  u64  lam_loc = term_val(lam);
  u32  lam_ext = term_ext(lam);
  Term body    = heap_read(lam_loc);
  Term body_cnf = cnf_at(body, depth + 1);
  if (body_cnf != body) {
    heap_set(lam_loc, body_cnf);
  }
  if (term_tag(body_cnf) != TAG_SUP) {
    return lam;
  }
  // Body lifted a SUP: split LAM(SUP{a, b}) into SUP{LAM(a), LAM(b)}.
  // Both halves share the original lam_loc binder via fresh body
  // cells; downstream substitutions see the same VAR site.
  u32  lab     = term_ext(body_cnf);
  u64  sup_loc = term_val(body_cnf);
  Term sup_a   = heap_read(sup_loc + 0);
  Term sup_b   = heap_read(sup_loc + 1);
  u64  loc0    = heap_alloc(1);
  u64  loc1    = heap_alloc(1);
  heap_set(loc0, sup_a);
  heap_set(loc1, sup_b);
  Term lam0 = term_new(0, TAG_LAM, lam_ext, loc0);
  Term lam1 = term_new(0, TAG_LAM, lam_ext, loc1);
  return cnf_make_sup(lab, lam0, lam1);
}

// CNF an n-ary compound node (APP/OP2/MAT/EQL/AND/OR/WHEN/CTR).
// Walks each child, scans for the first SUP, then lifts.  Children
// that aren't the lifted SUP are wrapped in fresh DUPs so the two
// branches each see an independent copy.  CTR has its layout NUM at
// slot 0; we skip it here and walk slots 1..n.
static Term cnf_ctr(Term ctr, u32 depth) {
  u32 n = term_ctr_n(ctr);
  if (n == 0) return ctr;
  u32 k       = term_ext(ctr);
  u64 base    = term_val(ctr) + 1;   // skip arity NUM
  Term ch[16];
  int  sup_idx = -1;
  for (u32 i = 0; i < n && i < 16; i++) {
    Term orig = heap_read(base + i);
    Term cn   = cnf_at(orig, depth);
    if (cn != orig) heap_set(base + i, cn);
    ch[i] = cn;
    if (term_tag(cn) == TAG_ERA) {
      // ERA in any slot collapses the whole CTR: HVM4 doesn't emit
      // CTR(ERA, ...) at the user level either.  Treat as non-stuck
      // pass-through; the surrounding consumer can decide.  (We do
      // NOT propagate ERA upward because CTR is a labelled product;
      // dropping fields would change semantics.)
    }
    if (sup_idx < 0 && term_tag(cn) == TAG_SUP) sup_idx = (int)i;
  }
  if (sup_idx < 0) return ctr;
  // Lift: split into two CTRs, one per SUP branch.  Each non-SUP
  // sibling is duplicated via a fresh DUP under the SUP's label.
  Term sup_t   = ch[sup_idx];
  u32  lab     = term_ext(sup_t);
  u64  sup_loc = term_val(sup_t);
  Term sup_a   = heap_read(sup_loc + 0);
  Term sup_b   = heap_read(sup_loc + 1);
  Term args0[16], args1[16];
  for (u32 i = 0; i < n && i < 16; i++) {
    if ((int)i == sup_idx) {
      args0[i] = sup_a;
      args1[i] = sup_b;
    } else {
      cnf_clone(lab, ch[i], &args0[i], &args1[i]);
    }
  }
  Term node0 = term_new_ctr(k, args0, n);
  Term node1 = term_new_ctr(k, args1, n);
  return cnf_make_sup(lab, node0, node1);
}

// Generic 2-slot compound: APP/OP2/EQL/AND/OR/WHEN.  Slot layout is
// [val, val+1] = (a, b).  Lift a SUP found in either slot.  After
// cnf'ing the children we re-wnf the parent so any newly-surfaced
// head (e.g. APP(LAM, arg) from a resolved DP-LAM) fires its
// interaction here -- otherwise the parent would stay stuck on the
// pre-cnf head.
static Term cnf_node2(Term node, u32 depth) {
  u8  tag  = term_tag(node);
  u32 ext  = term_ext(node);
  u64 base = term_val(node);
  Term a   = heap_read(base + 0);
  Term b   = heap_read(base + 1);
  Term a_cnf = cnf_at(a, depth);
  Term b_cnf = cnf_at(b, depth);
  if (a_cnf != a) heap_set(base + 0, a_cnf);
  if (b_cnf != b) heap_set(base + 1, b_cnf);
  // Re-drive the parent now that the children are CNF'd.  If a child
  // resolved to a head that the parent's wnf knows how to dispatch
  // (APP-LAM, OP2-NUM/NUM, EQL-NUM/NUM, ...) wnf fires it; we then
  // re-cnf the result to handle further structure.
  Term redriven = wnf(node);
  if (redriven != node) {
    return cnf_at(redriven, depth);
  }
  int sup_idx = -1;
  if (term_tag(a_cnf) == TAG_SUP) sup_idx = 0;
  else if (term_tag(b_cnf) == TAG_SUP) sup_idx = 1;
  if (sup_idx < 0) return node;
  Term sup_t   = (sup_idx == 0) ? a_cnf : b_cnf;
  Term other   = (sup_idx == 0) ? b_cnf : a_cnf;
  u32  lab     = term_ext(sup_t);
  u64  sup_loc = term_val(sup_t);
  Term sup_a   = heap_read(sup_loc + 0);
  Term sup_b   = heap_read(sup_loc + 1);
  Term other0, other1;
  cnf_clone(lab, other, &other0, &other1);
  // Build two fresh nodes of the same tag/ext.
  u64 n0_loc = heap_alloc(2);
  u64 n1_loc = heap_alloc(2);
  if (sup_idx == 0) {
    heap_set(n0_loc + 0, sup_a);  heap_set(n0_loc + 1, other0);
    heap_set(n1_loc + 0, sup_b);  heap_set(n1_loc + 1, other1);
  } else {
    heap_set(n0_loc + 0, other0); heap_set(n0_loc + 1, sup_a);
    heap_set(n1_loc + 0, other1); heap_set(n1_loc + 1, sup_b);
  }
  Term n0 = term_new(0, tag, ext, n0_loc);
  Term n1 = term_new(0, tag, ext, n1_loc);
  return cnf_make_sup(lab, n0, n1);
}

fn Term cnf_at(Term term, u32 depth) {
  term = wnf(term);
  switch (term_tag(term)) {
    case TAG_ERA:
    case TAG_NUM:
    case TAG_TEN:
    case TAG_VAR:
    case TAG_ANY:
    case TAG_REF:
    case TAG_ALO:
    case TAG_PRI:
    case TAG_INC:
    case TAG_SUP: {
      return term;
    }
    case TAG_DP0:
    case TAG_DP1: {
      // Grad-flag DPs are already handled by wnf (FWD passthrough /
      // BWD chain rule); they shouldn't reach here as DP roots, but
      // defensively pass them through unchanged.
      if (term_ext(term) & DUP_GRAD_FLAG) return term;
      return cnf_dp(term, depth);
    }
    case TAG_LAM: {
      return cnf_lam(term, depth);
    }
    case TAG_CTR: {
      return cnf_ctr(term, depth);
    }
    case TAG_APP:
    case TAG_OP2:
    case TAG_EQL:
    case TAG_AND:
    case TAG_OR:
    case TAG_WHEN:
    case TAG_ANN: {
      return cnf_node2(term, depth);
    }
    default: {
      // MAT / UOP / BRI / others: pass through.  MAT and UOP have
      // their own active interactions; if wnf returned them stuck we
      // can't safely commute SUPs through without firing their
      // semantics (e.g. UOP-KERNEL fires the actual compute).
      return term;
    }
  }
}

fn Term cnf(Term term) {
  return cnf_at(term, 0);
}
