(* ::Package:: *)
(* ATP_Relevance.wl - axiom-relevance filter: TRelevantAxioms and the Safe / Connected / SInE
   premise-selection modes the Method \"AxiomRelevance\" suboption drives.

   Sibling of ATP.wl in the WolframInstitute`THVMLink`ATP` context; the recursive Kernel
   loader Gets it after ATP.wl (files sorted by {depth, lowercased path}),
   and it shares the WolframInstitute`THVMLink`ATP`Private` context, so it references
   ATP.wl's loaders, encoder, and helpers by bare name. *)

BeginPackage["WolframInstitute`THVMLink`ATP`", {"WolframInstitute`THVMLink`", "Wolfram`Parser`"}];

GeneralUtilities`SetUsage[TRelevantAxioms, "TRelevantAxioms[conjecture$, axioms$] reports which axioms the relevance filter keeps versus drops for proving conjecture$, without running a proof.
TRelevantAxioms[\"Theorem\", \"Theory\"] resolves names through AxiomaticTheory.
Returns an association with keys \"Mode\", \"Kept\" (a list of axioms), and \"Dropped\" (a list of associations with keys \"Axiom\", \"Symbols\", \"Reason\").
The relevance mode is set by the Method \"AxiomRelevance\" suboption: None keeps all; \"Safe\" (default) drops only confined dead-weight axioms (sound and completeness-preserving); \"Connected\" applies symbol-reachability pruning (heuristic); \"SInE\" applies SInE premise selection. \"Connected\" and \"SInE\" accept tuning options ({\"Connected\", \"FrequencyCutoff\", \"MaxGenerations\"}, {\"SInE\", \"SineTolerance\", \"SineDepth\", \"SineGenerality\"}); see the ATP documentation for the full option surface."];
Begin["`Private`"];

(* === Axiom-relevance filter ======================================

   Prunes axioms that cannot (or, in the heuristic mode, are unlikely
   to) contribute to a proof of the conjecture, so completion does not
   waste effort - or diverge - on them.  Configurable via the Method
   suboption "AxiomRelevance" (back-compat alias: "DropDivergentAxioms"):

     None | All | False    - keep every axiom.
     Automatic | "Safe"     - DEFAULT.  Drop only axioms that are
       PROVABLY dead weight for this goal: a "confined" axiom carrying
       a private symbol on BOTH sides (see atpConfinedSymbols).  Sound
       AND completeness-preserving.
     "Connected" | {"Connected", "FrequencyCutoff" -> f,
                    "MaxGenerations" -> n}
       - SInE-style symbol-connectivity pruning: keep only axioms
       reachable from the goal's symbols (ignoring ubiquitous symbols
       that occur in >= f of the axioms; f defaults to 1, i.e. ignore
       only symbols common to ALL axioms).  HEURISTIC: may drop a
       needed axiom, so it is opt-in, not the default.

   The decision is inspectable without proving via TRelevantAxioms. *)

(* function symbols of an axiom/conjecture (raw ForAll form, a list of
   them, or an unquantified equation): non-variable atoms, minus the
   ForAll-bound variables, the Pattern-bound variables, and the
   structural heads. *)
atpFnSyms[expr_] := Block[{vars, patternVars, body},
    (* ForAll is HoldAll, so a `ForAll[v_, _] :> v` rule does NOT bind
       v - extract the bound-variable spec by Part instead. *)
    vars = Flatten @ Map[#[[1]] &, Cases[expr, _ForAll, {0, Infinity}]];
    (* Pattern[x, _] introduces x as a bound variable in equational
       axiom shape (an axiom without an outer ForAll wrapper).  Walk
       the expression for Pattern wrappers and collect their first
       arg. *)
    patternVars = DeleteDuplicates @
        Cases[expr, Verbatim[Pattern][s_Symbol, _] :> s, {0, Infinity}];
    body = expr /. ForAll[_, e_] :> e;
    (* Heads -> True is essential here: the discriminating function
       symbol is usually an operator (CenterDot, OverTilde, ...)
       appearing as a HEAD, which Cases skips by default. *)
    DeleteDuplicates @ DeleteCases[
        Cases[body, s_Symbol, {0, Infinity}, Heads -> True],
        Alternatives @@ Join[vars, patternVars,
            {Equal, Inactive, ForAll, Exists, List, And, Or, Not,
             Implies, Pattern, Blank, HoldPattern, Verbatim, Rule}]]
]

(* {lhs, rhs} symbol form of an axiom (raw or unquantified). *)
atpAxSides[axForm_] := Block[{eq},
    eq = axForm /. ForAll[_, e_] :> e;
    If[ MatchQ[eq, _Equal], List @@ eq, {eq, eq}]
]

(* private symbols of `ax` (relative to the conjecture + other
   axioms): function symbols of ax that occur nowhere else. *)
atpConfinedSymbols[ax_, others_, conjRaw_] :=
    Complement[atpFnSyms[ax],
        If[others === {}, {}, Union @@ (atpFnSyms /@ others)],
        atpFnSyms[conjRaw]]

(* Does ax carry a private symbol on BOTH sides?  Then neither rewrite
   direction can fire without that symbol already present, so ax can
   never enter (or be introduced into) a derivation of a goal free of
   it - dropping ax is sound (proof over a subset is valid) AND
   completeness-preserving.  The Y combinator Y x == x (Y x) is the
   canonical case: Y is private and on both sides.  Returns the
   witnessing symbols, or {}. *)
atpSafeDropSymbols[ax_, others_, conjRaw_] := Block[{priv, sides},
    priv = atpConfinedSymbols[ax, others, conjRaw];
    If[ priv === {}, Return[{}]];
    sides = atpAxSides[ax];
    Intersection[priv, atpFnSyms[sides[[1]]], atpFnSyms[sides[[2]]]]
]

(* normalize a Method spec / option value into a relevance spec:
   None | "Safe" | {"Connected", <|opts|>} | {"SInE", <|opts|>}.  The
   "AxiomRelevance" suboption is accepted on every completion-family
   method head, so portfolio entries like {"GoalDirected",
   "AxiomRelevance" -> "SInE"} are honored the same as
   {"Completion", "AxiomRelevance" -> "SInE"}. *)
atpRelevanceSpec[Automatic | "Portfolio"] := "Safe"
atpRelevanceSpec["Completion" | "GoalDirected" | "MNF" | "Waldmeister"] := "Safe"
atpRelevanceSpec[{("Completion" | "GoalDirected" | "MNF" | "Waldmeister"),
        subopts___Rule}] := Block[{o, r, dd},
    o = Association[subopts];
    r = Lookup[o, "AxiomRelevance", Automatic];
    dd = Lookup[o, "DropDivergentAxioms", Automatic];  (* back-compat *)
    Which[
        r =!= Automatic, atpNormRelevance[r],
        dd === False, None,
        True, "Safe"]
]
(* Bare rule list (no leading method head): user passing just relevance
   options, e.g. Method -> {"AxiomRelevance" -> "SInE"} or
   Method -> {"AxiomRelevance" -> {"SInE", "SineTolerance" -> 3}}.
   Honor it the same as the method-head form. *)
atpRelevanceSpec[{subopts___Rule}] := Block[{o, r},
    o = Association[subopts];
    r = Lookup[o, "AxiomRelevance", Automatic];
    If[ r === Automatic, "Safe", atpNormRelevance[r]]
]
atpRelevanceSpec[_] := "Safe"

atpNormRelevance[None | All | False] := None
atpNormRelevance[Automatic | True | "Safe"] := "Safe"
atpNormRelevance["Connected"] := {"Connected", <||>}
atpNormRelevance[{"Connected", a_Association}] := {"Connected", a}
atpNormRelevance[{"Connected", o___Rule}] := {"Connected", Association[o]}
(* SInE: the Hoder-Voronkov premise-selection algorithm (IJCAR 2011) as
   shipped in Vampire (Shell/SineUtils.cpp).  Knob names mirror
   Vampire's option flags --sine_tolerance (st), --sine_depth (sd),
   --sine_generality_threshold (sgt).  Defaults 3 / 2 / 8 reproduce
   the winning portfolio strategy lrs+10_1 st=3:sd=2:ss=axioms:sgt=8
   from Vampire. *)
atpNormRelevance["SInE"] := {"SInE", <||>}
atpNormRelevance[{"SInE", a_Association}] := {"SInE", a}
atpNormRelevance[{"SInE", o___Rule}] := {"SInE", Association[o]}
(* Numeric/symbolic shorthand: {"SInE", st, sd, sgt}. *)
atpNormRelevance[{"SInE", st_?NumericQ}] :=
    {"SInE", <|"SineTolerance" -> st|>}
atpNormRelevance[{"SInE", st_?NumericQ, sd_Integer}] :=
    {"SInE", <|"SineTolerance" -> st, "SineDepth" -> sd|>}
atpNormRelevance[{"SInE", st_?NumericQ, sd_Integer, sgt_Integer}] :=
    {"SInE", <|"SineTolerance" -> st, "SineDepth" -> sd,
               "SineGenerality" -> sgt|>}
atpNormRelevance[other_] := (
    Message[TFindProof::badrel, other]; "Safe")

(* Partition `axFormList` into kept / dropped (with reasons) under a
   relevance spec.  Returns <|"Kept" -> {...}, "Dropped" -> {<|"Axiom",
   "Symbols", "Reason"|>...}, "Mode" -> ...|>.  Order-preserving:
   auto-precedence keys off axiom/label order, so re-sorting would
   change the LPO orientation. *)
atpRelevancePartition[axFormList_, conjRaw_, specRaw_] :=
    Block[{spec = atpNormRelevance[specRaw], dropAssoc},
        Switch[spec,
            None,
                <|"Kept" -> axFormList, "Dropped" -> {}, "Mode" -> None|>,
            "Safe",
                dropAssoc = DeleteMissing @ Map[
                    ax |-> Block[{syms},
                        syms = atpSafeDropSymbols[ax,
                            DeleteCases[axFormList, ax], conjRaw];
                        If[ syms === {}, Missing[],
                            <|"Axiom" -> ax, "Symbols" -> syms,
                              "Reason" -> "ConfinedBothSides"|>]],
                    axFormList];
                <|"Kept" -> Select[axFormList,
                        FreeQ[#["Axiom"] & /@ dropAssoc, #] &],
                  "Dropped" -> dropAssoc, "Mode" -> "Safe"|>,
            {"Connected", _},
                atpConnectedPartition[axFormList, conjRaw, Last[spec]],
            {"SInE", _},
                atpSinePartition[axFormList, conjRaw, Last[spec]]
        ]
    ]

(* SInE-style connectivity partition. *)
atpConnectedPartition[axFormList_, conjRaw_, opts_Association] := Block[{
    symLists, nAx, freq, cutoff, ignored, relevant, keep, changed, gen,
    maxGen, dropAssoc},
    nAx = Length[axFormList];
    symLists = atpFnSyms /@ axFormList;
    freq = Counts[Flatten[symLists]];
    cutoff = Lookup[opts, "FrequencyCutoff", 1];
    maxGen = Lookup[opts, "MaxGenerations", nAx];
    (* ignore symbols occurring in >= cutoff fraction of the axioms *)
    ignored = Keys @ Select[freq, # >= Ceiling[cutoff * nAx] &];
    relevant = Complement[atpFnSyms[conjRaw], ignored];
    keep = ConstantArray[False, nAx];
    changed = True; gen = 0;
    While[ changed && gen < maxGen,
        changed = False;
        Do[ If[ ! keep[[i]] &&
                IntersectingQ[Complement[symLists[[i]], ignored], relevant],
                keep[[i]] = True;
                relevant = Union[relevant,
                    Complement[symLists[[i]], ignored]];
                changed = True],
            {i, nAx}];
        gen++];
    dropAssoc = Table[
        If[ ! keep[[i]],
            <|"Axiom" -> axFormList[[i]],
              "Symbols" -> Complement[symLists[[i]], ignored],
              "Reason" -> "Disconnected"|>, Nothing],
        {i, nAx}];
    <|"Kept" -> Pick[axFormList, keep],
      "Dropped" -> dropAssoc, "Mode" -> "Connected"|>
]

(* Vampire-faithful SInE (Sumo-Inspired premise selection, Hoder &
   Voronkov, IJCAR 2011) port.  Reference impl: vprover/vampire,
   Shell/SineUtils.cpp - the canonical D-relation + bounded BFS that
   the Vampire option block --sine_selection axioms / --sine_tolerance
   st / --sine_depth sd / --sine_generality_threshold sgt drives.

   Algorithm:
     1. Count occ(s) = number of axioms containing function symbol s.
     2. For each axiom A, minOcc(A) = min_{s in A} occ(s).
     3. D-relation: axiom A is D-related to symbol s iff
          s in A  AND  occ(s) <= st * minOcc(A).
        ("s is among the rarest symbols of A, up to tolerance st.")
     4. BFS from the conjecture's symbols (S_0) to depth sd:
        for each new symbol added at step k-1, pull in every axiom
        D-related to it; those axioms contribute their symbols to S_k.
        Stop at depth sd (Vampire default 2).
     5. Generality threshold sgt: a symbol with occ(s) > sgt is "too
        general" and is NOT used as a trigger (it neither seeds from
        the goal nor propagates through newly-pulled axioms), even if
        it would be the rarest in some axiom.  Drops the very common
        symbols (the operator that appears in every axiom of the
        theory) from D-relation triggering.

   Knobs (Method "AxiomRelevance" -> {"SInE", <|...|>} or "SInE" with
   Vampire defaults):
     "SineTolerance"  st, real, default 3.   (Vampire --sine_tolerance)
     "SineDepth"      sd, int,  default 2.   (Vampire --sine_depth)
     "SineGenerality" sgt, int, default 8.   (Vampire --sine_generality_threshold)

   Tolerance st=1 is strictest (only the strictly-rarest symbol(s) of
   an axiom can trigger it); st=Infinity collapses to plain
   symbol-reachability (any shared symbol triggers).  Depth sd=0 keeps
   no axiom; sd large saturates to the full connected component. *)
atpSinePartition[axFormList_, conjRaw_, opts_Association] := Block[{
    symLists, nAx, occ, minOccA, st, sd, sgt, conjSyms, frontier,
    visited, axTaken, keep, gen, newFrontier, dropAssoc, generalQ},
    nAx = Length[axFormList];
    If[ nAx === 0,
        Return[<|"Kept" -> {}, "Dropped" -> {}, "Mode" -> "SInE"|>]];
    symLists = atpFnSyms /@ axFormList;
    occ = Counts[Flatten[symLists]];
    st  = N @ Lookup[opts, "SineTolerance",  3.];
    sd  =     Lookup[opts, "SineDepth",      2 ];
    sgt =     Lookup[opts, "SineGenerality", 8 ];
    (* Per-axiom minOcc.  An axiom with no function symbols (e.g.
       a == a - vacuously true) has no minOcc; we never D-relate
       any symbol to it, so it stays dropped unless seeded directly,
       matching Vampire's behavior of skipping empty-signature units. *)
    minOccA = Table[
        If[ symLists[[i]] === {}, Infinity,
            Min[Lookup[occ, #, Infinity] & /@ symLists[[i]]]],
        {i, nAx}];
    (* "Too-general" predicate: a symbol with occ(s) > sgt is not used
       as a trigger.  sgt <= 0 disables the cutoff (mirrors Vampire's
       --sine_generality_threshold 0 = off). *)
    generalQ = If[ IntegerQ[sgt] && sgt > 0,
        s |-> Lookup[occ, s, 0] > sgt,
        s |-> False];
    (* Seed: conjecture's symbols, minus the too-general ones. *)
    conjSyms = atpFnSyms[conjRaw];
    frontier = Select[conjSyms, ! generalQ[#] &];
    visited = frontier;
    axTaken = ConstantArray[False, nAx];
    Do[ newFrontier = {};
        Do[ If[ ! axTaken[[i]] && symLists[[i]] =!= {} &&
                AnyTrue[frontier, s |->
                    MemberQ[symLists[[i]], s] &&
                    Lookup[occ, s, 0] <= st * minOccA[[i]]],
                axTaken[[i]] = True;
                newFrontier = Union[newFrontier,
                    Select[symLists[[i]],
                        ! MemberQ[visited, #] && ! generalQ[#] &]]],
            {i, nAx}];
        If[ newFrontier === {}, Break[]];
        visited = Union[visited, newFrontier];
        frontier = newFrontier,
        {gen, sd}];
    keep = axTaken;
    dropAssoc = Table[
        If[ ! keep[[i]],
            <|"Axiom" -> axFormList[[i]],
              "Symbols" -> symLists[[i]],
              "Reason" -> "SInEUnreachable"|>, Nothing],
        {i, nAx}];
    <|"Kept" -> Pick[axFormList, keep],
      "Dropped" -> dropAssoc, "Mode" -> "SInE"|>
]

(* Apply the relevance filter, Message the dropped axioms, return the
   kept list (order-preserving). *)
atpApplyRelevance[axFormList_, conjRaw_, specRaw_] := Block[{part},
    part = atpRelevancePartition[axFormList, conjRaw, specRaw];
    If[ part["Dropped"] =!= {},
        Message[TFindProof::dropax,
            #["Symbols"] & /@ part["Dropped"], part["Mode"]]];
    part["Kept"]
]

(* Public: inspect the relevance decision without proving. *)
Options[TRelevantAxioms] = {Method -> Automatic}
TRelevantAxioms[thm_String, theory_String, opts : OptionsPattern[]] :=
    Block[{axRaw, cjRaw},
        axRaw = AxiomaticTheory[theory];
        cjRaw = AxiomaticTheory[theory, "NotableTheorems"][thm];
        If[ ! ListQ[axRaw] || MissingQ[cjRaw], Return[$Failed]];
        TRelevantAxioms[cjRaw, axRaw, opts]
    ]
(* Conjecture-expression against a NAMED theory: resolve the axioms
   through AxiomaticTheory, then run the relevance partition. *)
TRelevantAxioms[
        conjRaw : (_List | _ForAll | _Equal | _Unequal
            | Inactive[Equal][_, _] | Inactive[Unequal][_, _]),
        theory_String, opts : OptionsPattern[]] :=
    Block[{axRaw},
        axRaw = AxiomaticTheory[theory];
        If[ ! ListQ[axRaw], Return[$Failed]];
        TRelevantAxioms[conjRaw, axRaw, opts]
    ]
(* Single non-list axiom: auto-wrap to a 1-element list, same shape
   as TFindProof's wrap. *)
TRelevantAxioms[conjRaw_, axiom : (_Equal | _Unequal | _ForAll
        | Inactive[Equal][_, _] | Inactive[Unequal][_, _]),
        opts : OptionsPattern[]] :=
    TRelevantAxioms[conjRaw, {axiom}, opts]

TRelevantAxioms[conjRaw_, axRaw_List, opts : OptionsPattern[]] :=
    (* Same normalization pipeline as TFindProof's entry:
       atpNormalizeConj + atpNormalizeAxioms. *)
    atpRelevancePartition[
        atpNormalizeAxioms[axRaw],
        atpNormalizeConj[conjRaw],
        atpRelevanceSpec[OptionValue[Method]]]

(* Render a held expression in the form WL's ProofObject expects
   for its top-level Axioms list / ConjectureStatement: keep the
   ForAll wrapper if present, but rewrite every nested `Equal[lhs,
   rhs]` to `Inactive[Equal][lhs, rhs]` so trivial tautology axioms
   `a == a` don't collapse to True under ReleaseHold. *)
holdToInactive[axHC_HoldComplete] :=
    ReleaseHold[axHC /. Equal -> Inactive[Equal]]

End[];
EndPackage[];
