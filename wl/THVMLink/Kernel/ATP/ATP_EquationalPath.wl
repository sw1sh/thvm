(* ::Package:: *)

(* ATP_EquationalPath.wl: the witnessing replacement-path surface.

   TFindEquationalPath returns the rewrite path of a proved equational
   goal and, on request, the rules, rewrites, substitutions, bindings,
   step justification, and a rewrite-reproduction check that witness it.

   The path machinery (findEquationalPath and the helpers below, between
   the `=== ported ... ===` banners) is the Wolfram Function Repository
   function FindEquationalPath (ResourceObject UUID
   4edacf77-2fd1-4637-8b86-b025f3d037d7), re-homed from its
   FunctionRepository`$* context into this package's private context and
   reformatted to the house style.  The one logic change: the bundled
   Robinson unifier is dropped in favour of
   ResourceFunction["MostGeneralUnifier"].  It runs on the ProofObject
   TFindProof emits, which carries the same ProofAssociation / per-step
   Proof structure FindEquationalProof produces.

   Sibling of ATP.wl / ATP_Strings.wl in the WolframInstitute`THVMLink`ATP` context,
   sharing WolframInstitute`THVMLink`ATP`Private`. *)

BeginPackage["WolframInstitute`THVMLink`ATP`", {"WolframInstitute`THVMLink`"}];

GeneralUtilities`SetUsage[TFindEquationalPath,
    "TFindEquationalPath[thm$, axioms$] proves thm$ over axioms$ and returns the witnessing rewrite path: the list of terms from the theorem's lhs to its rhs, where consecutive entries differ by one rewrite.
TFindEquationalPath[proof$] returns the path for a precomputed ProofObject.
TFindEquationalPath[$$, prop$] returns property prop$ of the replacement path instead of the default 'Path': one of 'Path', 'Rules' (the rule applied at each step), 'Rewrites' (the concrete ReplaceAt/Replace operation at each step), 'Substitutions' / 'Bindings' (the per-step variable bindings), 'Justification' ({lemma, orientation, position} per step), 'RewriteTest' (a Success/Failure checking the rewrite sequence reproduces the path), or 'ProofObject'; All returns every property as an Association, and a list of property names returns the corresponding sub-Association.
The path assembles the lhs chain forward then the rhs chain reversed through the shared normal form, matching the Wolfram Function Repository FindEquationalPath; with one-sided Rule axioms it is a forward replacement path.
Options are TFindProof's (for the [thm$, axioms$] form) plus the path options 'Reverse', 'Simplify', 'Canonicalize', 'TerminalLemmas', and 'Cache'."
];

(* Siblings whose public symbols this file references by bare name; the
   Kernel loader's path sort can place this file first, so forward-declare
   them in WolframInstitute`THVMLink`ATP` rather than minting Private shadows. *)

TFindProof;

Begin["`Private`"];

(* === ported WolframFunctionRepository FindEquationalPath (vendored; do not hand-edit) === *)

