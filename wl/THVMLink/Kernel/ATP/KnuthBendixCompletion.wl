(* ::Package:: *)
(* KnuthBendixCompletion.wl - TKnuthBendixCompletion: Waldmeister's unfailing
   Knuth-Bendix completion as one function, after the
   UnfailingKnuthBendixCompletion Function Repository prototype.  The axioms
   complete to oriented rules and unorientable equations; a term comes back as
   its normal form under them, the way Waldmeister's simplify mode runs the
   goal t == C with C a fresh constant and reads off t's normal form. *)

BeginPackage["WolframInstitute`THVMLink`ATP`", {"GeneralUtilities`", "WolframInstitute`THVMLink`", "Wolfram`Parser`"}]

SetUsage[TKnuthBendixCompletion, "TKnuthBendixCompletion[axioms$] completes the equations axioms$ by unfailing Knuth-Bendix completion and returns a ProofObject whose \"Theorems\" are the completed rules.
TKnuthBendixCompletion[axioms$, term$] gives the normal form of term$ under the completed rules.
TKnuthBendixCompletion[axioms$, {s$1, s$2, $$}] completes under the lexicographic path ordering with precedence s$1 > s$2 > $$.
TKnuthBendixCompletion[axioms$, {{s$1, w$1}, {s$2, w$2}, $$}] completes under the Knuth-Bendix ordering with symbol weights w$i and precedence s$1 > s$2 > $$.
TKnuthBendixCompletion[axioms$, term$, order$] gives the normal form of term$ under the ordering order$.
TKnuthBendixCompletion[\"Theory\", $$] completes the axioms of AxiomaticTheory[\"Theory\"].
A rule l$ -> r$ rewrites left to right; an unorientable equation l$ <-> r$ rewrites whichever way decreases the ordering.
Options: TimeConstraint (default 1 second), MaxSteps, Method (default \"Waldmeister\"; accepts every TFindProof Method)."];

Begin["`Private`"]

Options[TKnuthBendixCompletion] = {MaxSteps -> Automatic, Method -> "Waldmeister", TimeConstraint -> 1};

TKnuthBendixCompletion::badord = "`1` is not an ordering: give a precedence {s1, s2, ...} or symbol weights {{s1, w1}, {s2, w2}, ...}.";
TKnuthBendixCompletion::theory = "AxiomaticTheory[\"`1`\"] does not give a list of axioms.";
TKnuthBendixCompletion::heap = "Completion ran out of memory; the normal form comes from a completion of at most `1` steps.";
TKnuthBendixCompletion::nodata = "Deriving the `1` completed rules takes longer than the TimeConstraint, so the ProofObject's proof lists only the axioms.";

(* The fresh constant c of the goal t == c.  It ranks highest in the
   precedence, so no normal form ever prefers a term holding it. *)
$kbcGoalConstant = "KnuthBendixCompletionGoal";

TKnuthBendixCompletion[theory_String, rest___] := Block[{ax = atpAxiomaticTheory[theory]},
    If[ ListQ[ax],
        TKnuthBendixCompletion[ax, rest],
        Message[TKnuthBendixCompletion::theory, theory]; $Failed]];

TKnuthBendixCompletion[ax : (_Equal | _ForAll | _TwoWayRule | Inactive[Equal][_, _]), rest___] :=
    TKnuthBendixCompletion[{ax}, rest];

TKnuthBendixCompletion[axioms_List, opts : OptionsPattern[]] :=
    kbcComplete[axioms, None, Automatic, opts];
TKnuthBendixCompletion[axioms_List, order : _List | Automatic, opts : OptionsPattern[]] :=
    kbcComplete[axioms, None, order, opts];
TKnuthBendixCompletion[axioms_List, term : Except[_List | Automatic | _Rule | _RuleDelayed], opts : OptionsPattern[]] :=
    kbcComplete[axioms, term, Automatic, opts];
TKnuthBendixCompletion[axioms_List, term : Except[_List | Automatic | _Rule | _RuleDelayed], order : _List | Automatic, opts : OptionsPattern[]] :=
    kbcComplete[axioms, term, order, opts];

kbcComplete[axiomsIn_List, term_, order_, opts___] := Catch[
    Module[{method, maxSteps, wall, axioms, goal, cRes, steps, nf},
        If[ order =!= Automatic && ! kbcOrderQ[order],
            Message[TKnuthBendixCompletion::badord, order];
            Throw[$Failed, "TATPError"]];
        {method, maxSteps, wall} = OptionValue[TKnuthBendixCompletion, {opts}, {Method, MaxSteps, TimeConstraint}];
        wall = Replace[kbcSeconds[wall], Infinity -> 0.];
        axioms = atpNormalizeAxioms[axiomsIn];
        atpEnsureWmHeap[method];
        goal = Replace[term, Equal[l_, r_] :> kbcEqual[l, r]];
        method = kbcMethod[method, order, kbcAxiomSides[axioms], goal];
        steps = atpResolveMaxSteps[maxSteps, method];
        cRes = kbcRun[axioms, goal, method, steps, wall];
        (* A normal form needs only a sound system, not a complete one: when
           completion runs out of heap, a smaller one still gives it. *)
        If[ goal =!= None && ! IntegerQ[cRes["Status"]],
            steps = Min[steps, 1024];
            While[ ! IntegerQ[cRes["Status"]] && steps > 1,
                steps = Max[1, Quotient[steps, 4]];
                cRes = kbcRun[axioms, goal, method, steps, wall]];
            If[ IntegerQ[cRes["Status"]], Message[TKnuthBendixCompletion::heap, steps]]];
        If[ ! IntegerQ[cRes["Status"]], Throw[$Failed, "TATPError"]];
        If[ goal === None,
            kbcProofObject[cRes["Encoding"], cRes, wall],
            nf = Replace[cRes["GoalNormalForms"], {{{n_, _}, ___} :> n, _ :> Throw[$Failed, "TATPError"]}];
            (* An equation holds when both sides reach one normal form, and fails
               when they reach two under a saturated system: its ordering is
               total on ground terms, so the system is ground convergent. *)
            Replace[nf, kbcEqual[l_, r_] :> Which[
                l === r, True,
                cRes["Status"] === 4 && kbcCompleteQ[method], False,
                NumericQ[l] && NumericQ[r], Inactive[Equal][l, r],
                True, l == r]]]
    ],
    "TATPError"
];

(* One completion run of the axioms, with the goal goal == c when goal is not
   None.  The problem is encoded afresh each time: a run that runs out of heap
   collects it, which moves the cells of the previous encoding. *)
kbcRun[axioms_, goal_, method_, steps_, wall_] := Block[{enc, cRes},
    enc = Block[{$atpWmIntakeApply = atpWmIntakeOrderQ[method]},
        With[{ax = axioms, t = goal, c = $kbcGoalConstant},
            If[ goal === None,
                atpEncodeProblem[ax, None, False],
                atpEncodeProblem[ax, t == c, False]]]];
    cRes = cEngineProof[enc, steps, wall, Sequence @@ atpParseMethod[method]];
    If[ AssociationQ[cRes], Append[cRes, "Encoding" -> enc], <|"Status" -> $Failed|>]
];

(* Whether a run that ends with an empty queue (status 4) has saturated: no
   critical pair was dropped for its size, a queue cap or a resource limit. *)
kbcCompleteQ[m_List] := With[{o = Join[Lookup[$AtpPresetDefaults, First[m], <||>], Association[Rest[m]]]},
    ! (TrueQ[Lookup[o, "LRS"]] || TrueQ[Lookup[o, "MaxWeight", 0] > 0] ||
       TrueQ[Lookup[o, "AutoMaxWeight", 0] > 0] || KeyExistsQ[o, "QueueCap"])];

kbcSeconds[q_Quantity] := QuantityMagnitude[q, "Seconds"];
kbcSeconds[Infinity] := Infinity;
kbcSeconds[t_ ? NumericQ] /; t > 0 := N[t];
kbcSeconds[_] := 1.;

kbcOrderQ[order_List] := AllTrue[order, MatchQ[{_, _Integer ? NonNegative} | Except[_List]]];
kbcOrderQ[_] := False;

(* The Method TKnuthBendixCompletion runs.  Ground-joinable critical pairs are
   dropped, so completion terminates on permutative equations such as
   commutativity, and the precedence is total and fixed by the axioms and the
   term (kbcPrecedence), unless the Method sets these itself.  An explicit
   order is the lexicographic path ordering for a precedence list and the
   Knuth-Bendix ordering for {symbol, weight} pairs; the symbols it leaves out
   follow in kbcPrecedence's order. *)
kbcMethod[method_, order_, axSides_, term_] := Block[{m, given, prec, names, weights, sub},
    m = Replace[method, {s_String :> {s}, Except[{_String, ___Rule}] :> {"Completion"}}];
    given = Keys[Association[Rest[m]]];
    prec = kbcPrecedence[axSides, term];
    sub = Which[
        MatchQ[order, {__}],
            names = atpSymName /@ Replace[order, {s_, _Integer} :> s, {1}];
            weights = Cases[order, {s_, w_Integer} :> atpSymName[s] -> w];
            {"Ordering" -> If[ weights === {}, "LPO", "KBO"],
             "Precedence" -> Join[{$kbcGoalConstant}, names, Select[prec, ! MemberQ[names, #] &]],
             "SymbolWeights" -> If[ weights === {}, None, weights],
             "AutoPrecedence" -> False, "SkolemHighest" -> False},
        ContainsNone[given, {"Precedence", "AutoPrecedence", "SkolemHighest"}],
            {"Precedence" -> Prepend[prec, $kbcGoalConstant], "AutoPrecedence" -> False, "SkolemHighest" -> False},
        True,
            {}];
    Join[m, If[ MemberQ[given, "GroundJoin"], {}, {"GroundJoin" -> True}], sub]
];

(* The precedence, highest first, after W15's ELSimplify: the axioms'
   function symbols by descending arity (ties in order of appearance), the
   axioms' constants, then the term's other symbols in reverse canonical
   order, so that a term's normal form does not depend on how it is written
   and lists commuting arguments in canonical order.  kbcMethod puts the
   goal's fresh constant above all of them. *)
kbcPrecedence[axSides_, term_] := Block[{sig, fs, own, axNames},
    sig = kbcSignature[axSides];
    fs = Select[sig, #[[2]] > 0 &];
    fs = fs[[Ordering[Transpose[{-fs[[All, 2]], Range[Length[fs]]}]]]];
    axNames = Join[fs[[All, 1]], Select[sig, #[[2]] === 0 &][[All, 1]]];
    own = Select[If[ term === None, {}, kbcSignature[{term}]],
        ! MemberQ[axNames, #[[1]]] &];
    Join[axNames, Reverse[SortBy[own, Last]][[All, 1]]]
];

(* The two sides of every axiom as written, bound variables as patterns.
   The encoder's own pairs can come back side-swapped by Waldmeister's
   intake canonicalization, which depends on the goal too. *)
kbcAxiomSides[axioms_List] := Join @@ Map[
    Replace[forAllToPattern[#], {HoldComplete[_[l_, r_]] :> {l, r}, _ :> {}}] &,
    HoldComplete /@ axioms];

(* {name, arity, symbol} for every symbol of the terms, in order of first
   appearance, named the way the encoder labels them (encodeAtpTerm). *)
kbcSignature[terms_List] := DeleteDuplicatesBy[
    Flatten[Reap[Scan[kbcSow, terms]][[2]], 1], First];

kbcSow[Verbatim[Pattern][_, _]] := Null;
kbcSow[n : (_Integer | _Real | _Rational)] := Sow[{ToString[n, InputForm], 0, n}];
kbcSow[s_String] := Sow[{s, 0, s}];
kbcSow[e_ ? AtomQ] := Sow[{ToString[e], 0, e}];
kbcSow[e_] := (Sow[{ToString[Head[e]], Length[e], Head[e]}]; Scan[kbcSow, e]);

(* The completion ProofObject (atpCompletionProofObject, the derivation of
   every rule) with the completed system as its "Theorems".  The
   derivations get as long again as the completion's TimeConstraint (wall,
   0 for none); past it the proof lists only the axioms. *)
kbcProofObject[enc_, cRes_, wall_] := Block[{po, theorems = kbcTheorems[cRes]},
    Quiet[
        Check[
            po = atpCompletionProofObject[enc, cRes,
                If[ wall > 0., AbsoluteTime[] + wall, Infinity]],
            Message[TKnuthBendixCompletion::nodata, Length[theorems]],
            TFindProof::cplpo],
        TFindProof::cplpo];
    If[ MatchQ[po, ProofObject[_, _, _, _]],
        ReplacePart[po, 2 -> theorems],
        po]];

kbcTheorems[cRes_] := With[{vars = cRes["VarSyms"]},
    DeleteCases[MapThread[kbcTheorem[#1, #2, vars] &, {cRes["MainRules"], cRes["RuleKinds"]}], None]];

(* One rule of the completed system with its variables renamed to formal
   letters in order of appearance: kind 1 is an oriented rule l -> r, kind 0
   an unorientable equation l <-> r, -1 an empty slot. *)
kbcTheorem[{l_, r_}, kind : 0 | 1, vars_] := Block[{vs, fs},
    vs = DeleteDuplicates @ Cases[{l, r}, v_Symbol /; MemberQ[vars, v], {0, Infinity}, Heads -> True];
    fs = kbcFormalSymbols[Length[vs]];
    If[ kind === 1,
        Rule[l /. Thread[vs -> (Pattern[#, Blank[]] & /@ fs)], r /. Thread[vs -> fs]],
        TwoWayRule @@ ({l, r} /. Thread[vs -> (Pattern[#, Blank[]] & /@ fs)])]
];
kbcTheorem[_, _, _] := None;

kbcFormalSymbols[n_] := Take[
    Join[
        Symbol /@ CharacterRange["\[FormalA]", "\[FormalZ]"],
        Symbol /@ CharacterRange["\[FormalCapitalA]", "\[FormalCapitalZ]"],
        Table[Symbol["\[FormalX]" <> ToString[k]], {k, Max[n - 52, 0]}]],
    n];

End[]

EndPackage[]
