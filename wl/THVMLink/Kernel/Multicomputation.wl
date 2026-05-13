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
TCausalGraph::usage = "TCausalGraph[trace] returns a directed Graph[] of the causal structure of `trace` (= the \"Trace\" field of TMultiTrace).  Vertices are event ids; edges F -> E iff F's id appears in E's \"consumed\" list (wire provenance from M1).  Each vertex is coloured by its event's family (TERM / SLIDE / FORK / SPLIT / MERGE / PRUNE / PLUMB) so the trace shape is readable at a glance.  Options: \"VertexLabels\" -> Automatic labels each event with its rule name; \"Family\" -> {\"TERM\", ...} filters by family; \"PlotLegends\" -> Automatic emits a SwatchLegend of family -> colour (default None).";
TMultiwayGraph::usage = "TMultiwayGraph[steps] returns the multiway view of a `TMultiSteps` reduction.  Vertices are TERM-slices: per step, the active term's SUP-head is unfolded so that a term with head SUP{a, b} contributes one vertex per leaf (recursively, until non-SUP heads), and a term with non-SUP head contributes a single vertex.  Edges represent trace events between consecutive steps: source-target pairs are taken as a cross product (one edge per (source, target) pair across the two slices), coloured by the firing event's family (TERM / SLIDE / FORK / SPLIT / MERGE / PRUNE / PLUMB).  Options: \"Branchial\" -> True overlays a branchial clique (dashed WPP-styled edges) between sibling vertices within each SUP-bearing slice (default False); \"VertexLabels\" -> Automatic labels each vertex with the leaf term's tag/value; \"PlotLegends\" -> Automatic emits a SwatchLegend of family -> colour.";

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
            MapIndexed[{v, idx} |-> {First[idx] - 1, v}, wireProv],
            {_, -1}],
        Last -> First]

