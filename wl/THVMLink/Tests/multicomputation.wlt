(* multicomputation.wlt -- VerificationTests for the WL surface of the
   MultiEvent reduction trace (Multicomputation.wl + the C side in
   src/instrument/multi.c).  Mirrors the C-side tests/test_multi_trace.c
   coverage from WL.

   The default `make wl` builds a trace-enabled (Metal-enabled) dylib,
   so these tests run normally.  If you opted out with `WL_TRACE=0
   make wl`, TMultiTraceQ[] is False, the block is not evaluated,
   and this file contributes 0 tests rather than failures. *)

TInit[];

If[ ! TMultiTraceQ[],

    WriteString["stdout", "multicomputation.wlt: SKIPPED -- trace-free dylib (WL_TRACE=0); rebuild with default `make wl` to exercise these.\n"],

    (* === trace-enabled dylib: run the tests ====================== *)

    VerificationTest[
        TMultiTraceQ[],
        True,
        TestID -> "TMultiTraceQ[] is True (dylib built with -DTHVM_TRACE)"
    ];

    (* Trace TCollapse on TDup[TOp2["+", TSup[1,2], 3]] -- fresh labels
       on the SUP and the DUP, so DUP-SUP commutes (cross product) and
       both projections collapse to {4, 5}.  The trace records the
       OP2-SUP slide, the DUP-SUP commute (SPLIT), the OP2-NUM-NUM
       folds (TERM), and the DUP-NUM passthroughs (PLUMB); no spurious
       MERGE. *)
    VerificationTest[
        TInit[];
        Module[{dp0, dp1, out},
            {dp0, dp1} = TDup[TOp2["+", TSup[1, 2], 3]];
            out = TMultiTrace[TCollapse[dp0]];
            {
                Sort[TTermVal /@ out["Result"]],                       (* {4, 5} *)
                SubsetQ[                                               (* keys present *)
                    Keys[First[out["Trace"]]],
                    {"id", "rule", "ruleCode", "family", "familyCode",
                     "termA", "termB", "deltaLabel", "consumed"}],
                Count[out["Trace"], e_ /; e["family"] === "SLIDE"] >= 1,
                Count[out["Trace"], e_ /; e["family"] === "SPLIT"] >= 1,
                Count[out["Trace"], e_ /; e["rule"]   === "DUP_SUP_COM"] >= 1,
                Count[out["Trace"], e_ /; e["family"] === "MERGE"]                  (* distinct labels => no annihilate *)
            }],
        {{4, 5}, True, True, True, True, 0},
        TestID -> "TMultiTrace[TCollapse[dp0]] -- fresh labels, cross product, SPLIT not MERGE"
    ];

    (* M1 wire provenance.  Each event carries a "consumed" list of
       producer event ids.  Two properties to check:
        (a) every producer id mentioned in some consumed list is an
            event id that actually appears in the trace -- no dangling
            references;
        (b) producers always fire BEFORE their consumers (= the causal
            graph is a DAG, which TCausalGraph also enforces). *)
    VerificationTest[
        TInit[];
        Module[{dp0, dp1, trace, ids, consumers, dag},
            {dp0, dp1} = TDup[TOp2["+", TSup[1, 2], 3]];
            trace = TMultiTrace[TCollapse[dp0]]["Trace"];
            ids = trace[[All, "id"]];
            (* (a) every consumed id is a valid event id *)
            consumers = DeleteDuplicates @ Flatten[trace[[All, "consumed"]]];
            (* (b) for each (consumer, producer) pair, producer-id < consumer-id *)
            dag = AllTrue[trace,
                e |-> AllTrue[e["consumed"], # < e["id"] &]];
            {
                SubsetQ[ids, consumers],
                Length[consumers] > 0,         (* at least some chains observed *)
                dag
            }],
        {True, True, True},
        TestID -> "TMultiTrace -- consumed[] references valid earlier events"
    ];

    (* TCausalGraph builds a directed Graph[] from the consumed[]
       edges.  Properties: it's directed and acyclic, vertices match
       the event ids, edge count matches the total number of producer
       references that point at other events in the trace. *)
    VerificationTest[
        TInit[];
        Module[{dp0, dp1, trace, g, edgeRefs},
            {dp0, dp1} = TDup[TOp2["+", TSup[1, 2], 3]];
            trace = TMultiTrace[TCollapse[dp0]]["Trace"];
            g = TCausalGraph[trace];
            edgeRefs = Total @ Map[
                e |-> Length @ Select[
                    e["consumed"],
                    MemberQ[trace[[All, "id"]], #] &],
                trace];
            {
                Head[g] === Graph,
                AcyclicGraphQ[g],
                DirectedGraphQ[g],
                VertexCount[g] === Length[trace],
                EdgeCount[g]   === edgeRefs
            }],
        {True, True, True, True, True},
        TestID -> "TCausalGraph[trace] -- DAG matching consumed[] edges"
    ];

    (* Same shape but the SUP and DUP share label 7: same-label
       DUP-SUP is IC's annihilate rule -- correct semantics, projects
       pointwise to the diagonal.  dp0 -> {4} (= 1+3), dp1 -> {5}
       (= 2+3).  The trace has a MULTI_MERGE / DUP_SUP_ANN where the
       distinct-label version had a MULTI_SPLIT.  Reading this as a
       "bug" is a user-side judgement: it's a bug iff you meant the
       two branchial dimensions to be independent. *)
    VerificationTest[
        TInit[];
        Module[{e0, e1, out0, out1},
            {e0, e1} = TDup[7, TOp2["+", TSup[7, 1, 2], 3]];
            out0 = TMultiTrace[TCollapse[e0]];
            out1 = TMultiTrace[TCollapse[e1]];
            {
                TTermVal /@ out0["Result"],                            (* {4} *)
                TTermVal /@ out1["Result"],                            (* {5} *)
                Count[out0["Trace"], e_ /; e["family"] === "MERGE"] >= 1,
                Count[out0["Trace"], e_ /; e["rule"]   === "DUP_SUP_ANN"] >= 1
            }],
        {{4}, {5}, True, True},
        TestID -> "TMultiTrace -- shared label collapses to the diagonal, trace shows MERGE"
    ];

    (* Trace recording is scoped to the expr: multi_trace_init resets
       the buffer, so a prior (independent) reduction doesn't leak in.
       A throwaway reduction runs first, then the traced one; the
       trace must look exactly like a fresh trace of the same expr. *)
    VerificationTest[
        TInit[];
        Module[{a, b, fresh, after},
            {a, b}   = TDup[TOp2["+", TSup[1, 2], 3]];
            fresh    = TMultiTrace[TCollapse[a]];
            {a, b}   = TDup[TOp2["+", TSup[1, 2], 3]];      (* a fresh, independent term *)
            TCollapse[TDup[TOp2["*", TSup[4, 5], 6]][[1]]]; (* an unrelated reduction, untraced *)
            after    = TMultiTrace[TCollapse[a]];
            {
                Sort[TTermVal /@ after["Result"]] === Sort[TTermVal /@ fresh["Result"]],
                Sort[after["Trace"][[All, "family"]]] === Sort[fresh["Trace"][[All, "family"]]]
            }],
        {True, True},
        TestID -> "TMultiTrace buffer is reset -- a prior untraced reduction doesn't leak in"
    ];
]
