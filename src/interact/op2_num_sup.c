// (NUM op &L{a, b})
// ----------------- OP2-NUM-SUP
// &L{ OP2(opr, NUM, a), OP2(opr, NUM, b) }
//
// HVM4's wnf_op2_num_sup (TinyHVM/HVM4/clang/wnf/op2_num_sup.c); the NUM is an
// atom both OP2s share.  Cells: [num, a, num, b, OP2(num, a), OP2(num, b)].
fn Term interact_op2_num_sup(u32 opr, Term num, Term sup, u64 op2_loc) {
    ITRS++;
    // Trace carriers: the OP2's slots, as in interact_op2_sup.
    multi_emit(RULE_OP2_NUM_SUP, MULTI_SLIDE, op2_loc, op2_loc + 1, term_ext(sup));
    u64  sup_loc = term_val(sup);
    u32  lab     = term_ext(sup);
    Term a       = heap_read(sup_loc + 0);
    Term b       = heap_read(sup_loc + 1);

    u64 c = heap_alloc(6);
    heap_set(c + 0, num);
    heap_set(c + 1, a);
    heap_set(c + 2, num);
    heap_set(c + 3, b);
    heap_set(c + 4, term_new(0, TAG_OP2, opr, c + 0));
    heap_set(c + 5, term_new(0, TAG_OP2, opr, c + 2));

    return term_new(0, TAG_SUP, lab, c + 4);
}
