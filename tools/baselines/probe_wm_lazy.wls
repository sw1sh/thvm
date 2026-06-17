#!/usr/bin/env wolframscript
(* Run TFindProof Method->"WaldmeisterLazy" on a single conjunct.
   Output matches run_thvm_one_conjunct.wls for the merge_best.sh
   join: theory \t thm \t k/n \t status \t seconds \t proofLength. *)
PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];
$RecursionLimit = 16384;

args = $ScriptCommandLine;
theory = args[[2]];
thm    = args[[3]];
idx    = ToExpression[args[[4]]];
TC     = ToExpression[args[[5]]];

cjList = AxiomaticTheory[theory, "NotableTheorems"][thm];
cjList = If[ListQ[cjList], cjList, {cjList}];
n      = Length[cjList];
target = cjList[[idx]];

{t, r} = AbsoluteTiming @ TimeConstrained[
    TFindProof[target, theory, Method -> "WaldmeisterLazy",
        TimeConstraint -> N[TC]],
    TC + 10, $TimedOut];

status = Which[
    Head[r] === ProofObject, "PROVED",
    r === $TimedOut, "TimedOut",
    True, "Failed"];
proofLen = Which[
    Head[r] === ProofObject,
        ToString @ Quiet @ Check[r["ProofLength"], "-"],
    True, "-"];

WriteString["stdout", theory, "\t", thm, "\t",
    ToString[idx], "/", ToString[n], "\t", status, "\t",
    ToString[Round[t, 0.01]], "\t", proofLen, "\n"];
