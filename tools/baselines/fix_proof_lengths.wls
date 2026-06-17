#!/usr/bin/env wolframscript
(* Fix proofLength in tools/baselines/thvm_notable_theorems.tsv by
   loading the saved ProofObjects in tools/baselines/proofs/ and
   reading MainSteps length off each.  The original run wrote "-"
   for proofLength because the harness computed it before the
   ProofDataset was materialized.

   Usage:
     wolframscript -f tools/baselines/fix_proof_lengths.wls

   Rewrites tools/baselines/thvm_notable_theorems.tsv in place.
   For rows whose status is not PROVED, or whose saved proof file
   is missing, leaves proofLength as "-".  For multi-conjunct
   theorems (saved file holds a List of ProofObjects), sums the
   per-conjunct MainSteps lengths.
*)

tsvPath   = "tools/baselines/thvm_notable_theorems.tsv";
proofsDir = "tools/baselines/proofs";

readTsv[path_] := Module[{lines, header},
    lines = StringSplit[Import[path, "Text"], "\n"];
    lines = Select[lines, # =!= "" &];
    header = StringSplit[First @ lines, "\t"];
    {header, StringSplit[#, "\t"] & /@ Rest[lines]}];

(* ProofObject has a direct "ProofLength" property (integer); for
   multi-conjunct lists, sum the per-conjunct lengths. *)
extractLen[p_ProofObject] := Quiet @ Check[p["ProofLength"], "-"];
extractLen[ps_List] := Module[{lens},
    lens = extractLen /@ ps;
    If[ AllTrue[lens, IntegerQ], Total[lens], "-"]];
extractLen[_] := "-";

(* Must load THVMLink first so the ProofObject formatter resolves
   (otherwise Get[] returns the raw symbol with no properties). *)
PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];

readProofLen[theory_, thm_, status_] := Module[{file, expr},
    If[ status =!= "PROVED", Return["-"]];
    file = FileNameJoin[{proofsDir, theory <> "__" <> thm <> ".m"}];
    If[ ! FileExistsQ[file], Return["-"]];
    expr = Quiet @ Check[Get[file], $Failed];
    ToString @ extractLen[expr]];

{header, rows} = readTsv[tsvPath];

(* Replace column "proofLength" (5th, 1-based: index 5) with
   computed length. *)
plIdx = Position[header, "proofLength"][[1, 1]];

updated = Map[
    Module[{r = #, len},
        len = readProofLen[r[[1]], r[[2]], r[[3]]];
        r[[plIdx]] = len;
        r] &,
    rows];

(* Write back. *)
out = StringRiffle[#, "\t"] & /@ Prepend[updated, header];
Export[tsvPath, StringRiffle[out, "\n"] <> "\n", "Text"];

(* Stats. *)
WriteString["stdout", "Updated ", Length[updated], " rows in ", tsvPath, "\n"];
WriteString["stdout", "proofLength distribution:\n"];
counts = Tally @ Map[
    Switch[#, "-", "-", _String, If[StringMatchQ[#, NumberString], "<num>", "?"], _, "?"] &,
    updated[[All, plIdx]]];
Do[WriteString["stdout", "  ", c[[1]], ": ", c[[2]], "\n"], {c, counts}];
nums = Select[ToExpression /@ updated[[All, plIdx]], NumberQ];
If[ nums =!= {},
    WriteString["stdout", "  numeric range: min=", Min[nums],
        ", max=", Max[nums], ", median=", N @ Median[nums], "\n"]];
