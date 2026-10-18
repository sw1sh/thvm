// ! &L{X0, X1} = #K{a, b, ...}
// ---------------------------- DUP-CTR
// ! &L{A0, A1} = a
// ! &L{B0, B1} = b
// ...
// X0 <- #K{A0, B0, ...}
// X1 <- #K{A1, B1, ...}
//
// HVM4's wnf_dup_nod (TinyHVM/HVM4/clang/wnf/dup_nod.c) for TAG_CTR; an arity-0
// CTR is an atom, shared by both sides.

fn Term interact_dup_ctr(u32 lab, u64 loc, u8 side, Term ctr) {
  ITRS++;
  multi_emit(RULE_DUP_CTR, MULTI_FORK, loc, (u64)ctr, lab);
  u32 n = term_ctr_n(ctr);
  u32 k = term_ext(ctr);
  if (n == 0) {
    return heap_subst_cop(side, loc, ctr, ctr);
  }
  u64  c_loc = term_val(ctr) + 1;   // skip arity NUM cell
  Term ch0[16];
  Term ch1[16];
  for (u32 i = 0; i < n && i < 16; i++) {
    u64 body = heap_alloc(1);
    heap_set(body, heap_read(c_loc + i));
    ch0[i] = term_new(0, TAG_DP0, lab, body);
    ch1[i] = term_new(0, TAG_DP1, lab, body);
  }
  Term r0 = term_new_ctr(k, ch0, n);
  Term r1 = term_new_ctr(k, ch1, n);
  return heap_subst_cop(side, loc, r0, r1);
}