canonicalizePatterns[expr_, OptionsPattern[]] := Block[{
    chars = Replace[
        OptionValue["SymbolNames"],
        {
            Automatic -> Join[CharacterRange["", ""], CharacterRange["", ""]],
            "Alphabet" | Alphabet -> Alphabet[]
        }
    ],
    patts = DeleteDuplicates[Cases[expr, _Pattern, All, Heads -> True]]
},
    chars = First[
        NestWhile[
            Apply[
                Function[{cs, n},
                    {Join[cs, (StringJoin[#1, ToString[n]]&) /@ cs], n + 1}
                ]
            ],
            {chars, 1},
            Length[#1[[1]]] < Length[patts]&
        ]
    ];
    MapIndexed[
        With[{canonical = Pattern @@ {Symbol[Extract[chars, #2]], Last[#1]}},
            Verbatim[#1] :> canonical
        ]&,
        patts
    ]
]

Options[canonicalizePatterns] = {"SymbolNames" -> "Alphabet"}

extract[expr_, pos : {_Integer...}, f_] := If[ pos === {},
    f[expr],
    Enclose[
        ConfirmQuiet[Extract[Unevaluated[expr], pos, f], {Extract::partd, Extract::partw}],
        Missing[ReleaseHold[#1["HeldMessageName"]]]&
    ]
]

extract[expr_, pos : {_Integer...}] := If[ pos === {},
    expr,
    Enclose[
        ConfirmQuiet[Extract[Unevaluated[expr], pos], {Extract::partd, Extract::partw}],
        Missing[ReleaseHold[#1["HeldMessageName"]]]&
    ]
]

findEquationalPath[
    proof_ProofObject,
    defaultLemma : {_String, _Integer} | Automatic : Automatic,
    defaultConclusion : {_String, _Integer} | Automatic : Automatic,
    prop : _String | All | {_String...} : "Path",
    opts : OptionsPattern[]
] := Enclose[
    Block[{
        hyp,
        axioms,
        pa,
        vars,
        lemma = Replace[defaultLemma, Automatic -> {"Hypothesis", 1}],
        conclusion,
        statement,
        justification,
        rules,
        substitutions,
        bindings,
        rewrites,
        path,
        reverse = TrueQ[OptionValue["Reverse"]],
        terminal = OptionValue["TerminalLemmas"],
        $useJustifyLemmaCache = TrueQ[OptionValue["Cache"]],
        simplify = If[TrueQ[OptionValue["Simplify"]], simplifyJustification, Identity]
    },
        $justifyLemmaCache = Association[];
        conclusion = Replace[defaultConclusion, Automatic :> {"Conclusion", Replace[lemma, {{"Hypothesis", i_} :> i, _ -> 1}]}];
        hyp = proof["Theorem"];
        axioms = proof["Axioms"];
        pa = proof["ProofAssociation"];
        ConfirmAssert[ContainsAll[Keys[pa], {lemma, conclusion}]];
        vars = proof["Variables"];
        If[ MatchQ[lemma, {"Hypothesis", _}] && KeyExistsQ[pa, {"EquationalizedHypothesis", lemma[[2]]}],
            lemma = {"EquationalizedHypothesis", lemma[[2]]}
        ];
        If[ MatchQ[lemma, {"Hypothesis" | "EquationalizedHypothesis", _}],
            justification = Confirm[Quiet[justifyLemma[pa, vars, terminal, conclusion, simplify]]],
            justification = Confirm[Quiet[justifyLemma[pa, vars, terminal, lemma, simplify]]]
        ];
        If[ MatchQ[lemma, {"Hypothesis" | "EquationalizedHypothesis", _}]
        &&
        MatchQ[conclusion, {"Conclusion", _}],
            justification = Rest[justification];
            With[{n = First[FirstPosition[justification, {{lemma[[1]], _}, __}, {0}, 1]]},
                lemma = conclusion = justification[[n, 1]];
                justification = Most[
                    If[ justification[[n, 2]] === Right,
                        RotateRight[reverseJustification[justification], n - 1],
                        RotateLeft[justification, n]
                    ]
                ];
            ]
        ];
        justification = simplify[justification];
        If[reverse, justification = reverseJustification[justification]];
        statement = ReleaseHold[List @@@ patternsToSymbols[pa[conclusion, "Statement"]]];
        {rules, substitutions, bindings, rewrites, path} = Transpose[
            FoldPairList[
                List
                /*
                Replace[
                    {prevExpr_, {{key_, orientation_, position_}, nextExpr_}}
                    :>
                    Block[{rule, ruleRepl, newRule, uniq, substitution, repl, newExpr},
                        rule = ReleaseHold[
                            List
                            @@@
                            symbolsToPatterns[If[orientation === Left, Reverse[#1, 2]&, Identity][pa[key, "Statement"]], vars]
                        ];
                        uniq = uniquify[rule];
                        newRule = makeReplaceRule @@ {rule[[1]], rule[[2]] /. uniq};
                        newExpr = ReplaceAt[prevExpr, newRule, Replace[position, {} -> {{}}]];
                        substitution = Confirm[unify[newExpr, nextExpr], {prevExpr, newExpr, nextExpr, position, newRule}];
                        ruleRepl = Association[DeleteCases[Normal[(ReplaceAll[substitution]) /@ uniq], _[x_, x_]]];
                        newRule = makeReplaceRule @@ (rule /. ruleRepl);
                        repl = If[position === {}, Replace[newRule], ReplaceAt[newRule, position]];
                        newExpr = newExpr /. substitution;
                        {
                            {Rule @@ rule, ruleRepl, unify[rule[[1]], First[Extract[prevExpr, {position}]]], repl, newExpr},
                            newExpr
                        }
                    ]
                ],
                justification[[1]],
                Partition[justification[[2 ;; All]], 2]
            ]
        ];
        path = Prepend[path, justification[[1]]];
        With[{
            rewriteTest = With[
                {falsePositions = PositionIndex[MapThread[SameQ, {justification[[1 ;; All ;; 2]], path}]][False]},
                If[ MissingQ[falsePositions],
                    Success["Rewrite", Association["MessageTemplate" -> "Running rewrite sequence reproduced the path."]],
                    Failure[
                        "Rewrite",
                        Association[
                            "MessageTemplate" -> "Running rewrite sequence failed at positions `` of the path.",
                            "MessageParameters" -> falsePositions
                        ]
                    ]
                ]
            ]
        },
            If[ TrueQ[OptionValue["Canonicalize"]],
                With[{
                    canon = patternsToSymbols[canonicalizePatterns[{Insert[Most[path], Last[path], 2], substitutions}]]
                },
                    {path, rewrites, substitutions, bindings} = {path, rewrites, substitutions, bindings} /. canon
                ]
            ];
            If[FailureQ[rewriteTest], Append["DebugPath" -> justification[[1 ;; All ;; 2]]], Identity][
                Association[
                    "ProofObject" -> proof,
                    "RewriteTest" -> rewriteTest,
                    "Justification" -> justification[[2 ;; All ;; 2]],
                    "Rewrites" -> rewrites,
                    "Rules" -> rules,
                    "Substitutions" -> substitutions,
                    "Bindings" -> bindings,
                    "Path" -> path
                ]
            ][[Replace[prop, {All -> Sequence[], keys_List :> Key /@ keys}]]]
        ]
    ]
]

Options[findEquationalPath] = {
    "Reverse" -> False,
    "TerminalLemmas" -> {},
    "Simplify" -> True,
    "Canonicalize" -> False,
    "Cache" -> True
}

justifyLemma[pa_Association, _, _, lemma_, _] /; $useJustifyLemmaCache := With[{key = {KeySelect[pa, MatchQ[{"Axiom" | "Hypothesis", _}]][[All, Key["Statement"]]], lemma}},
    $justifyLemmaCache[key] /; KeyExistsQ[$justifyLemmaCache, key]
]

justifyLemma[pa_Association, vars_, terminal_List, key : {Except["CriticalPairLemma"], _}, simplify_] := $justifyLemmaCache[{KeySelect[pa, MatchQ[{"Axiom" | "Hypothesis", _}]][[All, Key["Statement"]]], key}] = Enclose[
    Block[{
        lemma = pa[key],
        statement,
        proof,
        input,
        construct,
        rule,
        subInput,
        inputOrientation,
        orientation,
        side,
        subPosition,
        position,
        ruleSub,
        inputJustification,
        constructJustification,
        justification
    },
        statement = lemma["Statement"];
        proof = lemma["Proof"];
        If[ MemberQ[terminal, key] || proof === Association[] || Keys[proof] === {"Input"},
            Return[Insert[ReleaseHold[List @@@ symbolsToPatterns[lemma["Statement"], vars]], {key, Right, {}}, {2}]]
        ];
        input = pa[proof["Input"], "Statement"];
        construct = pa[proof["Construct"], "Statement"];
        inputOrientation = proof["InputOrientation"];
        orientation = Replace[proof["ConstructSide"], {2 -> -1}] * proof["Orientation"];
        side = Replace[proof["Side"], 2 -> -1];
        subPosition = proof["Position"];
        position = Prepend[side][subPosition];
        inputJustification = If[inputOrientation === -1, reverseJustification, Identity][ConfirmBy[justifyLemma[pa, vars, terminal, proof["Input"], simplify], ListQ]];
        constructJustification = If[orientation === -1, reverseJustification, Identity][ConfirmBy[justifyLemma[pa, vars, terminal, proof["Construct"], simplify], ListQ]];
        ruleSub = With[{
            lhsVars = Union[Cases[constructJustification[[1]], _Pattern, All]],
            rhsVars = Union[Cases[constructJustification[[-1]], _Pattern, All]]
        },
            Join[
                AssociationMap[First, Intersection[lhsVars, rhsVars]],
                Confirm[unify[constructJustification[[-1]], proof["Rule"][[-1]], Complement[rhsVars, lhsVars]]]
            ]
        ];
        rule = Rule @@ {constructJustification[[1]], constructJustification[[-2]] /. ruleSub};
        input = inputJustification[[side]];
        subInput = Extract[inputJustification, position];
        constructJustification = Riffle[
            MapAt[Join[subPosition, #1]&, constructJustification[[2 ;; All ;; 2]], {All, 3}],
            (replacePart[input, subPosition -> #1]&) /@
                Rest[
                    FoldList[
                        Replace[#1, makeReplaceRule @@ #2]&,
                        subInput,
                        Partition[constructJustification[[1 ;; All ;; 2]], 2, 1]
                    ]
                ]
        ];
        justification = If[ side === 1,
            Join[reverseJustification[constructJustification], inputJustification],
            Join[inputJustification, constructJustification]
        ];
        justification = justification /.
            (If[MemberQ[vars, #1], Pattern[#1, _], #1]&)
            /@
            Confirm[unify[justification[[{1, -1}]], patternsToSymbols[ReleaseHold[List @@@ statement]]]];
        If[! MatchQ[key, {"Conclusion", _}], justification = simplify[justification]];
        justification
    ]
]

justifyLemma[pa_Association, vars_, terminal_, key : {"CriticalPairLemma", _}, simplify_] := $justifyLemmaCache[{KeySelect[pa, MatchQ[{"Axiom" | "Hypothesis", _}]][[All, Key["Statement"]]], key}] = Enclose[
    Block[{
        lemma = pa[key],
        statement,
        proof,
        pos,
        side,
        subPosition,
        orientation,
        matchingOrientation,
        constructJustification,
        matchJustification,
        subExpr,
        uniq,
        lhs,
        unification,
        input,
        justification
    },
        statement = lemma["Statement"];
        If[ MemberQ[terminal, key],
            Return[Insert[ReleaseHold[List @@@ symbolsToPatterns[statement, vars]], {key, Right, {}}, {2}]]
        ];
        proof = lemma["Proof"];
        subPosition = proof["Position"];
        pos = Prepend[-1][subPosition];
        orientation = (-Replace[proof["Side"], 2 -> -1]) * proof["Orientation"];
        matchingOrientation = Replace[proof["MatchingSide"], {2 -> -1}] * proof["MatchingOrientation"];
        constructJustification = If[orientation === -1, reverseJustification, Identity][ConfirmBy[justifyLemma[pa, vars, terminal, proof["Construct"], simplify], ListQ]];
        matchJustification = If[matchingOrientation === -1, reverseJustification, Identity][ConfirmBy[justifyLemma[pa, vars, terminal, proof["MatchingConstruct"], simplify], ListQ]];
        subExpr = Extract[constructJustification, pos];
        uniq = uniquify[matchJustification];
        lhs = matchJustification[[1]] /. uniq;
        unification = ResourceFunction["MostGeneralUnifier"][Evaluate[subExpr], Evaluate[lhs]];
        constructJustification = constructJustification /. unification;
        matchJustification = matchJustification /. uniq /. unification;
        input = constructJustification[[-1]];
        matchJustification = Riffle[
            MapAt[Join[subPosition, #1]&, {All, 3}][matchJustification[[2 ;; All ;; 2]]],
            (replacePart[input, subPosition -> #1]&) /@
                Rest[
                    FoldList[
                        Replace[#1, makeReplaceRule @@ #2]&,
                        extract[input, subPosition],
                        Partition[matchJustification[[1 ;; All ;; 2]], 2, 1]
                    ]
                ]
        ];
        justification = Catenate[{If[side === 1, reverseJustification, Identity][constructJustification], matchJustification}];
        justification = Block[{$ModuleNumber = 1},
            justification /. uniquify[justification]
        ];
        justification = justification /.
            (If[MemberQ[vars, #1], Pattern[#1, _], #1]&)
            /@
            Confirm[unify[justification[[{1, -1}]], patternsToSymbols[ReleaseHold[List @@@ statement]]]];
        simplify[justification]
    ]
]

Attributes[makeReplaceRule] = {HoldRest}

makeReplaceRule[lhs_, rhs_] := With[{
    newRHS = Hold[rhs]
    /.
    (Verbatim[#1] -> First[#1]&) /@ DeleteDuplicates[Cases[lhs, _Pattern, All, Heads -> True]]
},
    ReleaseHold[RuleDelayed @@ {lhs, newRHS}]
]

patternsToSymbols[expr_] := expr /. Verbatim[Pattern][name_, _] :> name

replacePart[expr_, pos : {_Integer...} -> part_] := If[pos === {}, part, ReplacePart[expr, pos -> part]]

replacePart[expr_, rules_List] := ReplacePart[expr, MapAt[Replace[{} -> {{}}], {1}] /@ rules]

reverseJustification[justification_List] := MapAt[Replace[{Left -> Right, Right -> Left}], Reverse[justification], {2 ;; All ;; 2, 2}]

simplifyJustification[justification_List] := Block[{duplicates = Keys[Select[Counts[justification[[1 ;; All ;; 2]]], #1 > 1&]], positions},
    positions = (Position[justification, Verbatim[#1], {1}]&) /@ duplicates;
    Delete[
        justification,
        List
        /@
        Union
        @@
        DeleteDuplicates[ReverseSortBy[(Range @@ (MinMax[Catenate[#1]] + {1, 0})&) /@ positions, Length], IntersectingQ]
    ]
]

symbolsToPatterns[expr_, syms_List] := expr /. Prepend[(#1 -> Pattern[#1, _]&) /@ syms, p_Pattern :> p]

unify[expr1_, expr2_] := unify[expr1, expr2, Union[Cases[expr1, _Pattern, All]]]

unify[expr1_, expr2_, patts_List] := Enclose[
    ConfirmBy[
        AssociationThread[patts, ConfirmBy[Replace[expr2, Rule @@ {expr1, First /@ patts}], ListQ]],
        AssociationQ,
        "Unification failed."
    ]
]

uniquify[expr_] := Association[
    Union[Cases[expr, x_Pattern, All, Heads -> True]] /.
        x_Pattern
        :>
        x -> Pattern @@ {Unique[Symbol[First[StringSplit[SymbolName[First[x]], "$"]]]], Last[x]}
]

uniquify[expr1_, expr2_] := Association[
    Intersection[Cases[expr1, _Pattern, All, Heads -> True], Cases[expr2, _Pattern, All, Heads -> True]]
    /.
    x_Pattern
    :>
    x -> Pattern @@ {Unique[Symbol[First[StringSplit[SymbolName[First[x]], "$"]]]], Last[x]}
]

$justifyLemmaCache = <||>

$useJustifyLemmaCache = True (* === end ported WolframFunctionRepository FindEquationalPath === *)

(* The replacement-path option surface FindEquationalPath exposes (string
   keys, as in the ported Options[findEquationalPath]).  Joined with
   TFindProof's options so the [thm$, axioms$] form can pass through both a
   proof budget and a path option in one call. *)

$atpEqPathOptions = {
    "Reverse" -> False,
    "TerminalLemmas" -> {},
    "Simplify" -> True,
    "Canonicalize" -> False,
    "Cache" -> True
};

Options[TFindEquationalPath] = Join[$atpEqPathOptions, Options[TFindProof]];

(* A property selector is a single name, the All wildcard, or a list of
   names: the prop argument findEquationalPath accepts. *)

atpEqPathPropQ[p_] := MatchQ[p, _String | All | {___String}];

(* ProofObject form: hand the object straight to the ported walker.  Falls
   back to the dataset re-walk (atpDatasetGoalPath, in ATP_Strings.wl) for
   the bare path when the ported walker cannot reconstruct it, e.g. a
   string-rewriting proof whose CriticalPairLemma steps the upstream
   unifier does not handle.  TStringPath uses that re-walk directly, so the
   string surface never depends on this. *)

TFindEquationalPath[po_ProofObject, opts : OptionsPattern[]] := TFindEquationalPath[po, "Path", opts]

TFindEquationalPath[po_ProofObject, prop_ ? atpEqPathPropQ, opts : OptionsPattern[]] := Block[{
    res = findEquationalPath[po, Automatic, Automatic, prop, Sequence @@ FilterRules[{opts}, $atpEqPathOptions]]
},
    Which[ 
        ! FailureQ[res],
            res,
        prop === "Path",
            atpDatasetEqPath[po],
        True,
            res
    ]
]

(* Theorem + axioms form: prove to a ProofObject via TFindProof, then walk
   it.  $Failed when the goal is not proved. *)

TFindEquationalPath[thm_, axioms_, opts : OptionsPattern[]] := TFindEquationalPath[thm, axioms, "Path", opts]

TFindEquationalPath[thm_, axioms_, prop_ ? atpEqPathPropQ, opts : OptionsPattern[]] := Block[
    {po = TFindProof[thm, axioms, "ProofObject", Sequence @@ FilterRules[{opts}, Options[TFindProof]]]},
    If[ MatchQ[po, _ProofObject],
        TFindEquationalPath[po, prop, Sequence @@ FilterRules[{opts}, $atpEqPathOptions]],
        $Failed
    ]
]

End[];

EndPackage[];