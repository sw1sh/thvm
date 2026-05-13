// ! X &L = T{a0, a1, ..., a_{n-1}}
// --------------------------------- DUP-NOD
// ! A0 &L = a0
// ! A1 &L = a1
// ...
// X0 <- T{A0_0, A1_0, ..., A_{n-1}_0}
// X1 <- T{A0_1, A1_1, ..., A_{n-1}_1}
//
// Generic eager-commute of DUP through an n-ary compound.  Matches
// HVM4's wnf_dup_nod (~/src/HVM4/clang/wnf/dup_nod.c).  Used for the
// "primitive / structural" tags where duplication doesn't replicate
// a beta-equivalent redex (OP2, MAT, EQL, AND, OR, WHEN, ANN, DSU,
// DDU, INC).  APP is *not* dispatched here; HVM4 forbids it because
// eager DUP-APP would duplicate a beta opportunity.
//
// For each of the n operand slots: allocate a fresh shared body
// cell, copy the operand into it, build a pair of DP0_L / DP1_L
// projections over that body.  Then build two new T-tagged cells
// over the projection pairs and substitute one into the dup loc.
//
// Trace: emits RULE_DUP_NOD; term_a carries the term being commuted
// (so the consumer can recover the inner tag via term_tag(term_a)),
// term_b is the dup cell loc.  delta_label is the DUP's label L.

fn u32 dup_nod_arity(u8 tag) {
  switch (tag) {
    case TAG_INC:                                                 return 1;
    case TAG_OP2: case TAG_MAT: case TAG_EQL: case TAG_AND:
    case TAG_OR:  case TAG_WHEN: case TAG_ANN:                    return 2;
    case TAG_DSU: case TAG_DDU:                                   return 3;
    default:                                                      return 0;
  }
}

fn Term interact_dup_nod(u32 lab, u64 loc, u8 side, Term term) {
  u8  t_tag = term_tag(term);
  u32 ari   = dup_nod_arity(t_tag);
  if (ari == 0) {
    /* No operands to commute through -- just substitute the value
       on both sides.  Mirrors HVM4's dup_nod ari==0 branch (used
       for atom-like cells; we never reach this from wnf today, but
       keep the path so a future caller dispatching atomic tags
       through dup_nod stays correct). */
    ITRS++;
    multi_emit(RULE_DUP_NOD, MULTI_DIST, loc, (u64)term, lab);
    return heap_subst_cop(side, loc, term, term);
  }
  ITRS++;
  multi_emit(RULE_DUP_NOD, MULTI_FORK, loc, (u64)term, lab);

  u64 t_loc = term_val(term);
  u32 t_ext = term_ext(term);
  u64 r0_loc = heap_alloc(ari);
  u64 r1_loc = heap_alloc(ari);
  for (u32 i = 0; i < ari; i++) {
    u64 body = heap_alloc(1);
    heap_set(body, heap_read(t_loc + i));
    heap_set(r0_loc + i, term_new(0, TAG_DP0, lab, body));
    heap_set(r1_loc + i, term_new(0, TAG_DP1, lab, body));
  }
  Term r0 = term_new(0, t_tag, t_ext, r0_loc);
  Term r1 = term_new(0, t_tag, t_ext, r1_loc);
  return heap_subst_cop(side, loc, r0, r1);
}
