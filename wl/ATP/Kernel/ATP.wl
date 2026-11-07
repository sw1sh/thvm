(* ::Package:: *)
(* WolframInstitute`ATP` - completed equational systems from thvm's prover,
   for simplifiers that ask many normal-form questions of one theory.

   TAtpComplete completes the axioms once by unfailing Knuth-Bendix
   completion (Waldmeister's configuration) and keeps the engine state;
   TAtpNormalForm and TAtpEqualQ answer from it by ordered rewriting without
   completing again.  The same functions as THVMLink's, standalone: this
   package needs only its library, libthvm_atp, found next to it under
   LibraryResources/$SystemID, and loads beside THVMLink in one kernel. *)

BeginPackage["WolframInstitute`ATP`"];

TAtpComplete::usage = "TAtpComplete[axioms] completes the equations axioms by unfailing Knuth-Bendix completion and gives a TAtpSystem that keeps the completed system in the engine.
TAtpComplete[axioms, order] completes under the ordering order: a precedence list {s1, s2, ...} gives the lexicographic path ordering, a list {{s1, w1}, ...} the Knuth-Bendix ordering with weights wi.
Options: MaxSteps (default 2000), \"AllocationLimit\" (heap cells the completion may allocate; Automatic is 2^24), TimeConstraint (default Infinity).";
TAtpSystem::usage = "TAtpSystem[...] is a completed equational system kept in the engine, from TAtpComplete.
sys[\"Rules\"] gives its rules and unorientable equations, sys[\"Ordering\"] the reduction ordering, sys[\"Status\"] \"Saturated\" or \"BudgetExhausted\", sys[\"Complete\"] whether it is ground convergent.";
TAtpNormalForm::usage = "TAtpNormalForm[sys, term] gives the normal form of term under the TAtpSystem sys.
TAtpNormalForm[sys, {term1, term2, ...}] gives the normal forms of several terms in one call.";
TAtpEqualQ::usage = "TAtpEqualQ[sys, a, b] gives True when a and b have the same normal form under the TAtpSystem sys, so the axioms entail a == b.
It gives Failure[\"NotEntailed\", ...] when sys is complete and the normal forms differ, and Indeterminate when the completion stopped at its budget.";

Begin["`Private`"];

(* === library === *)

$atpLib = FileNameJoin[{DirectoryName[$InputFileName], "..", "LibraryResources", $SystemID,
    "libthvm_atp" <> Switch[$OperatingSystem, "MacOSX", ".dylib", "Windows", ".dll", _, ".so"]}];

load[name_String, args_, ret_] := LibraryFunctionLoad[$atpLib, name, args, ret];

$initFn := $initFn = load["thvm_atp_lib_init", {}, Integer];
$fromPrefixFn := $fromPrefixFn = load["thvm_wl_atp_terms_from_prefix", {{"NumericArray", "Constant"}}, "NumericArray"];
$toPrefixFn := $toPrefixFn = load["thvm_wl_atp_terms_to_prefix", {{"NumericArray", "Constant"}}, "NumericArray"];
$intakeFn := $intakeFn = load["thvm_wl_atp_wm_intake_order", {{"NumericArray", "Shared"}}, "NumericArray"];
$nextRunFn := $nextRunFn = load["thvm_wl_atp_next_run", {Integer, Integer}, Integer];
$systemQFn := $systemQFn = load["thvm_wl_atp_system_q", {Integer}, Integer];
$normalizeFn := $normalizeFn = load["thvm_wl_atp_system_normalize", {Integer, {"NumericArray", "Constant"}}, "NumericArray"];
$runProofFn := $runProofFn = load["thvm_wl_atp_run_proof",
    Join[{{"NumericArray", "Shared"}, Integer, Integer, Real}, ConstantArray[Integer, 13], {{Integer, 1}},
        ConstantArray[Integer, 6], {{Integer, 1}}, ConstantArray[Integer, 40], {{Integer, 1}}],
    "NumericArray"];

(* The heap size of THVMLink's Waldmeister runs, so both give the same
   completions (collections move cells, and some caches are keyed by them). *)
$atpInitialized = False;
ensureInit[] := If[ ! $atpInitialized,
    If[ MatchQ[Environment["THVM_HEAP_CELLS"], $Failed | ""], SetEnvironment["THVM_HEAP_CELLS" -> "536870912"]];
    $initFn[];
    $atpInitialized = True];

(* thvm_wl_atp_run_proof's arguments for TKnuthBendixCompletion's
   configuration: Method "Waldmeister" with "GroundJoin" -> True, an explicit
   precedence, no AutoPrecedence or SkolemHighest.  Captured from THVMLink's
   cEngineProof; the slots named here vary per call. *)
$atpRunTemplate = {"WIRE", "STEPS", "MAXLAB", "WALL", 4, "ORDERING", 0, 0, 0, 0, 1, 51, 0, 1, 1, 0, 0,
    "PRECEDENCE", 0, 0, 0, 0, 0, 0, "WEIGHTS", 0, 0, 0, 0, 0, -1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, -1, -1, -1, 0, -1, -1, -1, {}};

$atpTagFVR = 22;

(* === encoding: WL terms to prefix form ===
   A compound term is its label, its arity, then its arguments; variable k is
   -(k + 1).  Labels 1 and 2 are the engine's reserved constants.  A symbol
   is named the way THVMLink's encoder names it, so both build the same
   signature. *)

atpName[s_Symbol] := ToString[Unevaluated[s]];
atpName[n : (_Integer | _Real | _Rational)] := ToString[n, InputForm];
atpName[s_String] := s;
atpName[e_] := ToString[e];
SetAttributes[atpName, HoldFirst];

newState[] := <|"sym" -> <|"cAtp1" -> 1, "cAtp2" -> 2|>, "obj" -> <||>, "var" -> <||>|>;

symLabel[name_String, obj_, st_] := If[ KeyExistsQ[st["sym"], name],
    {st["sym"][name], st},
    With[{lab = Length[st["sym"]] + 1},
        {lab, Join[st, <|"sym" -> Append[st["sym"], name -> lab], "obj" -> Append[st["obj"], lab -> obj]|>]}]];

(* encode[expr, st, alias] -> {prefix, st}; alias maps a query's own atoms
   and heads to spare labels. *)
encode[Verbatim[Pattern][v_Symbol, Blank[]], st_, _] := Block[{name = SymbolName[Unevaluated[v]], vars = st["var"], k},
    k = Lookup[vars, name, Length[vars]];
    {{-k - 1}, If[ KeyExistsQ[vars, name], st, Append[st, "var" -> Append[vars, name -> k]]]}];
encode[e_ ? AtomQ, st_, alias_] := If[ KeyExistsQ[alias, e],
    {{alias[e], 0}, st},
    With[{r = symLabel[atpName[e], Hold[e], st]}, {{r[[1]], 0}, r[[2]]}]];
encode[e_, st_, alias_] := With[{hd = Head[e]}, Block[{lab, s = st, args = {}, r},
    If[ KeyExistsQ[alias, hd],
        lab = alias[hd],
        {lab, s} = symLabel[atpName[hd], Hold[hd], s]];
    Do[{r, s} = encode[a, s, alias]; args = Join[args, r], {a, List @@ e}];
    {Join[{lab, Length[e]}, args], s}]];

encodeAll[terms_List, st_, alias_] := Fold[
    {acc, t} |-> With[{r = encode[t, acc[[2]], alias]}, {Join[acc[[1]], r[[1]]], r[[2]]}],
    {{}, st}, terms];

toTerms[prefix_List, n_Integer] := Normal @ $fromPrefixFn[NumericArray[Prepend[prefix, n], "Integer64"]];

(* === decoding: prefix form to WL terms === *)

decodeAll[terms_List, labToObj_, varFn_] := Block[{d = Normal @ $toPrefixFn[NumericArray[terms, "Integer64"]], pos = 2},
    Table[decodeOne[d, labToObj, varFn], {d[[1]]}]];
decodeOne[d_, labToObj_, varFn_] := Block[{v = d[[pos++]], ar},
    If[ v < 0,
        varFn[-v - 1],
        ar = d[[pos++]];
        If[ ar === 0,
            ReleaseHold[labToObj[v]],
            ReleaseHold[labToObj[v]] @@ Table[decodeOne[d, labToObj, varFn], {ar}]]]];

(* === axioms === *)

(* The two sides of an equation, its ForAll-bound symbols as patterns. *)
axiomSides[HoldPattern[ForAll[v_, body_]]] := With[{vs = Flatten[{v}]},
    axiomSides[body /. Thread[vs -> (Pattern[#, Blank[]] & /@ vs)]]];
axiomSides[(Equal | Inactive[Equal])[l_, r_]] := {l, r};
axiomSides[e_] := Throw[Failure["BadAxiom", <|"MessageTemplate" -> "`1` is not an equation.", "MessageParameters" -> {e}|>], "WolframInstitute`ATP`"];

(* Waldmeister's intake canonicalization (thvm's SpezNormierung port): the
   axioms in Waldmeister's order with its side swaps, as THVMLink's encoder
   applies them for the Waldmeister configuration. *)
intakeOrder[sides_List] := Block[{st = newState[], pre, terms, wire, r, n = Length[sides], order, swaps},
    {pre, st} = encodeAll[Flatten[sides, 1], st, <||>];
    terms = toTerms[pre, 2 n];
    wire = NumericArray[Join[{0, n}, terms, ConstantArray[0, n]], "Integer64"];
    r = Quiet @ Check[Normal @ $intakeFn[wire], $Failed];
    If[ ! ListQ[r] || Length[r] =!= 1 + 2 n || r[[1]] =!= n, Return[sides]];
    order = r[[2 ;; n + 1]] + 1;
    swaps = r[[n + 2 ;;]];
    MapThread[If[#2 === 1, Reverse[#1], #1] &, {sides, swaps}][[order]]];

(* === ordering === *)

(* {name, arity, atom-or-head} of every symbol of the terms, in order of
   first appearance. *)
signature[terms_List] := DeleteDuplicatesBy[Flatten[Reap[Scan[sow, terms]][[2]], 1], First];
sow[Verbatim[Pattern][_, _]] := Null;
sow[e_ ? AtomQ] := Sow[{atpName[e], 0, e}];
sow[e_] := (Sow[{ToString[Head[e]], Length[e], Head[e]}]; Scan[sow, e]);

(* The precedence, highest first, after W15's ELSimplify: the axioms'
   function symbols by descending arity (ties in order of appearance), then
   their constants.  The spare query labels follow, below all of them. *)
defaultPrecedence[sides_List] := Block[{sig = signature[Flatten[sides, 1]], fs},
    fs = Select[sig, #[[2]] > 0 &];
    fs = fs[[Ordering[Transpose[{-fs[[All, 2]], Range[Length[fs]]}]]]];
    Join[fs[[All, 1]], Select[sig, #[[2]] === 0 &][[All, 1]]]];

$spares = Table["ThvmAtpQuery" <> ToString[k], {k, 256}];

orderValidQ[order_List] := AllTrue[order, MatchQ[{_, _Integer ? NonNegative} | Except[_List]]];
orderValidQ[Automatic] := True;
orderValidQ[_] := False;

(* {"KBO" | "LPO", precedence names, weights rules} for an order argument. *)
orderSpec[sides_, Automatic | {}] := {"KBO", defaultPrecedence[sides], {}};
orderSpec[sides_, order_List] := Block[{names, weights},
    names = atpName /@ Replace[order, {s_, _Integer} :> s, {1}];
    weights = Cases[order, {s_, w_Integer} :> atpName[s] -> w];
    {If[ weights === {}, "LPO", "KBO"],
     Join[names, Select[defaultPrecedence[sides], ! MemberQ[names, #] &]], weights}];

(* Label-indexed precedence ranks (first name highest, unlisted 0) and KBO
   weights (-1 = the default weight 1). *)
rankArray[names_, st_, maxLab_] := Block[{arr = ConstantArray[0, maxLab + 1], n = Length[names]},
    MapIndexed[If[ KeyExistsQ[st["sym"], #1], arr[[st["sym"][#1] + 1]] = n + 1 - First[#2]] &, names];
    arr];
weightArray[{}, _, _] := {};
weightArray[weights_, st_, maxLab_] := Block[{arr = ConstantArray[-1, maxLab + 1]},
    Scan[If[ KeyExistsQ[st["sym"], First[#]], arr[[st["sym"][First[#]] + 1]] = Last[#]] &, weights];
    arr];

(* === completion === *)

Options[TAtpComplete] = {MaxSteps -> 2000, "AllocationLimit" -> Automatic, TimeConstraint -> Infinity};

TAtpComplete::badord = "`1` is not an ordering: give a precedence {s1, s2, ...} or symbol weights {{s1, w1}, {s2, w2}, ...}.";
TAtpComplete::fail = "The completion failed.";
TAtpComplete::theory = "AxiomaticTheory[\"`1`\"] does not give a list of axioms.";

TAtpComplete[theory_String, rest___] := With[{ax = AxiomaticTheory[theory]},
    If[ ListQ[ax], TAtpComplete[ax, rest], Message[TAtpComplete::theory, theory]; $Failed]];
TAtpComplete[ax : (_Equal | _ForAll | Inactive[Equal][_, _]), rest___] := TAtpComplete[{ax}, rest];
TAtpComplete[axioms_List, opts : OptionsPattern[]] := complete[axioms, Automatic, opts];
TAtpComplete[axioms_List, order : _List | Automatic, opts : OptionsPattern[]] := complete[axioms, order, opts];

(* Everything a completion run needs, from the axioms, order and options:
   the axioms' prefix form (Waldmeister's intake order), the encoder state
   with the spare query labels, and thvm_wl_atp_run_proof's arguments with
   the wire left out.  TAtpComplete and the determinism dump share it. *)
prepare[axioms_List, order_, opts___] := Module[{steps, alloc, wall, sides, spec, ordered, st, pre, n, maxLab},
    If[ ! orderValidQ[order], Message[TAtpComplete::badord, order]; Throw[$Failed, "WolframInstitute`ATP`"]];
    {steps, alloc, wall} = OptionValue[TAtpComplete, {opts}, {MaxSteps, "AllocationLimit", TimeConstraint}];
    alloc = Replace[alloc, {Automatic -> 2^24, Infinity | None -> 0}];
    wall = Replace[wall, {Infinity -> 0., q_Quantity :> N @ QuantityMagnitude[q, "Seconds"], t_ ? NumericQ :> N[t]}];
    ensureInit[];
    sides = axiomSides /@ axioms;
    spec = orderSpec[sides, order];
    ordered = intakeOrder[sides];
    n = Length[ordered];
    {pre, st} = encodeAll[Flatten[ordered, 1], newState[], <||>];
    st = Fold[Last[symLabel[#2, Hold[#2], #1]] &, st, $spares];
    maxLab = Length[st["sym"]] + 1;
    <|"Prefix" -> pre, "Count" -> n, "State" -> st, "Spec" -> spec, "Allocation" -> alloc,
      "Arguments" -> ReplacePart[$atpRunTemplate, {
          2 -> steps, 3 -> maxLab, 4 -> wall,
          6 -> If[ spec[[1]] === "LPO", 1, 0],
          18 -> rankArray[Join[spec[[2]], Reverse[$spares]], st, maxLab],
          25 -> weightArray[spec[[3]], st, maxLab]}]|>
];

complete[axioms_List, order_, opts___] := Catch[
    Module[{prep, terms, n, wire, handle, raw, status, nRules, st, spec},
        prep = prepare[axioms, order, opts];
        {n, st, spec} = Lookup[prep, {"Count", "State", "Spec"}];
        terms = toTerms[prep["Prefix"], 2 n];
        wire = NumericArray[Join[{0, n}, terms, ConstantArray[0, n]], "Integer64"];
        handle = CreateManagedLibraryExpression["ThvmAtpSystem", atpSystemHandle];
        $nextRunFn[ManagedLibraryExpressionID[handle], prep["Allocation"]];
        raw = Quiet @ Check[Normal[$runProofFn @@ ReplacePart[prep["Arguments"], 1 -> wire]], $Failed];
        If[ ! ListQ[raw] || $systemQFn[ManagedLibraryExpressionID[handle]] =!= 1,
            Message[TAtpComplete::fail]; Throw[$Failed, "WolframInstitute`ATP`"]];
        status = raw[[1]];
        nRules = raw[[2]];
        TAtpSystem[<|
            "Handle" -> handle,
            "Rules" -> If[ nRules === 0, {}, theorems[raw[[9 ;; 8 + 2 nRules]], raw[[-nRules ;;]], st]],
            "Ordering" -> <|"Ordering" -> spec[[1]], "Precedence" -> spec[[2]],
                "SymbolWeights" -> If[ spec[[3]] === {}, None, spec[[3]]]|>,
            "Status" -> Switch[status, 4, "Saturated", 3, "BudgetExhausted", _, "Stopped"],
            "Complete" -> (status === 4),
            "State" -> st
        |>]
    ],
    "WolframInstitute`ATP`"
];

(* The completed rules: kind 1 an oriented rule l -> r, 0 an unorientable
   equation l <-> r, -1 an empty slot; variables renamed to formal letters
   in order of appearance. *)
theorems[ruleTerms_List, kinds_List, st_] := Block[{labToObj = Association[st["obj"]], pairs},
    If[ ruleTerms === {}, Return[{}]];
    pairs = Partition[decodeAll[ruleTerms, labToObj, var], 2];
    DeleteCases[MapThread[theorem, {pairs, kinds}], None]];
theorem[{l_, r_}, kind : 0 | 1] := Block[{vs, fs},
    vs = DeleteDuplicates @ Cases[{l, r}, var[_], {0, Infinity}];
    fs = formal[Length[vs]];
    If[ kind === 1,
        Rule[l /. Thread[vs -> (Pattern[#, Blank[]] & /@ fs)], r /. Thread[vs -> fs]],
        TwoWayRule @@ ({l, r} /. Thread[vs -> (Pattern[#, Blank[]] & /@ fs)])]];
theorem[_, _] := None;

formal[n_] := Take[Join[
    Symbol /@ CharacterRange["\[FormalA]", "\[FormalZ]"],
    Symbol /@ CharacterRange["\[FormalCapitalA]", "\[FormalCapitalZ]"],
    Table[Symbol["\[FormalX]" <> ToString[k]], {k, Max[n - 52, 0]}]], n];

(* === the system === *)

TAtpSystem[a_Association][key_String] := Lookup[a, key, Missing["NotAvailable", key]];

TAtpSystem /: MakeBoxes[sys : TAtpSystem[a_Association], fmt_] := BoxForm`ArrangeSummaryBox[
    "TAtpSystem", sys, None,
    {{BoxForm`SummaryItem[{"Status: ", a["Status"]}]}, {BoxForm`SummaryItem[{"Rules: ", Length[a["Rules"]]}]}},
    {{BoxForm`SummaryItem[{"Complete: ", a["Complete"]}]},
     {BoxForm`SummaryItem[{"Ordering: ", a["Ordering"]["Ordering"]}]},
     {BoxForm`SummaryItem[{"Precedence: ", Short[a["Ordering"]["Precedence"], 1]}]}},
    fmt, "Interpretable" -> Automatic];

liveQ[TAtpSystem[a_Association]] := ManagedLibraryExpressionQ[a["Handle"]] &&
    $systemQFn[ManagedLibraryExpressionID[a["Handle"]]] === 1;
liveQ[_] := False;

TAtpNormalForm::nosys = "`1` is not a live TAtpSystem.";
TAtpNormalForm::fail = "Normalizing under the system failed.";

TAtpNormalForm[sys_TAtpSystem, terms_List] := Catch[normalize[sys, terms], "WolframInstitute`ATP`"];
TAtpNormalForm[sys_TAtpSystem, term_] := Replace[TAtpNormalForm[sys, {term}], {{t_} :> t}];
TAtpNormalForm[sys_, _] := (Message[TAtpNormalForm::nosys, sys]; $Failed);

(* A query's symbols and numbers outside the axioms take the spare labels,
   ranked below every axiom symbol, in canonical order; so a normal form does
   not depend on how the query is written, and decodes to the caller's own
   symbols. *)
normalize[sys : TAtpSystem[a_Association], terms_List] := Block[{q, raw},
    If[ ! liveQ[sys], Message[TAtpNormalForm::nosys, sys]; Throw[$Failed, "WolframInstitute`ATP`"]];
    q = encodeQuery[a["State"], terms];
    raw = Quiet @ Check[Normal @ $normalizeFn[ManagedLibraryExpressionID[a["Handle"]],
        NumericArray[toTerms[q["Prefix"], Length[terms]], "Integer64"]], $Failed];
    If[ ! ListQ[raw], Message[TAtpNormalForm::fail]; Throw[$Failed, "WolframInstitute`ATP`"]];
    decodeAll[raw, q["Objects"], Pattern[Symbol["x" <> ToString[#]], Blank[]] &]];

(* The query's prefix form against the system's encoder state, its own atoms
   on the spare labels, and the label -> original table to decode with. *)
encodeQuery[st_, terms_List] := Block[{foreign, alias},
    foreign = SortBy[Select[signature[terms], ! KeyExistsQ[st["sym"], #[[1]]] &], Last][[All, 3]];
    If[ Length[foreign] > Length[$spares], Message[TAtpNormalForm::fail]; Throw[$Failed, "WolframInstitute`ATP`"]];
    alias = AssociationThread[foreign, Lookup[st["sym"], Take[$spares, Length[foreign]]]];
    <|"Prefix" -> encodeAll[terms, st, alias][[1]],
      "Objects" -> Join[Association[st["obj"]], AssociationThread[Values[alias], Hold /@ foreign]]|>];

(* One case of the determinism harness (wl/ATP/CSource/determinism.c): the
   exact run arguments and query prefixes the package would send, as text. *)
determinismCase[name_String, axioms_List, order_, queries_List, opts___] := Catch[
    Module[{prep = prepare[axioms, order, opts], q, tok},
        q = encodeQuery[prep["State"], queries];
        tok[l_List] := StringRiffle[Join[{"A", ToString[Length[l]]}, ToString /@ l], " "];
        tok[r_Real] := ToString[NumberForm[r, {16, 6}]];
        tok[x_] := ToString[x];
        StringRiffle[{
            "case " <> name,
            "alloc " <> ToString[prep["Allocation"]],
            "args " <> StringRiffle[tok /@ ReplacePart[prep["Arguments"], 1 -> "W"], " "],
            "axioms " <> ToString[prep["Count"]] <> " " <> ToString[Length[prep["Prefix"]]] <> " " <> StringRiffle[ToString /@ prep["Prefix"], " "],
            "queries " <> ToString[Length[queries]] <> " " <> ToString[Length[q["Prefix"]]] <> " " <> StringRiffle[ToString /@ q["Prefix"], " "],
            "end"}, "\n"]
    ],
    "WolframInstitute`ATP`"];

TAtpEqualQ[sys_TAtpSystem, x_, y_] := Replace[TAtpNormalForm[sys, {x, y}], {
    {nx_, ny_} /; nx === ny :> True,
    {nx_, ny_} /; TrueQ[sys["Complete"]] :> Failure["NotEntailed", <|
        "MessageTemplate" -> "The axioms do not entail `1` == `2`: their normal forms differ under a complete system.",
        "MessageParameters" -> {x, y}, "NormalForms" -> {nx, ny}|>],
    {_, _} :> Indeterminate,
    _ :> $Failed}];

End[];

EndPackage[];
