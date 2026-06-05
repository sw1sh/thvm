#!/usr/bin/env wolframscript
(* Level 14 -- 3-conv mini-LeNet on thvm.

   Shape: 1x28x28 ->
     conv 5x5x6  + Ramp + 2x2 maxpool  (28 -> 24 -> 12)
     conv 5x5x16 + Ramp + 2x2 maxpool  (12 -> 8  -> 4)
     conv 3x3x32 + Ramp                ( 4 -> 2)
     flatten -> linear 10 + softmax.

   Stress-tests K=3 in the formula `thvm = 5K + 2L + 3`.
   Predicted: 5*3 + 2*1 + 3 = 20 kernels.  Note the third
   conv-block omits the pool because 4x4 -> 2x2 with stride 2
   pool would yield 1x1; by dropping pool the per-block cost
   is unclear -- we may see -1 vs full-block cost. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

TInit[];
SeedRandom[7];

net = NetInitialize @ NetChain[{
    ConvolutionLayer[6,  {5, 5}], ElementwiseLayer[Ramp],
    PoolingLayer[{2, 2}, "Stride" -> 2],
    ConvolutionLayer[16, {5, 5}], ElementwiseLayer[Ramp],
    PoolingLayer[{2, 2}, "Stride" -> 2],
    ConvolutionLayer[32, {3, 3}], ElementwiseLayer[Ramp],
    FlattenLayer[],
    LinearLayer[10], SoftmaxLayer[]
}, "Input" -> {1, 28, 28}];

x = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {1, 28, 28}], "Real32"];

TRealize @ TFromNet[net, x];

nKernels = TKernelCount[] - 1;
Print["mini_lenet3 kernel_count: ", nKernels];
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
