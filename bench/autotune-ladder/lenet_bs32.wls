#!/usr/bin/env wolframscript
(* Level 7 -- LeNet forward at batch=32 (real workload) on thvm.

   Same network as Level 6 (lenet.wls); only the input shape
   changes from {1, 28, 28} to a 32-sample batch.  Stresses
   broadcast / reduce shapes that batch=1 collapses
   (e.g. softmax-tail max/sum produce shape [32] instead of
   [1], so the singleton-broadcast lift fast path doesn't
   fire). *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

TInit[];
SeedRandom[7];

net = NetInitialize @ NetChain[{
    ConvolutionLayer[6, {5, 5}], ElementwiseLayer[Ramp],
    PoolingLayer[{2, 2}, "Stride" -> 2],
    ConvolutionLayer[16, {5, 5}], ElementwiseLayer[Ramp],
    PoolingLayer[{2, 2}, "Stride" -> 2],
    FlattenLayer[],
    LinearLayer[120], ElementwiseLayer[Ramp],
    LinearLayer[84], ElementwiseLayer[Ramp],
    LinearLayer[10], SoftmaxLayer[]
}, "Input" -> {1, 28, 28}];

x = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {32, 1, 28, 28}], "Real32"];

TRealize @ TFromNet[net, x];

nKernels = TKernelCount[] - 1;
print["lenet_bs32 kernel_count: ", nKernels];
print["dispatch_kinds: ",
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
            print["kid ", k, " kind=", dispatchPre,
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

print["totals_baseline_us: ", N @ totals["baseline_us"]];
print["totals_best_us: ", N @ totals["best_us"]];
If[ totals["best_us"] > 0,
    print["totals_speedup: ",
          N[totals["baseline_us"] / totals["best_us"]]]];
print["kernels_with_proposals: ", totals["n_with_proposals"]];
print["kernels_with_applied: ", totals["applied_count"]];
