// test_mat_op2.c - numeric switch (MAT) + binary ops (OP2),
// culminating in a terminating recursive countdown.
//
// The countdown definition is:
//
//     @count = λ acc. λ n. (λ{0: acc; λn'. @count (acc + 1) (n' - 1)} n)
//
// In our minimal IC + MAT/OP2/REF, that becomes a LAM whose body
// applies a MAT-on-zero to `n`.  When n == 0 the handler returns
// `acc` directly; otherwise the fallback is a LAM (binding `n'`
// implicitly = the same loc since MAT-MIS feeds the *same* arg
// back) that recurses with `(@count (acc+1) (n-1))`.
//
// We pick concrete starting values and let wnf run the recursion;
// the terminal NUM should be the count.

#include "../src/thvm.c"
#include "test.h"

static Term build_app(Term f, Term x) {
    u64 loc = heap_alloc(2);
    heap_set(loc + 0, f);
    heap_set(loc + 1, x);
    return term_new(0, TAG_APP, 0, loc);
}
static Term build_app2(Term f, Term a, Term b) {
    return build_app(build_app(f, a), b);
}

int main(void) {
    thvm_init();

    TEST_BEGIN("op2/sub-of-nums");
    {
        Term x = term_new(0, TAG_NUM, DT_INT32, 7);
        Term y = term_new(0, TAG_NUM, DT_INT32, 3);
        Term r = wnf(term_new_op2(OP_SUB, x, y));
        CHECK_EQ(term_tag(r), TAG_NUM);
        CHECK_EQ((u32)term_val(r), 4u);
    }

    TEST_BEGIN("op2/add-and-eq");
    {
        Term r1 = wnf(term_new_op2(OP_ADD,
            term_new(0, TAG_NUM, DT_INT32, 5),
            term_new(0, TAG_NUM, DT_INT32, 6)));
        CHECK_EQ((u32)term_val(r1), 11u);
        Term r2 = wnf(term_new_op2(OP_EQ,
            term_new(0, TAG_NUM, DT_INT32, 9),
            term_new(0, TAG_NUM, DT_INT32, 9)));
        CHECK_EQ((u32)term_val(r2), 1u);
        Term r3 = wnf(term_new_op2(OP_EQ,
            term_new(0, TAG_NUM, DT_INT32, 9),
            term_new(0, TAG_NUM, DT_INT32, 8)));
        CHECK_EQ((u32)term_val(r3), 0u);
    }

    TEST_BEGIN("mat/zero-match-returns-handler");
    {
        // (λ{0: NUM(99); λn. NUM(11)} NUM(0))  ->  NUM(99)
        Term handler  = term_new(0, TAG_NUM, DT_INT32, 99);
        // Fallback: λn. NUM(11)
        u64  fb_lam   = heap_alloc(1);
        heap_set(fb_lam, term_new(0, TAG_NUM, DT_INT32, 11));
        Term fallback = term_new(0, TAG_LAM, 0, fb_lam);
        Term mat      = term_new_mat(0, handler, fallback);
        Term zero     = term_new(0, TAG_NUM, DT_INT32, 0);
        Term r        = wnf(build_app(mat, zero));
        CHECK_EQ((u32)term_val(r), 99u);
    }

    TEST_BEGIN("mat/non-zero-falls-back");
    {
        Term handler  = term_new(0, TAG_NUM, DT_INT32, 99);
        u64  fb_lam   = heap_alloc(1);
        heap_set(fb_lam, term_new(0, TAG_NUM, DT_INT32, 11));
        Term fallback = term_new(0, TAG_LAM, 0, fb_lam);
        Term mat      = term_new_mat(0, handler, fallback);
        Term seven    = term_new(0, TAG_NUM, DT_INT32, 7);
        // MAT misses -> APP(fallback, 7) -> 11.
        Term r        = wnf(build_app(mat, seven));
        CHECK_EQ((u32)term_val(r), 11u);
    }

    TEST_BEGIN("ref+mat+op2/recursive-countdown");
    {
        // @count := λacc. λn.
        //              (λ{0: acc; λn'. @count (acc+1) (n'-1)} n)
        //
        // We reach into the heap and stitch the terms together by
        // hand to keep the test self-contained.

        // Outer LAM (acc binder).
        u64  acc_loc = heap_alloc(1);
        Term acc     = term_new(0, TAG_VAR, 0, acc_loc);

        // Inner LAM (n binder).
        u64  n_loc   = heap_alloc(1);
        Term nvar    = term_new(0, TAG_VAR, 0, n_loc);

        // Fallback LAM (n' binder; same shape as outer n -- the
        // MAT-MIS feeds the *original* n back into the fallback,
        // so we just bind it again).
        u64  np_loc  = heap_alloc(1);
        Term npvar   = term_new(0, TAG_VAR, 0, np_loc);

        // (acc + 1)
        Term acc_plus_1 = term_new_op2(OP_ADD,
                              acc,
                              term_new(0, TAG_NUM, DT_INT32, 1));
        // (n' - 1)
        Term np_minus_1 = term_new_op2(OP_SUB,
                              npvar,
                              term_new(0, TAG_NUM, DT_INT32, 1));

        // (@count (acc+1) (n'-1))
        Term recur = build_app2(term_new_ref(0), acc_plus_1, np_minus_1);

        // Fallback body: just recur.
        heap_set(np_loc, recur);
        Term fb_lam = term_new(0, TAG_LAM, 0, np_loc);

        // MAT[0]{handler=acc, fallback=fb_lam}
        Term mat = term_new_mat(0, acc, fb_lam);

        // Inner LAM body: APP(MAT, n)
        heap_set(n_loc, build_app(mat, nvar));
        Term inner_lam = term_new(0, TAG_LAM, 0, n_loc);

        // Outer LAM body = inner LAM
        heap_set(acc_loc, inner_lam);
        Term outer_lam = term_new(0, TAG_LAM, 0, acc_loc);

        thvm_def_register(0, outer_lam);

        // Call: @count 0 5  ->  5
        Term call = build_app2(term_new_ref(0),
                        term_new(0, TAG_NUM, DT_INT32, 0),
                        term_new(0, TAG_NUM, DT_INT32, 5));
        Term r = wnf(call);
        CHECK_EQ(term_tag(r), TAG_NUM);
        CHECK_EQ((u32)term_val(r), 5u);
    }

    thvm_free();
    TEST_REPORT();
}
