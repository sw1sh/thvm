(* ::Package:: *)
(* AtpSystem.wl - a completed equational system kept in the engine, for
   simplifiers that ask many normal-form questions of one theory:
   TAtpComplete completes the axioms once (TKnuthBendixCompletion's
   configuration) and keeps the engine state; TAtpNormalForm and TAtpEqualQ
   answer from it by ordered rewriting, without completing again. *)

BeginPackage["WolframInstitute`THVMLink`ATP`", {"GeneralUtilities`", "WolframInstitute`THVMLink`"}]

SetUsage[TAtpComplete, "TAtpComplete[axioms$] completes the equations axioms$ by unfailing Knuth-Bendix completion and gives a TAtpSystem that keeps the completed system in the engine.
TAtpComplete[axioms$, order$] completes under the ordering order$, as in TKnuthBendixCompletion.
TAtpComplete[\"Theory\", $$] completes the axioms of AxiomaticTheory[\"Theory\"].
Options: MaxSteps (default 2000), \"AllocationLimit\" (heap cells the completion may allocate), TimeConstraint (default Infinity), Method (default \"Waldmeister\")."];
SetUsage[TAtpSystem, "TAtpSystem[$$] is a completed equational system kept in the engine, from TAtpComplete.
sys$[\"Rules\"] gives its rules and unorientable equations, sys$[\"Ordering\"] the reduction ordering, sys$[\"Status\"] \"Saturated\" or \"BudgetExhausted\", sys$[\"Complete\"] whether it is ground convergent."];
SetUsage[TAtpNormalForm, "TAtpNormalForm[sys$, term$] gives the normal form of term$ under the TAtpSystem sys$.
TAtpNormalForm[sys$, {term$1, term$2, $$}] gives the normal forms of several terms in one call."];
SetUsage[TAtpEqualQ, "TAtpEqualQ[sys$, a$, b$] gives True when a$ and b$ have the same normal form under the TAtpSystem sys$, so the axioms entail a$ == b$.
It gives Failure[\"NotEntailed\", $$] when sys$ is complete and the normal forms differ, and Indeterminate when the completion stopped at its budget."];

Begin["`Private`"]

Options[TAtpComplete] = {MaxSteps -> 2000, "AllocationLimit" -> Automatic, TimeConstraint -> Infinity, Method -> "Waldmeister"};

TAtpComplete::fail = "The completion failed.";
TAtpNormalForm::nosys = "`1` is not a live TAtpSystem.";
TAtpNormalForm::fail = "Normalizing under the system failed.";

$atpNextRunFn := $atpNextRunFn = load["thvm_wl_atp_next_run", {Integer, Integer}, Integer];
$atpSystemQFn := $atpSystemQFn = load["thvm_wl_atp_system_q", {Integer}, Integer];
$atpSystemNormalizeFn := $atpSystemNormalizeFn = load[
    "thvm_wl_atp_system_normalize", {Integer, {"NumericArray", "Constant"}}, "NumericArray"];

(* Spare constant labels, ranked below every symbol of the axioms in
   increasing order.  A query's own symbols and numbers take them in
   canonical order, so they need no new labels and compare canonically. *)
$atpSystemSpares = Table["AtpSystemQuery" <> ToString[k], {k, 256}];

(* Allocation budget in heap cells: 2^24 cells (134 MB) is about a second
   of completion (the group axioms allocate 7.3M cells in 2000 steps,
   Sheffer's axiom 17.8M in 160).  The run ends cleanly when it is spent,
   and unlike a time limit it ends at the same point on every machine. *)
$atpSystemAllocation = 2^24;

TAtpComplete[theory_String, rest___] := Block[{ax = atpAxiomaticTheory[theory]},
    If[ ListQ[ax],
        TAtpComplete[ax, rest],
        Message[TKnuthBendixCompletion::theory, theory]; $Failed]];
TAtpComplete[ax : (_Equal | _ForAll | _TwoWayRule | Inactive[Equal][_, _]), rest___] :=
    TAtpComplete[{ax}, rest];
TAtpComplete[axioms_List, opts : OptionsPattern[]] := atpSystemComplete[axioms, Automatic, opts];
TAtpComplete[axioms_List, order : _List | Automatic, opts : OptionsPattern[]] :=
    atpSystemComplete[axioms, order, opts];

atpSystemComplete[axiomsIn_List, order_, opts___] := Catch[
    Module[{method, maxSteps, wall, alloc, axioms, enc, st, handle, cRes, status},
        If[ order =!= Automatic && ! kbcOrderQ[order],
            Message[TKnuthBendixCompletion::badord, order];
            Throw[$Failed, "TATPError"]];
        {method, maxSteps, wall, alloc} = OptionValue[TAtpComplete, {opts},
            {Method, MaxSteps, TimeConstraint, "AllocationLimit"}];
        wall = Replace[kbcSeconds[wall], Infinity -> 0.];
        alloc = Replace[alloc, {Automatic -> $atpSystemAllocation, Infinity | None -> 0}];
        axioms = atpNormalizeAxioms[axiomsIn];
        atpEnsureWmHeap[method];
        method = kbcMethod[method, order, kbcAxiomSides[axioms], None];
        method = atpSystemAddSpares[method];
        enc = Block[{$atpWmIntakeApply = atpWmIntakeOrderQ[method]},
            With[{ax = axioms}, atpEncodeProblem[ax, None, False]]];
        st = Fold[Last[ensureSym[#2, #1]] &, enc["State"], $atpSystemSpares];
        enc = Join[enc, <|"State" -> st, "MaxLab" -> st["next_lab"]|>];
        handle = CreateManagedLibraryExpression["AtpSystem", atpSystemHandle];
        $atpNextRunFn[ManagedLibraryExpressionID[handle], alloc];
        cRes = cEngineProof[enc, atpResolveMaxSteps[maxSteps, method], wall,
            Sequence @@ atpParseMethod[method]];
        If[ ! IntegerQ[cRes["Status"]] || $atpSystemQFn[ManagedLibraryExpressionID[handle]] =!= 1,
            Message[TAtpComplete::fail]; Throw[$Failed, "TATPError"]];
        status = cRes["Status"];
        TAtpSystem[<|
            "Handle" -> handle,
            "Rules" -> kbcTheorems[cRes],
            "Ordering" -> atpSystemOrdering[method],
            "Status" -> Switch[status, 4, "Saturated", 3, "BudgetExhausted", _, "Stopped"],
            "Complete" -> (status === 4 && kbcCompleteQ[method]),
            "State" -> st
        |>]
    ],
    "TATPError"
];

(* The precedence of the completion with the spare labels at its bottom,
   the first spare lowest. *)
atpSystemAddSpares[m_List] := With[{o = Association[Rest[m]]},
    If[ ListQ[Lookup[o, "Precedence"]],
        Join[{First[m]}, Normal[Append[o, "Precedence" -> Join[o["Precedence"], Reverse[$atpSystemSpares]]]]],
        m]];

atpSystemOrdering[m_List] := With[{o = Association[Rest[m]]},
    <|"Ordering" -> Lookup[o, "Ordering", "KBO"],
      "Precedence" -> DeleteCases[Lookup[o, "Precedence", Automatic], Alternatives @@ Append[$atpSystemSpares, $kbcGoalConstant]],
      "SymbolWeights" -> Lookup[o, "SymbolWeights", None]|>];

TAtpSystem[a_Association][key_String] := Lookup[a, key, Missing["NotAvailable", key]];

TAtpSystem /: MakeBoxes[sys : TAtpSystem[a_Association], fmt_] := BoxForm`ArrangeSummaryBox[
    "TAtpSystem", sys, None,
    {
        {BoxForm`SummaryItem[{"Status: ", a["Status"]}]},
        {BoxForm`SummaryItem[{"Rules: ", Length[a["Rules"]]}]}
    },
    {
        {BoxForm`SummaryItem[{"Complete: ", a["Complete"]}]},
        {BoxForm`SummaryItem[{"Ordering: ", a["Ordering"]["Ordering"]}]},
        {BoxForm`SummaryItem[{"Precedence: ", Short[a["Ordering"]["Precedence"], 1]}]}
    },
    fmt, "Interpretable" -> Automatic];

atpSystemLiveQ[TAtpSystem[a_Association]] :=
    ManagedLibraryExpressionQ[a["Handle"]] &&
    $atpSystemQFn[ManagedLibraryExpressionID[a["Handle"]]] === 1;
atpSystemLiveQ[_] := False;

TAtpNormalForm[sys_TAtpSystem, terms_List] := Catch[atpSystemNormalize[sys, terms], "TATPError"];
TAtpNormalForm[sys_TAtpSystem, term_] := Replace[TAtpNormalForm[sys, {term}], {{t_} :> t}];
TAtpNormalForm[sys_, _] := (Message[TAtpNormalForm::nosys, sys]; $Failed);

(* Map the query's symbols and numbers outside the system's signature onto
   the spare labels (in canonical order), encode against the system's
   encoder state, normalize in the engine, and decode back to the caller's
   own symbols. *)
atpSystemNormalize[sys : TAtpSystem[a_Association], terms_List] := Module[
    {st = a["State"], known, foreign, spares, enc, raws, out, lab2name, symObj},
    If[ ! atpSystemLiveQ[sys], Message[TAtpNormalForm::nosys, sys]; Throw[$Failed, "TATPError"]];
    known = st["sym"];
    foreign = SortBy[Select[kbcSignature[terms], ! KeyExistsQ[known, #[[1]]] &], Last];
    If[ Length[foreign] > Length[$atpSystemSpares], Message[TAtpNormalForm::fail]; Throw[$Failed, "TATPError"]];
    spares = Take[$atpSystemSpares, Length[foreign]];
    enc = Fold[
        {acc, t} |-> With[{r = encodeAtpTerm[t, Last[acc]]}, {Append[First[acc], First[r]], Last[r]}],
        {{}, st},
        terms /. Thread[foreign[[All, 3]] -> spares]];
    raws = Quiet @ Check[Normal @ $atpSystemNormalizeFn[ManagedLibraryExpressionID[a["Handle"]],
        NumericArray[First[enc], "Integer64"]], $Failed];
    If[ ! ListQ[raws], Message[TAtpNormalForm::fail]; Throw[$Failed, "TATPError"]];
    lab2name = Association[Reverse /@ Normal[st["sym"]]];
    symObj = Join[Lookup[st, "symObj", <||>],
        AssociationThread[Lookup[known, spares], Hold /@ foreign[[All, 3]]]];
    Block[{$atpSymObj = symObj, $atpVarObj = Lookup[st, "varObj", <||>]},
        decodeAtpTerm[#, lab2name, Association[Reverse /@ Normal[st["var"]]]] & /@ raws]
];

TAtpEqualQ[sys_TAtpSystem, x_, y_] := Replace[TAtpNormalForm[sys, {x, y}], {
    {nx_, ny_} /; nx === ny :> True,
    {nx_, ny_} /; TrueQ[sys["Complete"]] :> Failure["NotEntailed", <|
        "MessageTemplate" -> "The axioms do not entail `1` == `2`: their normal forms differ under a complete system.",
        "MessageParameters" -> {x, y}, "NormalForms" -> {nx, ny}|>],
    {_, _} :> Indeterminate,
    _ :> $Failed}];

End[]

EndPackage[]
