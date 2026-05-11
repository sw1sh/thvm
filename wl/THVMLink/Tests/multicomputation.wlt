(* multicomputation.wlt -- VerificationTests for the WL surface of the
   MultiEvent reduction trace (Multicomputation.wl + the C side in
   src/instrument/multi.c).  Mirrors the C-side tests/test_multi_trace.c
   coverage from WL.

   Requires a trace-enabled dylib (`make WL_TRACE=1 wl`).  The default
   `make wl` builds a trace-free (Metal-enabled) dylib, under which
   these tests SKIP -- TMulticompTraceQ[] is False, the whole block is
   not evaluated, and the file contributes 0 tests rather than
   failures.  Run `make WL_TRACE=1 wl && make wl-test` to exercise
   them. *)

TInit[];

If[ ! TMulticompTraceQ[],

    Print["multicomputation.wlt: SKIPPED -- trace-free dylib; "
          <> "rebuild with `make WL_TRACE=1 wl` to exercise these."],

    (* === trace-enabled dylib: run the tests ====================== *)

    VerificationTest[
        TMulticompTraceQ[],
        True,
        TestID -> "TMulticompTraceQ[] is True (dylib built with -DTHVM_TRACE)"
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
            out = TMulticompTrace[TCollapse[dp0]];
            {
                Sort[TTermVal /@ out["Result"]],                       (* {4, 5} *)
                SubsetQ[                                               (* keys present *)
                    Keys[First[out["Trace"]]],
                    {"id", "rule", "ruleCode", "family", "familyCode",
                     "termA", "termB", "deltaLabel"}],
                Count[out["Trace"], e_ /; e["family"] === "SLIDE"] >= 1,
                Count[out["Trace"], e_ /; e["family"] === "SPLIT"] >= 1,
                Count[out["Trace"], e_ /; e["rule"]   === "DUP_SUP_COM"] >= 1,
                Count[out["Trace"], e_ /; e["family"] === "MERGE"]                  (* no spurious merge *)
            }],
        {{4, 5}, True, True, True, True, 0},
        TestID -> "TMulticompTrace[TCollapse[dp0]] -- fresh labels, cross product, SPLIT not MERGE"
    ];

    (* Same shape but the SUP and DUP share label 7: DUP-SUP
       annihilates, only the diagonal survives -- dp0 -> {4} (= 1+3),
       dp1 -> {5} (= 2+3) -- and the trace has the spurious MERGE /
       DUP_SUP_ANN that the correct labeling didn't.  This is the
       headline "see the bug in the trace" case. *)
    VerificationTest[
        TInit[];
        Module[{e0, e1, out0, out1},
            {e0, e1} = TDup[7, TOp2["+", TSup[7, 1, 2], 3]];
            out0 = TMulticompTrace[TCollapse[e0]];
            out1 = TMulticompTrace[TCollapse[e1]];
            {
                TTermVal /@ out0["Result"],                            (* {4} *)
                TTermVal /@ out1["Result"],                            (* {5} *)
                Count[out0["Trace"], e_ /; e["family"] === "MERGE"] >= 1,
                Count[out0["Trace"], e_ /; e["rule"]   === "DUP_SUP_ANN"] >= 1
            }],
        {{4}, {5}, True, True},
        TestID -> "TMulticompTrace -- shared label collapses to the diagonal, trace shows MERGE"
    ];

    (* Trace recording is scoped to the expr: multi_trace_init resets
       the buffer, so a prior (independent) reduction doesn't leak in.
       A throwaway reduction runs first, then the traced one; the
       trace must look exactly like a fresh trace of the same expr. *)
    VerificationTest[
        TInit[];
        Module[{a, b, fresh, after},
            {a, b}   = TDup[TOp2["+", TSup[1, 2], 3]];
            fresh    = TMulticompTrace[TCollapse[a]];
            {a, b}   = TDup[TOp2["+", TSup[1, 2], 3]];      (* a fresh, independent term *)
            TCollapse[TDup[TOp2["*", TSup[4, 5], 6]][[1]]]; (* an unrelated reduction, untraced *)
            after    = TMulticompTrace[TCollapse[a]];
            {
                Sort[TTermVal /@ after["Result"]] === Sort[TTermVal /@ fresh["Result"]],
                Sort[after["Trace"][[All, "family"]]] === Sort[fresh["Trace"][[All, "family"]]]
            }],
        {True, True},
        TestID -> "TMulticompTrace buffer is reset -- a prior untraced reduction doesn't leak in"
    ];
]
