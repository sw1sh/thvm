// term/eq.c - structural term equality.
//
// term_eq_struct(a, b): compare two Terms structurally without
// reducing, modulo VAR alpha-aliasing.  Used by pattern compilers
// to verify repeat-binder patterns (`f[x_, x_]` matching `f[A, B]`
// requires A `term_eq_struct` B).  A separate `term_eq` wrapper
// drives both sides through cnf first so callers don't have to.
//
// Structural rules:
//   - Same tag is necessary.  Different tag => not equal.
//   - For atoms (NUM/ERA/REF/TEN/ANY): compare ext + val directly.
//   - For VAR: same lam_loc => same variable.
//   - For DP0/DP1: same loc + same side + same label.
//   - For LAM: descend into body.  Binders are heap-loc-distinct
//     across calls, so two structurally-equal lambdas built
//     independently won't compare equal -- callers should usually
//     beta-reduce / cnf before calling.
//   - For APP/SUP/DUP/OP2/MAT/EQL/AND/OR/WHEN/ANN/BRI: recurse on
//     fixed-arity children.
//   - For CTR: same ext (ctor tag) + same arity + recurse on each
//     child.  CTR(arity NUM) at slot 0 is metadata, compared via
//     the arity check.
//   - For ALO: descend into the wrapped body and compare state ids.
//   - For UOP: defer to atom rules on (opcode, val) -- same opcode
//     + same heap loc means same UOP cell.  Different cells with
//     identical structure aren't considered equal here (would need
//     a kernel-aware deep walk).
//   - For DSU/DDU: recurse on (label, a, b) / (label, value, body).
//   - For LAM-with-DSU/DDU/PRI/etc inside: structural comparison.
//
// Returns 1 if equal, 0 otherwise.  Cycles are bounded by the
// caller's heap depth -- this walker doesn't memoize, so callers
// who pass non-cnf'd terms with shared substructure pay O(size).

fn int term_eq_struct(Term a, Term b);

static int term_eq_atom(Term a, Term b) {
  return term_tag(a) == term_tag(b)
      && term_ext(a) == term_ext(b)
      && term_val(a) == term_val(b);
}

static int term_eq_n(Term a, Term b, u32 n) {
  u64 la = term_val(a);
  u64 lb = term_val(b);
  for (u32 i = 0; i < n; i++) {
    if (!term_eq_struct(heap_read(la + i), heap_read(lb + i))) return 0;
  }
  return 1;
}

fn int term_eq_struct(Term a, Term b) {
  // Same packed bits => trivially equal (covers same-loc compounds,
  // identical NUMs, etc.).
  if (a == b) return 1;
  u8 ta = term_tag(a);
  u8 tb = term_tag(b);
  if (ta != tb) return 0;
  switch (ta) {
    case TAG_ERA:
      return 1;
    case TAG_NUM:
    case TAG_TEN:
    case TAG_REF:
    case TAG_ANY:
    case TAG_FVR:
      return term_eq_atom(a, b);
    case TAG_VAR:
      // Same binder loc => same variable.  Different binders =>
      // distinct vars even if reducing would equate them.
      return term_val(a) == term_val(b);
    case TAG_DP0:
    case TAG_DP1:
      return term_ext(a) == term_ext(b)
          && term_val(a) == term_val(b);
    case TAG_LAM:
      return term_eq_struct(heap_read(term_val(a)),
                            heap_read(term_val(b)));
    case TAG_APP:
    case TAG_SUP:
    case TAG_DUP:
    case TAG_OP2:
    case TAG_MAT:
    case TAG_EQL:
    case TAG_AND:
    case TAG_OR:
    case TAG_WHEN:
    case TAG_ANN:
    case TAG_BRI:
      // SUP / DUP / EQL have a label in ext that must match too;
      // OP2 / MAT carry an opcode / ctor tag; ANN/BRI/AND/OR/WHEN
      // carry no significant ext.  Compare ext defensively.
      if (term_ext(a) != term_ext(b)) return 0;
      return term_eq_n(a, b, 2);
    case TAG_DSU:
    case TAG_DDU:
      return term_eq_n(a, b, 3);
    case TAG_ALO: {
      u64 la = term_val(a), lb = term_val(b);
      // state cell at +1 is a NUM; compare its val for state id eq.
      Term sa = heap_read(la + 1);
      Term sb = heap_read(lb + 1);
      if (term_val(sa) != term_val(sb)) return 0;
      return term_eq_struct(heap_read(la), heap_read(lb));
    }
    case TAG_CTR: {
      // CTR ext = ctor tag.  heap[val] = NUM(arity); children at
      // val+1..val+n.  Different ctor tag or arity => not equal.
      if (term_ext(a) != term_ext(b)) return 0;
      Term na = heap_read(term_val(a));
      Term nb = heap_read(term_val(b));
      u32 n_a = (u32)term_val(na);
      u32 n_b = (u32)term_val(nb);
      if (n_a != n_b) return 0;
      u64 la = term_val(a) + 1;
      u64 lb = term_val(b) + 1;
      for (u32 i = 0; i < n_a; i++) {
        if (!term_eq_struct(heap_read(la + i), heap_read(lb + i)))
          return 0;
      }
      return 1;
    }
    case TAG_UOP:
      // Same opcode + same heap loc.  Two structurally-identical
      // UOPs at different locs are considered distinct here; a
      // proper UOP-aware deep-eq would need to walk per-opcode
      // children (which uop_arity already knows) but isn't needed
      // for the pattern-eq use case.
      return term_eq_atom(a, b);
    default:
      // Conservative: unknown tags compare via raw bits.
      return a == b;
  }
}

// Reducing wrapper: cnf both sides first, then structural compare.
// Cnf normalizes plain DPs, lifts SUPs, and reduces APP/OP2/MAT
// strict frames.  Two terms that reduce to the same value compare
// equal.  When either side has a SUP at the top after cnf, we
// compare branchwise (left-to-left and right-to-right at same
// label) -- a different-label SUP has been collapsed by cnf into
// SUP-of-CTR / etc., so the branchwise compare bottoms out cleanly.
fn int term_eq_cnf(Term a, Term b) {
  Term ca = cnf(a);
  Term cb = cnf(b);
  return term_eq_struct(ca, cb);
}
