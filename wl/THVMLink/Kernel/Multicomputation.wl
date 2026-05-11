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
                                 "termA", "termB", "deltaLabel" |>.
                            `family` is one of TERM / SLIDE / FORK /
                            SPLIT / MERGE / PRUNE / PLUMB.

   TMulticompTraceQ[]       True iff the loaded THVMLink dylib was
                            built with -DTHVM_TRACE (the standard
                            `make wl` builds it on).

   The standard recipe: build the term outside the trace (heap
   construction fires no interactions), then trace the reduction --
   e.g.  {dp0, dp1} = TDup[TOp2["+", TSup[1, 2], 3]];
         TMulticompTrace[TCollapse[dp0]]. *)

BeginPackage["THVMLink`"];

TMulticompTrace::usage = "TMulticompTrace[expr] evaluates `expr` (HoldFirst) with the multicomputation reduction trace recording, then returns <|\"Result\" -> value of expr, \"Trace\" -> {event, ...}|>, where each `event` is an Association <|\"id\", \"rule\", \"ruleCode\", \"family\", \"familyCode\", \"termA\", \"termB\", \"deltaLabel\"|>.  An event is one interaction-net rule firing; `family` is one of TERM (within-branch compute), SLIDE (re-foliation: APP-SUP commute, INC, ...), FORK (1 -> 2), SPLIT (DUP-SUP cross product), MERGE (DUP-SUP annihilate), PRUNE (ERA), PLUMB (sharing housekeeping) -- see docs/multicomputation.md.  Recording is turned on for the duration of `expr` and off afterwards.  Requires a trace-enabled dylib: build it with `make WL_TRACE=1 wl` (the default `make wl` omits trace support); check TMulticompTraceQ[].";
TMulticompTraceQ::usage = "TMulticompTraceQ[] returns True iff the loaded THVMLink dylib was built with -DTHVM_TRACE (via `make WL_TRACE=1 wl`), so TMulticompTrace works.  The default `make wl` builds a trace-free dylib.";

Begin["`Private`"];

TMulticompTraceQ[] := (ensureInit[]; $multiTraceSupportedFn[] === 1)

(* Lazy, memoised name lookups -- the C side (multi_rule_name /
   multi_family_name in src/instrument/multi.c) owns the RULE_* /
   MULTI_* -> string tables, so there's no parallel WL list to keep
   in sync. *)
multiRuleName[c_Integer]   := multiRuleName[c]   = $multiRuleNameFn[c]
multiFamilyName[c_Integer] := multiFamilyName[c] = $multiFamilyNameFn[c]

TMulticompTrace::notrace = "TMulticompTrace requires a trace-enabled THVMLink dylib.  Rebuild with `make WL_TRACE=1 wl`.  TMulticompTraceQ[] reports the status.";

SetAttributes[TMulticompTrace, HoldFirst];
TMulticompTrace[expr_] := Module[{result, rows},
    ensureInit[];
    If[ ! TMulticompTraceQ[], Message[TMulticompTrace::notrace]; Return[$Failed]];
    $multiTraceInitFn[0];          (* allocate the events buffer; flag stays off *)
    $multiTraceSetFn[1];           (* recording on *)
    result = expr;                 (* HoldFirst -- evaluated here, traced *)
    $multiTraceSetFn[0];           (* recording off *)
    rows = $multiTraceSnapshotFn[];   (* {Integer, 2}, n x 6 *)
    $multiTraceFreeFn[];           (* release the buffer *)
    <| "Result" -> result,
       "Trace"  -> Map[
           row |-> <| "id"         -> row[[1]],
                      "rule"       -> multiRuleName[row[[2]]],
                      "ruleCode"   -> row[[2]],
                      "family"     -> multiFamilyName[row[[3]]],
                      "familyCode" -> row[[3]],
                      "termA"      -> row[[4]],
                      "termB"      -> row[[5]],
                      "deltaLabel" -> row[[6]] |>,
           rows] |>
]

End[];
EndPackage[];
