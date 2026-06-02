#!/usr/bin/env wolframscript
(* Single-conjunct probe with the spec'd LazyNormalize-safe combo
   (Waldmeister DISCOUNT shape): LazyNormalize defers the full
   normalize, CPSetInterreduce periodically purges joinable CPs from
   the queue (KPV_KPMengeInterreduzieren) so the deferred-normalize
   path doesn't accumulate over-deep terms, AutoMaxWeight caps the
   queue.  Output: theory \t thm \t k/n \t status \t seconds \t pl. *)
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
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

config = {"Completion", "Ordering" -> "LPO",
    "AutoPrecedence" -> True,
    "LazyNormalize" -> True,
    "CPSetInterreduce" -> True,
    "AutoMaxWeight" -> 30,
    "UnfailingCP" -> True,
    "RHSInterreduce" -> True};

{t, r} = AbsoluteTiming @ TimeConstrained[
    TFindProof[target, theory, Method -> config, TimeConstraint -> N[TC]],
    TC + 10, $TimedOut];

status = Which[
    Head[r] === ProofObject, "PROVED",
    r === $TimedOut, "TimedOut",
    True, "Failed"];
proofLen = Which[
    Head[r] === ProofObject,
        ToString @ Quiet @ Check[r["ProofLength"], "-"],
    True, "-"];

If[ Head[r] === ProofObject,
    Module[{outDir = "tools/baselines/proofs_thvm_lazy", f},
        If[ ! DirectoryQ[outDir], CreateDirectory[outDir]];
        f = FileNameJoin[{outDir,
            theory <> "__" <> thm <> "__c" <> ToString[idx] <> ".m"}];
        Quiet @ Check[Put[r, f], $Failed]]];

WriteString["stdout", theory, "\t", thm, "\t",
    ToString[idx], "/", ToString[n], "\t", status, "\t",
    ToString[Round[t, 0.01]], "\t", proofLen, "\n"];
