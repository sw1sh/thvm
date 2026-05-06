#!/usr/bin/env wolframscript
(* Level 2 -- matmul (M=N=K=128) on thvm. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

TInit[];
SeedRandom[7];

m = 128; k = 128; nn = 128;
A = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {m, k}], "Real32"];
B = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {k, nn}], "Real32"];

TRealize @ TMatMul[A, B];
kid = TKernelCount[] - 1;
print["kid: ", kid];
print["dispatch_pre: ", TKernelDispatchKind[kid]];
print["proposals: ", TKernelProposed[kid]];

variants = TKernelVariants[kid];
rows = First /@ variants;
baselineRow = SelectFirst[rows, #["Opt"] === None &];
candidateRows = Select[rows, #["Opt"] =!= None &];
bestRow = If[ candidateRows === {}, baselineRow,
    First @ SortBy[candidateRows, #["WallUs"] &]];

print["variants_tried: ", Length[variants]];
print["baseline_us: ", N @ baselineRow["WallUs"]];
print["best_variant_us: ", N @ bestRow["WallUs"], "  opt=", bestRow["Opt"]];

optsPost = TKernelAutotune[kid];
applied = First[optsPost]["Applied"];
print["applied: ", applied];
print["dispatch_post: ", TKernelDispatchKind[kid]];

speedup = N[baselineRow["WallUs"] / bestRow["WallUs"]];
print["speedup_baseline_to_best: ", speedup];
