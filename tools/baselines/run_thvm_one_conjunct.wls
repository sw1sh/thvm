#!/usr/bin/env wolframscript
(* Run thvm TFindProof on a SINGLE conjunct of a NotableTheorem,
   identified by its 1-based index.  Same TC + status logic as
   run_thvm_one.wls but operates on the conjunct directly so a
   conjunct-2 segfault doesn't lose conjunct-1's proof.  Output:
   theory \t thm \t i_of_n \t status \t seconds \t proofLen \t verifies. *)
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
args = $ScriptCommandLine;
theory   = args[[2]];
thm      = args[[3]];
idx      = ToExpression[args[[4]]];
TC       = ToExpression[args[[5]]];

cjList = AxiomaticTheory[theory, "NotableTheorems"][thm];
cjList = If[ListQ[cjList], cjList, {cjList}];
n      = Length[cjList];
target = cjList[[idx]];

{t, r} = AbsoluteTiming @ TimeConstrained[
    TFindProof[target, theory, TimeConstraint -> N[TC]],
    TC + 10, $TimedOut];

verifyOne[p_] := Head @ Quiet @ Check[p["ProofFunction"][p["Theorems"]], $Failed];

status = Which[
    Head[r] === ProofObject, "PROVED",
    r === $TimedOut, "TimedOut",
    True, "Failed"];

verifies = Which[
    Head[r] === ProofObject, ToString[verifyOne[r]],
    True, "-"];

proofLen = Which[
    Head[r] === ProofObject,
        ToString @ Quiet @ Check[r["ProofLength"], "-"],
    True, "-"];

If[ Head[r] === ProofObject,
    Module[{outDir = "tools/baselines/proofs_thvm", f},
        If[ ! DirectoryQ[outDir], CreateDirectory[outDir]];
        f = FileNameJoin[{outDir,
            theory <> "__" <> thm <> "__c" <> ToString[idx] <> ".m"}];
        Quiet @ Check[Put[r, f], $Failed]]];

WriteString["stdout", theory, "\t", thm, "\t",
    ToString[idx], "/", ToString[n], "\t", status, "\t",
    ToString[Round[t, 0.01]], "\t", proofLen, "\t", verifies, "\n"];
