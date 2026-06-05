#!/usr/bin/env wolframscript
(* Level 3 -- softmax (N=512) on thvm. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

TInit[];
SeedRandom[7];

n = 512;
x = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, n], "Real32"];

(* Realize the full softmax chain so all kernels live in cache. *)
TRealize @ TSoftmax[x];

nKernels = TKernelCount[] - 1;
Print["softmax kernel_count: ", nKernels];
Print["dispatch_kinds_pre: ",
    Tally[Table[TKernelDispatchKind[k], {k, 1, nKernels}]]];

(* For each kernel that has proposals, capture baseline + best. *)
totals = <|
    "baseline_us" -> 0., "best_us" -> 0.,
    "n_with_proposals" -> 0, "applied_count" -> 0
|>;
Do[
    Module[{props = TKernelProposed[k], variants, rows, baseRow, candRows,
            bestRow, optsApplied, dispatchPre = TKernelDispatchKind[k]},
        If[ props =!= {},
            variants = TKernelVariants[k];
            rows = First /@ variants;
            baseRow = SelectFirst[rows, #["Opt"] === None &];
            candRows = Select[rows, #["Opt"] =!= None &];
            bestRow = If[ candRows === {}, baseRow,
                First @ SortBy[candRows, #["WallUs"] &]];
            optsApplied = First[TKernelAutotune[k]]["Applied"];
            Print["kid ", k, " kind=", dispatchPre,
                  " baseline=", N @ baseRow["WallUs"], "us",
                  " best=", N @ bestRow["WallUs"], "us",
                  "  opt=", bestRow["Opt"],
                  "  applied=", optsApplied];
            totals["baseline_us"] += baseRow["WallUs"];
            totals["best_us"] += bestRow["WallUs"];
            totals["n_with_proposals"] += 1;
            If[ optsApplied =!= {},
                totals["applied_count"] += 1]
        ]],
    {k, 1, nKernels}];

Print["totals_baseline_us: ", N @ totals["baseline_us"]];
Print["totals_best_us: ", N @ totals["best_us"]];
If[ totals["best_us"] > 0,
    Print["totals_speedup: ",
          N[totals["baseline_us"] / totals["best_us"]]]];
Print["kernels_with_proposals: ", totals["n_with_proposals"]];
Print["kernels_with_applied: ", totals["applied_count"]];
