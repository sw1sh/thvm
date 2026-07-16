#!/usr/bin/env wolframscript
(* decode_bench.wls - DeepSeek-V2-Lite BS=1 decode-step microbenchmark
   (NN/Examples/DeepSeekV2/Decode.wl, synthetic bf16 weights, prefill-free).

   Captures the decode step once through TJit, runs WARM warm-up steps, then
   times STEPS replays and reports min/mean ms per step + tok/s (tok/s from the
   min, the steady-state figure; the mean is printed for honesty).  The token
   feeds back on device (argmax -> embed_row), the position lives in a device
   int32 buffer, so a timed step is a single replay with zero host values.

   Defaults are a scaled-down config so the CPU backend finishes in seconds
   (the bf16 matmul path on CPU is the un-BLASed interpret route); every knob
   is env-overridable, e.g.:
       DSV2_DIM=2048 DSV2_LAYERS=27 DEV=cuda wolframscript decode_bench.wls
   The full-size CUDA benchmark for the pod is the C binary
   tools/dsv2_decode_bench.c (no Wolfram needed there). *)

PacletDirectoryLoad[FileNameJoin[{DirectoryName[$InputFileName], "..", ".."}]];
Get["WolframInstitute`THVMLink`Examples`"];
TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);

ev[name_, d_] := ToExpression[Environment[name] /. x_ /; (x === $Failed || x === None) -> d];
dev = Environment["DEV"] /. x_ /; (x === $Failed || x === None) -> "cpu";

warm = ev["WARM", "20"];
steps = ev["STEPS", "40"];

cfg = dsvConfig[
    "Device" -> dev,
    "Dim" -> ev["DSV2_DIM", "128"],
    "NLayers" -> ev["DSV2_LAYERS", "4"],
    "NHeads" -> ev["DSV2_HEADS", "8"],
    "KvLora" -> ev["DSV2_KVLORA", "64"],
    "DNope" -> ev["DSV2_DNOPE", "32"],
    "DRope" -> ev["DSV2_DROPE", "16"],
    "DV" -> ev["DSV2_DV", "32"],
    "NExperts" -> ev["DSV2_EXPERTS", "8"],
    "TopK" -> ev["DSV2_TOPK", "3"],
    "MoeInter" -> ev["DSV2_MOEINTER", "64"],
    "DenseInter" -> ev["DSV2_DENSEINTER", "256"],
    "Vocab" -> ev["DSV2_VOCAB", "1024"],
    "MaxSeq" -> ev["DSV2_MAXSEQ", "128"]
];

If[ warm + steps + 1 > cfg["MaxSeq"],
    pr["ERROR: WARM + STEPS + 1 = ", warm + steps + 1, " exceeds MaxSeq = ", cfg["MaxSeq"]];
    Exit[1]
];

pr["dsv2 decode bench: dev=", dev, "  dim=", cfg["Dim"], " layers=", cfg["NLayers"],
    " heads=", cfg["NHeads"], " kv=", cfg["KvLora"], " experts=", cfg["NExperts"],
    " topk=", cfg["TopK"], " vocab=", cfg["Vocab"], " maxseq=", cfg["MaxSeq"]];

{initT, state} = AbsoluteTiming @ dsvSynthInit[cfg];
pr["  synth init: ", Round[initT, 0.01], " s"];

jit = dsvDecodeJit[state];
{capT, res} = AbsoluteTiming @ jit[];
pr["  capture (step 1): ", Round[capT, 0.01], " s   ops=", TJitOpCount[jit]];

Do[jit[], {warm - 1}];
pr["  warm: ", warm, " steps done, pos=", dsvPosition[state], " tok=", dsvToken[state]];

times = Table[First @ AbsoluteTiming @ jit[], {steps}];
msMin = 1000. * Min[times];
msMean = 1000. * Mean[times];
pr["  timed ", steps, " steps: min ", Round[msMin, 0.01], " ms/step   mean ",
    Round[msMean, 0.01], " ms/step   ", Round[1000./msMin, 0.1], " tok/s (min)   ",
    Round[1000./msMean, 0.1], " tok/s (mean)"];
pr["  final pos=", dsvPosition[state], " tok=", dsvToken[state]];
pr[If[0 <= dsvToken[state] < cfg["Vocab"] && dsvPosition[state] === warm + steps + 1, "OK", "FAIL(state)"]];
