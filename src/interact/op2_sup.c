// (OP2 &L{a, b} y)
// ---------------- OP2-SUP
// ! &L{y0, y1} = y
// &L{ OP2(opr, a, y0), OP2(opr, b, y1) }
//
// HVM4's wnf_op2_sup (TinyHVM/HVM4/clang/wnf/op2_sup.c), in fresh cells:
// [y, a, y0, b, y1, OP2(a, y0), OP2(b, y1)], the SUP over the last two.
fn Term interact_op2_sup(u32 opr, Term sup, Term y, u64 op2_loc) {
    ITRS++;
    // Trace carriers: the OP2's slots, not (sup, y).  A slot may hold a VAR
    // or DP that multi_resolve_producer follows to the SUP's producer.
    multi_emit(RULE_OP2_SUP, MULTI_SLIDE, op2_loc, op2_loc + 1, term_ext(sup));
    u64  sup_loc = term_val(sup);
    u32  lab     = term_ext(sup);
    Term a       = heap_read(sup_loc + 0);
    Term b       = heap_read(sup_loc + 1);

    u64 c = heap_alloc(7);
    heap_set(c + 0, y);
    heap_set(c + 1, a);
    heap_set(c + 2, term_new(0, TAG_DP0, lab, c + 0));
    heap_set(c + 3, b);
    heap_set(c + 4, term_new(0, TAG_DP1, lab, c + 0));
    heap_set(c + 5, term_new(0, TAG_OP2, opr, c + 1));
    heap_set(c + 6, term_new(0, TAG_OP2, opr, c + 3));

    return term_new(0, TAG_SUP, lab, c + 5);
}
