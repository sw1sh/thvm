#!/usr/bin/env wolframscript
(* Variant of run_thvm_one.wls that accepts a Method spec as the
   4th positional arg (a WL-expression string).  Outputs the same
   TSV-format line. *)
PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];
args = $ScriptCommandLine;
theory = args[[2]];
thm = args[[3]];
TC = ToExpression[args[[4]]];
methodExpr = ToExpression[args[[5]]];

{t, r} = AbsoluteTiming @ TimeConstrained[
    TFindProof[thm, theory, Method -> methodExpr, TimeConstraint -> N[TC]],
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
    Head[r] === ProofObject,
        ToString @ Quiet @ Check[r["ProofLength"], "-"],
    ListQ[r] && AllTrue[r, Head[#] === ProofObject &],
        ToString[Total[Quiet @ Check[#["ProofLength"], 0] & /@ r]],
    True, "-"];

isProofResult = (Head[r] === ProofObject) ||
    (ListQ[r] && AllTrue[r, Head[#] === ProofObject &]);
If[ isProofResult,
    Module[{outDir = "tools/baselines/proofs_thvm", f},
        If[ ! DirectoryQ[outDir], CreateDirectory[outDir]];
        f = FileNameJoin[{outDir, theory <> "__" <> thm <> ".m"}];
        Quiet @ Check[Put[r, f], $Failed]]];

WriteString["stdout", theory, "\t", thm, "\t", status, "\t",
    ToString[Round[t, 0.01]], "\t", proofLen, "\t", verifies, "\n"];
