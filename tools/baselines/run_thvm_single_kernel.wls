#!/usr/bin/env wolframscript
(* Single-kernel thvm 107-theorem sweep: process every NotableTheorem
   of every AxiomaticTheory in ONE wolframscript instance.  ~2GB peak
   total (one kernel start + paclet load + N proofs), vs the per-case
   driver's 107 x ~2GB sequential spikes.

   Each case runs with TimeConstraint -> 30 and an outer WL
   TimeConstrained safety wrap, so a single hung proof can't wedge
   the whole sweep.  Streams TSV to stdout AND tools/baselines/
   thvm_notable_theorems.tsv as it goes -- ^C-resumable. *)
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
TC = 30;
OUT = "tools/baselines/thvm_notable_theorems.tsv";

(* Write header *)
Module[{h = OpenWrite[OUT]},
    WriteString[h, "theory\tthm\tstatus\tseconds\tproofLength\tverifies\n"];
    Close[h]];

theories = AxiomaticTheory[];

runCase[theory_, thm_] := Module[{t, r, status, seconds, proofLen, verify},
    {t, r} = AbsoluteTiming @ TimeConstrained[
        TFindProof[thm, theory, TimeConstraint -> N[TC]],
        TC + 5, $TimedOut];
    status = Which[
        Head[r] === ProofObject, "PROVED",
        ListQ[r] && AllTrue[r, Head[#] === ProofObject &], "PROVED",
        r === $TimedOut, "TimedOut",
        True, "Failed"];
    verify = Which[
        Head[r] === ProofObject,
            ToString[Head @ Quiet @ Check[
                r["ProofFunction"][r["Theorems"]], $Failed]],
        ListQ[r] && AllTrue[r, Head[#] === ProofObject &],
            Module[{vs = Quiet @ Check[
                Map[Head[#["ProofFunction"][#["Theorems"]]] &, r], $Failed]},
                If[ ListQ[vs] && AllTrue[vs, # === Success &],
                    "Success", ToString[vs]]],
        True, "-"];
    (* Use ProofObject's "ProofLength" property (#axioms + #lemmas +
       #conclusion: the canonical proof-step count).  The old
       Length[r["ProofDataset"]["MainSteps"]] call landed in Failure[]
       (ProofDataset is a Dataset, not an Association) and Length
       [Failure[_, _]] == 2 -- the source of every "2" in legacy TSVs. *)
    proofLen = Which[
        Head[r] === ProofObject,
            ToString @ Quiet @ Check[r["ProofLength"], "-"],
        ListQ[r] && AllTrue[r, Head[#] === ProofObject &],
            ToString[Total[Quiet @ Check[#["ProofLength"], 0] & /@ r]],
        True, "-"];
    (* Save the thvm proof to tools/baselines/proofs_thvm/ for later
       inspection -- separate dir from the WL FindEquationalProof
       outputs in tools/baselines/proofs/. *)
    If[ Head[r] === ProofObject || (ListQ[r] && AllTrue[r, Head[#] === ProofObject &]),
        Module[{outDir = "tools/baselines/proofs_thvm", f},
            If[ ! DirectoryQ[outDir], CreateDirectory[outDir]];
            f = FileNameJoin[{outDir, theory <> "__" <> thm <> ".m"}];
            Quiet @ Check[Put[r, f], $Failed]]];
    seconds = Round[t, 0.01];
    {status, seconds, proofLen, verify}
];

n = 0;
proved = failed = timedOut = 0;
Scan[Function[theory,
    Module[{nt = AxiomaticTheory[theory, "NotableTheorems"]},
        If[ AssociationQ[nt],
            Scan[Function[thm,
                Module[{res = runCase[theory, thm]},
                    n++;
                    Switch[ res[[1]],
                        "PROVED",   proved++,
                        "TimedOut", timedOut++,
                        _,          failed++];
                    line = ToString[theory] <> "\t" <> ToString[thm] <> "\t" <>
                        res[[1]] <> "\t" <> ToString[res[[2]]] <> "\t" <>
                        res[[3]] <> "\t" <> res[[4]];
                    (* Append + close per case so a crash mid-sweep
                       preserves all earlier results to disk. *)
                    Module[{h = OpenAppend[OUT]},
                        WriteString[h, line, "\n"];
                        Close[h]];
                    Print[StringPadRight[ToString[theory], 28],
                        StringPadRight[ToString[thm], 38],
                        StringPadRight[res[[1]], 8],
                        res[[2]], "s  ", res[[4]]]]
                ], Keys[nt]]
        ]]
    ], theories];
Print["---"];
Print["DONE.  ", n, " cases. PROVED=", proved, " TimedOut=", timedOut, " Failed=", failed];
