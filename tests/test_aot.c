// test_aot.c - AOT (ahead-of-time) compiled definition path.
//
// Coverage:
//   1. aot_register / aot_lookup roundtrip + slot bounds check.
//   2. aot_pop_app_arg pops a real APP frame, returns its arg, and
//      decrements *sp; refuses non-APP top frames.
//   3. aot_make_dup creates DP0/DP1 sharing one body cell -- firing
//      one projection's DUP-CTR makes the other resolve via SUB
//      without a second interaction.
//   4. End-to-end: register an AOT for @id := λx.x; reducing
//      `(REF[id]) ERA` dispatches through the AOT (counter ticks)
//      and produces ERA.  AOT itrs delta = 1 (the APP-LAM the AOT
//      emits in its body).
//   5. Deopt: reducing bare `REF[id]` (no APP frame above) hits
//      the partial-application branch and falls back to ALO; the
//      interpreter then unfolds and we still see ERA when an APP
//      frame is added.
//   6. aot_reset clears AOT_FNS and zeroes the calls counter.

#include "../src/thvm.c"
#include "test.h"

static Term build_id_lam(void) {
    u64  lam_loc = heap_alloc(1);
    Term var     = term_new(0, TAG_VAR, 0, lam_loc);
    heap_set(lam_loc, var);
    return term_new(0, TAG_LAM, 0, lam_loc);
}

static Term build_app(Term f, Term x) {
    u64 loc = heap_alloc(2);
    heap_set(loc + 0, f);
    heap_set(loc + 1, x);
    return term_new(0, TAG_APP, 0, loc);
}

// Tiny AOT for @id: pops one APP arg and returns it.  No fallback
// needed for partial application because we test that branch
// separately by NOT pushing an APP -- the helper returns 0 and we
// fall back to aot_fallback().
static Term aot_test_id(Term *stack, u32 *sp, u32 base) {
    Term arg = aot_pop_app_arg(stack, sp, base);
    if (arg == 0) {
        // Partial application: rebuild the lazy form via the
        // generic fallback.  test_def_id_slot is the global slot
        // we registered under (set by the caller before invoking
        // wnf so the fallback finds the right book template).
        return aot_fallback(0);
    }
    ITRS++;   // counts as APP-LAM
    return arg;
}

