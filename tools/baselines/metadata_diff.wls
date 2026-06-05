#!/usr/bin/env wolframscript
(* Side-by-side diff of preset vs CLI ProofObject metadata.
   For each construct key, compare the Proof field's keys + values
   in FullForm and report any differences.  Useful for finding
   the specific field that prevents pf[Theorems] verification on
   the CLI-lifted side. *)
SetDirectory["/Users/swish/src/thvm"];
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
$RecursionLimit = 16384;
If[ Length[$ScriptCommandLine] < 3,
    Print["usage: metadata_diff.wls THEORY THM"];
    Exit[2]];
theory = $ScriptCommandLine[[2]];
thm    = $ScriptCommandLine[[3]];

Quiet @ TFindProof["InverseOfInverse", "GroupAxioms",
    Method -> "Completion", TimeConstraint -> 2.0];

pP = TFindProof[thm, theory,
    Method -> "Waldmeister", TimeConstraint -> 8];
pC = THVMLink`ATP`TWaldmeisterProofObject[theory, thm,
    TimeConstraint -> 12,
    "LiftToProofObject" -> True];

If[ Head[pP] =!= ProofObject || Head[pC] =!= ProofObject,
    Print["ERROR: head mismatch -- pP head=", Head[pP],
        " pC head=", Head[pC]];
    Exit[1]];

prfP = Association[pP[[4]]["Proof"]];
prfC = Association[pC[[4]]["Proof"]];

allKeys = Union[Keys[prfP], Keys[prfC]];
sortKey[k_] := Block[
    {idx = FirstPosition[
        {"Axiom", "Hypothesis", "CriticalPairLemma",
            "SubstitutionLemma", "Conclusion"},
        First[k], {99}][[1]]},
    {idx, Last[k]}];

Print["=== metadata diff: ", theory, " / ", thm, " ==="];
Print["preset entries:  ", Length[prfP]];
Print["cli entries:     ", Length[prfC]];
Print[];

Scan[
    k |-> Block[{eP, eC, proofP, proofC, allFields},
        eP = Lookup[prfP, Key[k], Missing[]];
        eC = Lookup[prfC, Key[k], Missing[]];
        Print["--- ", k, " ---"];
        If[ MissingQ[eP],
            Print["  ONLY-CLI"];
            Return[]];
        If[ MissingQ[eC],
            Print["  ONLY-PRESET"];
            Return[]];
        (* Compare Statement first. *)
        If[ eP["Statement"] === eC["Statement"],
            Print["  Statement: MATCH ", InputForm @ Short[
                eP["Statement"], 30]],
            Print["  Statement: MISMATCH"];
            Print["    preset: ", InputForm @ Short[
                eP["Statement"], 30]];
            Print["    cli:    ", InputForm @ Short[
                eC["Statement"], 30]]];
        (* Compare Proof keys. *)
        proofP = eP["Proof"];
        proofC = eC["Proof"];
        allFields = Union[Keys[proofP], Keys[proofC]];
        Scan[
            f |-> Block[{vP, vC},
                vP = Lookup[proofP, f, Missing[]];
                vC = Lookup[proofC, f, Missing[]];
                Which[
                    MissingQ[vP] && MissingQ[vC], Null,
                    MissingQ[vP], Print["  Proof[", f, "]:",
                        " ONLY-CLI = ", InputForm @ Short[vC, 20]],
                    MissingQ[vC], Print["  Proof[", f, "]:",
                        " ONLY-PRESET = ", InputForm @ Short[vP, 20]],
                    vP === vC, Null,
                    True,
                        Print["  Proof[", f, "]: MISMATCH"];
                        Print["    preset: ", InputForm @ Short[vP, 20]];
                        Print["    cli:    ", InputForm @ Short[vC, 20]]]],
            allFields]],
    SortBy[allKeys, sortKey]];
