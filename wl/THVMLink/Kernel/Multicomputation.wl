(* ::Package:: *)
(* Multicomputation.wl -- WL surface for the MultiEvent reduction
   trace (src/instrument/multi.c).  Conceptual reading in
   docs/multicomputation.md: a SUP-term is a "slice", reduction is
   "slice evolution", collapse is an "observer", INC is a "foliation".
   Build trajectory + the rule-family table in
   docs/plans/multicomputation_trace.md.

   TMultiTrace[expr]    evaluates `expr` (HoldFirst) with trace
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

   TMultiTraceQ[]       True iff the loaded THVMLink dylib was
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
         TMultiTrace[TCollapse[dp0]]. *)

BeginPackage["THVMLink`"];

TMultiTrace::usage = "TMultiTrace[expr] evaluates `expr` (HoldFirst) with the multicomputation reduction trace recording, then returns <|\"Result\" -> value of expr, \"Trace\" -> {event, ...}|>, where each `event` is an Association <|\"id\", \"rule\", \"ruleCode\", \"family\", \"familyCode\", \"termA\", \"termB\", \"deltaLabel\", \"consumed\", \"produced\"|>.  An event is one interaction-net rule firing; `family` is one of TERM (within-branch compute), SLIDE (re-foliation: APP-SUP commute, INC, ...), FORK (1 -> 2), SPLIT (DUP-SUP cross product), MERGE (DUP-SUP annihilate), PRUNE (ERA), PLUMB (sharing housekeeping) -- see docs/multicomputation.md.  `consumed` is the list of producer event ids (M1 wire provenance: a list element `id` means \"this event's active pair includes a wire most recently written by event `id`\"); empty for events whose active pair was built outside the trace.  `produced` is the dual: the list of heap locs the event wrote (last-writer-wins snapshot of wire_prov at trace end -- so cells overwritten by a later event aren't attributed here).  Recording is turned on for the duration of `expr` and off afterwards.  The default \"make wl\" already builds a trace-enabled dylib; check TMultiTraceQ[].";
TMultiTraceQ::usage = "TMultiTraceQ[] returns True iff the loaded THVMLink dylib was built with -DTHVM_TRACE -- the default for \"make wl\".  Pass WL_TRACE=0 to make to opt out of the trace machinery (e.g. for benching), in which case TMultiTrace returns $Failed.";
TMultiSteps::usage = "TMultiSteps[term] (or TMultiSteps[term, maxSteps]) fires one IC interaction at a time -- via TStep -- with the multicomputation trace recording on, snapshotting the heap diagram after every step.  Returns a list of Associations <|\"Step\" -> i, \"Term\" -> partiallyReducedTerm, \"Diagram\" -> THeapDiagram[partial, \"Arrange\"], \"Events\" -> {event, ...}, \"ITRS\" -> n|>; the diagram is built BEFORE the next TStep mutates the heap, so the per-step pictures are usable after the reduction completes.  Step 0 is the input term with no events.  Stops when no more interactions fire (normal form) or after `maxSteps` (default: unbounded).  Pass option \"DiagramSeeds\" -> {auxTerms...} to seed the per-step diagrams with additional roots (e.g. the sibling projection from TDup so the diagram surfaces both halves of a DUP-SUP commute).  Requires the trace dylib (TMultiTraceQ[] === True).";
TCausalGraph::usage = "TCausalGraph[trace] returns a directed Graph[] of the causal structure of `trace` (= the \"Trace\" field of TMultiTrace).  Vertices are event ids; edges F -> E iff F's id appears in E's \"consumed\" list (wire provenance from M1).  This is the interaction-net causal graph -- one of the four Wolfram-style views in docs/multicomputation.md.  Options: \"VertexLabels\" -> Automatic to label each event with its rule name; \"Family\" -> {\"TERM\", ...} to filter by family.";

Begin["`Private`"];

TMultiTraceQ[] := (ensureInit[]; $multiTraceSupportedFn[] === 1)

(* Lazy, memoised name lookups -- the C side (multi_rule_name /
   multi_family_name in src/instrument/multi.c) owns the RULE_* /
   MULTI_* -> string tables, so there's no parallel WL list to keep
   in sync. *)
