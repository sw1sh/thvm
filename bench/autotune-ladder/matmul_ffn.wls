#!/usr/bin/env wolframscript
(* Level 27 -- standalone matmul at the FFN dim that bottlenecks
   the transformer block (kid 14: 13935us best, no autotune win).

   Shape: (32, 64) @ (64, 256) -- the fc1 matmul of the
   transformer's FFN expansion.  If this benches in the same
   13-ish ms range standalone, the bottleneck is shape-specific
   (matmul codegen), not transformer-context-specific.  If it
   benches much faster, the transformer's wall is from
   surrounding fanin / cache pressure. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];

TInit[];
SeedRandom[7];

(* Direct matmul via NetMapOperator to mirror the transformer
   block's fc1 shape. *)
net = NetInitialize @ NetChain[{
    NetMapOperator[LinearLayer[256]]
}, "Input" -> {32, 64}];

x = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {32, 64}], "Real32"];

TRealize @ TFromNet[net, x];

nKernels = TKernelCount[] - 1;
Print["matmul_ffn kernel_count: ", nKernels];
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
                  "  applied=", optsApplied,
                  "  variants=", Length[variants]];
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
