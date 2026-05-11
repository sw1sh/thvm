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

// build_sup() and build_num() are provided by tests/test.h.

// Helper: returns the dup_loc holding `body`.  Callers wrap with
// TAG_DP0 / TAG_DP1 to construct the projections.
static u64 build_dup(Term body) {
    u64 loc = heap_alloc(1);
    heap_set(loc, body);
    return loc;
}

#ifdef THVM_TRACE
// Search the current trace for an event with the given (rule, family)
// pair appearing AT OR AFTER `from`.  Returns the event index or
// (u64)-1 if not found.  Used by the cross-family coverage tests.
static u64 find_event(u64 from, u8 rule, u8 family) {
    u64 n = multi_trace_count();
    for (u64 i = from; i < n; i++) {
        const MultiEvent *e = multi_trace_get(i);
        if (e && e->rule == rule && e->family == family) return i;
    }
    return (u64)-1;
}
#endif

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

    // === Cross-family coverage.  Each scenario fires one rule from a
    // different family and checks that the right event appears. =======
    //
    // Coverage matrix:
    //   MULTI_TERM    -- APP-LAM (already covered above)
    //   MULTI_PRUNE   -- APP-ERA: `(* arg)` => ERA
    //   MULTI_SLIDE   -- APP-SUP: `(&L{f,g}) arg` => `&L{(f arg_0), (g arg_1)}`
    //   MULTI_FORK    -- DUP-LAM via cnf on DP0(&L = LAM)
    //   MULTI_MERGE   -- DUP-SUP same label via cnf on DP0(&L = &L{a,b})
    //   MULTI_SPLIT   -- DUP-SUP different label via cnf on DP0(&L = &R{a,b})
    //   MULTI_PLUMB   -- DUP-NUM via cnf on DP0(dup = NUM)

    TEST_BEGIN("multi-trace/family-coverage/app-era-prunes");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        Term era = term_new(0, TAG_ERA, 0, 0);
        Term arg = term_new(0, TAG_LAM, 0, 0);   // any value
        (void)wnf(build_app(era, arg));
        u64 idx = find_event(from, RULE_APP_ERA, MULTI_PRUNE);
        CHECK(idx != (u64)-1);
        multi_trace_free();
    }

    TEST_BEGIN("multi-trace/family-coverage/app-sup-slides");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        Term l1  = build_id_lam();
        Term l2  = build_id_lam();
        Term sup = build_sup(7, l1, l2);
        Term era = term_new(0, TAG_ERA, 0, 0);
        (void)wnf(build_app(sup, era));
        u64 idx = find_event(from, RULE_APP_SUP, MULTI_SLIDE);
        CHECK(idx != (u64)-1);
        const MultiEvent *e = multi_trace_get(idx);
        CHECK_EQ(e->delta_label, 7u);  // SUP label propagates
        multi_trace_free();
    }

    TEST_BEGIN("multi-trace/family-coverage/dup-sup-same-label-merges");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        Term era = term_new(0, TAG_ERA, 0, 0);
        Term lam = term_new(0, TAG_LAM, 0, 0);
        Term sup = build_sup(7, era, lam);
        u64  dup = build_dup(sup);
        Term dp0 = term_new(0, TAG_DP0, 7, dup);
        (void)cnf(dp0);
        u64 idx = find_event(from, RULE_DUP_SUP_ANN, MULTI_MERGE);
        CHECK(idx != (u64)-1);
        const MultiEvent *e = multi_trace_get(idx);
        CHECK_EQ(e->delta_label, 7u);
        multi_trace_free();
    }

    TEST_BEGIN("multi-trace/family-coverage/dup-sup-diff-label-splits");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        Term era = term_new(0, TAG_ERA, 0, 0);
        Term lam = term_new(0, TAG_LAM, 0, 0);
        Term sup = build_sup(11, era, lam);       // sup label 11
        u64  dup = build_dup(sup);
        Term dp0 = term_new(0, TAG_DP0, 7, dup);  // dup label 7 (different)
        (void)cnf(dp0);
        u64 idx = find_event(from, RULE_DUP_SUP_COM, MULTI_SPLIT);
        CHECK(idx != (u64)-1);
        const MultiEvent *e = multi_trace_get(idx);
        CHECK_EQ(e->delta_label, 7u);  // the DUP's label is what we record
        multi_trace_free();
    }

    TEST_BEGIN("multi-trace/family-coverage/dup-lam-forks");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        Term lam = build_id_lam();
        u64  dup = build_dup(lam);
        Term dp0 = term_new(0, TAG_DP0, 3, dup);
        (void)cnf(dp0);
        u64 idx = find_event(from, RULE_DUP_LAM, MULTI_FORK);
        CHECK(idx != (u64)-1);
        const MultiEvent *e = multi_trace_get(idx);
        CHECK_EQ(e->delta_label, 3u);  // the DUP's label
        multi_trace_free();
    }

    TEST_BEGIN("multi-trace/family-coverage/dup-num-plumbs");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        Term num = term_new(0, TAG_NUM, 0, 42);
        u64  dup = build_dup(num);
        Term dp0 = term_new(0, TAG_DP0, 5, dup);
        (void)cnf(dp0);
        u64 idx = find_event(from, RULE_DUP_NUM, MULTI_PLUMB);
        CHECK(idx != (u64)-1);
        multi_trace_free();
    }

    TEST_BEGIN("multi-trace/family-coverage/dup-era-prunes");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        Term era = term_new(0, TAG_ERA, 0, 0);
        u64  dup = build_dup(era);
        Term dp0 = term_new(0, TAG_DP0, 9, dup);
        (void)cnf(dp0);
        u64 idx = find_event(from, RULE_DUP_ERA, MULTI_PRUNE);
        CHECK(idx != (u64)-1);
        multi_trace_free();
    }

    // === Inline WHNF-frame rules (handled in src/wnf/_.c, not as a
    // dedicated interact_*.c).  These fire when an OP2 / EQL / AND /
    // OR / WHEN frame pops. =============================================

    TEST_BEGIN("multi-trace/wnf-inline/op2-num-num-folds");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        Term r = wnf(term_new_op2(OP_ADD, build_num(20), build_num(22)));
        CHECK_EQ(term_tag(r), TAG_NUM);
        CHECK_EQ(term_val(r), 42u);
        u64 idx = find_event(from, RULE_OP2_NUM_NUM, MULTI_TERM);
        CHECK(idx != (u64)-1);
        const MultiEvent *e = multi_trace_get(idx);
        CHECK_EQ(e->delta_label, (u32)OP_ADD);  // opcode in delta_label
        multi_trace_free();
    }

    TEST_BEGIN("multi-trace/wnf-inline/eql-num-num");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        Term r = wnf(term_new_eql(build_num(7), build_num(7)));
        CHECK_EQ(term_tag(r), TAG_NUM);
        CHECK_EQ(term_val(r), 1u);
        CHECK(find_event(from, RULE_EQL_NUM, MULTI_TERM) != (u64)-1);
        multi_trace_free();
    }

    TEST_BEGIN("multi-trace/wnf-inline/eql-era-prunes");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        Term era = term_new(0, TAG_ERA, 0, 0);
        Term r = wnf(term_new_eql(era, build_num(5)));
        CHECK_EQ(term_tag(r), TAG_ERA);
        CHECK(find_event(from, RULE_EQL_ERA, MULTI_PRUNE) != (u64)-1);
        multi_trace_free();
    }

    TEST_BEGIN("multi-trace/wnf-inline/and-short-circuits");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        // AND is strict on its first operand; 0 short-circuits to 0.
        Term r = wnf(term_new_and(build_num(0), build_num(99)));
        CHECK_EQ(term_tag(r), TAG_NUM);
        CHECK_EQ(term_val(r), 0u);
        CHECK(find_event(from, RULE_AND_NUM, MULTI_TERM) != (u64)-1);
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
