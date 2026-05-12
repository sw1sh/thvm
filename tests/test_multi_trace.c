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

// Count events of a given family appearing AT OR AFTER `from`.
static u64 count_events(u64 from, u8 family) {
    u64 c = 0, n = multi_trace_count();
    for (u64 i = from; i < n; i++) {
        const MultiEvent *e = multi_trace_get(i);
        if (e && e->family == family) c++;
    }
    return c;
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

    // === Label discipline: cross product vs diagonal.  Same-label
    // DUP-SUP is IC's *annihilate* rule -- it's the correct semantics
    // (the DUP's projections correspond pointwise to the SUP's
    // branches), not a runtime bug.  It only *reads* like a bug when
    // the user intended two independent branchial dimensions and
    // accidentally reused a label -- the rule then projects to the
    // diagonal instead of the cross product.  The trace surfaces this
    // user-side mistake as a "spurious" MULTI_MERGE: the MERGE itself
    // is correct IC, but it sits where the user expected a SPLIT.
    // Distinct labels (independent dims) => DUP-SUP commute =>
    // MULTI_SPLIT => 4 leaves.  Shared label (identified dim) =>
    // DUP-SUP annihilate => MULTI_MERGE => 2 leaves. ====================

    TEST_BEGIN("multi-trace/label-collision/distinct-labels-cross-product");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        Term sa = build_sup(1, build_num(1),  build_num(2));    // &1{1,2}
        Term sb = build_sup(2, build_num(10), build_num(20));   // &2{10,20} -- distinct
        Term out[16];
        u64  n = thvm_collapse(term_new_op2(OP_ADD, sa, sb), out, 16);
        CHECK_EQ(n, 4u);                                        // full 2x2 cross product
        u64 sum = 0;
        for (u64 i = 0; i < n; i++) { CHECK_EQ(term_tag(out[i]), TAG_NUM); sum += term_val(out[i]); }
        CHECK_EQ(sum, 11u + 12u + 21u + 22u);                   // {1+10, 1+20, 2+10, 2+20} = 66
        CHECK(count_events(from, MULTI_SPLIT) >= 1u);           // the A-vs-B DUP-SUP commute
        CHECK_EQ(count_events(from, MULTI_MERGE), 0u);          // distinct labels => no DUP-SUP annihilate
        CHECK(find_event(from, RULE_DUP_SUP_COM, MULTI_SPLIT) != (u64)-1);
        multi_trace_free();
    }

    TEST_BEGIN("multi-trace/label-collision/shared-label-spurious-merge");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        Term sa = build_sup(1, build_num(1),  build_num(2));    // &1{1,2}
        Term sb = build_sup(1, build_num(10), build_num(20));   // &1{10,20} -- SAME label, identified dim
        Term out[16];
        u64  n = thvm_collapse(term_new_op2(OP_ADD, sa, sb), out, 16);
        CHECK_EQ(n, 2u);                                        // only the diagonal survives
        u64 sum = 0;
        for (u64 i = 0; i < n; i++) { CHECK_EQ(term_tag(out[i]), TAG_NUM); sum += term_val(out[i]); }
        CHECK_EQ(sum, 11u + 22u);                               // {1+10, 2+20} = 33
        CHECK(count_events(from, MULTI_MERGE) >= 1u);           // shared label => DUP-SUP annihilate fires
        CHECK(find_event(from, RULE_DUP_SUP_ANN, MULTI_MERGE) != (u64)-1);
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

    // === M1: wire provenance.  WIRE_PROV_BUMP stamps wire_prov[loc]
    // at every heap mutation while the trace is on; multi_emit_body
    // captures the active pair's wire_prov into MultiEvent.consumed.
    // Edge `F -> E` in the causal graph iff `E.consumed[*] == F.id`.

    // Sentinel-safe: locs that were never WIRE_PROV_BUMP'd (either
    // pre-trace or never written at all) read as MULTI_WIRE_NONE.
    TEST_BEGIN("multi-trace/wire-prov/sentinel-fresh");
    {
        multi_trace_init(0);
        CHECK_EQ(multi_wire_prov_get(0),         MULTI_WIRE_NONE);
        CHECK_EQ(multi_wire_prov_get(1000),      MULTI_WIRE_NONE);
        CHECK_EQ(multi_wire_prov_get(1ULL<<30),  MULTI_WIRE_NONE);
        // multi_trace_init flips trace off; pre-trace heap_set must
        // not stamp anything either.
        u64 loc = heap_alloc(1);
        heap_set(loc, term_new(0, TAG_NUM, 0, 7));
        CHECK_EQ(multi_wire_prov_get(loc), MULTI_WIRE_NONE);
        multi_trace_free();
    }

    // Heap_set inside an event stamps wire_prov correctly.  We drive
    // it directly: ITRS++, multi_emit, then heap_set -- mirrors the
    // standard interact_* pattern.  The freshly-stamped loc must
    // carry this event's id.
    TEST_BEGIN("multi-trace/wire-prov/heap-set-stamps");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 loc = heap_alloc(1);
        // Pre-emit: still sentinel.
        CHECK_EQ(multi_wire_prov_get(loc), MULTI_WIRE_NONE);
        u64 id_before = ITRS;
        ITRS++;
        multi_emit(RULE_APP_LAM, MULTI_TERM, 0, 0, 0);
        heap_set(loc, term_new(0, TAG_NUM, 0, 99));
        // Post-emit: stamped with id_before (= ITRS-1 captured by emit).
        CHECK_EQ(multi_wire_prov_get(loc), (u32)id_before);
        multi_trace_free();
    }

    // Two-event causal chain: event N stamps wire_prov[L]; event N+1
    // emits with term_a having term_val(term_a) == L, so consumed[0]
    // points back at event N.  This is the minimum hand-checkable
    // causal-graph edge.  Uses absolute ITRS-relative ids -- the global
    // ITRS counter is NOT reset by multi_trace_init, only the events
    // buffer is.
    TEST_BEGIN("multi-trace/wire-prov/consumed-points-at-producer");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 loc = heap_alloc(1);

        // Event A: heap_set bumps wire_prov[loc] = ev_a_id.
        u32 ev_a_id = (u32)ITRS;
        ITRS++;
        multi_emit(RULE_APP_LAM, MULTI_TERM, 0, 0, 0);
        heap_set(loc, term_new(0, TAG_NUM, 0, 7));
        CHECK_EQ(multi_wire_prov_get(loc), ev_a_id);

        // Event B: a fake APP term pointing at `loc` -- multi_emit
        // looks up wire_prov[term_val(term_a)] = wire_prov[loc] =
        // ev_a_id.
        Term faketerm = term_new(0, TAG_APP, 0, loc);
        u32 ev_b_id = (u32)ITRS;
        ITRS++;
        multi_emit(RULE_APP_LAM, MULTI_TERM, (u64)faketerm, 0, 0);

        // Event B is the 2nd event in this trace buffer.
        const MultiEvent *eB = multi_trace_get(1);
        CHECK_EQ(eB->id,          (u64)ev_b_id);
        CHECK_EQ(eB->n_consumed,  1u);
        CHECK_EQ(eB->consumed[0], ev_a_id);          // A is the producer
        CHECK_EQ(eB->consumed[1], MULTI_WIRE_NONE);  // unused slot
        // Event A's own consumed[] is empty (term_a/term_b both 0).
        const MultiEvent *eA = multi_trace_get(0);
        CHECK_EQ(eA->id,          (u64)ev_a_id);
        CHECK_EQ(eA->n_consumed,  0u);
        CHECK_EQ(eA->consumed[0], MULTI_WIRE_NONE);
        CHECK_EQ(eA->consumed[1], MULTI_WIRE_NONE);
        multi_trace_free();
    }

    // Real interaction (DUP-SUP-COM): when the active pair was built
    // BEFORE the trace turned on, the SPLIT event's consumed[0] is
    // MULTI_WIRE_NONE (the SUP was never WIRE_PROV_BUMP'd).  But the
    // event still stamps the freshly-allocated cells with its own
    // id; we verify that by scanning wire_prov over the heap range
    // newly bumped during the traced reduction.  This is the M1
    // analogue of "did this event produce anything?", needed before
    // M2's branch-tree can attribute branches to it.
    TEST_BEGIN("multi-trace/wire-prov/dup-sup-com-stamps-fresh-cells");
    {
        multi_trace_init(0);
        // Build the redex BEFORE turning trace on so wire_prov[sup_loc]
        // stays at MULTI_WIRE_NONE for a clean consumed[0] check.
        CURRENT_CTX->trace = 0;
        Term era = term_new(0, TAG_ERA, 0, 0);
        Term lam = term_new(0, TAG_LAM, 0, 0);
        Term sup = build_sup(11, era, lam);      // sup label 11
        u64  dup = build_dup(sup);
        Term dp0 = term_new(0, TAG_DP0, 7, dup); // dup label 7
        // Now turn trace on and reduce.
        u64 heap_before = HEAP_NEXT;
        CURRENT_CTX->trace = 1;
        u64 from = multi_trace_count();
        (void)cnf(dp0);
        u64 heap_after = HEAP_NEXT;
        u64 idx = find_event(from, RULE_DUP_SUP_COM, MULTI_SPLIT);
        CHECK(idx != (u64)-1);
        const MultiEvent *e = multi_trace_get(idx);
        // dup_sup.c emits with (loc, sup) as the two carriers:
        //   consumed[0] = wire_prov[loc]        -- DUP body cell
        //   consumed[1] = wire_prov[term_val(sup)] -- SUP partner
        // Both came from pre-trace construction here, so both are
        // sentinel.
        CHECK_EQ(e->n_consumed,  2u);
        CHECK_EQ(e->consumed[0], MULTI_WIRE_NONE);
        CHECK_EQ(e->consumed[1], MULTI_WIRE_NONE);
        // The SPLIT event allocated 6 cells and heap_set'd each; their
        // wire_prov entries must carry the SPLIT's id.  Scan the heap
        // range bumped during cnf and count stamps == e->id.
        u32 split_id = (u32)e->id;
        u64 n_stamped = 0;
        for (u64 i = heap_before; i < heap_after; i++) {
            if (multi_wire_prov_get(i) == split_id) n_stamped++;
        }
        CHECK(n_stamped >= 6u);   // dup_sup.c commute-case alloc(6)
        multi_trace_free();
    }

    // Wire-prov resets across multi_trace_reset: a fresh trace must
    // not see ids stamped by a prior reduction (those ids reference
    // events no longer in the buffer).
    TEST_BEGIN("multi-trace/wire-prov/reset-clears-prov");
    {
        multi_trace_init(0);
        CURRENT_CTX->trace = 1;
        u64 loc = heap_alloc(1);
        u32 ev_id = (u32)ITRS;
        ITRS++;
        multi_emit(RULE_APP_LAM, MULTI_TERM, 0, 0, 0);
        heap_set(loc, term_new(0, TAG_NUM, 0, 1));
        CHECK_EQ(multi_wire_prov_get(loc), ev_id);
        multi_trace_reset();
        CHECK_EQ(multi_wire_prov_get(loc), MULTI_WIRE_NONE);
        multi_trace_free();
    }
#endif // THVM_TRACE

    thvm_free();
    TEST_REPORT();
}
