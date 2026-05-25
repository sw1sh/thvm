#!/usr/bin/env wolframscript
(* Process one batch of theories in this kernel.  Comma-separated list
   of theory names comes via env THVM_CHUNK_THEORIES.  TC from env
   THVM_CHUNK_TC (default 30).  Appends to the shared TSV per case so
   a mid-chunk crash leaves earlier results on disk. *)
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
print[args___] := WriteString["stdout", StringJoin @@ Map[ToString, {args}], "\n"];

OUT = "tools/baselines/thvm_notable_theorems.tsv";
TC = ToExpression[Environment["THVM_CHUNK_TC"]];
If[ ! IntegerQ[TC], TC = 30];

(* AxiomaticTheory[] returns String names, not Symbols.  Keep them as
   Strings -- ToExpression'ing here would make Symbols in the wrong
   context (Global` instead of System`) and AxiomaticTheory[symbol]
   would notent on every one. *)
theories = StringSplit[Environment["THVM_CHUNK_THEORIES"], ","];
print["[chunk] processing ", Length[theories], " theories: ",
    StringRiffle[theories, ", "]];

runCase[theory_, thm_] := Module[{t, r, status, verify, proofLen},
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
        Head[r] === ProofObject && AssociationQ @ r["ProofDataset"],
            ToString @ Length[r["ProofDataset"]["MainSteps"]],
        ListQ[r] && AllTrue[r, Head[#] === ProofObject &],
            ToString[Total[Length[#["ProofDataset"]["MainSteps"]] & /@ r]],
        True, "-"];
    {status, Round[t, 0.01], proofLen, verify}
];

Scan[Function[theory,
    Module[{nt = AxiomaticTheory[theory, "NotableTheorems"]},
        If[ AssociationQ[nt],
            Scan[Function[thm,
                Module[{res = runCase[theory, thm]},
                    Module[{h = OpenAppend[OUT]},
                        WriteString[h, ToString[theory], "\t",
                            ToString[thm], "\t",
                            res[[1]], "\t",
                            ToString[res[[2]]], "\t",
                            res[[3]], "\t",
                            res[[4]], "\n"];
                        Close[h]];
                    print[StringPadRight[ToString[theory], 28],
                        StringPadRight[ToString[thm], 38],
                        StringPadRight[res[[1]], 8],
                        res[[2]], "s  ", res[[4]]]
                ]], Keys[nt]]
        ]]
    ], theories];
