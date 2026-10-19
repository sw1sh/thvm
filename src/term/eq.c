// Structural equality without reduction (TTermSame); term_eq_cnf compares the
// cnf of both sides instead (TTermEq).

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
      // No alpha-equivalence: distinct binders are distinct variables, even
      // where reducing would equate them.
      return term_val(a) == term_val(b);
    case TAG_DP0:
    case TAG_DP1:
      return term_ext(a) == term_ext(b)
          && term_val(a) == term_val(b);
    case TAG_LAM:
      return term_eq_struct(heap_read(term_val(a)),
                            heap_read(term_val(b)));
    case TAG_DUP:
    case TAG_BRI:
      if (term_ext(a) != term_ext(b)) return 0;
      return term_eq_n(a, b, 1);
    case TAG_APP:
    case TAG_SUP:
    case TAG_OP2:
    case TAG_MAT:
    case TAG_EQL:
    case TAG_AND:
    case TAG_OR:
    case TAG_WHEN:
    case TAG_ANN:
      // ext holds the SUP/DUP label, the OP2 opcode or the MAT match value.
      if (term_ext(a) != term_ext(b)) return 0;
      return term_eq_n(a, b, 2);
    case TAG_DSU:
    case TAG_DDU:
      return term_eq_n(a, b, 3);
    case TAG_ALO: {
      u64 la = term_val(a), lb = term_val(b);
      // Cell +1: NUM(state id), or ERA once alo_force cached the result in +0.
      Term sa = heap_read(la + 1);
      Term sb = heap_read(lb + 1);
      if (term_val(sa) != term_val(sb)) return 0;
      return term_eq_struct(heap_read(la), heap_read(lb));
    }
    case TAG_CTR: {
      // [NUM(n), c_0, ..., c_{n-1}]; ext = label.
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
      // By loc: equal UOP graphs at different locs are distinct.
      return term_eq_atom(a, b);
    default:
      return a == b;
  }
}

fn int term_eq_cnf(Term a, Term b) {
  Term ca = cnf(a);
  Term cb = cnf(b);
  return term_eq_struct(ca, cb);
}
