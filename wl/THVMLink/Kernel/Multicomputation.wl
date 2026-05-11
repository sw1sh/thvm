(* ::Package:: *)
(* Multicomputation.wl -- WL surface for the MultiEvent reduction
   trace (src/instrument/multi.c).  Conceptual reading in
   docs/multicomputation.md: a SUP-term is a "slice", reduction is
   "slice evolution", collapse is an "observer", INC is a "foliation".
   Build trajectory + the rule-family table in
   docs/plans/multicomputation_trace.md.

   TMulticompTrace[expr]    evaluates `expr` (HoldFirst) with trace
                            recording on, returns
                              <| "Result" -> value of expr,
                                 "Trace"  -> {event, ...} |>.
                            Each `event` is an Association
                              <| "id", "rule", "ruleCode",
                                 "family", "familyCode",
                                 "termA", "termB", "deltaLabel",
                                 "consumed" |>.
                            `family` is one of TERM / SLIDE / FORK /
                            SPLIT / MERGE / PRUNE / PLUMB.
                            "consumed" is a list of event ids the
                            current event read from (M1 wire prov,
                            see docs/plans/multicomputation_trace.md
                            section 10.2); empty list for events whose
                            active-pair payload came from outside the
                            trace (pre-trace construction).

   TMulticompTraceQ[]       True iff the loaded THVMLink dylib was
                            built with -DTHVM_TRACE (the standard
                            `make wl` builds it on).

   TCausalGraph[trace]      builds a Graph[] of events + causal edges
                            from the consumed[] field: edge F -> E iff
                            E.consumed contains F.id (interaction-net
                            wire provenance).  See M1 in the build
                            trajectory.

   The standard recipe: build the term outside the trace (heap
   construction fires no interactions), then trace the reduction --
   e.g.  {dp0, dp1} = TDup[TOp2["+", TSup[1, 2], 3]];
         TMulticompTrace[TCollapse[dp0]]. *)

BeginPackage["THVMLink`"];

TMulticompTrace::usage = "TMulticompTrace[expr] evaluates `expr` (HoldFirst) with the multicomputation reduction trace recording, then returns <|\"Result\" -> value of expr, \"Trace\" -> {event, ...}|>, where each `event` is an Association <|\"id\", \"rule\", \"ruleCode\", \"family\", \"familyCode\", \"termA\", \"termB\", \"deltaLabel\", \"consumed\"|>.  An event is one interaction-net rule firing; `family` is one of TERM (within-branch compute), SLIDE (re-foliation: APP-SUP commute, INC, ...), FORK (1 -> 2), SPLIT (DUP-SUP cross product), MERGE (DUP-SUP annihilate), PRUNE (ERA), PLUMB (sharing housekeeping) -- see docs/multicomputation.md.  `consumed` is the list of producer event ids (M1 wire provenance: a list element `id` means \"this event's active pair includes a wire most recently written by event `id`\"); empty for events whose active pair was built outside the trace.  Recording is turned on for the duration of `expr` and off afterwards.  Requires a trace-enabled dylib: build with `make WL_TRACE=1 wl`; check TMulticompTraceQ[].";
TMulticompTraceQ::usage = "TMulticompTraceQ[] returns True iff the loaded THVMLink dylib was built with -DTHVM_TRACE (via `make WL_TRACE=1 wl`), so TMulticompTrace works.  The default `make wl` builds a trace-free dylib.";
TCausalGraph::usage = "TCausalGraph[trace] returns a directed Graph[] of the causal structure of `trace` (= the \"Trace\" field of TMulticompTrace).  Vertices are event ids; edges F -> E iff F's id appears in E's \"consumed\" list (wire provenance from M1).  This is the interaction-net causal graph -- one of the four Wolfram-style views in docs/multicomputation.md.  Options: \"VertexLabels\" -> Automatic to label each event with its rule name; \"Family\" -> {\"TERM\", ...} to filter by family.";

Begin["`Private`"];

