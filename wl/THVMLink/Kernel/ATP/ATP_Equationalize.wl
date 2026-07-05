(* ::Package:: *)

(* ATP_Equationalize.wl: predicate-logic -> equational preprocessing.

   TFindProof's engine is an equational (unfailing Knuth-Bendix) prover:
   it proves `lhs == rhs` goals over equational theories.  A first-order
   / predicate-logic problem (predicates, And/Or/Not/Implies/Equivalent,
   ForAll/Exists) is not directly equational.  FindEquationalProof turns
   such a problem INTO an equational one - "equationalization" - before
   running the same core engine, so that e.g. the Socrates syllogism or
   `{ForAll[x, p[x] => p[f[x]]], p[a]} |- p[f[f[a]]]` become solvable.

   This file vendors that equationalization from the System source
   Kernel/StartUp/Language/EquationalProof.m (context
   Language`EquationalProofDump`, the implementation behind
   FindEquationalProof), re-homed into this package's private context.
   The transforms below (foldQuantifiers, Skolemize and its prenex /
   standardize / skolem helpers, findUnusedFormalSymbol, equate,
   equationalize) are copied verbatim from that source and renamed with
   an `eqz` prefix; the boolean encoding symbols (EquationalProof`and,
   `or, `not, ... and the \[FormalA]/\[FormalB]/\[FormalC]/\[FormalF]
   formal symbols) are kept identical so the encoding matches
   FindEquationalProof exactly.  atp_equationalize.wlt asserts
   eqzEquationalize is byte-identical to the live
   Language`EquationalProofDump`Equationalize.

   The one thvm-native adaptation is eqzDeForAll: FindEquationalProof
   hands its inner engine ForAll-wrapped axioms with Subscript-indexed
   bound variables; thvm's engine wants Pattern[v, Blank[]] variables, so
   eqzDeForAll strips the ForAll wrappers and rewrites each genuine bound
   variable (a bare Symbol or Subscript[sym, int], NOT a skolem-function
   application) to a fresh Pattern.  The free excluded-middle witness
   Subscript[\[FormalA], k] and the skolem constants Subscript[c, n] stay
   constants: the universally-quantified boolean base axioms
   (and[a, or[b, not[b]]] == a, ...) unify with them, so they need not be
   variables.

   Sibling of ATP.wl in the WolframInstitute`THVMLink`ATP` context,
   sharing WolframInstitute`THVMLink`ATP`Private`. *)

BeginPackage["WolframInstitute`THVMLink`ATP`", {"GeneralUtilities`", "WolframInstitute`THVMLink`"}];

(* Forward declaration (ATP_EquationalPath.wl / ATP_Strings.wl do the
   same): the recursive Kernel loader sorts `atp_equationalize.wl`
   BEFORE `atp.wl` (WL string sort compares alphanumerics ahead of
   punctuation), so without this the `OptionValue[TFindProof, ...]`
   below would parse against a fresh Private-context TFindProof with no
   Options -- OptionValue::nodef on every WaldmeisterProcess predicate
   call, and the user's TimeConstraint silently ignored. *)
TFindProof;

Begin["`Private`"];

(* === isEquationQ: EquationalProof.m routing test ===
   True when the input is already equational (so no equationalization is
   needed); False routes through the equationalization below.  Ported
   from EquationalProof.m's isEquationQ, EXTENDED for the thvm engine's
   wider native surface: FindEquationalProof's surface is Equal-only, but
   thvm's completion engine also natively accepts Unequal (disequation /
   refutation goals), TwoWayRule / Rule (pre-oriented axioms), and the
   Inactive[...] lemma forms - none of those need equationalization, so
   they must read as already-equational here or a bare `a != b` goal
   would be wrongly equationalized. *)
eqzIsEquationQ[eq : (_Equal | _Unequal | _TwoWayRule | _Rule)] :=
    FreeQ[eq, And | Or | Xor | Nand | Nor | Not | Implies | Equivalent];
eqzIsEquationQ[Inactive[Equal | Unequal | Rule | TwoWayRule][a_, b_]] :=
    FreeQ[{a, b}, And | Or | Xor | Nand | Nor | Not | Implies | Equivalent];
eqzIsEquationQ[quant : _ForAll | _Exists] := eqzIsEquationQ[Last[quant]];
eqzIsEquationQ[list_List] := AllTrue[list, eqzIsEquationQ];
eqzIsEquationQ[and_And] := AllTrue[and, eqzIsEquationQ];
eqzIsEquationQ[True] := True;
eqzIsEquationQ[False] := True;
eqzIsEquationQ[expression_] := Length[Cases[expression, _Symbol, Infinity]] == 0;

(* Whole-problem test: are BOTH propositions and axioms already
   equational?  Mirrors the `isEquationQ[props] && isEquationQ[axioms]`
   guard that selects the equational vs predicate branch. *)
eqzEquationalProblemQ[propositions_, axioms_] :=
    eqzIsEquationQ[propositions] && eqzIsEquationQ[axioms];

(* === foldQuantifiers (verbatim, renamed) ===
   ForAll[{x, y}, e] -> ForAll[x, ForAll[y, e]] so downstream sees one
   variable per quantifier. *)
eqzFoldQuantifiers[HoldPattern[ForAll[vars_List, expr_]]] := Fold[ForAll[#2, #1] &, eqzFoldQuantifiers[expr], vars];
eqzFoldQuantifiers[HoldPattern[Exists[vars_List, expr_]]] := Fold[Exists[#2, #1] &, eqzFoldQuantifiers[expr], vars];
eqzFoldQuantifiers[formula_] := formula;

(* === Skolemization chain (verbatim, renamed) === *)
eqzGetUniversallyQuantifiedVariables[formula_] := Flatten[Cases[formula, HoldPattern[ForAll[q_, _]] :> q, {0, Infinity}]];

eqzGetExistentiallyQuantifiedVariables[formula_] := Flatten[Cases[formula, HoldPattern[Exists[q_, _]] :> q, {0, Infinity}]];

eqzNestVariables[formula_] := Flatten[Reverse[Cases[formula, HoldPattern[_[q_, _]] :> q, {0, Infinity}]]];

eqzInitialStep[formula_] := If[ FreeQ[formula, ForAll],
    formula /. Thread[
        eqzGetExistentiallyQuantifiedVariables[formula] ->
        Table[
            Subscript[EquationalProof`c, n],
            {n, Count[eqzGetExistentiallyQuantifiedVariables[formula], _]}
        ]
    ],
    formula /. Thread[
        eqzNestVariables[formula][[1 ;; Flatten[Position[Reverse[Map[Head, Cases[FullForm[formula],
            _ForAll | _Exists, {0, Infinity}]]], ForAll]][[1]] - 1]] ->
        Table[
            Subscript[EquationalProof`c, n],
            {n, Flatten[Position[Reverse[Map[Head, Cases[FullForm[formula], _ForAll | _Exists, {0, Infinity}]]],
                ForAll]][[1]] - 1}
        ]
    ],
    formula
];

eqzNthStep[formula_] := Nest[eqzInitialStep, formula, Count[Cases[formula, _ForAll | _Exists, {0, Infinity}], _] + 1];

eqzSkolemFunctionRules[formula_] := With[{
    evars = eqzGetExistentiallyQuantifiedVariables[formula],
    uvars = eqzGetUniversallyQuantifiedVariables[formula]
},
Thread[
    evars -> Table[
        Apply[
            Subscript[\[FormalF], n],
            Reverse @ Table[
                Reverse[uvars][[1 ;; m]],
                {m, Map[
                        Length,
                        Table[
                            Cases[Map[Head, Reverse[Cases[Level[formula, {0, Infinity}], _Exists | _ForAll]][[1 ;; m]]], ForAll],
                            {m, Flatten[Position[Map[Head, Reverse[Cases[Level[formula, {0, Infinity}], _Exists | _ForAll]]], Exists]]}
                        ]
                    ]
                }
            ]
        ],
        {n, Length[evars]}
    ]
]
];

eqzSkolemize[formula_] := eqzNthStep[formula] /. eqzSkolemFunctionRules[formula] //. HoldPattern[Exists[_, q_]] :> q;

eqzEliminateEquivalence[formula_] := Block[{Equivalent = EquationalProof`and[Implies[#1, #2], Implies[#2, #1]] &}, formula];

eqzEliminateImplication[formula_] := Block[{Implies = EquationalProof`or[EquationalProof`not[#1], #2] &}, formula];

eqzStandardizeVariables[And[formula1_, formula2_], n_, fixed_] := Module[{standardizedFormula1, standardizedFormula2},
    standardizedFormula1 = eqzStandardizeVariables[formula1, n, fixed];
    standardizedFormula2 = eqzStandardizeVariables[formula2, standardizedFormula1[[2]], fixed];
    {And[standardizedFormula1[[1]], standardizedFormula2[[1]]], standardizedFormula2[[2]] + 1, fixed}
];

eqzStandardizeVariables[Or[formula1_, formula2_], n_, fixed_] := Module[{standardizedFormula1, standardizedFormula2},
    standardizedFormula1 = eqzStandardizeVariables[formula1, n, fixed];
    standardizedFormula2 = eqzStandardizeVariables[formula2, standardizedFormula1[[2]], fixed];
    {Or[standardizedFormula1[[1]], standardizedFormula2[[1]]], standardizedFormula2[[2]] + 1, fixed}
];

eqzStandardizeVariables[HoldPattern[ForAll[x_, formula_]], n_, fixed_] := If[ MemberQ[fixed, x],
    {ForAll[x, (eqzStandardizeVariables[formula, n, fixed])[[1]]], n, fixed},
    With[{standardizedFormula = eqzStandardizeVariables[formula /. {x -> Subscript[x, n]}, n + 1, Append[fixed, Subscript[x, n]]]},
        {ForAll[Subscript[x, n], standardizedFormula[[1]]], standardizedFormula[[2]], fixed}
    ]
];

eqzStandardizeVariables[HoldPattern[Exists[x_, formula_]], n_, fixed_] := If[ MemberQ[fixed, x],
    {Exists[x, (eqzStandardizeVariables[formula, n, fixed])[[1]]], n, fixed},
    With[{standardizedFormula = eqzStandardizeVariables[formula /. {x -> Subscript[x, n]}, n + 1, Append[fixed, Subscript[x, n]]]},
        {Exists[Subscript[x, n], standardizedFormula[[1]]], standardizedFormula[[2]], fixed}
    ]
];

eqzStandardizeVariables[formula_, n_, fixed_] := {formula, n, fixed};

eqzShiftQuantifiers[formula_] := Function[{expression, variables}, Fold[#2[[1]][#2[[2]], #1] &, expression,
    Reverse[variables[[1]]]]] @@ Reap[Replace[formula, (h : Verbatim[ForAll] | Verbatim[Exists])[variable_,
        expression_] :> (Sow[{Inactive[h], variable}, "unwrap"];
        expression), {0, Infinity}], "unwrap"] // Activate;

eqzFindPrenexNormalForm[formula_] := eqzShiftQuantifiers[(eqzStandardizeVariables[eqzEliminateImplication[eqzEliminateEquivalence[formula]], 0, {}])[[1]]];

eqzSkolemizeTop[formula_] := eqzSkolemize[eqzFindPrenexNormalForm[formula]] /; ! FreeQ[formula, ForAll | Exists];

eqzSkolemizeTop[formula_] := formula;

eqzFindUnusedFormalSymbol[formula_] := eqzFindUnusedFormalSymbol[formula, 0];
eqzFindUnusedFormalSymbol[formula_, subscript_Integer] := Subscript[\[FormalA], subscript] /; (Length[Cases[formula, Subscript[\[FormalA], subscript], Infinity]] == 0);
eqzFindUnusedFormalSymbol[formula_, subscript_Integer] := eqzFindUnusedFormalSymbol[formula, subscript + 1] /; (Length[Cases[formula, Subscript[\[FormalA], subscript], Infinity]] > 0);

eqzEquate[expr : _ForAll | _Exists, to_] := MapAt[eqzEquate[#, to] &, expr, {2}];
eqzEquate[expr_, to_] := expr == to;

(* equationalize: EquationalProof.m verbatim, with the final
   ForAll[skolemFns, ...] wrapper DROPPED - that wrapper is
   FindEquationalProof-internal bookkeeping over skolem-function terms;
   thvm treats skolem functions as ordinary uninterpreted function
   symbols and needs no wrapper.  eqzDeForAll (below) never converts a
   skolem-function application to a variable regardless. *)
eqzEquationalize[formula_] := Module[{skolemizedFormula, unusedFormalSymbol},
    skolemizedFormula = eqzSkolemizeTop[formula];
    unusedFormalSymbol = eqzFindUnusedFormalSymbol[formula];
    With[{true = EquationalProof`or[unusedFormalSymbol, EquationalProof`not[unusedFormalSymbol]]},
        eqzEquate[skolemizedFormula, true]
    ]
];

eqzEquationalize[formula_] := formula /; ! FreeQ[formula, Equal];

(* === boolean-algebra base (EquationalProof.m lines 910-958, verbatim) === *)
eqzBooleanAlgebraReplacements = {
    True -> EquationalProof`true, False -> EquationalProof`false,
    Equivalent -> EquationalProof`eq,
    And -> EquationalProof`and, Or -> EquationalProof`or,
    Not -> EquationalProof`not,
    Nand -> EquationalProof`nand, Nor -> EquationalProof`nor,
    Xor -> EquationalProof`xor
};

eqzBaseAxioms = {
    ForAll[{\[FormalA], \[FormalB]}, EquationalProof`and[\[FormalA], \[FormalB]] == EquationalProof`and[\[FormalB], \[FormalA]]],
    ForAll[{\[FormalA], \[FormalB]}, EquationalProof`or[\[FormalA], \[FormalB]] == EquationalProof`or[\[FormalB], \[FormalA]]],
    ForAll[{\[FormalA], \[FormalB]}, EquationalProof`and[\[FormalA], EquationalProof`or[\[FormalB], EquationalProof`not[\[FormalB]]]] == \[FormalA]],
    ForAll[{\[FormalA], \[FormalB]}, EquationalProof`or[\[FormalA], EquationalProof`and[\[FormalB], EquationalProof`not[\[FormalB]]]] == \[FormalA]],
    ForAll[{\[FormalA], \[FormalB], \[FormalC]}, EquationalProof`and[\[FormalA], EquationalProof`or[\[FormalB], \[FormalC]]] == EquationalProof`or[EquationalProof`and[\[FormalA], \[FormalB]], EquationalProof`and[\[FormalA], \[FormalC]]]],
    ForAll[{\[FormalA], \[FormalB], \[FormalC]}, EquationalProof`or[\[FormalA], EquationalProof`and[\[FormalB], \[FormalC]]] == EquationalProof`and[EquationalProof`or[\[FormalA], \[FormalB]], EquationalProof`or[\[FormalA], \[FormalC]]]]
};

(* defineNand/Nor/Xor (verbatim, renamed): add the connective's
   definition only when it appears in the problem. *)
eqzDefineNand[eqAxioms_List, propositions_, axioms_] := Join[eqAxioms,
    {ForAll[{\[FormalA], \[FormalB]}, EquationalProof`nand[\[FormalA], \[FormalB]] == EquationalProof`not[EquationalProof`and[\[FormalA], \[FormalB]]]]}] /;
        (StringContainsQ[ToString[FullForm[propositions]], "Nand"] || StringContainsQ[ToString[FullForm[axioms]], "Nand"]);
eqzDefineNand[eqAxioms_List, propositions_, axioms_] := eqAxioms;

eqzDefineNor[eqAxioms_List, propositions_, axioms_] := Join[eqAxioms,
    {ForAll[{\[FormalA], \[FormalB]}, EquationalProof`nor[\[FormalA], \[FormalB]] == EquationalProof`not[EquationalProof`or[\[FormalA], \[FormalB]]]]}] /;
        (StringContainsQ[ToString[FullForm[propositions]], "Nor"] || StringContainsQ[ToString[FullForm[axioms]], "Nor"]);
eqzDefineNor[eqAxioms_List, propositions_, axioms_] := eqAxioms;

eqzDefineXor[eqAxioms_List, propositions_, axioms_] := Join[eqAxioms,
    {ForAll[{\[FormalA], \[FormalB]}, EquationalProof`xor[\[FormalA], \[FormalB]] == EquationalProof`or[EquationalProof`and[\[FormalA], EquationalProof`not[\[FormalB]]],
        EquationalProof`and[EquationalProof`not[\[FormalA]], \[FormalB]]]]}] /;
            (StringContainsQ[ToString[FullForm[propositions]], "Xor"] || StringContainsQ[ToString[FullForm[axioms]], "Xor"]);
eqzDefineXor[eqAxioms_List, propositions_, axioms_] := eqAxioms;

(* === thvm-native adaptation: ForAll[Subscript-vars, eqn] -> Pattern vars ===
   A genuine bound variable is a bare Symbol or Subscript[sym, int]; a
   skolem-function application Subscript[\[FormalF], n][...] is NOT a
   variable (it stays a function symbol). *)
eqzVariableQ[Subscript[_, _Integer]] := True;
eqzVariableQ[Subscript[_Symbol]] := True;
eqzVariableQ[s_Symbol] := True;
eqzVariableQ[_] := False;

eqzDeForAll[expr_] := Module[{bv = {}, body = expr, rules},
    body = body //. {
        HoldPattern[ForAll[Verbatim[List][vs__], b_]] :> (bv = Join[bv, {vs}]; b),
        HoldPattern[ForAll[v_, b_]] :> (AppendTo[bv, v]; b)
    };
    bv = DeleteDuplicates[Select[Flatten[{bv}], eqzVariableQ]];
    rules = Function[{v, s}, v -> Pattern[s, Blank[]]] @@@ Transpose[{bv, Table[Unique["eqzv$"], {Length[bv]}]}];
    body /. rules
];

(* === predicate-proof reconstruction (EquationalProof.m lines 970-981 +
   helpers 2292-2325, verbatim, renamed) ===
   FindEquationalProof does not stop at the equational proof: it rebuilds
   a `"Predicate/EquationalLogic"` ProofObject that displays the ORIGINAL
   predicate goal/axioms and maps the boolean function symbols back to
   Inactive[And]/Inactive[Or]/... so the steps read in predicate form.
   thvm's equational ProofObject carries the same per-step Association
   structure FindEquationalProof's inner prover produces (keys
   {"Axiom",n} / {"Hypothesis",n} / {"CriticalPairLemma",n} / ..., each
   -> <|"Statement" -> HoldForm[...], "Proof" -> <|...|>|>), so the exact
   same reconstruction applies, and the result verifies through the
   System ProofObject's ProofFunction for "Predicate/EquationalLogic". *)

eqzBooleanInverseReplacements = {
    EquationalProof`true -> True, EquationalProof`false -> False,
    EquationalProof`eq -> Inactive[Equivalent],
    EquationalProof`and -> Inactive[And], EquationalProof`or -> Inactive[Or],
    EquationalProof`not -> Inactive[Not],
    EquationalProof`nand -> Inactive[Nand], EquationalProof`nor -> Inactive[Nor],
    EquationalProof`xor -> Inactive[Xor]
};

eqzGetPredicateStepType[step_] := step[[1, 1]];
eqzGetPredicateStepNumber[step_] := step[[1, 2]];
eqzGetPredicateStepStatement[step_] := step[[2, 1]];

eqzAddToPredicateAxiomStepsList[axiom_, l_List] := Append[l, {"Axiom", Length[l] + 1}];
eqzAddToPredicateHypothesisStepsList[hyp_, l_List] := Append[l, {"Hypothesis", Length[l] + 1}];
eqzAddToPredicateAxiomList[axiom_, l_List] :=
    Append[l, {"Axiom", Length[l] + 1} -> <|"Statement" -> axiom, "Proof" -> <||>|>];
eqzAddToPredicateHypothesisList[hyp_, l_List] :=
    Append[l, {"Hypothesis", Length[l] + 1} -> <|"Statement" -> hyp, "Proof" -> <||>|>];

eqzEquationalizationReplacement[step_, proofList_List] := Append[proofList, step] /;
    (eqzGetPredicateStepType[step] === "EquationalizedAxiom" || eqzGetPredicateStepType[step] === "EquationalizedHypothesis");
eqzEquationalizationReplacement[step_, proofList_List] := Append[proofList, step /.
    {"Axiom" -> "EquationalizedAxiom", "Hypothesis" -> "EquationalizedHypothesis"}] /;
    ! (eqzGetPredicateStepType[step] === "EquationalizedAxiom" || eqzGetPredicateStepType[step] === "EquationalizedHypothesis");

eqzAddToPredicateProofList[step_, proofList_List, axiomList_List, hypothesisList_List] :=
    Append[proofList, {"EquationalizedAxiom", eqzGetPredicateStepNumber[step]} ->
        <|"Statement" -> eqzGetPredicateStepStatement[step], "Proof" -> <|"Input" -> axiomList|>|>] /;
        eqzGetPredicateStepType[step] === "Axiom";
eqzAddToPredicateProofList[step_, proofList_List, axiomList_List, hypothesisList_List] :=
    Append[proofList, {"EquationalizedHypothesis", eqzGetPredicateStepNumber[step]} ->
        <|"Statement" -> eqzGetPredicateStepStatement[step], "Proof" -> <|"Input" -> hypothesisList|>|>] /;
        eqzGetPredicateStepType[step] === "Hypothesis";
eqzAddToPredicateProofList[step_, proofList_List, axiomList_List, hypothesisList_List] :=
    Append[proofList, step] /; (eqzGetPredicateStepType[step] =!= "Axiom" && eqzGetPredicateStepType[step] =!= "Hypothesis");

(* Rebuild an equational proof over the equationalized problem into a
   "Predicate/EquationalLogic" ProofObject over the ORIGINAL predicate
   propositions/axioms.  The source P is either the equational
   ProofObject (thvm's completion engine) or an Association carrying the
   same "Proof"/"Variables"/"Constants" keys (the un-mangled real-binary
   lift - an Association avoids re-triggering ProofObject validation,
   which would downgrade the wrapped predicate axioms to a bare
   FindEquationalProof call).  Pass-through on anything else (a failed
   proof). *)
atpReconstructableQ[P_] := Head[P] === ProofObject || (AssociationQ[P] && KeyExistsQ[P, "Proof"]);
atpReconstructPredicateProof[P_, propositions_, axioms_] := P /; ! atpReconstructableQ[P];
atpReconstructPredicateProof[P_, propositions_, axioms_] /; atpReconstructableQ[P] := Module[{
    axiomStepsList, hypothesisStepsList, axiomList, hypothesisList, proofList, proofListNew
},
    axiomStepsList = Fold[ReverseApplied[eqzAddToPredicateAxiomStepsList], {}, Flatten[{axioms}]];
    hypothesisStepsList = Fold[ReverseApplied[eqzAddToPredicateHypothesisStepsList], {}, Flatten[{propositions}]];
    axiomList = Fold[ReverseApplied[eqzAddToPredicateAxiomList], {}, Flatten[{axioms}]];
    hypothesisList = Fold[ReverseApplied[eqzAddToPredicateHypothesisList], {}, Flatten[{propositions}]];

    proofList = Fold[eqzAddToPredicateProofList[#2, #1, axiomStepsList, hypothesisStepsList] &, {}, P["Proof"]];
    proofListNew = Fold[eqzEquationalizationReplacement[#2, #1] &, {}, proofList];
    proofListNew = Join[axiomList, hypothesisList, proofListNew];

    ProofObject[
        <|
            "Logic" -> "Predicate/EquationalLogic",
            "Theorems" -> propositions,
            "Axioms" -> axioms,
            "Variables" -> P["Variables"],
            "Constants" -> P["Constants"],
            "Proof" -> proofListNew
        |> /. eqzBooleanInverseReplacements
    ]
];

(* === top-level: equationalize a predicate problem for thvm's engine ===
   Returns {equationalizedPropositions, equationalizedAxioms}, each a
   list of thvm-native equations (Pattern variables, EquationalProof`
   boolean function symbols).  Mirrors EquationalProof.m's
   findEquationalProof[True, ...] up to the inner prover call. *)
(* ForAll-form: the propositions/axioms are `ForAll[vars, lhs == rhs]`
   (multi-var vars from prenex), boolean function symbols in the
   EquationalProof` context.  Consumed BOTH by atpEquationalizeProblem
   (which converts ForAll -> Pattern for thvm's engine) and by the
   Waldmeister .pr writer (which reads variables from ForAll, so it must
   NOT be Pattern-converted). *)
atpEquationalizeProblemForAll[propositions_, axioms_] := Module[{
    foldedPropositions, foldedAxioms, eqProps, eqAxioms
},
    foldedPropositions = Map[eqzFoldQuantifiers, Flatten[{propositions}]];
    foldedAxioms = Map[eqzFoldQuantifiers, Flatten[{axioms}]];

    eqProps = Map[eqzEquationalize, foldedPropositions];
    eqAxioms = Map[eqzEquationalize, foldedAxioms];

    eqProps = eqProps /. eqzBooleanAlgebraReplacements;
    eqAxioms = eqAxioms /. eqzBooleanAlgebraReplacements;

    eqAxioms = Join[eqAxioms, eqzBaseAxioms];
    eqAxioms = eqzDefineNand[eqAxioms, propositions, axioms];
    eqAxioms = eqzDefineNor[eqAxioms, propositions, axioms];
    eqAxioms = eqzDefineXor[eqAxioms, propositions, axioms];

    {eqProps, eqAxioms}
];

atpEquationalizeProblem[propositions_, axioms_] :=
    With[{eq = atpEquationalizeProblemForAll[propositions, axioms]},
        {eqzDeForAll /@ eq[[1]], eqzDeForAll /@ eq[[2]]}
    ];

(* === WaldmeisterProcess (real Waldmeister binary) predicate path =====
   Rather than thvm's completion engine, discharge the equationalized
   problem with the actual `wmcli` binary - the same engine
   FindEquationalProof uses - then reconstruct.  The .pr writer
   (wmGenerateProblem) reads variables from ForAll and renders every
   head through a plain-symbol path, so the EquationalProof` boolean ops
   and Subscript witnesses/skolems are first renamed to clean lowercase
   symbols, then un-mangled after the lift. *)

(* Forward: EquationalProof` ops -> clean heads; Subscript constants /
   skolem functions -> clean atoms/heads.  ForAll is preserved. *)
eqzWmSymRules = {
    EquationalProof`and -> Global`wmand, EquationalProof`or -> Global`wmor, EquationalProof`not -> Global`wmnot,
    EquationalProof`eq -> Global`wmeq, EquationalProof`nand -> Global`wmnand, EquationalProof`nor -> Global`wmnor,
    EquationalProof`xor -> Global`wmxor, EquationalProof`true -> Global`wmtrue, EquationalProof`false -> Global`wmfalse,
    Subscript[\[FormalA], k_] :> Symbol["Global`emwit" <> ToString[k]],
    Subscript[EquationalProof`c, n_] :> Symbol["Global`skc" <> ToString[n]],
    Subscript[\[FormalF], n_] :> Symbol["Global`skf" <> ToString[n]]
};

(* Reverse the wmGenerateProblem + reparse mangling on the lifted
   equational ProofObject.  wmGenerateProblem renders a head `h` as
   "op_<lowercase>" then camel-sanitizes, and the reparse lowercases and
   string-heads it - so `wmor` -> "opwmor"[...], `man` -> "opman"[...],
   nullary constants -> a Global` symbol.  Recover the EquationalProof`
   boolean ops, the excluded-middle / skolem constants, and the
   problem's own predicate symbols so atpReconstructPredicateProof (which
   rewrites EquationalProof`* -> Inactive[*]) applies. *)
eqzWmUnmangle[expr_, propositions_, axioms_] := Module[{
    predSyms, boolHeadRules, skfRules, predHeadRules, predConstRules
},
    boolHeadRules = {
        "opwmand"[a___] :> EquationalProof`and[a],
        "opwmor"[a___] :> EquationalProof`or[a],
        "opwmnot"[a___] :> EquationalProof`not[a],
        "opwmeq"[a___] :> EquationalProof`eq[a],
        "opwmnand"[a___] :> EquationalProof`nand[a],
        "opwmnor"[a___] :> EquationalProof`nor[a],
        "opwmxor"[a___] :> EquationalProof`xor[a]
    };
    (* The excluded-middle / skolem CONSTANTS come back as clean Global`
       atoms (emwit<k>, skc<n>) already, so leave them as-is: keeping
       them atomic (rather than Subscript[\[FormalA], k]) is what lets
       ProofObject dispatch on the reconstructed proof.  Only the skolem
       FUNCTION heads arrive "op"-prefixed; map those to clean Global`
       heads. *)
    skfRules = (With[{sym = Symbol["Global`skf" <> StringDrop[#, 5]]}, #[a___] :> sym[a]] &) /@ DeleteDuplicates @ Cases[expr, h_String /; StringMatchQ[h, "opskf" ~~ DigitCharacter ..], {0, Infinity}, Heads -> True];
    predSyms = DeleteDuplicates @ Cases[
        {propositions, axioms},
        s_Symbol /; Context[s] =!= "System`" && ! MemberQ[{ForAll, Exists, Implies, And, Or, Not, Equivalent, Xor, Nand, Nor, Equal, Unequal, List, True, False, Rule, TwoWayRule}, s],
        {0, Infinity},
        Heads -> True
    ];
    (* Keep only NON-identity predicate renames (a symbol whose lowercase
       already equals its own name, e.g. `man`, maps to itself). *)
    predHeadRules = DeleteCases[
        Function[sym, With[{mn = "op" <> ToLowerCase[SymbolName[sym]]}, mn[a___] :> sym[a]]] /@ predSyms,
        Verbatim[RuleDelayed][h_[__], h_[__]]
    ];
    predConstRules = DeleteCases[
        Function[sym, Symbol["Global`" <> ToLowerCase[SymbolName[sym]]] -> sym] /@ predSyms,
        Verbatim[Rule][x_, x_]
    ];
    (* A head-rewrite `h[a___] :> ...` does not descend into the matched
       arguments, so nested boolean ops (or[..., and[..., not[...]]])
       need re-scanning; FixedPoint (bounded) re-applies until stable
       without ReplaceRepeated's runaway-iteration risk. *)
    FixedPoint[# /. Join[boolHeadRules, skfRules, predHeadRules, predConstRules] &, expr, 30]
];

atpWaldmeisterPredicateProof[propositions_, axioms_, opts___] := Module[{
    eqForAll, wmAxioms, wmGoal, pr, tmp, tc
},
    eqForAll = atpEquationalizeProblemForAll[propositions, axioms];
    wmAxioms = eqForAll[[2]] /. eqzWmSymRules;
    wmGoal = First[eqForAll[[1]] /. eqzWmSymRules];
    pr = WolframInstitute`THVMLink`ATP`Private`wmGenerateProblem["thvmPredicateWm", wmAxioms, wmGoal];
    If[ FailureQ[pr], Return[$Failed]];
    tmp = FileNameJoin[{$TemporaryDirectory, "thvm_predicate_wm.pr"}];
    Export[tmp, pr, "Text"];
    (* Bound the lift + metadata reconstruction: on a complex proof the
       superposition-reconstruction enumeration can be slow, and its
       CriticalPairLemma coverage is incomplete (the reconstruct step may
       yield a non-verifying proof).  wmcli itself is bounded by its own
       TimeConstraint; cap this stage too so it cannot run away. *)
    tc = Replace[OptionValue[TFindProof, {opts}, TimeConstraint], Except[_ ? NumericQ] -> 60];
    TimeConstrained[
        Module[{lifted, unmangled},
            lifted = WolframInstitute`THVMLink`ATP`TWaldmeisterProofObject[tmp, "LiftToProofObject" -> True, FilterRules[{opts}, {TimeConstraint}]];
            If[ Head[lifted] =!= ProofObject,
                $Failed,
                (* Un-mangle the extracted PARTS (rewriting the ProofObject
                   wrapper re-runs its validation), then rename the lift's
                   Global`x<n> universal variables to fresh symbols
                   (ProofObject's property dispatch rejects Global-context
                   universal variables). *)
                unmangled = eqzWmUnmangle[<|"Proof" -> lifted["Proof"], "Variables" -> lifted["Variables"], "Constants" -> lifted["Constants"]|>, propositions, axioms];
                unmangled = With[{vmap = # -> Unique["eqzWmVar"] & /@ Cases[unmangled["Variables"], _Symbol]}, unmangled /. vmap];
                atpReconstructPredicateProof[unmangled, propositions, axioms]
            ]
        ],
        tc,
        $Failed
    ]
];

(* Already-equational problem under Method -> "WaldmeisterProcess": no
   equationalization, just generate the .pr from the equations and lift
   the real binary's proof (mirrors the named-theory string form). *)
atpWaldmeisterEquationalProof[conjecture_, axioms_, opts___] := Module[{pr, tmp},
    pr = WolframInstitute`THVMLink`ATP`Private`wmGenerateProblem["thvmEquationalWm", axioms, conjecture];
    If[ FailureQ[pr], Return[$Failed]];
    tmp = FileNameJoin[{$TemporaryDirectory, "thvm_equational_wm.pr"}];
    Export[tmp, pr, "Text"];
    WolframInstitute`THVMLink`ATP`TWaldmeisterProofObject[tmp, "LiftToProofObject" -> True, FilterRules[{opts}, {TimeConstraint}]]
];

End[];

EndPackage[];
