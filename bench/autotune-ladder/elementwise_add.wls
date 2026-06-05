#!/usr/bin/env wolframscript
(* Level 1 -- elementwise add (N=1024).
   Builds a single rank-1 ADD kernel, runs autotune, reports
   baseline + winner wall_us + JIT cache stats. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

TInit[];
SeedRandom[7];

n = 1024;
a = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, n], "Real32"];
b = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, n], "Real32"];

(* Force kernel to materialise. *)
TRealize @ TUOpAdd[a, b];
kid = TKernelCount[] - 1;
Print["kid: ", kid];
Print["dispatch_pre: ", TKernelDispatchKind[kid]];

(* Get proposals + bench all variants.  variants[[1,1]] is the
   baseline (Opt -> None); the rest are proposed candidates with
   measured WallUs. *)
proposals = TKernelProposed[kid];
Print["proposals: ", proposals];

variants = TKernelVariants[kid];
rows = First /@ variants;
baselineRow = SelectFirst[rows, #["Opt"] === None &];
candidateRows = Select[rows, #["Opt"] =!= None &];
bestRow = First @ SortBy[candidateRows, #["WallUs"] &];

Print["variants_tried: ", Length[variants]];
Print["baseline_us: ", N @ baselineRow["WallUs"]];
Print["best_variant_us: ", N @ bestRow["WallUs"], "  opt=", bestRow["Opt"]];

(* Autotune: applies the winner. *)
optsPost = TKernelAutotune[kid];
applied = First[optsPost]["Applied"];
Print["applied: ", applied];
Print["dispatch_post: ", TKernelDispatchKind[kid]];

speedup = N[baselineRow["WallUs"] / bestRow["WallUs"]];
Print["speedup_baseline_to_best: ", speedup];
