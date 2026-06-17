#!/usr/bin/env wolframscript
(* Level 11 -- 4-layer MLP forward (784 -> 128 -> 64 -> 32 -> 10).

   Stress-tests the leak formula `leak = 3K + L` at L=4.
   Predicted thvm = 11 kernels (vs MLP2=7, MLP3=9 -- delta
   +2 per added LinearLayer-Activation per Level 9). *)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];

TInit[];
SeedRandom[7];

net = NetInitialize @ NetChain[{
    LinearLayer[128], ElementwiseLayer[Ramp],
    LinearLayer[64],  ElementwiseLayer[Ramp],
    LinearLayer[32],  ElementwiseLayer[Ramp],
    LinearLayer[10],  SoftmaxLayer[]
}, "Input" -> 784];

x = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, 784], "Real32"];

TRealize @ TFromNet[net, x];

nKernels = TKernelCount[] - 1;
Print["mlp4 kernel_count: ", nKernels];
Print["dispatch_kinds: ",
    Tally[Table[TKernelDispatchKind[k], {k, 1, nKernels}]]];

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
