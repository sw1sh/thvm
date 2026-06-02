#!/usr/bin/env wolframscript
(* Side-by-side proof-SHAPE comparison: extract a structural
   signature from both thvm's ProofObject and the parsed Vampire
   SZS DAG, then report the divergence.

   Shape signature shape (per side):
       <|
         "RuleHistogram" -> <|rule -> count, ...|>,
         "ParentDegree"  -> Sort @ {n_parents_of_each_inference},
         "DAGDepth"      -> longest-path length from input to closure,
         "AxiomFanIn"    -> #distinct-input-axioms cited as parents,
         "Inferences"    -> total non-axiom inferences
       |>

   Two proofs are "structurally identical" if their signatures are
   `===`.  Two proofs are "shape-similar" if RuleHistogram and
   DAGDepth match.  Differences in CPL count vs superposition count
   are the FIRST divergence to investigate.

   Usage:
     compare_proof_shape.wls <theory> <thm> <preset> <TC_seconds> *)

SetDirectory["/Users/swish/src/thvm"];
PacletDirectoryLoad["wl/THVMLink"];
PacletDirectoryLoad["/Users/swish/src/wolfram/AISkills/examples/WolframParser"];
Get["THVMLink`"];
Needs["Wolfram`Parser`"];
$RecursionLimit = 16384;

args   = $ScriptCommandLine;
theory = args[[2]];
thm    = args[[3]];
preset = args[[4]];
TC     = ToExpression[args[[5]]];

debugPrint[a___] := WriteString[
    "stderr", StringJoin @@ Map[ToString, {a}], "\n"
]

(* WARMUP -- see compare_proof_one.wls cold-start note. *)
Quiet @ TimeConstrained[
    TFindProof["InverseOfInverse", "GroupAxioms",
        Method -> "Completion", TimeConstraint -> 2.0],
    3, $TimedOut
];

