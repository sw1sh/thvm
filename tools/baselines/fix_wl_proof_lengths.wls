#!/usr/bin/env wolframscript
(* Update the proofLength column of tools/baselines/wl_notable_theorems.tsv
   by reading the saved WL FindEquationalProof outputs in
   tools/baselines/proofs/<theory>__<thm>.m and pulling
   p["ProofLength"].  The original run_wl_one.wls computed
   Length[r["ProofDataset"]["MainSteps"]] which landed in Failure[]
   (ProofDataset is a Dataset, not an Association) and Length[Failure
   [_, _]] == 2 -- the source of every "2" in the TSV. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

tsvPath   = "tools/baselines/wl_notable_theorems.tsv";
proofsDir = "tools/baselines/proofs";

readTsv[path_] := Module[{lines, header},
    lines = Select[StringSplit[Import[path, "Text"], "\n"], # =!= "" &];
    header = StringSplit[First @ lines, "\t"];
    {header, StringSplit[#, "\t"] & /@ Rest[lines]}];

extractLen[p_ProofObject] := Quiet @ Check[p["ProofLength"], "-"];
extractLen[ps_List] := Module[{lens},
    lens = extractLen /@ ps;
    If[ AllTrue[lens, IntegerQ], Total[lens], "-"]];
extractLen[_] := "-";

readProofLen[theory_, thm_, status_] := Module[{file, expr},
    If[ status =!= "PROVED", Return["-"]];
    file = FileNameJoin[{proofsDir, theory <> "__" <> thm <> ".m"}];
    If[ ! FileExistsQ[file], Return["-"]];
    expr = Quiet @ Check[Get[file], $Failed];
    ToString @ extractLen[expr]];

{header, rows} = readTsv[tsvPath];
plIdx = Position[header, "proofLength"][[1, 1]];

updated = Map[
    Module[{r = #, len},
        len = readProofLen[r[[1]], r[[2]], r[[3]]];
        r[[plIdx]] = len;
        r] &,
    rows];

out = StringRiffle[#, "\t"] & /@ Prepend[updated, header];
Export[tsvPath, StringRiffle[out, "\n"] <> "\n", "Text"];

WriteString["stdout", "Updated ", Length[updated], " WL rows in ", tsvPath, "\n"];
nums = Select[
    Map[Quiet @ Check[ToExpression[#], $Failed] &, updated[[All, plIdx]]],
    NumericQ];
If[ nums =!= {},
    WriteString["stdout", "proofLength numeric stats: n=", Length @ nums,
        ", min=", Min @ nums, ", max=", Max @ nums,
        ", median=", N @ Median @ nums, "\n"]];
