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
                            SPLIT / MERGE / PRUNE / DIST.
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

BeginPackage["WolframInstitute`THVMLink`"];

GeneralUtilities`SetUsage[TMultiTrace, "TMultiTrace[expr$] evaluates expr$ (HoldFirst) with the multicomputation reduction trace recording on, runs the collapse walk one TStep at a time, and returns a list of per-step Associations.
TMultiTrace[expr$, keys$] selects which step fields to keep: a list of keys or All returns per-step Associations; a single key string returns a flat list of that field's values (one per step).
TMultiTrace[expr$, max$] and TMultiTrace[expr$, keys$, max$] cap the walk at max$ steps.
Available keys: \"Step\", \"Term\", \"Events\", \"ITRS\", \"Slice\", \"CanonicalSlice\", \"SliceBoxes\", \"Diagram\"; \"SliceBoxes\" and \"Diagram\" are expensive (TraditionalForm rendering, diagram materialisation), so request only what the view needs.
Step 0 captures events fired during expr$'s own evaluation, so TMultiTrace[TCollapse[t$]] attributes the whole collapse to step 0.
Each event is an Association with keys \"id\", \"rule\", \"ruleCode\", \"family\", \"familyCode\", \"termA\", \"termB\", \"deltaLabel\", \"consumed\", \"produced\"; family is one of TERM, SLIDE, FORK, SPLIT, MERGE, PRUNE, DIST. consumed lists producer event ids (wire provenance); produced lists the heap locs the event wrote.
Needs a trace-enabled dylib (the default make wl); check TMultiTraceQ[].
Option \"DiagramSeeds\" seeds the per-step diagrams with additional root terms."];
GeneralUtilities`SetUsage[TMultiTraceQ, "TMultiTraceQ[] returns True iff the loaded THVMLink dylib was built with -DTHVM_TRACE, the default for make wl.
Rebuilding with WL_TRACE=0 opts out of the trace machinery (e.g. for benching), in which case TMultiTrace returns $Failed."];
GeneralUtilities`SetUsage[TCausalGraph, "TCausalGraph[input$] returns a directed Graph of the causal structure of a reduction.
TCausalGraph[t$] and TCausalGraph[t$, max$] run TMultiTrace on a TTerm t$ internally (capping at max$ steps).
input$ is either a flat trace (a list of event Associations) or a steps list (per-step Associations whose events are flattened across all records).
Vertices are event ids; an edge F$ to E$ appears iff F$'s id is in E$'s consumed list (wire provenance). Each vertex is coloured by its event's family (TERM, SLIDE, FORK, SPLIT, MERGE, PRUNE, DIST).
Options: VertexLabels (Automatic labels each event with its rule name), Family (filter to a list of family names), TransitiveReduction, PlotLegends (Automatic emits a SwatchLegend of family colours, default None)."];
GeneralUtilities`SetUsage[TMultiwayGraph, "TMultiwayGraph[steps$] returns the multiway view of a reduction.
TMultiwayGraph[t$] and TMultiwayGraph[t$, max$] run TMultiTrace on a TTerm t$ internally (capping at max$ steps), capturing the slice keys the view needs.
steps$ is a list of step Associations carrying \"CanonicalSlice\", \"SliceBoxes\", and \"Events\"; unlike TCausalGraph this needs slice info, so a flat trace is rejected.
Vertices are slice leaves: per step the active term's SUP-head is unfolded so a SUP contributes one vertex per leaf (recursively, until non-SUP heads), and a non-SUP head contributes a single vertex. Edges connect leaves of consecutive slices, coloured by the firing event's family (TERM, SLIDE, FORK, SPLIT, MERGE, PRUNE, DIST).
Options: Branchial (overlay a dashed branchial clique between new sibling leaves, default False), VertexLabels, PlotLegends (Automatic emits a SwatchLegend of family colours)."];
TMultiwayGraph::needsteps = "TMultiwayGraph requires step records (from TMultiSteps), not a flat trace.  Use TCausalGraph[trace] for an event-id-vertices view, or call TMultiSteps to capture the slice canonicals needed for the multiway view.";

Begin["`Private`"];

TMultiTraceQ[] := (ensureInit[]; $multiTraceSupportedFn[] === 1)

(* Lazy, memoised name lookups -- the C side (multi_rule_name /
   multi_family_name in src/instrument/multi.c) owns the RULE_* /
   MULTI_* -> string tables, so there's no parallel WL list to keep
   in sync. *)
multiRuleName[c_Integer] := multiRuleName[c] = $multiRuleNameFn[c]
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
    "consumed"   -> DeleteDuplicates @ DeleteCases[{row[[7]], row[[8]]}, -1]
|>

(* `produced` is the implicit dual of `consumed`: the set of heap
   locs whose wire_prov[loc] == event.id at the end of the trace.
   We compute it host-side from a single wire_prov snapshot.
   Output is an Association id -> {locs}. *)
producedFromWireProv[wireProv_List] :=
    GroupBy[
        DeleteCases[
            MapIndexed[{v, idx} |-> {First[idx] - 1, v}, wireProv],
            {_, -1}],
        Last -> First]

(* All step-record keys the implementation can populate. *)
$multiAllKeys = {"Step", "Term", "Events", "ITRS", "Slice", "CanonicalSlice", "SliceBoxes", "Diagram"}

(* Keys that require the slice walk + its dependent computations. *)
$multiSliceKeys = {"Slice", "CanonicalSlice", "SliceBoxes"}
$multiDiagramKeys = {"Diagram"}
keysNeedSlice[keys_] := IntersectingQ[keys, $multiSliceKeys]
keysNeedDiagram[keys_] := IntersectingQ[keys, $multiDiagramKeys]

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
SetAttributes[TMultiTrace, HoldFirst];
Options[TMultiTrace] = {"DiagramSeeds" -> {}}

(* Default keys: the minimal set that both TCausalGraph and
   TMultiwayGraph consume.  "Events" covers TCausalGraph and the
   edge labels/colours in TMultiwayGraph; "CanonicalSlice" covers
   TMultiwayGraph's vertex identity.  Diagrams / SliceBoxes /
   Term / etc. are opt-in via explicit keys or `All`. *)
$multiDefaultKeys = {"Events", "CanonicalSlice"}

resolveKeys[All] := $multiAllKeys
resolveKeys[ks_List] := ks
resolveKeys[k_String] := {k}

(* When `keys` is a single string, return a flat list of that key's
   values (one per step) instead of the list-of-associations form;
   list / All keys keep the list-of-associations shape. *)
TMultiTrace[expr_, opts : OptionsPattern[]] :=
    multiTraceImpl[expr, $multiDefaultKeys, Infinity, OptionValue["DiagramSeeds"], None]
TMultiTrace[expr_, max_Integer ? Positive, opts : OptionsPattern[]] :=
    multiTraceImpl[expr, $multiDefaultKeys, max, OptionValue["DiagramSeeds"], None]
TMultiTrace[expr_, key_String, opts : OptionsPattern[]] :=
    multiTraceImpl[expr, {key}, Infinity, OptionValue["DiagramSeeds"], key]
TMultiTrace[expr_, keys : (_List | All), opts : OptionsPattern[]] :=
    multiTraceImpl[expr, resolveKeys[keys], Infinity, OptionValue["DiagramSeeds"], None]
TMultiTrace[expr_, key_String, max_Integer ? Positive, opts : OptionsPattern[]] :=
    multiTraceImpl[expr, {key}, max, OptionValue["DiagramSeeds"], key]
TMultiTrace[expr_, keys : (_List | All), max_Integer ? Positive, opts : OptionsPattern[]] :=
    multiTraceImpl[expr, resolveKeys[keys], max, OptionValue["DiagramSeeds"], None]

(* `term0` is held by the HoldFirst on TMultiSteps.  Inside the
   impl we evaluate it AFTER turning the trace flag on, so any
   construction events get logged alongside the reduction events.
   `auxSeeds` is a list of TTerms whose reachable agents will be
   added to every per-step diagram -- needed for DUP-driven
   reductions where the sibling projection (dpOther) holds half of
   the commuted result and isn't reachable from the active side.

   Semantics: collapse walk (src/collapse/_.c).  Each iteration calls
   TStep on the active head; if an IC interaction fires, emit a step
   record and continue on the result.  If TStep didn't progress, the
   head is at WHNF -- inspect its tag: SUP pushes the right branch
   onto a LIFO frontier and continues on the left, AND records the
   SUP as an anchor so the per-step diagrams keep showing the full
   multiway tree even after the walker has descended into a single
   branch.  ERA / any other leaf pops the frontier.  The walk
   terminates when both TStep is a no-op and the frontier is empty. *)
SetAttributes[multiTraceImpl, HoldFirst];
multiTraceImpl[term0_, keys_List, maxSteps_, auxSeeds_List,
               singleKey_] := Block[
    {seedTerm, snapshot, mkRecord, advance, recs, preEvents,
     wantSlice = keysNeedSlice[keys], wantDiagram = keysNeedDiagram[keys],
     produced, wireProv, records},
    ensureInit[];
    If[ ! TMultiTraceQ[],
        Message[TMultiTrace::notrace];
        Return[$Failed]
    ];
    $multiTraceInitFn[0];
    $multiTraceSetFn[1];
    seedTerm = term0;
    (* Capture events that fired during seedTerm's evaluation (e.g.
       TCollapse[expr] traces the entire collapse in one shot).
       These get attributed to step 0; the per-step iteration below
       then handles subsequent firings as it descends through the
       reduction tree. *)
    preEvents = Map[eventFromRow, $multiTraceSnapshotFn[]];
    (* Diagram seeds: anchors (SUPs the walker descended through),
       user auxSeeds, and the active term.  `seedTerm` is included
       ONLY when it is a DP projection -- in that case
       expandDupSiblings (inside THeapDiagram) auto-adds the sibling
       DP, keeping both halves of a DUP-driven reduction visible
       after the active side has resolved.  For non-DP seeds the
       initial term often becomes "dead" mid-walk (e.g. the original
       APP after APP-LAM fires) -- including it would drag the stale
       APP / SUP arg into every later step's diagram. *)
    snapshot[t_, anchors_List] := THeapDiagram @ Join[
        anchors, auxSeeds, {t},
        If[ TTermTag[seedTerm] === $TagDP0 || TTermTag[seedTerm] === $TagDP1,
            {seedTerm},
            {}]];
    (* Slice for the multiway view: live state of every alive branch
       at the time the record is built (heap reads are point-in-time;
       must run BEFORE the next advance mutates the heap).  Branches
       come from each anchor SUP's two slot cells, with the SUP head
       unfolded so a SUP-inside-a-SUP contributes multiple leaves.
       Before any anchor is recorded, the slice is just the active
       term's head-unfold. *)
    sliceNow[t_, anchors_List] := If[
        anchors === {},
        unfoldSupHead[t],
        Catenate @ Map[
            a |-> With[{loc = TTermVal[a]},
                Join[unfoldSupHead[THeapRead[loc + 0]],
                     unfoldSupHead[THeapRead[loc + 1]]]],
            anchors]];
    mkRecord[stepNum_, term_, events_, itrs_, evCursor_, frontier_, anchors_, parentSlot_] := Block[{sl},
        sl = If[ wantSlice, sliceNow[term, anchors], None];
        <|
            "Step" -> stepNum,
            "Term" -> term,
            "Diagram" -> If[ wantDiagram, snapshot[term, anchors], None],
            "Slice" -> sl,
            "CanonicalSlice" -> If[ wantSlice && MemberQ[keys, "CanonicalSlice"],
                Map[Term, sl], None],
            (* Pre-compute the TraditionalForm rendering NOW, while
               the heap is in this step's state.  We can't defer to
               display time -- subsequent steps mutate the heap, so
               a late TraditionalForm[t] would render the FINAL
               state for every step.  Store as box expressions to
               drop straight into vertex labels via RawBoxes. *)
            "SliceBoxes" -> If[ wantSlice && MemberQ[keys, "SliceBoxes"],
                Map[t |-> ToBoxes[t, TraditionalForm], sl],
                None],
            "Events" -> events,
            "ITRS" -> itrs,
            "_evBefore" -> evCursor,
            "_frontier" -> frontier,
            "_anchors" -> anchors,
            "_parent" -> parentSlot
        |>];
    (* Each advance call walks until an IC interaction fires OR
       the frontier drains.  SUP descent / ERA-pop / leaf-pop are
       silent plumbing of the collapse walker; they don't get their
       own step records.  Only firings produce records.

       We track `parentSlot` -- the heap loc the active term was
       read from when the walker descended through a SUP.  After
       TStep fires and produces a new head, we write the new head
       back to that loc so the anchor SUP's slot reflects the
       reduced branch.  Without this, the SUP's slot keeps holding
       the original (now-stale) compound term and the per-step
       diagrams show ghost OP2s / DPs that have already been
       reduced.  Frontier entries are {term, parentSlot} pairs so
       the deferred branch knows its own write-back target. *)
    advance[s_Association] := Block[
        {term = s["Term"],
         frontier = s["_frontier"],
         anchors = s["_anchors"],
         parent = s["_parent"],
         evBefore = s["_evBefore"],
         before, after, next, evAfter, rows, events, tag, loc, l, r,
         done = False, result = $Done},
        (* Step the collapse walk only while the active term is a
           TTerm.  When the user passed e.g. TCollapse[expr] (which
           evaluates to a List of TTerms before TMultiTrace's
           HoldFirst lets the expr settle), there's nothing left to
           step -- the step-0 record already holds the pre-evaluation
           events.  Return $Done so NestWhileList stops. *)
        If[ ! MatchQ[term, _TTerm], Return[$Done, Block]];
        While[ ! done,
            before = TItrs[];
            next = TStep[term];
            after = TItrs[];
            If[ after > before,
                (* Rule fired -- write the reduced head back to the
                   parent SUP-slot (if we descended through one) so
                   the anchor's slot reflects the new head, then
                   emit the step record. *)
                If[ ! MissingQ[parent], THeapSet[parent, next]];
                evAfter = $multiTraceCountFn[];
                rows = $multiTraceSnapshotFn[];
                events = If[ evBefore < evAfter,
                    Map[eventFromRow, rows[[evBefore + 1 ;; evAfter]]],
                    {}
                ];
                result = mkRecord[s["Step"] + 1, next, events, after, evAfter, frontier, anchors, parent];
                done = True,
                (* No firing -- walk silently. *)
                tag = TTermTag[next];
                Which[
                    tag === $TagSUP,
                        (* Descend into left, stash right on frontier
                           tagged with its own slot, anchor the SUP. *)
                        loc = TTermVal[next];
                        l = THeapRead[loc + 0];
                        r = THeapRead[loc + 1];
                        anchors = Append[anchors, next];
                        frontier = Prepend[frontier, {r, loc + 1}];
                        term = l;
                        parent = loc + 0,
                    True,
                        (* Leaf (NUM/LAM/CTR/...) or ERA: pop the
                           frontier.  If drained, the collapse is
                           complete -- $Done. *)
                        If[ frontier === {},
                            result = $Done; done = True,
                            {term, parent} = First[frontier];
                            frontier = Rest[frontier]]
                ]
            ]
        ];
        result
    ];
    advance[$Done] := $Done;
    recs = NestWhileList[
        advance,
        mkRecord[0, seedTerm, preEvents, TItrs[], $multiTraceCountFn[], {}, {}, Missing[]],
        r |-> r =!= $Done,
        1,
        maxSteps
    ];
    $multiTraceSetFn[0];
    wireProv = $multiWireProvSnapshotFn[];
    $multiTraceFreeFn[];
    (* Decorate each event with `produced` (the heap locs the event
       wrote, derived from a single wireProv snapshot at trace end).
       Done as a post-processing pass so the cost is paid once. *)
    produced = producedFromWireProv[wireProv];
    records = Map[
        r |-> KeySelect[
            Append[r, "Events" -> Map[
                e |-> Append[e, "produced" -> Lookup[produced, e["id"], {}]],
                r["Events"]]],
            MemberQ[keys, #] &],
        DeleteCases[recs, $Done]];
    If[ StringQ[singleKey],
        records[[All, singleKey]],
        records]
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
$wppMultiwayStyle := $wppMultiwayStyle =
    ResourceFunction["WolframPhysicsProjectStyleData"]["MultiwayGraph"]
$wppBranchialStyle := $wppBranchialStyle =
    ResourceFunction["WolframPhysicsProjectStyleData"]["BranchialGraph"]

(* Family -> colour for events and rule-family edges.  Chosen to read
   well on both light and dark notebook themes (StandardBlue/etc are
   theme-aware via LightDarkSwitched internally). *)
$familyColors = <|
    "TERM" -> StandardBlue,
    "SLIDE" -> StandardOrange,
    "FORK" -> StandardGreen,
    "SPLIT" -> StandardCyan,
    "MERGE" -> StandardRed,
    "PRUNE" -> LightDarkSwitched[GrayLevel[0.5], GrayLevel[0.6]],
    "DIST" -> LightDarkSwitched[GrayLevel[0.75], GrayLevel[0.35]]
|>

(* The legend SwatchLegend; only include families actually present in
   the trace so the legend stays compact. *)
familyLegend[families_List] := SwatchLegend[
    Map[f |-> Lookup[$familyColors, f, Gray], families],
    families,
    LegendLabel -> Style["family",
        FontFamily -> "Helvetica", FontSize -> 10,
        LightDarkSwitched[GrayLevel[0.15], GrayLevel[0.9]]],
    LabelStyle -> Directive[
        FontFamily -> "Helvetica", FontSize -> 9,
        LightDarkSwitched[GrayLevel[0.15], GrayLevel[0.9]]]]

(* Both TCausalGraph and TMultiwayGraph accept either form of the
   reduction record:
     - a flat trace (a list of event Associations, as returned by
       TMultiTrace[...]["Trace"]), or
     - a steps list (a list of per-step Associations from
       TMultiSteps[...]), each with at least an "Events" key.
   Distinguished by KeyValuePattern on the first element.  The
   shared lifter `traceFromInput` extracts events; the result is the
   minimal data both functions consume. *)
$multiTracePat = {} | {KeyValuePattern["id" -> _] ..}
$multiStepsPat = {KeyValuePattern["Events" -> _] ..}
multiTraceQ[x_] := MatchQ[x, $multiTracePat]
multiStepsQ[x_] := MatchQ[x, $multiStepsPat]
traceFromInput[x : $multiStepsPat] := Catenate @ Map[#["Events"] &, x]
traceFromInput[x : $multiTracePat] := x

Options[TCausalGraph] = Join[
    {"Family" -> All,
     "TransitiveReduction" -> True,
     PlotLegends -> None},
    Options[Graph]]
(* Term-input forms: run TMultiTrace internally with just the
   keys this view needs (Events) and feed the result into the
   list-of-steps dispatch.  `max` defaults to Infinity. *)
TCausalGraph[t_TTerm, opts : OptionsPattern[]] :=
    TCausalGraph[TMultiTrace[t, {"Events"}], opts]
TCausalGraph[t_TTerm, max_Integer ? Positive, opts : OptionsPattern[]] :=
    TCausalGraph[TMultiTrace[t, {"Events"}, max], opts]

TCausalGraph[input_List ? multiStepsQ, opts : OptionsPattern[]] :=
    TCausalGraph[traceFromInput[input], opts]
TCausalGraph[trace_List, opts : OptionsPattern[]] := Block[
    {events, ids, families, keep, edges, wpp, labelStyle,
     vstyles, legendOpt, presentFamilies, legend, userGraphOpts, tr},
    families = OptionValue["Family"];
    keep = If[ families === All,
        trace,
        Select[trace, e |-> MemberQ[families, e["family"]]]];
    events = keep;
    ids = events[[All, "id"]];
    edges = Flatten @ Map[
        e |-> Map[
            p |-> DirectedEdge[p, e["id"]],
            Select[e["consumed"], p |-> MemberQ[ids, p]]],
        events];
    (* Default: drop transitively redundant edges so the graph shows
       only direct causal dependencies (the Hasse-style view).  Pass
       "TransitiveReduction" -> False to keep the full edge set. *)
    tr = OptionValue["TransitiveReduction"];
    edges = If[ TrueQ[tr],
        EdgeList @ TransitiveReductionGraph @ Graph[ids, edges],
        edges];
    wpp = $wppCausalStyle;
    labelStyle = Directive[
        FontFamily -> "Helvetica", FontSize -> 9,
        LightDarkSwitched[GrayLevel[0.15], GrayLevel[0.95]]];
    (* Per-vertex style: family colour, mixed with the WPP vertex
       directive so size/edge-form/etc. stay consistent.  The colour
       wins over WPP's default yellow-orange fill. *)
    vstyles = Thread[ids ->
        Map[
            e |-> Directive[
                EdgeForm[LightDarkSwitched[GrayLevel[0.25], GrayLevel[0.85]]],
                FaceForm[Lookup[$familyColors, e["family"], Gray]]],
            events]];
    legendOpt = OptionValue[PlotLegends];
    presentFamilies = DeleteDuplicates[events[[All, "family"]]];
    legend = Switch[legendOpt,
        None | False, None,
        Automatic | True, familyLegend[presentFamilies],
        _, legendOpt];
    (* User-supplied Graph options.  We resolve VertexLabels here
       (Automatic -> rule-name labels) and then strip it from the
       pass-through so the user-Automatic still gets our semantics.
       Other Graph options (ImageSize, GraphLayout, AspectRatio, ...)
       come first in Graph[]'s arg list so they win on overlap. *)
    userGraphOpts = FilterRules[{opts}, Options[Graph]];
    With[{g = Graph[
        ids,
        edges,
        userGraphOpts,
        VertexLabels -> Thread[ids -> events[[All, "rule"]]],
        VertexLabelStyle -> labelStyle,
        VertexStyle -> vstyles,
        EdgeStyle -> wpp["EdgeStyle"],
        Background -> LightDarkSwitched[White, GrayLevel[0.13]],
        GraphLayout -> "LayeredDigraphEmbedding"]},
        If[ legend === None, g, Legended[g, legend]]]
]

(* ====================================================================
   TMultiwayGraph -- multiway view from TMultiSteps records.
   ====================================================================
   Each step's "Term" is a term whose head MAY be SUP.  We unfold the
   head SUP-tree to a list of "leaf" terms (terms whose head is NOT
   SUP).  Each leaf is one vertex of the step's slice.  Between two
   consecutive slices, the firing trace event contributes ONE EDGE
   per (source-leaf, target-leaf) pair (cross product), coloured by
   that event's family.

   Branchial edges (off by default) overlay an undirected clique
   inside each SUP-bearing slice -- the WPP "branchial graph" view.
   v1: takes TMultiSteps records.  A future variant on raw traces
   will need per-event term snapshots from the C side. *)

(* Unfold the SUP-tree at the head of `term`: keep recursing into
   SUP slots until the head is non-SUP.  Returns a flat list of
   leaf terms (each with non-SUP head). *)
unfoldSupHead[term_] := If[ TTermTag[term] === $TagSUP,
    With[{loc = TTermVal[term]},
        Join[unfoldSupHead[THeapRead[loc + 0]],
             unfoldSupHead[THeapRead[loc + 1]]]],
    {term}]

(* Canonical form of a term = `Term[t_TTerm]`, defined in Heap.wl.
   Walks the heap recursively, chasing SUB-flagged DP / VAR
   projections, and returns a nested Term[head, args...] expression.
   Used as the vertex identity in TMultiwayGraph -- a branch whose
   head Term value is unchanged across steps but whose SUB-resolved
   content changed shows up as a new vertex. *)

(* Short caption for a term (used as the vertex label).  Mirrors
   what diagrams print: e.g. "NUM 4@12", "DP0@4", "SUP@14..15". *)
termCaption[t_] := Block[{tag, val, nm},
    tag = TTermTag[t];
    val = TTermVal[t];
    nm = TTagName[tag];
    Switch[tag,
        $TagNUM, nm <> " " <> ToString[val],
        $TagSUP, nm <> "@" <> ToString[val] <> ".." <> ToString[val + 1],
        _, nm <> "@" <> ToString[val]]]

Options[TMultiwayGraph] = Join[
    {"Branchial" -> False, PlotLegends -> None},
    Options[Graph]
]

(* Term-input forms: run TMultiTrace internally with the keys
   needed for vertices (CanonicalSlice), edge labels (Events), and
   nice rendering (SliceBoxes), then dispatch to the steps form. *)
TMultiwayGraph[t_TTerm, opts : OptionsPattern[]] :=
    TMultiwayGraph[
        TMultiTrace[t, {"Events", "CanonicalSlice", "SliceBoxes"}],
        opts]
TMultiwayGraph[t_TTerm, max_Integer ? Positive, opts : OptionsPattern[]] :=
    TMultiwayGraph[
        TMultiTrace[t, {"Events", "CanonicalSlice", "SliceBoxes"}, max],
        opts]

(* Trace shape -- no slice info, can't build the multiway view. *)
TMultiwayGraph[trace_List ? multiTraceQ, OptionsPattern[]] :=
    (Message[TMultiwayGraph::needsteps]; $Failed)

TMultiwayGraph[steps_List, opts : OptionsPattern[]] := Block[{
    slices, sliceKeys, vertexLabel, allKeys, edges, branchial,
    vlabels, labelStyle, presentFamilies, legend,
    edgeStyles, edgeFamilies, edgeRules,
    edgeList
},
    (* Vertex identity = canonical SUB-resolved form of the slice
       leaf.  Two leaves at different steps with the same canonical
       form are the SAME vertex (an unchanged branch persists across
       steps as a single node).  Each step's record already carries
       the per-leaf canonical forms as "CanonicalSlice". *)
    sliceKeys = Map[r |-> r["CanonicalSlice"], steps];
    (* First-occurrence pre-computed TraditionalForm boxes for each
       canonical key.  Captured at slice time so SUB-chase saw the
       per-step heap state, not the post-run final state.  Falls
       back to the canonical key as a plain Text label when
       SliceBoxes isn't in the step record (minimal-keys mode). *)
    slices = Map[
        r |-> Lookup[r, "SliceBoxes",
                     ConstantArray[None, Length[r["CanonicalSlice"]]]],
        steps];
    vertexLabel = <||>;
    MapThread[
        {keys, boxes} |-> MapThread[
            {k, b} |-> If[ ! KeyExistsQ[vertexLabel, k], vertexLabel[k] = b],
            {keys, boxes}
        ],
        {sliceKeys, slices}
    ];
    allKeys = DeleteDuplicates @ Flatten[sliceKeys, 1];
    (* Edges: for each consecutive (prev, cur) slice pair, match
       leaves by canonical equality.  prev \cap cur = unchanged
       (no edge); prev \ cur = consumed; cur \ prev = produced;
       cross-product over the residuals attributes the firing only
       to the leaves that actually transformed.  This is robust to
       the slice walker emitting duplicate canonical entries
       (DeleteDuplicates collapses them to a single multiway state).
       Index-pairing the size-equal case would also misalign if the
       walker reorders entries -- canonical matching avoids that.
       Self-loop policy: when neither residual exists, the firing
       happened entirely "inside" an unresolved DP0/DP1 wrapper and
       no canonical changed; keep one self-loop per shared state so
       the event still appears in the view. *)
    edgeList = Catenate[
        MapIndexed[
            {prevKeys, idx} |-> Block[{
                i, curKeys, fam, rule, ev, prevSet, curSet,
                shared, prevResid, curResid, pairs
            },
                i = First[idx];
                If[ i >= Length[sliceKeys], Return[{}, Block]];
                curKeys = sliceKeys[[i + 1]];
                ev = steps[[i + 1]]["Events"];
                {fam, rule} = If[ ev === {},
                    {"WALK", ""}
                    ,
                    {ev[[1, "family"]], ev[[1, "rule"]]}
                ];
                prevSet = DeleteDuplicates[prevKeys];
                curSet = DeleteDuplicates[curKeys];
                shared = Intersection[prevSet, curSet];
                (* Select-not-MemberQ preserves first-occurrence
                   order so index-pairing below is meaningful;
                   Complement re-sorts and would scramble it. *)
                prevResid = Select[prevSet, ! MemberQ[shared, #] &];
                curResid = Select[curSet, ! MemberQ[shared, #] &];
                pairs = Which[
                    prevResid === {} && curResid === {},
                        Map[k |-> {k, k}, shared],
                    Length[prevResid] === Length[curResid],
                        (* Size-preserving firing (DIST / TERM /
                           same-size SPLIT): pair by slice-order so
                           each consumed leaf has exactly one outgoing
                           edge to its corresponding produced leaf,
                           not a cross-product to every sibling. *)
                        Transpose[{prevResid, curResid}],
                    prevResid =!= {} && curResid =!= {},
                        (* Genuine fan-out / fan-in (SLIDE / FORK /
                           MERGE with mismatched cardinality). *)
                        Tuples[{prevResid, curResid}],
                    prevResid =!= {} && curResid === {},
                        Tuples[{prevResid, shared}],
                    True,
                        Tuples[{shared, curResid}]
                ];
                Map[
                    p |-> {DirectedEdge[First[p], Last[p]], fam, rule},
                    pairs
                ]
            ],
            sliceKeys
        ]
    ];
    (* Multiple events can collapse onto the same edge -- most often
       a self-loop on a stable vertex that successive intra-wrapper
       rules all fire on.  Keep one edge but accumulate the rule
       names so the EdgeLabel shows the full firing sequence; family
       takes the first occurrence (used only for colour). *)
    edges = DeleteDuplicates[edgeList[[All, 1]]];
    edgeFamilies = AssociationMap[
        e |-> First @ Cases[edgeList, {e, f_, _} :> f],
        edges
    ];
    edgeRules = AssociationMap[
        e |-> StringRiffle[
            DeleteDuplicates @ Cases[edgeList, {e, _, r_} :> r],
            ", "],
        edges
    ];
    (* Optional branchial clique: connect ONLY the new sibling
       cohort introduced by a SPLIT or SLIDE firing.  Pre-existing
       slice leaves and DIST / TERM / MERGE post-states don't get
       redrawn -- those clutter without conveying new branchial
       structure.  A new sibling cohort = curSet \setminus prevSet,
       the leaves that didn't exist in the previous slice. *)
    branchial = If[
        TrueQ[OptionValue["Branchial"]]
        ,
        DeleteDuplicates @ Catenate @ Table[
            Block[{
                curKeys = sliceKeys[[i]],
                prevKeys = sliceKeys[[i - 1]],
                ev = steps[[i]]["Events"], fam, curResid
            },
                fam = If[ev === {}, "WALK", ev[[1, "family"]]];
                If[ ! MemberQ[{"SLIDE", "SPLIT"}, fam],
                    {}
                    ,
                    curResid = Select[
                        DeleteDuplicates[curKeys],
                        ! MemberQ[DeleteDuplicates[prevKeys], #] &];
                    If[ Length[curResid] >= 2,
                        Map[
                            pair |-> UndirectedEdge @@ pair,
                            Subsets[curResid, {2}]],
                        {}
                    ]
                ]
            ],
            {i, 2, Length[sliceKeys]}
        ]
        ,
        {}
    ];
    vlabels = OptionValue[VertexLabels];
    labelStyle = Directive[
        FontFamily -> "Helvetica", FontSize -> 9,
        LightDarkSwitched[GrayLevel[0.15], GrayLevel[0.95]]
    ];
    edgeStyles = Join[
        KeyValueMap[
            {edge, fam} |-> edge -> Directive[
                Arrowheads[Small],
                Lookup[$familyColors, fam,
                    LightDarkSwitched[GrayLevel[0.5], GrayLevel[0.6]]],
                AbsoluteThickness[1.5]
            ],
            edgeFamilies
        ],
        Map[
            be |-> be -> Directive[
                Dashing[Small],
                $wppBranchialStyle["EdgeStyle"],
                AbsoluteThickness[1.2]
            ],
            branchial
        ]
    ];
    presentFamilies = DeleteCases[DeleteDuplicates @ Values[edgeFamilies], "WALK"];
    legend = Replace[OptionValue[PlotLegends], {
        None | False -> None,
        Automatic | True :> familyLegend[presentFamilies]
    }];
    With[{g = Graph[
        allKeys,
        Join[edges, branchial],
        VertexLabels -> Map[
            k |-> k -> With[{b = vertexLabel[k]},
                If[ b === None, ToString[k], RawBoxes[b]]],
            allKeys],
        VertexLabelStyle -> labelStyle,
        EdgeLabels -> Normal[edgeRules],
        EdgeLabelStyle -> labelStyle,
        EdgeStyle -> edgeStyles,
        Background -> LightDarkSwitched[White, GrayLevel[0.13]],
        GraphLayout -> "LayeredDigraphEmbedding",
        FilterRules[{opts}, Options[Graph]]]
    },
        If[ legend === None, g, Legended[g, legend]]
    ]
]

End[];
EndPackage[];