(* ---- thvm side -------------------------------------------------- *)
debugPrint["[thvm:", preset, "] running ", theory, "/", thm];
{thvmT, thvmR} = AbsoluteTiming @ TimeConstrained[
    TFindProof[thm, theory, Method -> preset, TimeConstraint -> N[TC]],
    TC + 10, $TimedOut
];
thvmStatus = Which[
    MatchQ[thvmR, _ProofObject], "Proved",
    ListQ[thvmR] && AllTrue[thvmR, MatchQ[#, _ProofObject] &], "Proved",
    thvmR === $TimedOut, "TimedOut",
    True, "Failed"
];

(* thvm proof dataset -> signature.  The dataset is an Association
   keyed by {Type, n}; each value has Proof -> <|... parents ...|>. *)
thvmShape = If[
    thvmStatus === "Proved",
    Block[{ds, keys, rules, parentsOf, degrees, axiomCount},
        (* ProofDataset returns a Dataset; pull the underlying
           Association so Keys[] returns a list, not a Dataset.
           Keys are {String, Integer} tuples like {"Axiom", 1}
           -- the Symbol-like print form hides the string quotes. *)
        ds   = Normal @ thvmR["ProofDataset"];
        keys = Keys[ds];
        rules = Cases[keys,
            {t_String /; ! MemberQ[{"Axiom", "Hypothesis"}, t], _Integer} :> t];
        (* parents per inference = the parent constructs cited by
           the entry's Proof Association under any of these slot names. *)
        parentsOf[k_] := Block[{p = Lookup[ds[k], "Proof", <||>], slotVals},
            slotVals = Map[
                Lookup[p, #, Missing["KeyAbsent"]] &,
                {"Construct", "MatchingConstruct", "Input"}
            ];
            DeleteDuplicates @ Cases[slotVals, {_String, _Integer}]
        ];
        degrees = Sort @ Map[Length @ parentsOf[#] &, keys];
        axiomCount = Length @ Cases[keys, {"Axiom", _}];
        <|
            "RuleHistogram" -> KeySort @ Counts[rules],
            "ParentDegree"  -> degrees,
            "AxiomFanIn"    -> axiomCount,
            "Inferences"    -> Length[rules]
        |>
    ],
    <||>
];

(* ---- vampire side ----------------------------------------------- *)
debugPrint["[vampire-cli] running"];
vampR     = TVampireProof[theory, thm, TimeConstraint -> TC];
vampStat  = vampR["Status"];
vampShape = If[
    vampStat === "Proved",
    Block[{infs = vampR["Inferences"], rules, parentsOf, degrees},
        rules = Cases[infs,
            a_Association /; a["Rule"] =!= "file" :> a["Rule"]
        ];
        parentsOf[i_Association] := Lookup[i, "Parents", {}];
        degrees = Sort @ Map[Length @ parentsOf[#] &, infs];
        <|
            "RuleHistogram" -> KeySort @ Counts[rules],
            "ParentDegree"  -> degrees,
            "AxiomFanIn"    -> Length @ Cases[infs,
                a_Association /; a["Rule"] === "file"],
            "Inferences"    -> Length[rules]
        |>
    ],
    <||>
];

(* ---- rule-name mapping thvm <-> Vampire -------------------------
   Apply BEFORE comparing histograms, so a faithful port shows as
   "identical" instead of as a vocabulary mismatch.  See
   docs/atp/vampire_case_teardown.md "Inference-rule mapping". *)
canonRule[r_String] := Replace[r, {
    "CriticalPairLemma"              -> "superposition",
    "SubstitutionLemma"              -> "forward_demodulation",
    "Conclusion"                     -> "forward_subsumption_resolution",
    "trivial_inequality_removal"     -> "forward_subsumption_resolution"
}]

(* Wrap in {} -- Merge takes a LIST of associations, not one. *)
canonHist[hist_Association] := Merge[
    {KeyMap[canonRule, hist]},
    Total
]

(* ---- report ----------------------------------------------------- *)
debugPrint[];
debugPrint["================================================"];
debugPrint["theory/thm:    ", theory, "/", thm];
debugPrint["preset:        ", preset, "    TC: ", TC, "s"];
debugPrint["thvm status:   ", thvmStatus, " (", Round[thvmT, 0.01], "s)"];
debugPrint["vampire status:", vampStat, " (", vampR["Seconds"], "s)"];
debugPrint[];

If[ thvmStatus === "Proved" && vampStat === "Proved",
    Block[{thvmCanon, vampCanon, deltas},
        thvmCanon = canonHist @ thvmShape["RuleHistogram"];
        vampCanon = canonHist @ vampShape["RuleHistogram"];
        debugPrint["----- thvm shape -----"];
        debugPrint["  rules raw:    ", thvmShape["RuleHistogram"]];
        debugPrint["  rules canon:  ", thvmCanon];
        debugPrint["  parents:      ", thvmShape["ParentDegree"]];
        debugPrint["  axioms:       ", thvmShape["AxiomFanIn"]];
        debugPrint["  inf-tot:      ", thvmShape["Inferences"]];
        debugPrint["----- vampire shape -----"];
        debugPrint["  rules raw:    ", vampShape["RuleHistogram"]];
        debugPrint["  rules canon:  ", vampCanon];
        debugPrint["  parents:      ", vampShape["ParentDegree"]];
        debugPrint["  axioms:       ", vampShape["AxiomFanIn"]];
        debugPrint["  inf-tot:      ", vampShape["Inferences"]];
        debugPrint[];
        debugPrint["----- divergence (canonicalized) -----"];
        debugPrint["  rules identical?", thvmCanon === vampCanon];
        deltas = Merge[
            {thvmCanon, -vampCanon},
            Total
        ];
        debugPrint["  per-rule delta (thvm - vamp): ", deltas];
        debugPrint["  axiomFanIn match?", thvmShape["AxiomFanIn"] === vampShape["AxiomFanIn"]];
        debugPrint["  inf-tot match?  ", thvmShape["Inferences"] === vampShape["Inferences"]];
    ]
];
debugPrint["================================================"];

(* TSV emit: theory thm preset thvm_status thvm_secs thvm_inf vamp_status
             vamp_secs vamp_inf identical? *)
WriteString["stdout",
    StringRiffle[{
        theory, thm, preset, thvmStatus, ToString[Round[thvmT, 0.01]],
        ToString @ Lookup[thvmShape, "Inferences", 0],
        vampStat, ToString[vampR["Seconds"]],
        ToString @ Lookup[vampShape, "Inferences", 0],
        ToString[thvmShape === vampShape]
    }, "\t"],
    "\n"
];
