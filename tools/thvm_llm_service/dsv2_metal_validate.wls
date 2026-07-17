#!/usr/bin/env wl
(* ============================================================================
   HUMAN-RUN ONLY -- THIS SCRIPT DISPATCHES DSV2 DECODE KERNELS ON THE METAL GPU.

   Do NOT run this in an agent / CI / sandbox.  A buggy Metal dispatch can jam
   the GPU and force a machine reboot.  Run it yourself, from the repo root:

       wl -f tools/thvm_llm_service/dsv2_metal_validate.wls

   What it checks: the fused DeepSeek-V2-Lite decode step (Decode.wl) produces
   the SAME greedy token stream on Device -> "metal" (the $dsv*Metal custom
   kernels + metal_custom_dispatch) as on Device -> "cpu" (the plain-C twins),
   over the small synthetic-weight config.  Both backends run the identical bf16
   math over identically seeded weights, so the argmax token ids must match
   exactly.  The first-step f32 logits differ only by reduction order (Metal's
   32-lane simd_shuffle_down tree vs. the CPU serial loop), so they are reported
   at bf16 tolerance rather than compared bit-exactly.
   ============================================================================ *)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];
Get["WolframInstitute`THVMLink`Examples`"];

$prompt = {5, 9, 13};
$maxTokens = 8;
$tol = 2.*^-2;

(* The small test config (matches deepseek_decode.wlt), on device dev.  MaxSeq
   64 leaves room for prompt + generated tokens. *)
smallCfg[dev_] := dsvConfig[
    "Dim" -> 64, "NLayers" -> 2, "NHeads" -> 4, "KvLora" -> 32,
    "DNope" -> 16, "DRope" -> 8, "DV" -> 16, "NExperts" -> 8,
    "NShared" -> 2, "TopK" -> 3, "MoeInter" -> 48, "DenseInter" -> 96,
    "Vocab" -> 256, "MaxSeq" -> 64, "Device" -> dev
]

(* Greedy token stream on device dev via the public generate loop (prefill +
   captured-step replay), over the same seeded synthetic weights. *)
runTokens[dev_] := (
    TInit[]; TReset[];
    dsvGenerate[dsvSynthInit[smallCfg[dev]], $prompt, "MaxTokens" -> $maxTokens, "EosId" -> -1]
)

(* First-step logits on device dev, for a reduction-order numeric signal. *)
runLogits[dev_] := Block[{state},
    TInit[]; TReset[];
    state = dsvSynthInit[smallCfg[dev]];
    First @ Normal @ TTensorData @ Last @ dsvDecodeStep[state]
]

cpuTokens = runTokens["cpu"];
metalTokens = runTokens["metal"];
reldiff = Max @ Abs[runLogits["metal"] - runLogits["cpu"]] / Max @ Abs[runLogits["cpu"]];

Print["prompt         : ", $prompt];
Print["cpu   tokens   : ", cpuTokens];
Print["metal tokens   : ", metalTokens];
Print["logits rel diff: ", reldiff, " (tol ", $tol, ")"];

If[ cpuTokens === metalTokens,
    Print["VALIDATE PASS: metal decode matches cpu decode (", Length[cpuTokens], " tokens)"]
    ,
    Print["VALIDATE FAIL: metal decode DIVERGES from cpu decode"]
]
