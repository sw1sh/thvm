// test_multi_trace.c -- multicomputation trace M0 vertical slice.
//
// Built TWICE from this one source:
//
//   bin/test_multi_trace        (default CFLAGS, no -DTHVM_TRACE)
//     - All trace-on assertions are #ifdef'd out.
//     - The remaining checks confirm that the runtime behaves
//       identically to its pre-trace self: an APP-LAM beta still
//       fires, ITRS still bumps by exactly one.  This is the
//       acceptance gate for "default builds pay nothing."
//
//   bin/test_multi_trace_on     (-DTHVM_TRACE)
//     - multi_trace_init / multi_trace_reset / multi_trace_count /
//       multi_trace_get are wired up.
//     - With CURRENT_CTX->trace = 0 (the default after _reset), no
//       events are recorded even though the macro is live -- proves
//       the runtime-flag gate.
//     - With CURRENT_CTX->trace = 1, an APP-LAM beta produces
//       exactly one MultiEvent whose `rule` is RULE_APP_LAM and
//       `family` is MULTI_TERM.
//
// Conceptual reading: docs/multicomputation.md.
// Build trajectory: docs/plans/multicomputation_trace.md.

#include "../src/thvm.c"
#include "test.h"

// Build (λx.x) -- heap[lam_loc] = VAR(lam_loc).  Same helper as
// tests/test_app_lam.c.
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

int main(void) {
    thvm_init();

    // === Baseline: the beta still works identically. =================
    // This part runs in BOTH build variants and is the load-bearing
    // gate that the multi_emit() macro hasn't changed behaviour.
    TEST_BEGIN("multi-trace/baseline-beta-still-works");
    {
        Term era  = term_new(0, TAG_ERA, 0, 0);
        Term id   = build_id_lam();
        Term app  = build_app(id, era);
        u64  itrs_before = ITRS;
        Term out  = wnf(app);
        CHECK_EQ(term_tag(out), TAG_ERA);
        CHECK_EQ(ITRS - itrs_before, 1);   // exactly one APP-LAM fired
    }

#ifdef THVM_TRACE
    // === Trace mode, flag off: macro fires the branch but stores
    // nothing.  Proves the runtime-flag gate dominates. =================
    TEST_BEGIN("multi-trace/runtime-flag-off-records-nothing");
    {
        multi_trace_init(0);
        // multi_trace_init leaves CURRENT_CTX->trace = 0
        CHECK_EQ(CURRENT_CTX->trace, 0);
        Term era  = term_new(0, TAG_ERA, 0, 0);
        Term id   = build_id_lam();
        Term app  = build_app(id, era);
        (void)wnf(app);
        CHECK_EQ(multi_trace_count(), 0u);
        multi_trace_free();
    }

    // === Trace mode, flag on: APP-LAM produces exactly one
    // MultiEvent with the right rule + family. ==========================
    TEST_BEGIN("multi-trace/runtime-flag-on-records-app-lam");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        Term era  = term_new(0, TAG_ERA, 0, 0);
        Term id   = build_id_lam();
        Term app  = build_app(id, era);
        u64  itrs_before  = ITRS;
        u64  count_before = multi_trace_count();
        Term out  = wnf(app);
        CHECK_EQ(term_tag(out), TAG_ERA);
        CHECK_EQ(ITRS - itrs_before, 1);
        // Exactly one event was appended.
        CHECK_EQ(multi_trace_count() - count_before, 1u);
        const MultiEvent *e = multi_trace_get(count_before);
        CHECK(e != NULL);
        CHECK_EQ(e->rule,        (u8)RULE_APP_LAM);
        CHECK_EQ(e->family,      (u8)MULTI_TERM);
        CHECK_EQ(e->id,          itrs_before);  // captured pre-bump? no, post.
                                                // ITRS++ runs first, then emit,
                                                // so id == ITRS-1 from the
                                                // observer's POV, i.e. itrs_before.
        // term_a / term_b should be the LAM and ERA terms we passed in.
        CHECK_EQ(e->term_a, (u64)id);
        CHECK_EQ(e->term_b, (u64)era);
        CHECK_EQ(e->delta_label, 0u);  // term-events carry no label delta
        multi_trace_free();
    }

    // === Trace can be toggled on/off mid-run. =========================
    TEST_BEGIN("multi-trace/toggle-mid-run");
    {
        multi_trace_init(0);

        // Beta 1: trace off -> no event.
        {
            Term era = term_new(0, TAG_ERA, 0, 0);
            Term id  = build_id_lam();
            (void)wnf(build_app(id, era));
        }
        CHECK_EQ(multi_trace_count(), 0u);

        // Beta 2: trace on -> one event.
        CURRENT_CTX->trace = 1;
        {
            Term era = term_new(0, TAG_ERA, 0, 0);
            Term id  = build_id_lam();
            (void)wnf(build_app(id, era));
        }
        CHECK_EQ(multi_trace_count(), 1u);

        // Beta 3: trace off again -> still one event total.
        CURRENT_CTX->trace = 0;
        {
            Term era = term_new(0, TAG_ERA, 0, 0);
            Term id  = build_id_lam();
            (void)wnf(build_app(id, era));
        }
        CHECK_EQ(multi_trace_count(), 1u);

        // Reset wipes the log AND clears the flag.
        CURRENT_CTX->trace = 1;
        multi_trace_reset();
        CHECK_EQ(multi_trace_count(), 0u);
        CHECK_EQ(CURRENT_CTX->trace, 0);

        multi_trace_free();
    }

    // === Capacity growth: cross the initial-cap boundary. =============
    // multi_events_push doubles on overflow; check it actually does.
    TEST_BEGIN("multi-trace/capacity-grows");
    {
        multi_trace_init(2);                    // tiny initial cap
        CHECK_EQ(CURRENT_CTX->multi_events_cap, 2u);
        CURRENT_CTX->trace = 1;
        for (int i = 0; i < 5; i++) {
            Term era = term_new(0, TAG_ERA, 0, 0);
            Term id  = build_id_lam();
            (void)wnf(build_app(id, era));
        }
        CHECK_EQ(multi_trace_count(), 5u);
        CHECK(CURRENT_CTX->multi_events_cap >= 5u);
        multi_trace_free();
    }
#endif // THVM_TRACE

    thvm_free();
    TEST_REPORT();
}
