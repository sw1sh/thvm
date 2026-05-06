#!/usr/bin/env wolframscript
(* Level 5 -- single conv2d (1x32x28x28 input, 5x5 kernel, 32 channels)
   on thvm.  Matches LeNet's first conv shape. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

TInit[];
SeedRandom[7];

(* {C_in=32, H=28, W=28} input + {C_out=32, C_in=32, kh=5, kw=5} weights. *)
xH = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {32, 28, 28}], "Real32"];
wH = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {32, 32, 5, 5}], "Real32"];
bH = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, 32], "Real32"];

TRealize @ TConv2D[xH, wH, bH];

nKernels = TKernelCount[] - 1;
print["conv2d kernel_count: ", nKernels];
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
