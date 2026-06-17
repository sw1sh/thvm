(* qwen_encoder_parity.wls: incremental parity for the Qwen3-4B text encoder
   against a transformers ground truth.  Validates each stage (embedding,
   after-layer-0, after-layer-1, after-layer 8/17/26, final text_emb) by
   correlation + max|d| vs the reference safetensors dumps.

   bf16-native: weights stay bf16 and ride fxLinear's bf16-direct matmul, so
   this wants the Metal backend (DEV=metal; CPU has no bf16 gemm and falls to a
   slow scalar path).  Point QWEN_SHARD1 / QWEN_SHARD2 at the two Qwen3 weight
   safetensors (the FLUX.2-klein text_encoder model-0000{1,2}-of-00002).

   Run:  DEV=metal QWEN_SHARD1=... QWEN_SHARD2=... wolframscript -file <this>
   (cwd = repo root) *)

PacletDirectoryLoad["wl/THVMLink"]; Get["WolframInstitute`THVMLink`"];
Get["wl/THVMLink/Examples/flux/FluxForward.wl"];
Get["wl/THVMLink/Examples/flux/QwenEncoder.wl"];

pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);
dev = If[ ToLowerCase[Environment["DEV"]] === "metal", "metal", "cpu"];
report[name_, o_, r_] := With[{x = Flatten@Normal@o, y = Flatten@Normal@r},
    pr[
        StringPadRight[name, 10], "  corr = ", ToString[Correlation[x, y], InputForm],
        "   max|d| = ", ToString[Max@Abs[x - y], InputForm]
    ]
]

ref = TSafeTensorLoad["/tmp/flux_qwen_ref/qwen_hs.safetensors"];
e2e = TSafeTensorLoad["/tmp/flux_e2e_ref/inputs.safetensors"];
ids = Round @ Normal @ ref["input_ids"];
am = Round @ Normal @ ref["attention_mask"];
pr["seq ", ToString@Length@ids, "  real tokens ", ToString@Total@am, "  dev ", dev];

qwt = Join[TSafeTensorLoad[Environment["QWEN_SHARD1"]], TSafeTensorLoad[Environment["QWEN_SHARD2"]]];
wf[n_] := wf[n] = If[ n === "model.embed_tokens.weight", qwt[n], TToDevice[TRealize@qwt[n], dev]];

cfg = <|"heads" -> 32, "kv_heads" -> 8, "head_dim" -> 128, "eps" -> 1.*^-6, "theta" -> 1000000|>;
{cos, sin} = TRoPEHalfSplitTable[Length@ids, cfg["head_dim"], cfg["theta"]];
addMask = TPaddingCausalMask[am];

x = qwEmbed[wf["model.embed_tokens.weight"], ids];
report["hs_0", x, ref["hs_0"]];
cap = <||>;
Do[ x = TRealize @ qwLayer[x, cos, sin, addMask, qwLayerW[wf, i], cfg];
    Which[
        i == 0, report["hs_1", x, ref["hs_1"]],
        i == 1, report["hs_2", x, ref["hs_2"]],
        i == 8, cap[9] = x;  report["hs_9", x, ref["hs_9"]],
        i == 17, cap[18] = x;  report["hs_18", x, ref["hs_18"]],
        i == 26, cap[27] = x;  report["hs_27", x, ref["hs_27"]]
    ],
    {i, 0, 26}
];
report["text_emb", TRealize @ Join[cap[9], cap[18], cap[27], 2], e2e["text_emb"]];
pr["DONE"];
