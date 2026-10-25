// ! X &L = T{a0, a1, ..., a_{n-1}}
// --------------------------------- DUP-NOD
// ! A0 &L = a0
// ! A1 &L = a1
// ...
// X0 <- T{A0_0, A1_0, ..., A_{n-1}_0}
// X1 <- T{A0_1, A1_1, ..., A_{n-1}_1}
//
// HVM4's wnf_dup_nod (HVM4 clang/wnf/dup_nod.c) for the tags
// dup_nod_arity lists; never APP, where an eager DUP would copy a beta redex.
// Trace: term_a is the DUP cell, term_b the term (its tag names T), label L.

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
    // An atom goes to both sides, as in HVM4; neither caller passes one.
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
