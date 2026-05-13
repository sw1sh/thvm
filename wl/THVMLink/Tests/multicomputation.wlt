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
       folds (TERM), and the DUP-NUM passthroughs (DIST); no spurious
       MERGE. *)
    VerificationTest[
        TInit[];
        Block[{dp0, dp1, out, trace},
            {dp0, dp1} = TDup[TOp2["+", TSup[1, 2], 3]];
            out = TMultiTrace[TCollapse[dp0]];
            trace = Catenate[out[[All, "Events"]]];
            {
                Sort[TTermVal /@ Last[out]["Term"]],                   (* {4, 5} *)
                SubsetQ[                                               (* keys present *)
                    Keys[First[trace]],
                    {"id", "rule", "ruleCode", "family", "familyCode",
                     "termA", "termB", "deltaLabel", "consumed"}],
                Count[trace, e_ /; e["family"] === "SLIDE"] >= 1,
                Count[trace, e_ /; e["family"] === "SPLIT"] >= 1,
                Count[trace, e_ /; e["rule"]   === "DUP_SUP_COM"] >= 1,
                Count[trace, e_ /; e["family"] === "MERGE"]            (* distinct labels => no annihilate *)
            }
        ],
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
        Block[{dp0, dp1, trace, ids, consumers, dag},
            {dp0, dp1} = TDup[TOp2["+", TSup[1, 2], 3]];
            trace = Catenate[TMultiTrace[TCollapse[dp0]][[All, "Events"]]];
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
            }
        ],
        {True, True, True},
        TestID -> "TMultiTrace -- consumed[] references valid earlier events"
    ];

    (* TCausalGraph builds a directed Graph[] from the consumed[]
       edges.  Properties: it's directed and acyclic, vertices match
       the event ids, edge count matches the total number of producer
       references that point at other events in the trace. *)
    VerificationTest[
        TInit[];
        Block[{dp0, dp1, trace, g, edgeRefs},
            {dp0, dp1} = TDup[TOp2["+", TSup[1, 2], 3]];
            trace = Catenate[TMultiTrace[TCollapse[dp0]][[All, "Events"]]];
            (* Use unreduced graph: this test checks one-to-one
               correspondence between consumed[] entries and graph
               edges, which only holds before transitive reduction. *)
            g = TCausalGraph[trace, "TransitiveReduction" -> False];
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
            }
        ],
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
        Block[{e0, e1, out0, out1, trace0},
            {e0, e1} = TDup[7, TOp2["+", TSup[7, 1, 2], 3]];
            out0 = TMultiTrace[TCollapse[e0]];
            out1 = TMultiTrace[TCollapse[e1]];
            trace0 = Catenate[out0[[All, "Events"]]];
            {
                TTermVal /@ Last[out0]["Term"],                        (* {4} *)
                TTermVal /@ Last[out1]["Term"],                        (* {5} *)
                Count[trace0, e_ /; e["family"] === "MERGE"] >= 1,
                Count[trace0, e_ /; e["rule"]   === "DUP_SUP_ANN"] >= 1
            }
        ],
        {{4}, {5}, True, True},
        TestID -> "TMultiTrace -- shared label collapses to the diagonal, trace shows MERGE"
    ];

    (* Trace recording is scoped to the expr: multi_trace_init resets
       the buffer, so a prior (independent) reduction doesn't leak in.
       A throwaway reduction runs first, then the traced one; the
       trace must look exactly like a fresh trace of the same expr. *)
    VerificationTest[
        TInit[];
        Block[{a, b, fresh, after, freshFams, afterFams},
            {a, b}   = TDup[TOp2["+", TSup[1, 2], 3]];
            fresh    = TMultiTrace[TCollapse[a]];
            {a, b}   = TDup[TOp2["+", TSup[1, 2], 3]];      (* a fresh, independent term *)
            TCollapse[TDup[TOp2["*", TSup[4, 5], 6]][[1]]]; (* an unrelated reduction, untraced *)
            after    = TMultiTrace[TCollapse[a]];
            freshFams = Catenate[fresh[[All, "Events"]]][[All, "family"]];
            afterFams = Catenate[after[[All, "Events"]]][[All, "family"]];
            {
                Sort[TTermVal /@ Last[after]["Term"]] ===
                    Sort[TTermVal /@ Last[fresh]["Term"]],
                Sort[afterFams] === Sort[freshFams]
            }
        ],
        {True, True},
        TestID -> "TMultiTrace buffer is reset -- a prior untraced reduction doesn't leak in"
    ];

    (* === DP canonical: pre-resolution wraps body with projection
       index (`{DP0|DP1, label, body}`), post-SUB resolves to the
       branch's canonical alone.  Both projections of an UNRESOLVED
       DUP are distinct vertices; post-fire they collapse onto the
       resolved branch.  Locks in the user's `0/1 superscript`
       semantic for the multiway view. *)
    VerificationTest[
        TInit[];
        Block[{dp0, dp1, body},
            {dp0, dp1} = TDup[3, 42];
            body = THVMLink`Private`canonicalForm[dp0];
            {
                MatchQ[body, {"DP0", 3, {"NUM", 42}}],
                MatchQ[THVMLink`Private`canonicalForm[dp1], {"DP1", 3, {"NUM", 42}}]
            }
        ],
        {True, True},
        TestID -> "canonicalForm wraps unresolved DP with projection index + label"
    ];

    (* === TMultiSteps on `TSup[L, ...] + N` (no enclosing DUP):
       OP2 commutes over SUP, the introduced inner DUP gets atom-
       copied, then both branch arithmetics fold.  4 events, 5
       slice snapshots, no self-loops, final slice = {4, 5}. *)
    VerificationTest[
        TInit[];
        Block[{seed, steps, g, edges},
            seed = TSup[1, 2] + 3;
            steps = TMultiTrace[seed];
            g = TMultiwayGraph[steps];
            edges = EdgeList[g];
            {
                Length[steps],
                Total[Length /@ steps[[All, "Events"]]],
                AllTrue[edges, e |-> First[e] =!= Last[e]],
                Sort[Last[steps]["SliceCanonical"]] ===
                    Sort[{{"NUM", 4}, {"NUM", 5}}]
            }
        ],
        {5, 4, True, True},
        TestID -> "TMultiSteps + TMultiwayGraph: TSup[1, 2] + 3 yields {4, 5}, no self-loops"
    ];

    (* === TMultiSteps on `First @ TDup[L, TLam[x, x + N][TSup[L, ...]]]`
       (same-label DUP-SUP-ANN): six steps, five events, no self-loops
       in the multiway view (every event changes canonical).  Locks in
       the user's "no self-loop after DUP_NUM" requirement. *)
    VerificationTest[
        TInit[];
        Block[{seed, steps, g, edges},
            seed = First @ TDup[0, TLam[x, x + 3][TSup[0, 1, 2]]];
            steps = TMultiTrace[seed];
            g = TMultiwayGraph[steps];
            edges = EdgeList[g];
            {
                Length[steps],
                Total[Length /@ steps[[All, "Events"]]],
                AllTrue[edges, e |-> First[e] =!= Last[e]],
                Length[edges]
            }
        ],
        {6, 5, True, 5},
        TestID -> "TMultiSteps + TMultiwayGraph: no self-loops on First@TDup[0, lam[sup[0,...]]]"
    ];

    (* === TMultiSteps + TMultiwayGraph on distinct-label DUP-SUP-COM:
       cross-product splits into 2 branches.  Locks in the
       multiway view structure for the canonical fan-out case. *)
    VerificationTest[
        TInit[];
        Block[{seed, steps, g, edges},
            seed = First @ TDup[3, TLam[x, x + 3][TSup[7, 1, 2]]];
            steps = TMultiTrace[seed];
            g = TMultiwayGraph[steps];
            edges = EdgeList[g];
            {
                AllTrue[edges, e |-> First[e] =!= Last[e]],
                Sort[Last[steps]["SliceCanonical"]] ===
                    Sort[{{"NUM", 4}, {"NUM", 5}}]
            }
        ],
        {True, True},
        TestID -> "TMultiSteps + TMultiwayGraph: distinct-label DUP-SUP-COM yields {4, 5}"
    ];

    (* === DUP_SUP_ANN trace event has deduplicated `consumed` list:
       both wires often come from the same prior OP2_SUP, but the
       causal graph should record one edge per producer, not two. *)
    VerificationTest[
        TInit[];
        Block[{out, trace, ann},
            out = TMultiTrace[TCollapse[
                First @ TDup[0, TLam[x, x + 3][TSup[0, 1, 2]]]]];
            trace = Catenate[out[[All, "Events"]]];
            ann = SelectFirst[trace, #["rule"] === "DUP_SUP_ANN" &];
            {ann["consumed"], DeleteDuplicates[ann["consumed"]]}
        ],
        {{1}, {1}},
        TestID -> "DUP_SUP_ANN event consumed list is deduplicated"
    ];

    (* === TCausalGraph accepts either a flat trace or a steps list
       (an event-only TMultiTrace OR a full per-step record).  The
       resulting graph is identical. *)
    VerificationTest[
        TInit[];
        Block[{traceForm, stepsForm, traced, stepped},
            traced  = TMultiTrace[TCollapse[TSup[1, 2] + 3]];
            traceForm = TCausalGraph[Catenate[traced[[All, "Events"]]]];
            TInit[];
            stepped = TMultiTrace[TSup[1, 2] + 3];
            stepsForm = TCausalGraph[stepped];
            {
                Length @ VertexList[traceForm] === Length @ VertexList[stepsForm],
                Sort @ EdgeList[traceForm] === Sort @ EdgeList[stepsForm]
            }
        ],
        {True, True},
        TestID -> "TCausalGraph accepts either trace or steps; result is the same"
    ];

    (* TMultiwayGraph requires steps -- a flat trace is missing the
       slice info needed to form WPP-style vertices.  Should fail
       loudly rather than producing a wrong-looking graph. *)
    VerificationTest[
        TInit[];
        Block[{trace},
            trace = Catenate[
                TMultiTrace[TCollapse[TSup[1, 2] + 3]][[All, "Events"]]];
            TMultiwayGraph[trace]
        ],
        $Failed,
        TMultiwayGraph::needsteps,
        TestID -> "TMultiwayGraph rejects flat-trace input with a clear message"
    ];
]
