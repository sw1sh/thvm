// ! &L{x0, x1} = &R{a, b}
// ----------------------- DUP-SUP
// if L == R:                     (annihilate)
//   x0 <- a
//   x1 <- b
// else:                          (commute)
//   ! &L{A0, A1} = a
//   ! &L{B0, B1} = b
//   x0 <- &R{A0, B0}
//   x1 <- &R{A1, B1}
//
// Annihilation uses heap_subst_cop directly on the SUP's two cells.
// Commutation allocates a 6-cell block:
//
//   c+0  shared dup body for `a`
//   c+1  shared dup body for `b`
//   c+2  DP0_L over c+0   (= A0; lives in x0[0])
//   c+3  DP0_L over c+1   (= B0; lives in x0[1])
//   c+4  DP1_L over c+0   (= A1; lives in x1[0])
//   c+5  DP1_L over c+1   (= B1; lives in x1[1])
//
// x0 = &R at c+2, x1 = &R at c+4.  The active side is returned and the
// inactive side is substituted into `loc` via heap_subst_cop.
fn Term interact_dup_sup(u32 lab, u64 loc, u8 side, Term sup) {
  u64 sup_loc = term_val(sup);
  u32 sup_lab = term_ext(sup);
  ITRS++;
  /* Pass `loc` (the DUP cell's body slot) as the first carrier and
     the SUP partner as the second.  When OP2_SUP slid a SUP across
     an operator, the new DUP cell at `loc` was freshly allocated by
     that slide -- wire_prov[loc] picks up the slide's event id,
     giving the DUP_SUP_(ANN|COM) a causal predecessor.  The SUP
     partner often comes from pre-trace construction (the user-built
     literal), so its wire_prov is sentinel and the second slot stays
     unfilled. */
  if (lab == sup_lab) {
    multi_emit(RULE_DUP_SUP_ANN, MULTI_MERGE, loc, (u64)sup, lab);
    Term tm0 = heap_read(sup_loc + 0);
    Term tm1 = heap_read(sup_loc + 1);
    return heap_subst_cop(side, loc, tm0, tm1);
  }
  multi_emit(RULE_DUP_SUP_COM, MULTI_SPLIT, loc, (u64)sup, lab);

  Term a = heap_read(sup_loc + 0);
  Term b = heap_read(sup_loc + 1);

  u64 c = heap_alloc(6);
  heap_set(c + 0, a);
  heap_set(c + 1, b);
  heap_set(c + 2, term_new(0, TAG_DP0, lab, c + 0));
  heap_set(c + 3, term_new(0, TAG_DP0, lab, c + 1));
  heap_set(c + 4, term_new(0, TAG_DP1, lab, c + 0));
  heap_set(c + 5, term_new(0, TAG_DP1, lab, c + 1));

  Term x0 = term_new(0, TAG_SUP, sup_lab, c + 2);
  Term x1 = term_new(0, TAG_SUP, sup_lab, c + 4);
  return heap_subst_cop(side, loc, x0, x1);
}
