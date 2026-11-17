(* Tests for Kernel/ATP/ProofCheck.wl: atpProofCheck replays an equational ProofObject from its
   four arguments alone, the check TFindProof falls back on where the kernel's ProofObject has no
   ProofFunction of its own.  Wolfram 15's proofs and THVMLink's give Wolfram 15's Success; a proof
   with one step changed gives a Failure that names that step.  $atpProofCheckNative makes
   TFindProof accept on the replay, so these tests check that path on Wolfram 15.  The recorded
   proofs are docs/ATP/data/fep_w15_records.wl, read from the repository root, where run.wls runs. *)

proofCheck = WolframInstitute`THVMLink`ATP`Private`atpProofCheck;
proofVerify = WolframInstitute`THVMLink`ATP`Private`atpProofVerify;
success = Success["ProofFunction", <|"MessageTemplate" -> "Success!"|>];
groupAxioms = {ForAll[{x, y, z}, op[op[x, y], z] == op[x, op[y, z]]], ForAll[x, op[e, x] == x], ForAll[x, op[inv[x], x] == e]};
problems = {
    {op[a, e] == a, groupAxioms},
    {op[a, inv[a]] == e, groupAxioms},
    {inv[op[a, b]] == op[inv[b], inv[a]], groupAxioms},
    {ForAll[{x}, op[x, e] == x], groupAxioms},
    {f[f[f[f[n]]]] == n, {ForAll[a, f[f[a]] == a]}},
    {a == c, {a == b, b == c}},
    {a == b, a == b},
    {"InverseOfInverse", "AbelianGroupAxioms"},
    {{op[a, e] == a, op[a, inv[a]] == e}, groupAxioms},
    {mortal[socrates], {ForAll[x, Implies[man[x], mortal[x]]], man[socrates]}}
};

(* the proof with its rows changed by change, a function of the list of rows *)
changed[p_ProofObject, change_] := ReplacePart[p, {4, "Proof"} -> change[p[[4, "Proof"]]]]
(* the row key -> the row with its "Proof" field set, or its Statement *)
withField[key_, field_String, value_][rows_] := Replace[rows, (key -> row_) :> key -> ReplacePart[row, {"Proof", field} -> value], {1}]
withStatement[key_, statement_][rows_] := Replace[rows, (key -> row_) :> key -> Append[row, "Statement" -> statement], {1}]
(* the step a Failure of the replay names, or the result itself *)
failedStep[result_] := Replace[result, {
    f : Failure["TFindProof", _] /; f["Tag"] === "pfstep" :> First[f["MessageParameters"]],
    f : Failure["TFindProof", _] :> f["Tag"]
}]
firstKey[p_ProofObject, kind_String] := SelectFirst[Keys[p[[4, "Proof"]]], MatchQ[{kind, _}]]
statementOf[p_ProofObject, key_] := Lookup[Association[p[[4, "Proof"]]], Key[key]]["Statement"]
(* a statement with its right side wrapped in inv, which no step derives *)
wrongStatement[HoldForm[l_ == r_]] := HoldForm[l == inv[r]]

