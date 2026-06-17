#!/usr/bin/env wolframscript
(* Level 2 -- matmul (M=N=K=128) on thvm. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];

TInit[];
SeedRandom[7];

m = 128; k = 128; nn = 128;
A = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {m, k}], "Real32"];
B = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {k, nn}], "Real32"];

TRealize @ TMatMul[A, B];
kid = TKernelCount[] - 1;
Print["kid: ", kid];
Print["dispatch_pre: ", TKernelDispatchKind[kid]];
Print["proposals: ", TKernelProposed[kid]];

variants = TKernelVariants[kid];
rows = First /@ variants;
baselineRow = SelectFirst[rows, #["Opt"] === None &];
candidateRows = Select[rows, #["Opt"] =!= None &];
bestRow = If[ candidateRows === {}, baselineRow,
    First @ SortBy[candidateRows, #["WallUs"] &]];

Print["variants_tried: ", Length[variants]];
Print["baseline_us: ", N @ baselineRow["WallUs"]];
Print["best_variant_us: ", N @ bestRow["WallUs"], "  opt=", bestRow["Opt"]];

optsPost = TKernelAutotune[kid];
applied = First[optsPost]["Applied"];
Print["applied: ", applied];
Print["dispatch_post: ", TKernelDispatchKind[kid]];

speedup = N[baselineRow["WallUs"] / bestRow["WallUs"]];
Print["speedup_baseline_to_best: ", speedup];
