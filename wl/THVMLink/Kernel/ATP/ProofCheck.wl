(* ::Package:: *)

(* ProofCheck.wl: atpProofCheck, a replay of an equational ProofObject from its four arguments
   alone, for a kernel whose ProofObject has no "ProofFunction" of its own; Wolfram 15's has one,
   and TFindProof keeps using it there (atpProofVerify).

   Each row of the proof is checked against the rows it cites, which must come before it. Symbols
   in the proof's "Variables" and "Constants" are variables, as Wolfram 15's ProofFunction
   patterns them; every other symbol is a constant. A statement holds when it is the equation the
   row computes up to a renaming of its variables and the order of its sides.

     Axiom                     one of the axioms, its bound variables renamed
     Hypothesis n              theorem n, its bound and free variables renamed
     CriticalPairLemma         the overlap of the Construct's rule with the MatchingConstruct's at
                               Position of the first rule's left side, under their most general
                               unifier, the two rules' variables renamed apart: {right side of the
                               first rule, its left side rewritten at Position by the second}
     SubstitutionLemma         the Input, turned around when InputOrientation is -1, with the
                               Construct's rule applied at Position of side Side
     Conclusion                a SubstitutionLemma whose two sides are the same term
     EquationalizedAxiom       taken as given, citing axioms, as Wolfram 15's ProofFunction does
     EquationalizedHypothesis  taken as given, citing hypotheses

   A row's rule is its statement read left to right, or right to left when the Orientation times
   the sign of the side field (ConstructSide, Side or MatchingSide; 2 is -1) is -1. A row derived
   from a hypothesis is no lemma for all values: it may be an Input, but its rule only when that
   rewrites a term to itself. Every theorem needs a Conclusion derived from its hypothesis. The
   Rule, Subpattern and OutputExpression fields are display only and not read: in Wolfram 15's own
   predicate proof of mortal[socrates], a MatchingRule is not its MatchingConstruct's rule. *)

BeginPackage["WolframInstitute`THVMLink`ATP`", {"GeneralUtilities`", "WolframInstitute`THVMLink`"}];

TFindProof;

Begin["`Private`"];

TFindProof::pfform = "`1` is not an equational ProofObject that the proof check reads."
TFindProof::pfstep = "Step `1` of the proof does not hold: `2`."
TFindProof::pfgoal = "No conclusion of the proof proves theorem `1`."

(* === terms === *)

(* Terms become inert trees, so that no step evaluates them: f[a, b] is proofCheckNode[f, {a, b}]
   with its head and arguments converted in turn, and a pattern x_ is the symbol x. *)
SetAttributes[proofCheckTree, HoldAllComplete]
proofCheckTree[Verbatim[Pattern][s_Symbol, _]] := s
proofCheckTree[head_[arguments___]] := proofCheckNode[proofCheckTree[head], proofCheckTree /@ Unevaluated[{arguments}]]
proofCheckTree[atom_] := atom

(* {left, right} of an equation, held or not, as trees; None for any other statement.  A rule
   stands for its equation: Wolfram 15 states one SubstitutionLemma of the mortal[socrates] proof
   as a rule. *)
proofCheckSides[HoldForm[(Equal | Rule | RuleDelayed | TwoWayRule | Inactive[Equal | Rule | TwoWayRule])[l_, r_]]] :=
    {proofCheckTree[l], proofCheckTree[r]}
proofCheckSides[(Equal | Rule | RuleDelayed | TwoWayRule | Inactive[Equal | Rule | TwoWayRule])[l_, r_]] :=
    {proofCheckTree[l], proofCheckTree[r]}
proofCheckSides[_] := None

(* {sides, bound variables} of an axiom or theorem, ForAll[x, eq], ForAll[{x, ...}, eq] or eq, where
   the names of patterns x_ are bound too, as in an axiom TFindProof takes from AxiomaticTheory; None
   when it is no equation.  HoldPattern: ForAll[v_Symbol, body_] evaluates to body_, v being absent. *)
proofCheckQuantified[HoldPattern[ForAll[v_Symbol, body_]]] :=
    proofCheckBound[Union[{v}, proofCheckPatternNames[body]], proofCheckSides[Unevaluated[body]]]
