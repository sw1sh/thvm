#!/usr/bin/env wolframscript
(* Level 18 thvm -- probe AttentionLayer through TFromNet.

   Mirror the tinygrad attention config: seq_len=32,
   d_model=64, n_heads=4, d_head=16.  AttentionLayer's
   default has Input -> Output of the same shape, with
   internal QKV projections sized via "QueryDimensions" /
   "ValueDimensions" / "Heads".

   Test whether TFromNet lowers NetChain[{AttentionLayer[]}]
   to actual Metal kernels.  BN (Level 13) returned 0
   kernels silently; AttentionLayer may face the same gap. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];

TInit[];
SeedRandom[7];

(* AttentionLayer's scoring net needs Query & Key input ports;
   default form (no scoring net argument) uses dot-product. *)
net = NetInitialize @ NetChain[{
    AttentionLayer[]
}, "Input" -> {"Input" -> {32, 64}, "Query" -> {32, 64}}];

x = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {32, 64}], "Real32"];

TRealize @ TFromNet[net, x];

nKernels = TKernelCount[] - 1;
Print["attention kernel_count: ", nKernels];
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