TMulticompTraceQ[] := (ensureInit[]; $multiTraceSupportedFn[] === 1)

(* Lazy, memoised name lookups -- the C side (multi_rule_name /
   multi_family_name in src/instrument/multi.c) owns the RULE_* /
   MULTI_* -> string tables, so there's no parallel WL list to keep
   in sync. *)
multiRuleName[c_Integer]   := multiRuleName[c]   = $multiRuleNameFn[c]
multiFamilyName[c_Integer] := multiFamilyName[c] = $multiFamilyNameFn[c]

TMulticompTrace::notrace = "TMulticompTrace requires a trace-enabled THVMLink dylib.  Rebuild with `make WL_TRACE=1 wl`.  TMulticompTraceQ[] reports the status.";

(* Build the per-event Association from a snapshot row.  The C side
   emits an n x 8 matrix per multi_trace_snapshot:
     row[[1..6]] = {id, rule, family, termA, termB, deltaLabel}
     row[[7..8]] = {consumed[0], consumed[1]}, each `-1` for
                   MULTI_WIRE_NONE (no producer recorded).
   `consumed` becomes a list of valid producer ids (the -1 sentinels
   are filtered out). *)
eventFromRow[row_List] := <|
    "id"         -> row[[1]],
    "rule"       -> multiRuleName[row[[2]]],
    "ruleCode"   -> row[[2]],
    "family"     -> multiFamilyName[row[[3]]],
    "familyCode" -> row[[3]],
    "termA"      -> row[[4]],
    "termB"      -> row[[5]],
    "deltaLabel" -> row[[6]],
    "consumed"   -> DeleteCases[{row[[7]], row[[8]]}, -1]
|>

SetAttributes[TMulticompTrace, HoldFirst];
TMulticompTrace[expr_] := Module[{result, rows},
    ensureInit[];
    If[ ! TMulticompTraceQ[], Message[TMulticompTrace::notrace]; Return[$Failed]];
    $multiTraceInitFn[0];          (* allocate the events buffer; flag stays off *)
    $multiTraceSetFn[1];           (* recording on *)
    result = expr;                 (* HoldFirst -- evaluated here, traced *)
    $multiTraceSetFn[0];           (* recording off *)
    rows = $multiTraceSnapshotFn[];   (* {Integer, 2}, n x 8 *)
    $multiTraceFreeFn[];           (* release the buffer *)
    <| "Result" -> result,
       "Trace"  -> Map[eventFromRow, rows] |>
]

(* TCausalGraph[trace] -- the M1 causal graph view.  Vertices = event
   ids.  Edges: F -> E iff F.id appears in E.consumed.  Wire-provenance
   is recorded by the C side (wire_prov[loc]) and surfaced here via the
   "consumed" field on each event.  Soundness: interaction nets are
   causally invariant in Wolfram's sense (docs/multicomputation.md
   section 3.1 layer (4)), so the graph is a property of the program,
   not the reducer's schedule.  Option "VertexLabels" -> Automatic
   labels each event with its rule name; "Family" -> {"TERM", ...}
   restricts to those families. *)
Options[TCausalGraph] = {
    VertexLabels -> None,
    "Family"     -> All
};
TCausalGraph[trace_List, OptionsPattern[]] := Module[
    {events, ids, families, keep, edges, vlabels},
    families = OptionValue["Family"];
    keep = If[families === All,
        trace,
        Select[trace, MemberQ[families, #["family"]] &]];
    events = keep;
    ids    = events[[All, "id"]];
    edges  = Flatten @ Map[
        e |-> Map[p |-> DirectedEdge[p, e["id"]],
                  Select[e["consumed"], MemberQ[ids, #] &]],
        events];
    vlabels = OptionValue[VertexLabels];
    Graph[
        ids,
        edges,
        VertexLabels -> Switch[vlabels,
            Automatic, Thread[ids -> events[[All, "rule"]]],
            _,         vlabels],
        GraphLayout -> "LayeredDigraphEmbedding"
    ]
]

End[];
EndPackage[];