proofCheckQuantified[HoldPattern[ForAll[{v___Symbol}, body_]]] :=
    proofCheckBound[Union[{v}, proofCheckPatternNames[body]], proofCheckSides[Unevaluated[body]]]
proofCheckQuantified[e_] := proofCheckBound[proofCheckPatternNames[e], proofCheckSides[Unevaluated[e]]]

SetAttributes[proofCheckPatternNames, HoldAllComplete]
proofCheckPatternNames[e_] := Union @ Cases[Unevaluated[e], Verbatim[Pattern][s_Symbol, _] :> s, {0, Infinity}, Heads -> True]
proofCheckBound[_List, None] := None
proofCheckBound[bound_List, sides_List] := {sides, bound}

(* The statement itself, its HoldForm taken off, for a row of predicate logic. *)
proofCheckFormula[HoldForm[e_]] := e
proofCheckFormula[e_] := e

(* expr with each of the symbols the variable proofCheckVariable[tag, symbol]; the tags keep the
   variables of two rules, or of a rule and the term it rewrites, apart. *)
proofCheckMark[expr_, symbols_List, tag_String] :=
    expr /. Dispatch[Thread[symbols -> (proofCheckVariable[tag, #] & /@ symbols)]]

(* expr with its variables numbered in the order they first occur, so that two equations that
   differ by a renaming of their variables become the same. *)
proofCheckCanonical[expr_] := With[{variables = DeleteDuplicates @ Cases[expr, _proofCheckVariable, {0, Infinity}]},
    expr /. Thread[variables -> (proofCheckVariable["canonical", #] & /@ Range[Length[variables]])]
]

(* True when the statement's sides are the computed sides, up to renaming and their order. *)
proofCheckVariantQ[sides_List, statement_List] := With[{canonical = proofCheckCanonical[statement]},
    proofCheckCanonical[sides] === canonical || proofCheckCanonical[Reverse[sides]] === canonical
]

(* The subterm at a position of the term, {} its root; proofCheckNoTerm when there is none. *)
proofCheckSubterm[term_, {}] := term
proofCheckSubterm[proofCheckNode[_, arguments_List], {p_Integer, rest___}] /; 1 <= p <= Length[arguments] :=
    proofCheckSubterm[arguments[[p]], {rest}]
proofCheckSubterm[_, _] := proofCheckNoTerm

(* The term with its subterm at the position, which exists, replaced by new. *)
proofCheckReplaced[_, {}, new_] := new
proofCheckReplaced[proofCheckNode[head_, arguments_List], {p_Integer, rest___}, new_] :=
    proofCheckNode[head, ReplacePart[arguments, p -> proofCheckReplaced[arguments[[p]], {rest}, new]]]

(* === unification === *)

(* The most general substitution, variable -> tree, of the variables bindableQ admits that makes
   a and b one tree; proofCheckNoTerm when there is none.  Matching a rule's left side against a
   term is unification that binds only the rule's variables.  It runs as a loop over the pairs
   still to unify, so a deep term does not deepen the evaluation stack. *)
proofCheckUnifier[a_, b_, bindableQ_] := Replace[
    NestWhile[proofCheckUnifyStep[#, bindableQ] &, {{{a, b}}, <||>}, MatchQ[#, {{__}, _Association}] &],
    {{{}, substitution_Association} :> substitution, _ -> proofCheckNoTerm}
]

proofCheckUnifyStep[{{{s_, t_}, rest___}, substitution_Association}, bindableQ_] := With[
    {a = s /. Normal[substitution], b = t /. Normal[substitution]},
    Which[
        a === b, {{rest}, substitution},
        bindableQ[a], proofCheckBind[a, b, {rest}, substitution],
        bindableQ[b], proofCheckBind[b, a, {rest}, substitution],
        MatchQ[{a, b}, {proofCheckNode[_, x_List], proofCheckNode[_, y_List]} /; Length[x] === Length[y]],
            {Join[{{First[a], First[b]}}, Transpose[{Last[a], Last[b]}], {rest}], substitution},
        True, proofCheckNoTerm
    ]
]

proofCheckBind[v_, t_, rest_List, substitution_Association] := If[ ! FreeQ[t, v],
    proofCheckNoTerm,
    {rest, Append[(# /. v -> t) & /@ substitution, v -> t]}
]

proofCheckApply[expr_, substitution_Association] := expr /. Normal[substitution]

(* === rows === *)

proofCheckFail[key_, reason_String] := Throw[{key, reason}, proofCheckFail]

proofCheckField[key_, proof_Association, field_String] := If[KeyExistsQ[proof, field], proof[field], proofCheckFail[key, "it has no " <> field]]

proofCheckSign[2] := -1
proofCheckSign[_] := 1

(* The sides of a cited row, which must be checked already and be an equation. *)
proofCheckCited[key_, checked_Association, cited_] := Which[
    ! KeyExistsQ[checked, cited], proofCheckFail[key, "it cites " <> ToString[cited, InputForm] <> ", which is not a step before it"],
    checked[cited]["Sides"] === None, proofCheckFail[key, "it cites " <> ToString[cited, InputForm] <> ", which is no equation"],
    True, checked[cited]
]

(* The rule of a cited row, its variables marked with tag: {left, right}.  A row that rests on a
   hypothesis gives a rule only when that rule rewrites a term to itself. *)
proofCheckRule[key_, checked_Association, cited_, orientation_, side_, variables_List, tag_String] := With[
    {row = proofCheckCited[key, checked, cited]},
    {sides = proofCheckMark[If[orientation * proofCheckSign[side] === -1, Reverse[row["Sides"]], row["Sides"]], variables, tag]},
    Which[
        ! MemberQ[{1, -1}, orientation], proofCheckFail[key, "its orientation is not 1 or -1"],
        row["Goal"] =!= None && First[sides] =!= Last[sides], proofCheckFail[key, "it rewrites with " <> ToString[cited, InputForm] <> ", which rests on a hypothesis"],
        True, sides
    ]
]

proofCheckPosition[key_, position_] := If[ MatchQ[position, {___Integer ? Positive}], position, proofCheckFail[key, "its position is not a list of positive integers"]]

(* The value of each row: <|"Sides" -> {left, right} or None, "Goal" -> the theorem it rests on, or
   None|>. *)
proofCheckStep[key : {"Axiom", _Integer}, statement_, _, _, context_] := With[{sides = proofCheckSides[statement]},
    Which[
        sides === None && AnyTrue[context["Axioms"], # === proofCheckFormula[statement] &],
            <|"Sides" -> None, "Goal" -> None|>,
        sides =!= None && AnyTrue[context["Axioms"], proofCheckGivenQ[proofCheckQuantified[#], sides, context["Variables"]] &],
            <|"Sides" -> sides, "Goal" -> None|>,
        True,
            proofCheckFail[key, "it is none of the axioms"]
    ]
]

proofCheckStep[key : {"Hypothesis", n_Integer}, statement_, _, _, context_] := Module[{theorem, sides},
    If[ ! 1 <= n <= Length[context["Theorems"]], proofCheckFail[key, "there is no theorem " <> ToString[n]]];
    theorem = context["Theorems"][[n]];
    sides = proofCheckSides[statement];
    Which[
        sides === None && proofCheckFormula[statement] === theorem,
            <|"Sides" -> None, "Goal" -> n|>,
        sides =!= None && proofCheckGivenQ[proofCheckQuantified[theorem], sides, context["Variables"], context["Variables"]],
            <|"Sides" -> sides, "Goal" -> n|>,
        True,
            proofCheckFail[key, "it is not theorem " <> ToString[n]]
    ]
]

proofCheckStep[key : {"EquationalizedAxiom", _Integer}, statement_, proof_, checked_, _] := With[
    {inputs = proofCheckField[key, proof, "Input"], sides = proofCheckSides[statement]},
    If[ sides === None || ! MatchQ[inputs, {{"Axiom", _Integer}..}] || ! AllTrue[inputs, KeyExistsQ[checked, #] &],
        proofCheckFail[key, "it is no equation from the axioms before it"],
        <|"Sides" -> sides, "Goal" -> None|>
    ]
]

proofCheckStep[key : {"EquationalizedHypothesis", _Integer}, statement_, proof_, checked_, _] := With[
    {inputs = proofCheckField[key, proof, "Input"], sides = proofCheckSides[statement]},
    If[ sides === None || ! MatchQ[inputs, {{"Hypothesis", _Integer}..}] || ! AllTrue[inputs, KeyExistsQ[checked, #] &],
        proofCheckFail[key, "it is no equation from the hypotheses before it"],
        <|"Sides" -> sides, "Goal" -> checked[First[inputs]]["Goal"]|>
    ]
]

proofCheckStep[key : {kind : ("SubstitutionLemma" | "Conclusion"), _Integer}, statement_, proof_, checked_, context_] := Module[
    {variables = context["Variables"], input, inputSides, rule, side, position, target, subterm, match, result, sides},
    input = proofCheckCited[key, checked, proofCheckField[key, proof, "Input"]];
    inputSides = proofCheckMark[input["Sides"], variables, "term"];
    inputSides = Switch[Lookup[proof, "InputOrientation", 1],
        1, inputSides,
        -1, Reverse[inputSides],
        _, proofCheckFail[key, "its input orientation is not 1 or -1"]
    ];
    rule = proofCheckRule[key, checked, proofCheckField[key, proof, "Construct"], proofCheckField[key, proof, "Orientation"],
        Lookup[proof, "ConstructSide", 1], variables, "rule"];
    side = proofCheckField[key, proof, "Side"];
    If[ ! MemberQ[{1, 2}, side], proofCheckFail[key, "its side is not 1 or 2"]];
    position = proofCheckPosition[key, proofCheckField[key, proof, "Position"]];
    target = inputSides[[side]];
    subterm = proofCheckSubterm[target, position];
    If[ subterm === proofCheckNoTerm, proofCheckFail[key, "its input has no subterm at position " <> ToString[position]]];
    match = proofCheckUnifier[First[rule], subterm, MatchQ[#, proofCheckVariable["rule", _]] &];
    If[ match === proofCheckNoTerm, proofCheckFail[key, "its rule does not match at position " <> ToString[position]]];
    result = ReplacePart[inputSides, side -> proofCheckReplaced[target, position, proofCheckApply[Last[rule], match]]];
    sides = proofCheckSides[statement];
    Which[
        sides === None || ! proofCheckVariantQ[result, proofCheckMark[sides, variables, "statement"]],
            proofCheckFail[key, "its statement is not the equation the rewrite gives"],
        kind === "Conclusion" && First[sides] =!= Last[sides],
            proofCheckFail[key, "its sides are not the same term"],
        kind === "Conclusion" && input["Goal"] === None,
            proofCheckFail[key, "it rests on no hypothesis"],
        True,
            <|"Sides" -> sides, "Goal" -> input["Goal"]|>
    ]
]

proofCheckStep[key : {"CriticalPairLemma", _Integer}, statement_, proof_, checked_, context_] := Module[
    {variables = context["Variables"], rule, matchingRule, position, subterm, unifier, overlap, pair, sides},
    rule = proofCheckRule[key, checked, proofCheckField[key, proof, "Construct"], proofCheckField[key, proof, "Orientation"],
        Lookup[proof, "Side", 1], variables, "construct"];
    matchingRule = proofCheckRule[key, checked, proofCheckField[key, proof, "MatchingConstruct"],
        proofCheckField[key, proof, "MatchingOrientation"], Lookup[proof, "MatchingSide", 1], variables, "matching"];
    position = proofCheckPosition[key, proofCheckField[key, proof, "Position"]];
    subterm = proofCheckSubterm[First[rule], position];
    If[ subterm === proofCheckNoTerm, proofCheckFail[key, "its rule has no subterm at position " <> ToString[position]]];
    unifier = proofCheckUnifier[subterm, First[matchingRule], MatchQ[#, proofCheckVariable["construct" | "matching", _]] &];
    If[ unifier === proofCheckNoTerm, proofCheckFail[key, "its rules do not overlap at position " <> ToString[position]]];
    overlap = proofCheckApply[First[rule], unifier];
    pair = {proofCheckApply[Last[rule], unifier], proofCheckReplaced[overlap, position, proofCheckApply[Last[matchingRule], unifier]]};
    sides = proofCheckSides[statement];
    If[ sides === None || ! proofCheckVariantQ[pair, proofCheckMark[sides, variables, "statement"]],
        proofCheckFail[key, "its statement is not the critical pair of its rules"],
        <|"Sides" -> sides, "Goal" -> None|>
    ]
]

proofCheckStep[key_, _, _, _, _] := proofCheckFail[key, "it is a step of no kind the proof check knows"]

(* An axiom or theorem, as proofCheckQuantified gives it, is the statement up to renaming: its
   bound variables, and for a theorem the proof's variables too, against the proof's variables in
   the statement. *)
proofCheckGivenQ[None, __] := False
proofCheckGivenQ[{given_List, bound_List}, sides_List, variables_List, free_List : {}] :=
    proofCheckVariantQ[proofCheckMark[given, Join[bound, free], "given"], proofCheckMark[sides, variables, "statement"]]

proofCheckRow[context_][checked_Association, key_ -> row_Association] := If[ KeyExistsQ[checked, key],
    proofCheckFail[key, "its key repeats"],
    Append[checked, key -> proofCheckStep[key, proofCheckField[key, row, "Statement"],
        Replace[Lookup[row, "Proof", <||>], Except[_Association] :> proofCheckFail[key, "its Proof is no Association"]], checked, context]]
]
proofCheckRow[_][_, row_] := proofCheckFail[row, "it is not a key -> <|\"Statement\" -> ..., \"Proof\" -> ...|>"]

(* === entry === *)

atpProofCheck::usage = "atpProofCheck[proof] replays the equational ProofObject proof from its dataset and gives Success[\"ProofFunction\", ...] \
as Wolfram 15's ProofFunction does when every step holds, or a Failure that names the first step that does not."
atpProofCheck[ProofObject[_String, theorems_, axioms_, data_Association]] := Module[{
    context = <|
        "Variables" -> DeleteDuplicates @ Join[Lookup[data, "Variables", {}], Lookup[data, "Constants", {}]],
        "Axioms" -> If[ListQ[axioms], axioms, {axioms}],
        "Theorems" -> If[ListQ[theorems], theorems, {theorems}]
    |>,
    rows = Replace[Lookup[data, "Proof", {}], a_Association :> Normal[a]],
    checked,
    proved
},
    Catch[
        checked = Fold[proofCheckRow[context], <||>, rows];
        proved = Union @ Cases[Normal[checked], ({"Conclusion", _} -> row_) :> row["Goal"]];
        Replace[Complement[Range[Length[context["Theorems"]]], proved], {
            {} -> Success["ProofFunction", <|"MessageTemplate" -> "Success!"|>],
            {n_, ___} :> atpOutcome[TFindProof::pfgoal, n]
        }],
        proofCheckFail,
        Function[{value, tag}, atpOutcome[TFindProof::pfstep, First[value], Last[value]]]
    ]
]
atpProofCheck[other_] := atpOutcome[TFindProof::pfform, Short[other]]

(* TFindProof accepts a proof on the ProofObject's own ProofFunction where the kernel gives one, as
   Wolfram 15 does, and on atpProofCheck where it does not.  $atpProofCheckNative = True makes it
   use atpProofCheck always, which the tests Block to check that path on Wolfram 15. *)
$atpProofCheckNative = False

atpProofVerify::usage = "atpProofVerify[proof] checks the ProofObject proof as TFindProof accepts it: by its own ProofFunction applied to its \
theorems where the kernel gives one, otherwise by atpProofCheck."
atpProofVerify[proof_] := If[ TrueQ[$atpProofCheckNative],
    atpProofCheck[proof],
    Replace[proof["ProofFunction"], {function_Function :> function[proof["Theorems"]], _ :> atpProofCheck[proof]}]
]

End[];

EndPackage[];
