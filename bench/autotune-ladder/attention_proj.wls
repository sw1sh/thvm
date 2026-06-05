#!/usr/bin/env wolframscript
(* Level 18c -- thvm self-attention with Q/K/V/O projections.

   NetGraph routes:
     X -> NetMapOperator[LinearLayer[64]] (Q proj)  -> attn.Query
     X -> NetMapOperator[LinearLayer[64]] (K proj)  -> attn.Key
     X -> NetMapOperator[LinearLayer[64]] (V proj)  -> attn.Value
     attn -> NetMapOperator[LinearLayer[64]] (O proj) -> output

   Tinygrad equivalent (Level 18) = 8 kernels.  thvm identity
   attention (Level 18b) = 4 kernels.  Predicted thvm with
   projections: 8 (if fuses) or 12 (per-LinearLayer +2 cost). *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

TInit[];
SeedRandom[7];

net = NetInitialize @ NetGraph[
    {"q_proj"  -> NetMapOperator[LinearLayer[64]],
     "k_proj"  -> NetMapOperator[LinearLayer[64]],
     "v_proj"  -> NetMapOperator[LinearLayer[64]],
     "attn"    -> AttentionLayer["Dot"],
     "out_proj"-> NetMapOperator[LinearLayer[64]]},
    {NetPort["X"] -> "q_proj" -> NetPort["attn", "Query"],
     NetPort["X"] -> "k_proj" -> NetPort["attn", "Key"],
     NetPort["X"] -> "v_proj" -> NetPort["attn", "Value"],
     "attn" -> "out_proj"},
    "X" -> {32, 64}];

x = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {32, 64}], "Real32"];

TRealize @ TFromNet[net, x];

nKernels = TKernelCount[] - 1;
Print["attention_proj kernel_count: ", nKernels];
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
