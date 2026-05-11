// (APP MAT[#K: h; m] &L{a, b})
// ---------------------------- APP-MAT-SUP
// ! H &L = h
// ! M &L = m
// &L{ APP(MAT[#K: H0; M0], a),
//     APP(MAT[#K: H1; M1], b) }
//
// When the MAT's scrutinee is a SUP, distribute the MAT through
// the SUP so each branch gets its own copy of the case-tree.  The
// MAT's handler+fallback are duplicated via DUPs at the SUP label
// (no per-arg DUPs needed since the MAT cell has 2 children, both
// duplicated together).  Direct port of HVM4's wnf_app_mat_sup
// (TinyHVM/HVM4/clang/wnf/app_mat_sup.c).
//
// Wired into the wnf strict-eval loop's APP -> TAG_MAT path (see
// src/wnf/_.c): when the reduced arg is SUP, fire this commute
// instead of falling through to the "miss" / fallback branch.
//
// Allocates 9 cells: 2 DUP body cells (handler, fallback) + 2
// MAT layouts (2 cells each) + 2 APP layouts (2 cells each) + 1
// SUP wrapper (2 cells)... wait, 2+2+2+2+2+2+2 = 14.  Recount:
//   c+0    DUP body 0 = handler
//   c+1    DUP body 1 = fallback
//   c+2,3  MAT_0: handler=DP0(c+0), fallback=DP0(c+1)
//   c+4,5  MAT_1: handler=DP1(c+0), fallback=DP1(c+1)
//   c+6,7  APP_0: fun = MAT_0, arg = a
//   c+8,9  APP_1: fun = MAT_1, arg = b
//   c+10,11 SUP children (point at APP_0, APP_1)
// Total = 12 cells.
fn Term interact_app_mat_sup(Term mat, Term sup) {
    ITRS++;
    multi_emit(RULE_APP_MAT_SUP, MULTI_SLIDE, (u64)mat, (u64)sup, term_ext(sup));
    u64  sup_loc = term_val(sup);
    u32  lab     = term_ext(sup);
    u64  mat_loc = term_val(mat);
    u32  match   = term_ext(mat);
    Term handler = heap_read(mat_loc + 0);
    Term fallback = heap_read(mat_loc + 1);
    Term a       = heap_read(sup_loc + 0);
    Term b       = heap_read(sup_loc + 1);

    u64 c = heap_alloc(12);
    // DUP bodies for handler + fallback at the SUP's label.
    heap_set(c + 0, handler);
    heap_set(c + 1, fallback);
    // MAT_0: handler = DP0(L, c+0), fallback = DP0(L, c+1)
    heap_set(c + 2, term_new(0, TAG_DP0, lab, c + 0));
    heap_set(c + 3, term_new(0, TAG_DP0, lab, c + 1));
    // MAT_1: handler = DP1(L, c+0), fallback = DP1(L, c+1)
    heap_set(c + 4, term_new(0, TAG_DP1, lab, c + 0));
    heap_set(c + 5, term_new(0, TAG_DP1, lab, c + 1));
    // APP_0: fun = MAT_0, arg = a
    heap_set(c + 6, term_new(0, TAG_MAT, match, c + 2));
    heap_set(c + 7, a);
    // APP_1: fun = MAT_1, arg = b
    heap_set(c + 8, term_new(0, TAG_MAT, match, c + 4));
    heap_set(c + 9, b);
    // SUP children point at the two APPs.
    heap_set(c + 10, term_new(0, TAG_APP, 0, c + 6));
    heap_set(c + 11, term_new(0, TAG_APP, 0, c + 8));

    return term_new(0, TAG_SUP, lab, c + 10);
}
