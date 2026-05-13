// (NUM op &L{a, b})
// ----------------- OP2-NUM-SUP
// &L{ OP2(opr, NUM, a), OP2(opr, NUM, b) }
//
// The left arg is a NUM (atomic, not duplicable), so it can be reused
// in BOTH new OP2 cells without a DUP.  Direct port of HVM4's
// `wnf_op2_num_sup` (TinyHVM/HVM4/clang/wnf/op2_num_sup.c).
//
// Wired into the wnf strict-eval loop's TAG_F_OP2_NUM frame (see
// src/wnf/_.c): when the right arg reduces to SUP and the left is
// already a baked-in NUM, fire this commute instead of stalling.
//
// Allocates 6 cells: 2 OP2 layouts (2 cells each) + 1 SUP wrapper
// (2 cells).
fn Term interact_op2_num_sup(u32 opr, Term num, Term sup, u64 op2_loc) {
    ITRS++;
    // See interact_op2_sup: pass the OP2's own slot locs so
    // multi_resolve_producer can chase through any SUB-flagged
    // VAR/DP that an earlier event substituted into the cell.
    multi_emit(RULE_OP2_NUM_SUP, MULTI_SLIDE, op2_loc, op2_loc + 1, term_ext(sup));
    u64  sup_loc = term_val(sup);
    u32  lab     = term_ext(sup);
    Term a       = heap_read(sup_loc + 0);
    Term b       = heap_read(sup_loc + 1);

    u64 c = heap_alloc(6);
    // OP2_0 = OP2(opr, num, a)
    heap_set(c + 0, num);
    heap_set(c + 1, a);
    // OP2_1 = OP2(opr, num, b)
    heap_set(c + 2, num);
    heap_set(c + 3, b);
    // SUP children point at the two OP2 layouts.
    heap_set(c + 4, term_new(0, TAG_OP2, opr, c + 0));
    heap_set(c + 5, term_new(0, TAG_OP2, opr, c + 2));

    return term_new(0, TAG_SUP, lab, c + 4);
}
