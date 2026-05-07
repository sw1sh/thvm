// (OP2 &L{a, b} y)
// ---------------- OP2-SUP
// ! &L{y0, y1} = y
// &L{ OP2(opr, a, y0), OP2(opr, b, y1) }
//
// Distributes OP2 over the SUP's left arg.  The right arg `y` is
// duplicated via a fresh DUP at the SUP's label so each branch gets
// its own copy.  Direct port of HVM4's `wnf_op2_sup`
// (TinyHVM/HVM4/clang/wnf/op2_sup.c).
//
// Wired into the wnf strict-eval loop's TAG_OP2 frame (see
// src/wnf/_.c): when the left arg reduces to SUP, fire this commute
// instead of stalling.
//
// Allocates 7 cells: 1 DUP body + 2 OP2 layouts (2 cells each) + 1
// SUP wrapper (2 cells).
fn Term interact_op2_sup(u32 opr, Term sup, Term y) {
    ITRS++;
    u64  sup_loc = term_val(sup);
    u32  lab     = term_ext(sup);
    Term a       = heap_read(sup_loc + 0);
    Term b       = heap_read(sup_loc + 1);

    u64 c = heap_alloc(7);
    // Shared DUP body holds `y`; DP0 / DP1 fetch the two copies.
    heap_set(c + 0, y);
    // OP2_0 = OP2(opr, a, DP0(L, c+0))
    heap_set(c + 1, a);
    heap_set(c + 2, term_new(0, TAG_DP0, lab, c + 0));
    // OP2_1 = OP2(opr, b, DP1(L, c+0))
    heap_set(c + 3, b);
    heap_set(c + 4, term_new(0, TAG_DP1, lab, c + 0));
    // SUP children point at the two OP2 layouts.
    heap_set(c + 5, term_new(0, TAG_OP2, opr, c + 1));
    heap_set(c + 6, term_new(0, TAG_OP2, opr, c + 3));

    return term_new(0, TAG_SUP, lab, c + 5);
}
