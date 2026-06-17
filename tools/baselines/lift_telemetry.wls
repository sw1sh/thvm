#!/usr/bin/env wolframscript
(* Sweep TWaldmeisterProofObject[..., LiftToProofObject -> True]
   across the 8 easy parity cases and record what works:
     ProofObject head?    structural success
     ProofFunction Fn?    property dispatch wired
     ProofGraph Graph?    DAG accessible
     ProofLength int?     basic stats
     pf[Theorems] OK?     full verification

   Emits tools/baselines/lift_telemetry.tsv -- one row per case. *)
SetDirectory["/Users/swish/src/thvm"];
PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];
$RecursionLimit = 16384;
Quiet @ TFindProof["InverseOfInverse", "GroupAxioms",
    Method -> "Completion", TimeConstraint -> 2.0];

cases = {
    {"AbelianGroupAxioms", "InverseOfInverse"},
    {"AbelianGroupAxioms", "InverseOfComposite"},
    {"AbelianGroupAxioms", "ImpliesMcCuneAxioms"},
    {"AbelianGroupAxioms", "ImpliesAbelianMcCuneAxioms"},
    {"BooleanAxioms",      "DoubleNegation"},
    {"GroupAxioms",        "InverseOfInverse"},
    {"HillmanAxioms",      "Commutativity"},
    {"MeredithAxioms",     "Commutativity"}
};

probe[{theory_, thm_}] := Block[
    {po, head, pfHead, pgHead, plen, verifyOK, verifyHead, n},
    po = Quiet @ WolframInstitute`THVMLink`ATP`TWaldmeisterProofObject[theory, thm,
        TimeConstraint -> 12,
        "LiftToProofObject" -> True];
    head = Head[po];
    If[ head =!= ProofObject,
        Return[{theory, thm, "non-PO", "-", "-", "-", "-", "-"}]];
    pfHead = Quiet @ Head[po["ProofFunction"]];
    pgHead = Quiet @ Head[po["ProofGraph"]];
    plen   = Quiet @ po["ProofLength"];
    n      = If[ Length[po] >= 4,
        Length @ Lookup[po[[4]], "Proof", {}], "-"];
    (* Try full verification (the deep check). *)
    verifyHead = Quiet @ Head[po["ProofFunction"][po["Theorems"]]];
    verifyOK = verifyHead === Success;
    {theory, thm, ToString[head], ToString[pfHead],
        ToString[pgHead], ToString[plen],
        If[ verifyOK, "Success", ToString[verifyHead]], ToString[n]}
];

Print[StringRiffle[
    {"theory", "theorem", "head", "ProofFunction",
        "ProofGraph", "ProofLength", "Verify[Theorems]", "n"},
    "\t"]];
rows = probe /@ cases;
Scan[Print[StringRiffle[#, "\t"]] &, rows];

Export["tools/baselines/lift_telemetry.tsv",
    Prepend[rows, {"theory", "theorem", "head", "ProofFunction",
        "ProofGraph", "ProofLength", "Verify[Theorems]", "n"}],
    "TSV"];
Print[];
Print["wrote tools/baselines/lift_telemetry.tsv"];