int main(void) {
    thvm_init();

    // === 1. register / lookup roundtrip =====================
    TEST_BEGIN("aot/register-and-lookup");
    {
        CHECK_EQ((u64)aot_lookup(0), (u64)NULL);
        aot_register(0, aot_test_id);
        CHECK_EQ((u64)aot_lookup(0), (u64)aot_test_id);
        // Bounds: out-of-range register should be a silent no-op
        // (no crash, lookup still returns NULL).
        aot_register(DEFS_CAP + 5, aot_test_id);
        CHECK_EQ((u64)aot_lookup(DEFS_CAP + 5), (u64)NULL);
    }

    // === 2. aot_pop_app_arg ================================
    TEST_BEGIN("aot/pop-app-arg");
    {
        Term *stack = WNF_STACK;
        u32 base = 0;
        u32 sp   = 0;

        // Empty stack: pop returns 0.
        CHECK_EQ(aot_pop_app_arg(stack, &sp, base), 0);
        CHECK_EQ(sp, base);

        // Push an APP cell and verify pop returns its arg + sp shrinks.
        Term era = term_new(0, TAG_ERA, 0, 0);
        Term app = build_app(term_new(0, TAG_LAM, 0, 0), era);
        stack[sp++] = app;
        Term got = aot_pop_app_arg(stack, &sp, base);
        CHECK_EQ(got, era);
        CHECK_EQ(sp, base);

        // Push something that's NOT an APP (a NUM) -- pop refuses.
        Term num = term_new(0, TAG_NUM, DT_INT32, 42);
        stack[sp++] = num;
        CHECK_EQ(aot_pop_app_arg(stack, &sp, base), 0);
        CHECK_EQ(sp, base + 1);   // unchanged: caller still owns the frame
        sp = base;                // clean up
    }

    // === 3. aot_make_dup =====================================
    TEST_BEGIN("aot/make-dup-shares-body");
    {
        // Build a CTR (#SUC{ZER}) and dup it.  Firing DP0 should
        // produce a new SUC; reading DP1 should hit the SUB path
        // (no second DUP-NOD) and resolve to a SUC of equivalent
        // shape.  Total interactions = 1 (single DUP-CTR fire,
        // arity 1) + 0 (DP1 follows SUB).
        Term zer    = term_new_ctr(0, NULL, 0);
        Term suc[1] = { zer };
        Term ctr    = term_new_ctr(1, suc, 1);

        Term dp0, dp1;
        aot_make_dup(13 /*label*/, ctr, &dp0, &dp1);
        CHECK_EQ(term_tag(dp0), TAG_DP0);
        CHECK_EQ(term_tag(dp1), TAG_DP1);
        CHECK_EQ(term_val(dp0), term_val(dp1));   // SAME loc -> sharing
        CHECK_EQ(term_ext(dp0), 13);
        CHECK_EQ(term_ext(dp1), 13);

        u64 itrs0 = ITRS;
        Term r0 = wnf(dp0);
        Term r1 = wnf(dp1);
        CHECK_EQ(term_tag(r0), TAG_CTR);
        CHECK_EQ(term_ext(r0), 1);
        CHECK_EQ(term_tag(r1), TAG_CTR);
        CHECK_EQ(term_ext(r1), 1);
        // One DUP-CTR fire for the outer SUC (arity 1).  The
        // inner DP0/DP1 wrapping each child stay lazy because wnf
        // is weak-head -- nothing forces the SUC's child here.
        // The second projection (DP1) resolves via SUB without
        // firing, so the count stays at 1 across both wnf() calls.
        CHECK_EQ(ITRS - itrs0, 1);
    }

    // === 4. AOT dispatch end-to-end ==========================
    TEST_BEGIN("aot/dispatch-end-to-end");
    {
        thvm_def_register(0, build_id_lam());
        aot_register(0, aot_test_id);

        u64 itrs0  = ITRS;
        u64 calls0 = aot_calls();
        Term era   = term_new(0, TAG_ERA, 0, 0);
        Term app   = build_app(term_new_ref(0), era);
        Term out   = wnf(app);
        CHECK_EQ(term_tag(out), TAG_ERA);
        // AOT did its own ITRS++ (one APP-LAM).
        CHECK_EQ(ITRS - itrs0, 1);
        // wnf entered the AOT exactly once.
        CHECK_EQ(aot_calls() - calls0, 1);
    }

    // === 5. Partial-application deopt path ===================
    TEST_BEGIN("aot/deopt-partial-application");
    {
        // Bare REF[0] with no APP frame on top: AOT pops 0, falls
        // back to aot_fallback() which rebuilds the lazy ALO.
        // wnf re-enters the ALO, unfolds to LAM, and parks at LAM
        // (WHNF for an unapplied lambda).  Then we apply it to
        // ERA in a separate wnf and verify it still beta-reduces.
        thvm_def_register(0, build_id_lam());
        aot_register(0, aot_test_id);
        u64 calls0 = aot_calls();
        Term ref   = term_new_ref(0);
        Term out1  = wnf(ref);
        // The AOT fired once (returned aot_fallback) -- the wnf
        // re-entered the resulting ALO via the existing TAG_ALO
        // case.  Not a LAM directly because alo_realize for a LAM
        // returns a fresh dyn LAM cell, which the interpreter
        // unwraps next.
        CHECK(aot_calls() - calls0 >= 1);
        CHECK_EQ(term_tag(out1), TAG_LAM);

        // Now apply to ERA: the AOT fires again (this time with an
        // APP frame on top), pops the arg, returns it.
        Term era  = term_new(0, TAG_ERA, 0, 0);
        Term app  = build_app(term_new_ref(0), era);
        Term out2 = wnf(app);
        CHECK_EQ(term_tag(out2), TAG_ERA);
    }

    // === 6. aot_reset ========================================
    TEST_BEGIN("aot/reset-clears-table-and-counter");
    {
        thvm_def_register(0, build_id_lam());
        aot_register(0, aot_test_id);
        // Drive a call so the counter ticks above 0.
        Term era = term_new(0, TAG_ERA, 0, 0);
        wnf(build_app(term_new_ref(0), era));
        CHECK(aot_calls() > 0);
        CHECK(aot_lookup(0) != NULL);

        aot_reset();
        CHECK_EQ((u64)aot_lookup(0), (u64)NULL);
        CHECK_EQ(aot_calls(), 0);
    }

    thvm_free();
    TEST_REPORT();
}