SetAttributes[TMultiTrace, HoldFirst];
TMultiTrace[expr_] := Block[
    {result, rows, wireProv, produced, events},
    ensureInit[];
    If[ ! TMultiTraceQ[], Message[TMultiTrace::notrace]; Return[$Failed]];
    $multiTraceInitFn[0];     (* allocate the events buffer; flag stays off *)
    $multiTraceSetFn[1];      (* recording on *)
    result = expr;            (* HoldFirst -- evaluated here, traced *)
    $multiTraceSetFn[0];      (* recording off *)
    rows = $multiTraceSnapshotFn[];          (* {Integer, 2}, n x 8 *)
    wireProv = $multiWireProvSnapshotFn[];   (* {Integer, 1}, HEAP_NEXT *)
    $multiTraceFreeFn[];      (* release the buffer *)
    events = Map[eventFromRow, rows];
    produced = producedFromWireProv[wireProv];
    <|
        "Result" -> result,
        "Trace" -> Map[
            e |-> Append[e, "produced" -> Lookup[produced, e["id"], {}]],
            events]
    |>
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
multicompStepsImpl[term0_, maxSteps_, auxSeeds_List] := Block[
    {seedTerm, snapshot, mkRecord, advance, recs},
    ensureInit[];
    If[ ! TMultiTraceQ[],
        Message[TMultiTrace::notrace];
        Return[$Failed]
    ];
    $multiTraceInitFn[0];
    $multiTraceSetFn[1];
    seedTerm = term0;
    (* Diagram seeds: anchors (SUPs the walker descended through) +
       user auxSeeds + the active term.  Anchors keep the lifted
       SUP(s) visible in every later step's diagram; the active
       term ensures the walker's current focus appears even if it
       is a free atom (NUM/ERA/...) not yet tied into the heap. *)
    snapshot[t_, anchors_List] :=
        THeapDiagram[Join[anchors, auxSeeds, {t}]];
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
    mkRecord[stepNum_, term_, events_, itrs_, evCursor_, frontier_,
             anchors_, parentSlot_] := With[
        {sl = sliceNow[term, anchors]},
        <|
            "Step"           -> stepNum,
            "Term"           -> term,
            "Diagram"        -> snapshot[term, anchors],
            "Slice"          -> sl,
            "SliceCanonical" -> Map[canonicalForm, sl],
            (* Pre-compute the TraditionalForm rendering NOW, while
               the heap is in this step's state.  We can't defer to
               display time -- subsequent steps mutate the heap, so
               a late TraditionalForm[t] would render the FINAL
               state for every step.  Store as box expressions to
               drop straight into vertex labels via RawBoxes. *)
            "SliceBoxes"     -> Map[
                t |-> ToBoxes[t, TraditionalForm],
                sl],
            "Events"         -> events,
            "ITRS"           -> itrs,
            "_evBefore"      -> evCursor,
            "_frontier"      -> frontier,
            "_anchors"       -> anchors,
            "_parent"        -> parentSlot
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
         anchors  = s["_anchors"],
         parent   = s["_parent"],
         evBefore = s["_evBefore"],
         before, after, next, evAfter, rows, events, tag, loc, l, r,
         done = False, result = $Done},
        While[ ! done,
            before = TItrs[];
            next   = TStep[term];
            after  = TItrs[];
            If[ after > before,
                (* Rule fired -- write the reduced head back to the
                   parent SUP-slot (if we descended through one) so
                   the anchor's slot reflects the new head, then
                   emit the step record. *)
                If[ ! MissingQ[parent], THeapSet[parent, next]];
                evAfter = $multiTraceCountFn[];
                rows    = $multiTraceSnapshotFn[];
                events  = If[ evBefore < evAfter,
                    Map[eventFromRow,
                        rows[[evBefore + 1 ;; evAfter]]],
                    {}
                ];
                result = mkRecord[s["Step"] + 1, next, events, after,
                                  evAfter, frontier, anchors, parent];
                done = True,
                (* No firing -- walk silently. *)
                tag = TTermTag[next];
                Which[
                    tag === $TagSUP,
                        (* Descend into left, stash right on frontier
                           tagged with its own slot, anchor the SUP. *)
                        loc = TTermVal[next];
                        l   = THeapRead[loc + 0];
                        r   = THeapRead[loc + 1];
                        anchors  = Append[anchors, next];
                        frontier = Prepend[frontier, {r, loc + 1}];
                        term     = l;
                        parent   = loc + 0,
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
        mkRecord[0, seedTerm, {}, TItrs[], $multiTraceCountFn[], {},
                 {}, Missing[]],
        r |-> r =!= $Done,
        1,
        maxSteps
    ];
    $multiTraceSetFn[0];
    $multiTraceFreeFn[];
    Map[
        r |-> KeyDrop[r, {"_evBefore", "_frontier", "_anchors", "_parent"}],
        DeleteCases[recs, $Done]]
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
    "TERM"  -> StandardBlue,
    "SLIDE" -> StandardOrange,
    "FORK"  -> StandardGreen,
    "SPLIT" -> StandardPurple,
    "MERGE" -> StandardRed,
    "PRUNE" -> LightDarkSwitched[GrayLevel[0.5], GrayLevel[0.6]],
    "PLUMB" -> LightDarkSwitched[GrayLevel[0.75], GrayLevel[0.35]]
|>;

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
        LightDarkSwitched[GrayLevel[0.15], GrayLevel[0.9]]]];

Options[TCausalGraph] = Join[
    {"Family" -> All,
     "TransitiveReduction" -> True,
     PlotLegends -> None},
    Options[Graph]];
TCausalGraph[trace_List, opts : OptionsPattern[]] := Block[
    {events, ids, families, keep, edges, vlabels, wpp, labelStyle,
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
    vlabels = OptionValue[VertexLabels];
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
    userGraphOpts = FilterRules[{opts}, Options[Graph]] /.
        (VertexLabels -> _) -> Nothing;
    With[{g = Graph[
        ids,
        edges,
        Sequence @@ userGraphOpts,
        VertexLabels -> Switch[vlabels,
            Automatic, Thread[ids -> events[[All, "rule"]]],
            None, None,
            _, vlabels],
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
    {term}];

(* Canonical form of a term: recursive structural representation
   that chases through SUB-flagged DP/VAR projections, so two terms
   that resolve to the same content (e.g. OP2(1, DP@4-sub-NUM(3))
   vs OP2(1, NUM(3))) compare equal.  Used as the vertex identity
   in TMultiwayGraph -- a branch whose head Term value is unchanged
   across steps but whose SUB-resolved content changed shows up as
   a new vertex.  Depth-limited at 16 to bound recursion through
   cyclic structures (e.g. self-referential ALOs). *)
canonicalForm[term_] := canonicalFormDepth[term, 16];
canonicalFormDepth[term_, depth_Integer] := If[ depth <= 0,
    {"?"},
    Block[{tag = TTermTag[term], val = TTermVal[term],
           ext = TTermExt[term], cell},
        Switch[tag,
            $TagNUM, {"NUM", val},
            $TagERA, {"ERA"},
            $TagTEN, {"TEN", val},
            $TagREF, {"REF", val},
            $TagANY, {"ANY"},
            $TagDP0 | $TagDP1,
                cell = THeapRead[val];
                If[ TTermSub[cell] === 1,
                    canonicalFormDepth[
                        packTerm[0, TTermTag[cell], TTermExt[cell],
                                 TTermVal[cell]],
                        depth - 1],
                    {If[tag === $TagDP0, "DP0", "DP1"], val, ext}],
            $TagVAR,
                cell = THeapRead[val];
                If[ TTermSub[cell] === 1,
                    canonicalFormDepth[
                        packTerm[0, TTermTag[cell], TTermExt[cell],
                                 TTermVal[cell]],
                        depth - 1],
                    {"VAR", val}],
            $TagOP2,
                {"OP2", ext,
                 canonicalFormDepth[THeapRead[val + 0], depth - 1],
                 canonicalFormDepth[THeapRead[val + 1], depth - 1]},
            $TagAPP,
                {"APP",
                 canonicalFormDepth[THeapRead[val + 0], depth - 1],
                 canonicalFormDepth[THeapRead[val + 1], depth - 1]},
            $TagSUP,
                {"SUP", ext,
                 canonicalFormDepth[THeapRead[val + 0], depth - 1],
                 canonicalFormDepth[THeapRead[val + 1], depth - 1]},
            $TagLAM,
                {"LAM", val,
                 canonicalFormDepth[THeapRead[val], depth - 1]},
            $TagDUP,
                {"DUP", val,
                 canonicalFormDepth[THeapRead[val], depth - 1]},
            _, {ToString[tag], val, ext}]]];

(* Short caption for a term (used as the vertex label).  Mirrors
   what diagrams print: e.g. "NUM 4@12", "DP0@4", "SUP@14..15". *)
$tagShortName = <|
    $TagVAR -> "VAR", $TagDP0 -> "DP0", $TagDP1 -> "DP1",
    $TagLAM -> "LAM", $TagAPP -> "APP", $TagSUP -> "SUP",
    $TagERA -> "ERA", $TagDUP -> "DUP", $TagNUM -> "NUM",
    $TagREF -> "REF", $TagOP2 -> "OP2", $TagMAT -> "MAT",
    $TagCTR -> "CTR", $TagALO -> "ALO", $TagDSU -> "DSU",
    $TagDDU -> "DDU", $TagTEN -> "TEN", $TagANY -> "ANY",
    $TagUOP -> "UOP", $TagINC -> "INC"|>;
termCaption[t_] := Block[{tag, val, nm},
    tag = TTermTag[t];
    val = TTermVal[t];
    nm = Lookup[$tagShortName, tag, "?" <> ToString[tag]];
    Switch[tag,
        $TagNUM, nm <> " " <> ToString[val],
        $TagSUP, nm <> "@" <> ToString[val] <> ".." <> ToString[val + 1],
        _, nm <> "@" <> ToString[val]]];

Options[TMultiwayGraph] = Join[
    {"Branchial" -> False,
     PlotLegends -> None},
    Options[Graph]];
TMultiwayGraph[steps_List, opts : OptionsPattern[]] := Block[
    {slices, sliceKeys, vertexLabel, allKeys, edges, branchial,
     vlabels, labelStyle, legendOpt, presentFamilies, legend,
     edgeStyles, edgeFamilies, brStyle, userGraphOpts, edgeList},
    (* Vertex identity = canonical SUB-resolved form of the slice
       leaf.  Two leaves at different steps with the same canonical
       form are the SAME vertex (an unchanged branch persists across
       steps as a single node).  Each step's record already carries
       the per-leaf canonical forms as "SliceCanonical". *)
    sliceKeys = Map[r |-> r["SliceCanonical"], steps];
    slices = Map[r |-> r["SliceBoxes"], steps];
    (* First-occurrence pre-computed TraditionalForm boxes for each
       canonical key.  Captured at slice time so SUB-chase saw the
       per-step heap state, not the post-run final state. *)
    vertexLabel = <||>;
    MapThread[
        {keys, boxes} |-> MapThread[
            {k, b} |-> If[ ! KeyExistsQ[vertexLabel, k],
                vertexLabel[k] = b],
            {keys, boxes}],
        {sliceKeys, slices}];
    allKeys = DeleteDuplicates @ Flatten[sliceKeys, 1];
    (* Edges: for each consecutive (prev, cur) slice pair, draw
       edges between same-index leaves (same-size) or cross product
       (size mismatch -- SPLIT / MERGE).  Drop self-loops (a branch
       whose canonical form is unchanged by the firing). *)
    edgeList = Catenate[
        MapIndexed[
            {prevKeys, idx} |-> Block[{i, curKeys, fam, pairs},
                i = First[idx];
                If[ i >= Length[sliceKeys], Return[{}, Block]];
                curKeys = sliceKeys[[i + 1]];
                fam = If[ steps[[i + 1]]["Events"] === {},
                    "WALK",
                    steps[[i + 1]]["Events"][[1, "family"]]];
                pairs = If[ Length[prevKeys] === Length[curKeys],
                    Transpose[{prevKeys, curKeys}],
                    Tuples[{prevKeys, curKeys}]];
                Select[
                    Map[
                        p |-> {DirectedEdge[First[p], Last[p]], fam},
                        pairs],
                    pe |-> First[pe][[1]] =!= First[pe][[2]]]],
            sliceKeys]];
    edgeFamilies = AssociationThread[edgeList[[All, 1]] -> edgeList[[All, 2]]];
    edges = DeleteDuplicates[edgeList[[All, 1]]];
    (* Optional branchial clique inside each multi-leaf slice. *)
    branchial = If[ OptionValue["Branchial"],
        DeleteDuplicates @ Catenate[
            Map[
                s |-> If[ Length[s] >= 2,
                    Map[
                        pair |-> UndirectedEdge @@ pair,
                        DeleteDuplicates[Map[Sort, Subsets[s, {2}]]]],
                    {}],
                sliceKeys]],
        {}];
    vlabels = OptionValue[VertexLabels];
    labelStyle = Directive[
        FontFamily -> "Helvetica", FontSize -> 9,
        LightDarkSwitched[GrayLevel[0.15], GrayLevel[0.95]]];
    brStyle = $wppBranchialStyle;
    edgeStyles = Join[
        KeyValueMap[
            {edge, fam} |-> edge -> Directive[
                Arrowheads[Small],
                Lookup[$familyColors, fam,
                    LightDarkSwitched[GrayLevel[0.5], GrayLevel[0.6]]],
                AbsoluteThickness[1.5]],
            edgeFamilies],
        Map[
            be |-> be -> Directive[
                Dashing[Small],
                brStyle["EdgeStyle"],
                AbsoluteThickness[1.2]],
            branchial]];
    legendOpt = OptionValue[PlotLegends];
    presentFamilies = DeleteCases[
        DeleteDuplicates @ Values[edgeFamilies], "WALK"];
    legend = Switch[legendOpt,
        None | False, None,
        Automatic | True, familyLegend[presentFamilies],
        _, legendOpt];
    userGraphOpts = FilterRules[{opts}, Options[Graph]] /.
        (VertexLabels -> _) -> Nothing;
    With[{g = Graph[
        allKeys,
        Join[edges, branchial],
        Sequence @@ userGraphOpts,
        VertexLabels -> Switch[vlabels,
            Automatic, Map[
                k |-> k -> RawBoxes[vertexLabel[k]],
                allKeys],
            None, None,
            _, vlabels],
        VertexLabelStyle -> labelStyle,
        EdgeStyle -> edgeStyles,
        Background -> LightDarkSwitched[White, GrayLevel[0.13]],
        GraphLayout -> "LayeredDigraphEmbedding"]},
        If[ legend === None, g, Legended[g, legend]]]
]

End[];
EndPackage[];
