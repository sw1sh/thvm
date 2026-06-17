#!/usr/bin/env wolframscript
(* Run WL's built-in FindEquationalProof on a single NotableTheorem
   under a hard wall-clock cap.  Args: theory thm TC.  Prints
   one tab-separated line to stdout:
     theory<TAB>thm<TAB>status<TAB>seconds<TAB>proofLength<TAB>proofFunctionVerifies
   where status is "PROVED" | "TimedOut" | "Failed" and verifies is
   "Success" | "Failed".  ProofLength is the count of MainSteps for
   PROVED single-conjunct goals, or a Plus-summed list for multi-
   conjunct goals; "-" otherwise.  Designed to be invoked from a
   bash driver one-case-at-a-time so a kernel crash kills only that
   one case and the driver moves on.

   When PROVED, the WL ProofObject (or List of) is serialized to
   tools/baselines/proofs/<theory>__<thm>.m -- a single Put[] file
   readable with Get[] for later inspection / comparison. *)
PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];   (* not strictly needed for the WL built-in path *)
args = $ScriptCommandLine;
theory = args[[2]];
thm = args[[3]];
TC = ToExpression[args[[4]]];

cj = AxiomaticTheory[theory, "NotableTheorems"][thm];
ax = AxiomaticTheory[theory];

{t, r} = AbsoluteTiming @ TimeConstrained[
    FindEquationalProof[cj, ax],
    TC, $TimedOut];

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
        ToString @ Length[r["ProofDataset"]["MainSteps"]],
    ListQ[r] && AllTrue[r, Head[#] === ProofObject &],
        ToString[Total[Length[#["ProofDataset"]["MainSteps"]] & /@ r]],
    True, "-"];

If[ status === "PROVED",
    Module[{dir = "tools/baselines/proofs",
            f = "tools/baselines/proofs/" <> theory <> "__" <> thm <> ".m"},
        If[ !DirectoryQ[dir], CreateDirectory[dir]];
        Put[r, f]]];

WriteString["stdout", theory, "\t", thm, "\t", status, "\t",
    ToString[Round[t, 0.01]], "\t", proofLen, "\t", verifies, "\n"];
