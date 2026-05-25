#!/usr/bin/env wolframscript
(* Run thvm TFindProof on a single NotableTheorem under a hard
   wall-clock cap.  Mirrors run_wl_one.wls's TSV output line for
   line so the same downstream comparator parses both. *)
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
args = $ScriptCommandLine;
theory = args[[2]];
thm = args[[3]];
TC = ToExpression[args[[4]]];

{t, r} = AbsoluteTiming @ TimeConstrained[
    TFindProof[thm, theory, TimeConstraint -> N[TC]],
    TC + 2, $TimedOut];

verifyOne[p_] := Head @ Quiet @ Check[p["ProofFunction"][p["Theorems"]], $Failed];

status = Which[
    Head[r] === ProofObject, "PROVED",
    ListQ[r] && AllTrue[r, Head[#] === ProofObject &], "PROVED",
    r === $TimedOut, "TimedOut",
    True, "Failed"];

verifies = Which[
    Head[r] === ProofObject,
        ToString[verifyOne[r]],
    ListQ[r] && AllTrue[r, Head[#] === ProofObject &],
        Module[{vs = Map[verifyOne, r]},
            If[ AllTrue[vs, # === Success &], "Success", ToString[vs]]],
    True, "-"];

proofLen = Which[
    Head[r] === ProofObject && AssociationQ @ r["ProofDataset"],
        ToString @ Length[r["ProofDataset"]["MainSteps"]],
    ListQ[r] && AllTrue[r, Head[#] === ProofObject &],
        ToString[Total[Length[#["ProofDataset"]["MainSteps"]] & /@ r]],
    True, "-"];

WriteString["stdout", theory, "\t", thm, "\t", status, "\t",
    ToString[Round[t, 0.01]], "\t", proofLen, "\t", verifies, "\n"];
