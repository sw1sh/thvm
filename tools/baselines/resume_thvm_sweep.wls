#!/usr/bin/env wolframscript
(* Resume the thvm baseline sweep from where the previous single-kernel
   runner left off.  Reads tools/baselines/thvm_notable_theorems.tsv,
   skips any (theory, thm) already present, runs the rest with TC=30,
   appends to the same TSV.  Use this when the previous run died
   mid-sweep (the kernel can ooM on heavy theorems). *)
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
print[args___] := WriteString["stdout", StringJoin @@ Map[ToString, {args}], "\n"];

TC = 30;
OUT = "tools/baselines/thvm_notable_theorems.tsv";
PROOFDIR = "tools/baselines/proofs_thvm";
If[ ! DirectoryQ[PROOFDIR], CreateDirectory[PROOFDIR]];

(* Already-completed keys. *)
existingLines = Select[StringSplit[Import[OUT, "Text"], "\n"], # =!= "" &];
existingRows = StringSplit[#, "\t"] & /@ Rest[existingLines];
existingKeys = {#[[1]], #[[2]]} & /@ existingRows;
existingSet = Association[(# -> True) & /@ existingKeys];
print["Resume: ", Length[existingKeys], " cases already in TSV."];

runCase[theory_, thm_] := Module[{t, r, status, seconds, proofLen, verify, f},
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
    proofLen = Which[
        Head[r] === ProofObject,
            ToString @ Quiet @ Check[r["ProofLength"], "-"],
        ListQ[r] && AllTrue[r, Head[#] === ProofObject &],
            ToString[Total[Quiet @ Check[#["ProofLength"], 0] & /@ r]],
        True, "-"];
    If[ Head[r] === ProofObject
        || (ListQ[r] && AllTrue[r, Head[#] === ProofObject &]),
        f = FileNameJoin[{PROOFDIR, theory <> "__" <> thm <> ".m"}];
        Quiet @ Check[Put[r, f], $Failed]];
    seconds = Round[t, 0.01];
    {status, seconds, proofLen, verify}];

n = 0;
Scan[Function[theory,
    Module[{nt = AxiomaticTheory[theory, "NotableTheorems"]},
        If[ AssociationQ[nt],
            Scan[Function[thm,
                If[ KeyExistsQ[existingSet, {ToString[theory], ToString[thm]}],
                    Null,
                    Module[{res, line, h},
                        n++;
                        res = runCase[theory, thm];
                        line = StringJoin[
                            ToString[theory], "\t", ToString[thm], "\t",
                            res[[1]], "\t", ToString[res[[2]]], "\t",
                            res[[3]], "\t", res[[4]]];
                        h = OpenAppend[OUT];
                        WriteString[h, line, "\n"];
                        Close[h];
                        print[StringPadRight[ToString[theory], 28],
                            StringPadRight[ToString[thm], 38],
                            StringPadRight[res[[1]], 8],
                            res[[2]], "s  ", res[[4]]]]]],
                Keys[nt]]]]],
    AxiomaticTheory[]];
print["Resume done: ran ", n, " new cases."];