(* ===== Wolfram 15's proofs ===== *)

VerificationTest[
    proofCheck /@ (FindEquationalProof @@@ problems),
    ConstantArray[success, Length[problems]],
    TestID -> "ATP/proofcheck/w15-proofs-hold"
]

VerificationTest[
    Module[{records = ReadList[FileNameJoin[{Directory[], "docs", "ATP", "data", "fep_w15_records.wl"}]], proofs},
        proofs = Cases[records, r : KeyValuePattern["Head" -> ProofObject] :> With[{arguments = r["Arguments"]},
            ProofObject[arguments[[1]], arguments[[2]], arguments[[3]],
                Append[arguments[[4]], "Proof" -> Normal[r["Values", "ProofDataset", "Normal"]]]]]];
        {Length[proofs] >= 10, Union[proofCheck /@ proofs]}
    ],
    {True, {success}},
    TestID -> "ATP/proofcheck/recorded-w15-proofs-hold"
]

(* ===== THVMLink's proofs, accepted on the replay ===== *)

VerificationTest[
    Block[{WolframInstitute`THVMLink`ATP`Private`$atpProofCheckNative = True},
        Module[{proofs = TFindProof[##, TimeConstraint -> 60] & @@@ problems},
            {Head /@ proofs, Union[proofCheck /@ proofs], Union[#["ProofFunction"][#["Theorems"]] & /@ proofs]}
        ]
    ],
    {ConstantArray[ProofObject, Length[problems]], {success}, {success}},
    TestID -> "ATP/proofcheck/thvmlink-proofs-hold-on-the-replay"
]

VerificationTest[
    Module[{p = TFindProof[op[a, e] == a, groupAxioms]},
        {proofVerify[p], Block[{WolframInstitute`THVMLink`ATP`Private`$atpProofCheckNative = True}, proofVerify[p]]}
    ],
    {success, success},
    TestID -> "ATP/proofcheck/verify-uses-the-proof-function-or-the-replay"
]

(* ===== changed proofs fail at the step changed ===== *)

VerificationTest[
    Module[{p = FindEquationalProof[op[a, e] == a, groupAxioms], key},
        key = firstKey[p, "CriticalPairLemma"];
        failedStep[proofCheck[changed[p, withStatement[key, wrongStatement[statementOf[p, key]]]]]] === key
    ],
    True,
    TestID -> "ATP/proofcheck/changed-critical-pair-statement-fails"
]

VerificationTest[
    Module[{p = FindEquationalProof[op[a, e] == a, groupAxioms], key, position},
        key = firstKey[p, "CriticalPairLemma"];
        position = Lookup[Association[p[[4, "Proof"]]], Key[key]]["Proof", "Position"];
        failedStep[proofCheck[changed[p, withField[key, "Position", Replace[position, {{} -> {1}, {1} -> {2}, _ -> {1}}]]]]] === key
    ],
    True,
    TestID -> "ATP/proofcheck/changed-critical-pair-position-fails"
]

VerificationTest[
    Module[{p = FindEquationalProof[op[a, e] == a, groupAxioms], key},
        key = firstKey[p, "SubstitutionLemma"];
        failedStep /@ {
            proofCheck[changed[p, withField[key, "Orientation", -Lookup[Association[p[[4, "Proof"]]], Key[key]]["Proof", "Orientation"]]]],
            proofCheck[changed[p, withField[key, "Side", 3 - Lookup[Association[p[[4, "Proof"]]], Key[key]]["Proof", "Side"]]]],
            proofCheck[changed[p, withField[key, "Position", {7}]]]
        }
    ],
    ConstantArray[{"SubstitutionLemma", 1}, 3],
    TestID -> "ATP/proofcheck/changed-substitution-orientation-side-position-fail"
]

VerificationTest[
    Module[{p = FindEquationalProof[op[a, e] == a, groupAxioms]},
        failedStep /@ {
            proofCheck[changed[p, withStatement[{"Axiom", 1}, HoldForm[op[\[FormalA], \[FormalA]] == \[FormalA]]]]],
            proofCheck[changed[p, withStatement[{"Hypothesis", 1}, HoldForm[op[a, e] == e]]]],
            proofCheck[changed[p, withStatement[{"Conclusion", 1}, HoldForm[e == e]]]],
            proofCheck[changed[p, DeleteCases[#, {"Conclusion", _} -> _] &]]
        }
    ],
    {{"Axiom", 1}, {"Hypothesis", 1}, {"Conclusion", 1}, "pfgoal"},
    TestID -> "ATP/proofcheck/changed-axiom-hypothesis-conclusion-fail"
]

VerificationTest[
    (* a lemma that rests on the goal's hypothesis is no lemma for all values: rewriting with it,
       or citing a step after it, does not hold *)
    Module[{p = FindEquationalProof[op[a, e] == a, groupAxioms], key},
        key = firstKey[p, "CriticalPairLemma"];
        failedStep /@ {
            proofCheck[changed[p, withField[key, "Construct", {"Hypothesis", 1}]]],
            proofCheck[changed[p, RotateRight]]
        }
    ],
    {{"CriticalPairLemma", 1}, {"Conclusion", 1}},
    TestID -> "ATP/proofcheck/goal-premise-and-later-step-fail"
]

VerificationTest[
    Module[{p = Block[{WolframInstitute`THVMLink`ATP`Private`$atpProofCheckNative = True}, TFindProof[inv[op[a, b]] == op[inv[b], inv[a]], groupAxioms]], key},
        key = firstKey[p, "CriticalPairLemma"];
        {
            failedStep[proofCheck[changed[p, withStatement[key, wrongStatement[statementOf[p, key]]]]]] === key,
            failedStep[proofCheck[changed[p, withField[key, "MatchingConstruct", key]]]] === key,
            Head[Block[{WolframInstitute`THVMLink`ATP`Private`$atpProofCheckNative = True},
                proofVerify[changed[p, withStatement[key, wrongStatement[statementOf[p, key]]]]]]]
        }
    ],
    {True, True, Failure},
    TestID -> "ATP/proofcheck/changed-thvmlink-proof-fails"
]

VerificationTest[
    Module[{p = FindEquationalProof[mortal[socrates], {ForAll[x, Implies[man[x], mortal[x]]], man[socrates]}]},
        failedStep[proofCheck[changed[p, withStatement[{"Hypothesis", 1}, man[socrates]]]]]
    ],
    {"Hypothesis", 1},
    TestID -> "ATP/proofcheck/changed-predicate-hypothesis-fails"
]

VerificationTest[
    {failedStep[proofCheck[1 + x]], proofCheck[{"EquationalLogic", a == b, {a == b}, <||>}]["Tag"]},
    {"pfform", "pfform"},
    TestID -> "ATP/proofcheck/not-a-proof-object"
]
