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

(* +10s gap (was +2): the C engine consults wall_seconds at
   coarse granularity (per CP / per portfolio-slot boundary), so a
   hard case can overrun by several seconds.  Too-tight a guard
   would interrupt mid-call and leave the script with nothing to
   write to stdout -- the bench wrapper then classifies the row as
   CRASH.  Outer bash hard-limit must stay above TC + this margin.

   Note also: TFindProof's default returnSpec "ProofObject" COLLAPSES
   a multi-conjunct partial success to $Failed (see atpProveFromTheory
   in ATP.wl).  We don't bypass that here -- if a future arc surfaces
   partial-conjunct progress, do it inside TFindProof's contract, not
   by mutating the bench harness's expected return shape. *)
{t, r} = AbsoluteTiming @ TimeConstrained[
    TFindProof[thm, theory, TimeConstraint -> N[TC]],
    TC + 10, $TimedOut];

verifyOne[p_] := Head @ Quiet @ Check[p["ProofFunction"][p["Theorems"]], $Failed];

(* Multi-conjunct (e.g. WolframAxioms/ImpliesMeredithAxioms = 2
   conjuncts, *AxiomSet entailments = 2-3 conjuncts): TFindProof
   returns a List of ProofObject | Failure[].  The bench's previous
   all-or-nothing collapse hid genuine partial progress -- e.g. 1/2
   conjuncts proven counted as "Failed" identically to 0/2.  Emit
   "Partial(k/n)" for k > 0 partial successes; sum proof-lengths
   over the proven conjuncts so even partial rows carry signal. *)
status = Which[
    Head[r] === ProofObject, "PROVED",
    ListQ[r] && AllTrue[r, Head[#] === ProofObject &], "PROVED",
    ListQ[r] && AnyTrue[r, Head[#] === ProofObject &],
        Module[{k = Count[r, _ProofObject], n = Length[r]},
            StringJoin["Partial(", ToString[k], "/", ToString[n], ")"]],
    r === $TimedOut, "TimedOut",
    True, "Failed"];

verifies = Which[
    Head[r] === ProofObject,
        ToString[verifyOne[r]],
    ListQ[r] && AllTrue[r, Head[#] === ProofObject &],
        Module[{vs = Map[verifyOne, r]},
            If[ AllTrue[vs, # === Success &], "Success", ToString[vs]]],
    ListQ[r] && AnyTrue[r, Head[#] === ProofObject &],
        Module[{vs = verifyOne /@ Cases[r, _ProofObject]},
            If[ AllTrue[vs, # === Success &], "PartialSuccess",
                ToString[vs]]],
    True, "-"];

(* Use ProofObject's "ProofLength" property (#axioms+#lemmas+
   #conclusion).  Length[r["ProofDataset"]["MainSteps"]] landed in
   Failure[] (ProofDataset is a Dataset, not Association) and
   Length[Failure[_, _]] == 2 -- the source of every "2" in legacy
   TSVs. *)
proofLen = Which[
    Head[r] === ProofObject,
        ToString @ Quiet @ Check[r["ProofLength"], "-"],
    ListQ[r] && AnyTrue[r, Head[#] === ProofObject &],
        (* Sum lengths over proven conjuncts (full OR partial). *)
        ToString[Total[
            Quiet @ Check[#["ProofLength"], 0] & /@
                Cases[r, _ProofObject]]],
    True, "-"];

(* Save thvm's proof to tools/baselines/proofs_thvm/ -- separate
   dir from the WL proofs in tools/baselines/proofs/. *)
isProofResult = (Head[r] === ProofObject) ||
    (ListQ[r] && AllTrue[r, Head[#] === ProofObject &]);
If[ isProofResult,
    Module[{outDir = "tools/baselines/proofs_thvm", f},
        If[ ! DirectoryQ[outDir], CreateDirectory[outDir]];
        f = FileNameJoin[{outDir, theory <> "__" <> thm <> ".m"}];
        Quiet @ Check[Put[r, f], $Failed]]];

WriteString["stdout", theory, "\t", thm, "\t", status, "\t",
    ToString[Round[t, 0.01]], "\t", proofLen, "\t", verifies, "\n"];
