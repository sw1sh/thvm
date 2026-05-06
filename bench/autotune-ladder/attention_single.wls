#!/usr/bin/env wolframscript
(* Level 18b -- thvm single-head AttentionLayer via NetGraph.

   Wraps a single AttentionLayer["Dot"] with identity Q/K/V
   (no projections), feeding the same input tensor 3 times.
   Tests whether thvm's fromLayer[AttentionLayer, _, qkv_]
   handler at wl/THVMLink/Kernel/NN.wl:1017 lowers cleanly
   when the Q/K/V triple is supplied via NetGraph multi-input.

   This is NOT the full transformer attention pattern (no
   per-head projections + CatenateLayer); that's blocked on
   Phase 13.  Probe-level only. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

TInit[];
SeedRandom[7];

(* NetGraph routing: single input "X" feeds Query/Key/Value
   ports of the AttentionLayer. *)
net = NetInitialize @ NetGraph[
    {"attn" -> AttentionLayer["Dot"]},
    {NetPort["X"] -> NetPort["attn", "Query"],
     NetPort["X"] -> NetPort["attn", "Key"],
     NetPort["X"] -> NetPort["attn", "Value"]},
    "X" -> {32, 64}];

x = TTensorCreate @ NumericArray[
    RandomReal[{-1, 1}, {32, 64}], "Real32"];

TRealize @ TFromNet[net, x];

nKernels = TKernelCount[] - 1;
print["attention_single kernel_count: ", nKernels];
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
