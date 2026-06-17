#!/usr/bin/env wolframscript
(* Level 19 thvm -- full transformer block.

   NetGraph routing:
     X -> LN1 -> Q/K/V/O attention -> residual1
     -> LN2 -> FFN (Linear -> Ramp -> Linear) -> residual2

   Predicted thvm by component costs:
     9 (attn+proj from Level 18c) + 2*2 (LN x2) + 2*2 (FFN
     linears) + 0-2 (residuals) = ~17-19 kernels.

   Tinygrad transformer block (Level 19) = 14 kernels, so
   leak ~+3 to +5. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];

TInit[];
SeedRandom[7];

(* Self-attention sub-graph: 3 input ports merged from a
   single normalized X. *)
attnSub = NetGraph[
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

block = NetGraph[
    {"ln1"  -> NormalizationLayer[],
     "attn" -> attnSub,
     "add1" -> ThreadingLayer[Plus],
     "ln2"  -> NormalizationLayer[],
     "fc1"  -> NetMapOperator[LinearLayer[256]],
     "ramp" -> ElementwiseLayer[Ramp],
     "fc2"  -> NetMapOperator[LinearLayer[64]],
     "add2" -> ThreadingLayer[Plus]},
    {NetPort["X"] -> "ln1" -> "attn",
     {NetPort["X"], "attn"} -> "add1",
     "add1" -> "ln2" -> "fc1" -> "ramp" -> "fc2",
     {"add1", "fc2"} -> "add2"},
    "X" -> {32, 64}];

net = NetInitialize @ block;

x = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {32, 64}], "Real32"];

TRealize @ TFromNet[net, x];

nKernels = TKernelCount[] - 1;
Print["transformer_block kernel_count: ", nKernels];
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
Print["kernels_with_proposals: ", totals["n_with_proposals"]];
Print["kernels_with_applied: ", totals["applied_count"]];
