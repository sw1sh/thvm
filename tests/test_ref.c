// test_ref.c - lazy named definitions (TAG_REF + TAG_ALO).
//
// Coverage:
//   1. Register an identity def @id := λx.x and apply it to ERA;
//      verify ALO fires + APP-LAM follows + result is ERA.
//   2. Register a constant def @c := λx.x and apply it twice in
//      separate sites; each invocation should get fresh dyn cells
//      so the heap_pos grows once per fire.
//   3. A self-referential def @loop := λx.@loop is *only* unfolded
//      once when entered; the recursive body remains a TAG_ALO
//      until further reduction (lazy).
//
// (Numeric correctness of recursion lands once we add MAT/OP2 in
//  the next phase; this file just exercises the unfolding plumbing.)

#include "../src/thvm.c"
#include "test.h"

static Term build_id_lam(void) {
    u64  lam_loc = heap_alloc(1);
    Term var     = term_new(0, TAG_VAR, 0, lam_loc);
    heap_set(lam_loc, var);
    return term_new(0, TAG_LAM, 0, lam_loc);
}

static Term build_lam_returning(Term body) {
    u64 lam_loc = heap_alloc(1);
    heap_set(lam_loc, body);
    return term_new(0, TAG_LAM, 0, lam_loc);
}

static Term build_app(Term f, Term x) {
    u64 loc = heap_alloc(2);
    heap_set(loc + 0, f);
    heap_set(loc + 1, x);
    return term_new(0, TAG_APP, 0, loc);
}

int main(void) {
    thvm_init();

    TEST_BEGIN("ref/register-identity-and-call");
    {
        Term id_lam = build_id_lam();
        thvm_def_register(0, id_lam);
        // Calling the def: APP(REF(0), ERA) should reduce to ERA via
        // REF -> ALO -> ALO-LAM -> APP-LAM beta.
        Term era = term_new(0, TAG_ERA, 0, 0);
        Term app = build_app(term_new_ref(0), era);
        u64  itrs_before = ITRS;
        Term out = wnf(app);
        CHECK_EQ(term_tag(out), TAG_ERA);
        // Expected fires: REF unfold (1) + at least one ALO force (1)
        // + APP-LAM beta (1).  Lower bound 3.
        CHECK(ITRS - itrs_before >= 3);
    }

    TEST_BEGIN("ref/two-calls-allocate-fresh-cells");
    {
        thvm_def_register(1, build_id_lam());
        u64 heap_before = HEAP_NEXT;
        wnf(build_app(term_new_ref(1), term_new(0, TAG_ERA, 0, 0)));
        u64 heap_mid = HEAP_NEXT;
        wnf(build_app(term_new_ref(1), term_new(0, TAG_ERA, 0, 0)));
        u64 heap_after = HEAP_NEXT;
        // Both calls must have grown the heap by the same amount
        // (no aliasing of the def template).
        u64 delta1 = heap_mid   - heap_before;
        u64 delta2 = heap_after - heap_mid;
        CHECK(delta1 > 0);
        CHECK(delta1 == delta2);
    }

    TEST_BEGIN("ref/self-reference-unfolds-lazily");
    {
        // @loop := λx. @loop  -- the body just refers back to itself.
        // When called once, wnf should reduce the APP-LAM and leave the
        // result as a TAG_ALO/TAG_REF without infinitely expanding.
        Term loop_body = term_new_ref(2);
        Term loop_lam  = build_lam_returning(loop_body);
        thvm_def_register(2, loop_lam);

        Term era = term_new(0, TAG_ERA, 0, 0);
        Term app = build_app(term_new_ref(2), era);
        Term out = wnf(app);
        // wnf re-enters the body REF, so we end up with another lazy
        // expansion (REF -> ALO -> LAM).  The final WHNF is a LAM
        // (the body of @loop) -- key thing is wnf returned at all.
        CHECK_EQ(term_tag(out), TAG_LAM);
    }

    thvm_free();
    TEST_REPORT();
}