multiRuleName[c_Integer]   := multiRuleName[c]   = $multiRuleNameFn[c]
multiFamilyName[c_Integer] := multiFamilyName[c] = $multiFamilyNameFn[c]

(* Plain double-quotes around the build command -- backticks would be
   interpreted as StringForm placeholders and trigger a spurious sfr
   warning whenever the message fires. *)
TMultiTrace::notrace = "TMultiTrace requires a trace-enabled THVMLink dylib.  The default \"make wl\" builds one; if you rebuilt with WL_TRACE=0 (opt-out for benchers), just \"make wl\" again.  TMultiTraceQ[] reports the status.";

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

(* `produced` is the implicit dual of `consumed`: the set of heap
   locs whose wire_prov[loc] == event.id at the end of the trace.
   We compute it host-side from a single wire_prov snapshot.
   Output is an Association id -> {locs}. *)
producedFromWireProv[wireProv_List] :=
    GroupBy[
        DeleteCases[
            MapIndexed[{#2[[1]] - 1, #1} &, wireProv],
            {_, -1}],
        Last -> First]

SetAttributes[TMultiTrace, HoldFirst];
TMultiTrace[expr_] := Module[{result, rows, wireProv, produced, events},
    ensureInit[];
    If[ ! TMultiTraceQ[], Message[TMultiTrace::notrace]; Return[$Failed]];
    $multiTraceInitFn[0];          (* allocate the events buffer; flag stays off *)
    $multiTraceSetFn[1];           (* recording on *)
    result = expr;                 (* HoldFirst -- evaluated here, traced *)
    $multiTraceSetFn[0];           (* recording off *)
    rows     = $multiTraceSnapshotFn[];     (* {Integer, 2}, n x 8 *)
    wireProv = $multiWireProvSnapshotFn[];  (* {Integer, 1}, HEAP_NEXT *)
    $multiTraceFreeFn[];           (* release the buffer *)
    events   = Map[eventFromRow, rows];
    produced = producedFromWireProv[wireProv];
    <| "Result"   -> result,
       "Trace"    -> Map[
           e |-> Append[e, "produced" -> Lookup[produced, e["id"], {}]],
           events] |>
]

(* TMultiSteps -- stepwise replay of a reduction.  Each TStep
   fires exactly one IC interaction; we snapshot the heap (= render
   the diagram) before the NEXT TStep mutates it, so the per-step
   pictures stay valid after the whole reduction completes.  Trace
   events fired during a step are sliced out of the global event log
   and attached to the step record.  ITRS captures the cumulative
   interaction count to make the boundaries machine-readable.

   Iteration is via NestWhileList, not a While/AppendTo loop -- see
   wl/GUIDE.md (Mutation, Control flow).  The step record carries a
   private "_evBefore" key to thread the event-buffer cursor through
   the iteration; we strip it before returning. *)
SetAttributes[TMultiSteps, HoldFirst];
Options[TMultiSteps] = {"DiagramSeeds" -> {}};

TMultiSteps[term0_, opts : OptionsPattern[]] :=
    multicompStepsImpl[term0, Infinity, OptionValue["DiagramSeeds"]]
TMultiSteps[term0_, max_Integer ? Positive, opts : OptionsPattern[]] :=
    multicompStepsImpl[term0, max, OptionValue["DiagramSeeds"]]
TMultiSteps[term0_, 0, opts : OptionsPattern[]] :=
    multicompStepsImpl[term0, Infinity, OptionValue["DiagramSeeds"]]

(* `term0` is held by the HoldFirst on TMultiSteps.  Inside the
   impl we evaluate it AFTER turning the trace flag on, so any
   construction events get logged alongside the reduction events.
   `auxSeeds` is a list of TTerms whose reachable agents will be
   added to every per-step diagram -- needed for DUP-driven
   reductions where the sibling projection (dpOther) holds half of
   the commuted result and isn't reachable from the active side. *)
multicompStepsImpl[term0_, maxSteps_, auxSeeds_List] := Block[
    {seedTerm, snapshot, advance, recs},
    ensureInit[];
    If[ ! TMultiTraceQ[],
        Message[TMultiTrace::notrace];
        Return[$Failed]
    ];
    $multiTraceInitFn[0];
    $multiTraceSetFn[1];
    seedTerm = term0;
    snapshot[t_] := THeapDiagram[Prepend[auxSeeds, t]];
    advance[s_Association] := Block[
        {next, before = s["ITRS"], after, evAfter, rows, events},
        next  = TStep[s["Term"]];
        after = TItrs[];
        If[ after === before,
            $Done,
            evAfter = $multiTraceCountFn[];
            rows    = $multiTraceSnapshotFn[];
            events  = If[ s["_evBefore"] < evAfter,
                Map[eventFromRow,
                    rows[[s["_evBefore"] + 1 ;; evAfter]]],
                {}
            ];
            <|
                "Step"      -> s["Step"] + 1,
                "Term"      -> next,
                "Diagram"   -> snapshot[next],
                "Events"    -> events,
                "ITRS"      -> after,
                "_evBefore" -> evAfter
            |>
        ]
    ];
    advance[$Done] := $Done;
    recs = NestWhileList[
        advance,
        <|
            "Step"      -> 0,
            "Term"      -> seedTerm,
            "Diagram"   -> snapshot[seedTerm],
            "Events"    -> {},
            "ITRS"      -> TItrs[],
            "_evBefore" -> $multiTraceCountFn[]
        |>,
        # =!= $Done &,
        1,
        maxSteps
    ];
    $multiTraceSetFn[0];
    $multiTraceFreeFn[];
    KeyDrop[#, "_evBefore"] & /@ DeleteCases[recs, $Done]
]

(* TCausalGraph[trace] -- the M1 causal graph view.  Vertices = event
   ids.  Edges: F -> E iff F.id appears in E.consumed.  Wire-provenance
   is recorded by the C side (wire_prov[loc]) and surfaced here via the
   "consumed" field on each event.  Soundness: interaction nets are
   causally invariant in Wolfram's sense (docs/multicomputation.md
   section 3.1 layer (4)), so the graph is a property of the program,
   not the reducer's schedule.  Option "VertexLabels" -> Automatic
   labels each event with its rule name; "Family" -> {"TERM", ...}
   restricts to those families.

   Styling mirrors the Wolfram Physics Project causal-graph palette
   (yellow-orange vertices, red edges) via ResourceFunction[
   "WolframPhysicsProjectStyleData"]["CausalGraph"], so thvm causal
   graphs sit visually next to a `ResourceFunction["MultiwaySystem"]`
   side-by-side comparison.  The label text is LightDarkSwitched so
   rule names stay readable on both notebook themes. *)
$wppCausalStyle := $wppCausalStyle =
    ResourceFunction["WolframPhysicsProjectStyleData"]["CausalGraph"]

Options[TCausalGraph] = {
    VertexLabels -> None,
    "Family"     -> All
};
TCausalGraph[trace_List, OptionsPattern[]] := Module[
    {events, ids, families, keep, edges, vlabels, wpp, labelStyle},
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
    vlabels    = OptionValue[VertexLabels];
    wpp        = $wppCausalStyle;
    (* WPP defaults pin label colour from the theme; explicitly set
       a high-contrast colour per mode so rule names stay readable
       on a dark notebook background (where the WPP vertices are
       saturated yellow-orange and need a dark label). *)
    labelStyle = Directive[
        FontFamily -> "Helvetica",
        FontSize   -> 9,
        LightDarkSwitched[GrayLevel[0.15], GrayLevel[0.95]]];
    Graph[
        ids,
        edges,
        VertexLabels -> Switch[vlabels,
            Automatic, Thread[ids -> events[[All, "rule"]]],
            _,         vlabels],
        VertexLabelStyle -> labelStyle,
        VertexStyle      -> wpp["VertexStyle"],
        EdgeStyle        -> wpp["EdgeStyle"],
        Background       -> LightDarkSwitched[White, GrayLevel[0.13]],
        GraphLayout      -> "LayeredDigraphEmbedding"
    ]
]

End[];
EndPackage[];
