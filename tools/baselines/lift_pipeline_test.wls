#!/usr/bin/env wolframscript
(* Diagnostic: feed PRESET's ProofObject components through the
   lift's wrap pattern (ProofObject["EquationalLogic", goal,
   axioms, data]) and confirm verification still passes.

   Establishes that the lift's STRUCTURAL pipeline is correct --
   any pf[Theorems] failures on CLI-derived ProofObjects come
   from specific field-value differences in the reconstructed
   metadata, not from how arg1/2/3/4 are assembled. *)
SetDirectory["/Users/swish/src/thvm"];
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
$RecursionLimit = 16384;
Quiet @ TFindProof["InverseOfInverse", "GroupAxioms",
    Method -> "Completion", TimeConstraint -> 2.0];

pP = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
    Method -> "Waldmeister", TimeConstraint -> 8];

probe[label_, po_] := Block[
    {head = Head[po], verify},
    verify = If[ head === ProofObject,
        Head @ po["ProofFunction"][po["Theorems"]],
        "n/a (not ProofObject)"];
    Print[StringPadRight[label, 30], "  head=",
        StringPadRight[ToString[head], 12],
        "  verify=", verify]];

probe["preset (as-is)", pP];

(* Rebuild preset's ProofObject via my lift's wrap pattern. *)
po2 = ProofObject @@ {pP[[1]], pP[[2]], pP[[3]], pP[[4]]};
probe["preset rebuilt", po2];

po3 = ProofObject["EquationalLogic", pP[[2]], pP[[3]], pP[[4]]];
probe["preset re-wrapped", po3];

(* Re-emit each Proof entry through the lift's KeyValueMap pattern. *)
proofRebuilt = KeyValueMap[
    #1 -> Association[
        "Statement" -> #2["Statement"],
        "Proof" -> #2["Proof"]] &,
    Association[pP[[4]]["Proof"]]];
po4 = ProofObject["EquationalLogic", pP[[2]], pP[[3]],
    Association[pP[[4]], "Proof" -> proofRebuilt]];
probe["preset proof remapped", po4];
