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
print[a___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {a}], "\n"];

If[ Length[$ScriptCommandLine] < 3,
    print["usage: metadata_diff.wls THEORY THM"];
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
    print["ERROR: head mismatch -- pP head=", Head[pP],
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

print["=== metadata diff: ", theory, " / ", thm, " ==="];
print["preset entries:  ", Length[prfP]];
print["cli entries:     ", Length[prfC]];
print[];

Scan[
    k |-> Block[{eP, eC, proofP, proofC, allFields},
        eP = Lookup[prfP, Key[k], Missing[]];
        eC = Lookup[prfC, Key[k], Missing[]];
        print["--- ", k, " ---"];
        If[ MissingQ[eP],
            print["  ONLY-CLI"];
            Return[]];
        If[ MissingQ[eC],
            print["  ONLY-PRESET"];
            Return[]];
        (* Compare Statement first. *)
        If[ eP["Statement"] === eC["Statement"],
            print["  Statement: MATCH ", InputForm @ Short[
                eP["Statement"], 30]],
            print["  Statement: MISMATCH"];
            print["    preset: ", InputForm @ Short[
                eP["Statement"], 30]];
            print["    cli:    ", InputForm @ Short[
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
                    MissingQ[vP], print["  Proof[", f, "]:",
                        " ONLY-CLI = ", InputForm @ Short[vC, 20]],
                    MissingQ[vC], print["  Proof[", f, "]:",
                        " ONLY-PRESET = ", InputForm @ Short[vP, 20]],
                    vP === vC, Null,
                    True,
                        print["  Proof[", f, "]: MISMATCH"];
                        print["    preset: ", InputForm @ Short[vP, 20]];
                        print["    cli:    ", InputForm @ Short[vC, 20]]]],
            allFields]],
    SortBy[allKeys, sortKey]];
