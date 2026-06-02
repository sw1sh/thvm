#!/usr/bin/env wolframscript
(* Cross-Method proof comparator.  Runs TFindProof with TWO
   different Method specs on the same (theory, thm) and reports
   structural diff of the two resulting proofs.

   Methods can be ANY thvm Method spec, including the external CLI
   process methods ("VampireProcess", "TweeProcess") wired in
   commit 11be81ce.  The first arg's result is the "REFERENCE"
   (typically the external CLI = ground truth), the second arg's
   the "PORT" (typically the internal preset that should match).

   Output: per-side rule histograms + delta + identity verdict.

   Usage:
     compare_methods.wls <theory> <thm> <refMethod> <portMethod> <TC> *)

SetDirectory["/Users/swish/src/thvm"];
PacletDirectoryLoad["wl/THVMLink"];
PacletDirectoryLoad["/Users/swish/src/wolfram/AISkills/examples/WolframParser"];
Get["THVMLink`"];
Needs["Wolfram`Parser`"];
$RecursionLimit = 16384;

args       = $ScriptCommandLine;
theory     = args[[2]];
thm        = args[[3]];
refMethod  = args[[4]];
portMethod = args[[5]];
TC         = ToExpression[args[[6]]];

debugPrint[a___] := WriteString["stderr",
    StringJoin @@ Map[ToString, {a}], "\n"]

emit[a___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {a}], "\n"]

(* canonicalize per-side rule names so faithful ports collapse. *)
canonRule[r_String] := Replace[r, {
    "CriticalPairLemma"               -> "superposition",
    "SubstitutionLemma"               -> "forward_demodulation",
    "Conclusion"                      -> "forward_subsumption_resolution",
    "trivial_inequality_removal"      -> "forward_subsumption_resolution",
    "twee-rewrite"                    -> "forward_demodulation"
}]

(* Extract a normalized shape signature from any TFindProof result
   (ProofObject _ Association from process method).  Handles both
   shapes the package returns. *)
shapeOf[r_] := Block[
    {ds, keys, rules, rawHist},
    Which[
        MatchQ[r, _ProofObject],
            ds = Normal @ r["ProofDataset"];
            keys = Keys[ds];
            rules = Cases[keys,
                {t_String /; ! MemberQ[{"Axiom", "Hypothesis"}, t], _Integer} :> t];
            <|
                "Rules" -> KeySort @ Counts[Map[canonRule, rules]],
                "AxiomFanIn" -> Length @ Cases[keys, {"Axiom", _}],
                "Inferences" -> Length[rules]
            |>,
        AssociationQ[r] && r["Backend"] === "SZS",
            ds = Normal @ r["ProofDataset"];
            keys = Keys[ds];
            rules = Cases[keys,
                {t_String /; ! MemberQ[{"Axiom", "Hypothesis"}, t], _Integer} :> t];
            <|
                "Rules" -> KeySort @ Counts[Map[canonRule, rules]],
                "AxiomFanIn" -> Length @ Cases[keys, {"Axiom", _}],
                "Inferences" -> Length[rules]
            |>,
        AssociationQ[r] && r["Backend"] === "Twee-TSTP",
            rawHist = r["RuleHistogram"];
            <|
                "Rules" -> KeySort @ KeyMap[canonRule, rawHist],
                "AxiomFanIn" -> Length @ r["Axioms"],
                "Inferences" -> Total @ Values @ rawHist
            |>,
        True, <||>
    ]
]

statusOf[r_] := Which[
    MatchQ[r, _ProofObject], "Proved",
    AssociationQ[r] && r["Status"] === "Proved", "Proved",
    ListQ[r] && AllTrue[r, MatchQ[#, _ProofObject] &], "Proved",
    r === $TimedOut, "TimedOut",
    True, "Failed"
]

(* Warmup *)
debugPrint["[warmup]"];
Quiet @ TFindProof["InverseOfInverse", "GroupAxioms",
    Method -> "Completion", TimeConstraint -> 2.0];

debugPrint["[ref:", refMethod, "] running ", theory, "/", thm];
{refT, refR} = AbsoluteTiming @ TimeConstrained[
    TFindProof[thm, theory, Method -> refMethod, TimeConstraint -> N[TC]],
    TC + 10, $TimedOut
];
debugPrint["  -> ", statusOf[refR], " in ", Round[refT, 0.01], "s"];

debugPrint["[port:", portMethod, "] running"];
{portT, portR} = AbsoluteTiming @ TimeConstrained[
    TFindProof[thm, theory, Method -> portMethod, TimeConstraint -> N[TC]],
    TC + 10, $TimedOut
];
debugPrint["  -> ", statusOf[portR], " in ", Round[portT, 0.01], "s"];

refShape  = shapeOf[refR];
portShape = shapeOf[portR];

debugPrint[];
debugPrint["====================================================="];
debugPrint["theory/thm: ", theory, "/", thm];
debugPrint["    ref :", refMethod];
debugPrint["    port:", portMethod];
debugPrint[];
debugPrint["status        ",
    StringPadRight[statusOf[refR], 14],
    StringPadRight[statusOf[portR], 14]];
debugPrint["seconds       ",
    StringPadRight[ToString[Round[refT, 0.01]], 14],
    StringPadRight[ToString[Round[portT, 0.01]], 14]];
debugPrint["inferences    ",
    StringPadRight[ToString @ Lookup[refShape, "Inferences", "-"], 14],
    StringPadRight[ToString @ Lookup[portShape, "Inferences", "-"], 14]];
debugPrint["axiom fan-in  ",
    StringPadRight[ToString @ Lookup[refShape, "AxiomFanIn", "-"], 14],
    StringPadRight[ToString @ Lookup[portShape, "AxiomFanIn", "-"], 14]];
debugPrint[];
debugPrint["ref rule histogram:  ", Lookup[refShape, "Rules", <||>]];
debugPrint["port rule histogram: ", Lookup[portShape, "Rules", <||>]];

If[ AssociationQ[refShape["Rules"]] && AssociationQ[portShape["Rules"]],
    Block[{delta = Merge[{portShape["Rules"], -refShape["Rules"]}, Total]},
        debugPrint["per-rule delta (port - ref): ", delta];
        debugPrint["proofs identical?  ", refShape === portShape];
    ]
];
debugPrint["====================================================="];

emit[StringRiffle[{
    theory, thm, refMethod, portMethod,
    statusOf[refR], ToString[Round[refT, 0.01]],
    ToString @ Lookup[refShape, "Inferences", "-"],
    statusOf[portR], ToString[Round[portT, 0.01]],
    ToString @ Lookup[portShape, "Inferences", "-"],
    ToString[refShape === portShape]
}, "\t"]];
