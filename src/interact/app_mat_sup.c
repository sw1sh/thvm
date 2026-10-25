// (lam{#K:h; m} &L{a, b})
// ----------------------- APP-MAT-SUP
// ! H &L = h
// ! M &L = m
// &L{(lam{#K:H0; M0} a), (lam{#K:H1; M1} b)}
//
// HVM4's wnf_app_mat_sup (HVM4 clang/wnf/app_mat_sup.c); fresh cells
// [h, m, H0, M0, H1, M1, MAT_0, a, MAT_1, b, APP_0, APP_1], the SUP at c+10.
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
    heap_set(c + 0, handler);
    heap_set(c + 1, fallback);
    heap_set(c + 2, term_new(0, TAG_DP0, lab, c + 0));
    heap_set(c + 3, term_new(0, TAG_DP0, lab, c + 1));
    heap_set(c + 4, term_new(0, TAG_DP1, lab, c + 0));
    heap_set(c + 5, term_new(0, TAG_DP1, lab, c + 1));
    heap_set(c + 6, term_new(0, TAG_MAT, match, c + 2));
    heap_set(c + 7, a);
    heap_set(c + 8, term_new(0, TAG_MAT, match, c + 4));
    heap_set(c + 9, b);
    heap_set(c + 10, term_new(0, TAG_APP, 0, c + 6));
    heap_set(c + 11, term_new(0, TAG_APP, 0, c + 8));

    return term_new(0, TAG_SUP, lab, c + 10);
}
