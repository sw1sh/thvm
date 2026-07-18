(* deepseek_decode.wlt - DeepSeek-V2-Lite decode step (NN/Examples/DeepSeekV2/Decode.wl)
   on the CPU backend: the 2-pass argmax kernels against a host argmax, and a
   scaled-down 2-layer decode pipeline (custom rope/cache/MLA3/route/moe_full
   kernels + native bf16 projections, replayed through TJit) against the
   independent host-precision reference (dsvRefStep) at bf16 tolerance. *)

Get["WolframInstitute`THVMLink`Examples`"]

$cfg = dsvConfig[
    "Dim" -> 64,
    "NLayers" -> 2,
    "NHeads" -> 4,
    "KvLora" -> 32,
    "DNope" -> 16,
    "DRope" -> 8,
    "DV" -> 16,
    "NExperts" -> 8,
    "NShared" -> 2,
    "TopK" -> 3,
    "MoeInter" -> 48,
    "DenseInter" -> 96,
    "Vocab" -> 256,
    "MaxSeq" -> 32
]

VerificationTest[
    TInit[]; TReset[];
    (* 2-pass argmax over a vocab that spans several partial blocks (vocab 1000
       -> 4 blocks x chunk 250), winner planted off the block boundaries *)
    Block[{acfg, k, lg, pv, pi, tok, host},
        acfg = dsvConfig["Vocab" -> 1000];
        k = dsvKernels[acfg];
        SeedRandom[11];
        host = RandomReal[{-1., 1.}, 1000];
        host[[777]] = 2.5;
        lg = TRealize @ TTensorCreate @ NumericArray[host, "Real32"];
        pv = TTensor[{acfg["ArgmaxBlocks"]}, "f32"];
        pi = TTensor[{acfg["ArgmaxBlocks"]}, "i32"];
        tok = TTensor[{1}, "i32"];
        k["ArgmaxPart"][pv, pi, lg];
        k["ArgmaxFin"][tok, pv, pi];
        First @ Normal @ TTensorData @ tok
    ],
    776,
    TestID -> "deepseek-decode/argmax-2pass"
]

VerificationTest[
    (* 6 self-fed decode steps through the captured TJit step; per-step logits
       vs the host reference (same synthetic bf16 weights, same input token
       stream) stay within bf16 tolerance *)
    state = dsvSynthInit[$cfg];
    ref = dsvRefInit[state];
    jit = dsvDecodeJit[state];
    diffs = Table[
        Block[{tokIn, refOut, logits},
            tokIn = dsvToken[state];
            refOut = dsvRefStep[ref, tokIn];
            ref = refOut["State"];
            logits = First @ Normal @ Last @ jit[];
            Max @ Abs[logits - refOut["Logits"]] / Max @ Abs[refOut["Logits"]]
        ],
        {6}
    ];
    Max[diffs] < 2.*^-2,
    True,
    TestID -> "deepseek-decode/2layer-vs-host-reference"
]

VerificationTest[
    (* the device length advanced once per step (6 steps from L = 1) and the
       fed-back token id is a valid vocab index *)
    {dsvPosition[state], 0 <= dsvToken[state] < $cfg["Vocab"]},
    {7, True},
    TestID -> "deepseek-decode/device-length-and-token"
]

VerificationTest[
    (* dsvGenerate: prefill a 3-token prompt then greedy-decode 5 tokens; the
       device output must equal the independent host reference fed the same
       prompt and then self-fed its own argmax (so the TSetData prompt
       injection + the greedy loop are exact, not just close) *)
    Block[{st, prompt, dev, r, hostGen},
        st = dsvSynthInit[$cfg];
        prompt = {5, 9, 13};
        dev = dsvGenerate[st, prompt, "MaxTokens" -> 5, "EosId" -> -1];
        r = dsvRefStep[dsvRefInit[st], First[prompt]];
        Do[r = dsvRefStep[r["State"], prompt[[i]]], {i, 2, Length[prompt]}];
        hostGen = Reap[Module[{g = r["Token"]},
            Do[Sow[g]; r = dsvRefStep[r["State"], g]; g = r["Token"], {5}]]][[2, 1]];
        dev === hostGen
    ],
    True,
    TestID -> "deepseek-decode/generate-vs-host-reference"
]

VerificationTest[
    (* EosId stops decoding: forcing the eos to the first predicted token yields
       an empty completion, and the MaxSeq horizon guard never overruns the
       caches (MaxTokens far past MaxSeq still returns, no SIGABRT) *)
    Block[{st = dsvSynthInit[$cfg], first},
        first = First @ dsvGenerate[st, {5, 9, 13}, "MaxTokens" -> 3, "EosId" -> -1];
        {dsvGenerate[st, {5, 9, 13}, "MaxTokens" -> 3, "EosId" -> first],
         Length @ dsvGenerate[st, {5}, "MaxTokens" -> 10000, "EosId" -> -1] <= $cfg["MaxSeq"]}
    ],
    {{}, True},
    TestID -> "deepseek-decode/generate-eos-and-horizon"
]

VerificationTest[
    (* dsvReset rewinds the device state to a fresh conversation in place *)
    Block[{st = dsvSynthInit[$cfg], j},
        j = dsvDecodeJit[st]; j[]; j[];
        dsvReset[st];
        {dsvPosition[st], dsvToken[st]}
    ],
    {1, 1},
    TestID -> "deepseek-decode/reset"
]

VerificationTest[
    (* Q8_0 MoE kernels (cfg "ExpertsQ8"): on crafted Q8_0 data whose dequant is
       exact in bf16 (scale 1.0, small-int quants), the q8 kernels must match
       the bf16 kernels BITWISE -- same accumulation order, no rounding slack *)
    Block[{d = 64, e = 8, mi = 64, kTop = 3, si = 128, mk, bf, f32c, i32c, q8stream, mkq8, runMoe,
        wgQ, wuQ, wdQ, x, swg, swu, swd, sel, probs, hBf, oBf, hQ8, oQ8},
        TReset[];
        SeedRandom[42];
        mk = keys |-> dsvConfig["Dim" -> d, "NLayers" -> 2, "NHeads" -> 4, "KvLora" -> 32,
            "DNope" -> 16, "DRope" -> 8, "DV" -> 16, "NExperts" -> e, "NShared" -> 2,
            "TopK" -> kTop, "MoeInter" -> mi, "DenseInter" -> 96, "Vocab" -> 256, "MaxSeq" -> 32, keys];
        bf = vals |-> TRealize @ TUOpCast[TTensorCreate @ NumericArray[N @ vals, "Real32"], "bf16"];
        f32c = vals |-> TRealize @ TTensorCreate @ NumericArray[N @ vals, "Real32"];
        i32c = vals |-> TRealize @ TTensorCreate[vals, "i32"];
        q8stream = quants |-> Join @@ Map[Join[{0, 60}, Mod[#, 256]] &, Partition[quants, 32]];
        mkq8 = Function[{quants, tag}, Module[{f = FileNameJoin[{$TemporaryDirectory, "q8t_" <> tag <> ".bin"}], bytes = q8stream[quants]},
            BinaryWrite[f, bytes, "UnsignedInteger8"]; Close[f];
            TRealize @ TTensorMMap[f, 0, Length[bytes], "u8", {Length[bytes]}]]];
        wgQ = RandomInteger[{-8, 8}, e * mi * d]; wuQ = RandomInteger[{-8, 8}, e * mi * d]; wdQ = RandomInteger[{-8, 8}, e * d * mi];
        x = bf[RandomInteger[{-4, 4}, d] / 4.];
        swg = bf[RandomInteger[{-4, 4}, si * d] / 4.]; swu = bf[RandomInteger[{-4, 4}, si * d] / 4.];
        swd = bf[RandomInteger[{-4, 4}, d * si] / 4.];
        sel = i32c @ {0, 3, 5}; probs = f32c @ {0.5, 0.3, 0.2};
        runMoe = Function[{cfg, wgT, wuT, wdT}, Module[{k = dsvKernels[cfg], h, out},
            h = TRealize @ TTensor[{kTop * mi + si}, "f32"];
            out = TRealize @ TTensor[{d}, "bf16"];
            k["MoeH"][h, x, sel, wgT, wuT, swg, swu];
            k["MoeOut"][out, h, sel, probs, wdT, swd];
            {Normal @ TTensorData @ h, Normal @ TUOpCast[out, "f32"]}]];
        {hBf, oBf} = runMoe[mk[{}], bf @ wgQ, bf @ wuQ, bf @ wdQ];
        {hQ8, oQ8} = runMoe[mk["ExpertsQ8" -> True], mkq8[wgQ, "wg"], mkq8[wuQ, "wu"], mkq8[wdQ, "wd"]];
        {hBf === hQ8, oBf === oQ8}
    ],
    {True, True},
    TestID -> "deepseek-decode/moe-q8-bitwise"
]

VerificationTest[
    (* dsvChatPrompt renders the DeepSeek-V2-Chat template: bos marker, the turn
       lines, and the trailing generation cue *)
    Block[{p = dsvChatPrompt[{<|"Role" -> "user", "Content" -> "Hi"|>}]},
        {StringContainsQ[p, "begin\:2581of\:2581sentence"], StringEndsQ[p, "User: Hi\n\nAssistant:"]}
    ],
    {True, True},
    TestID -> "deepseek-decode/chat-prompt"
]

VerificationTest[
    (* dsvHfWeights mapping: in-memory HF-LAYOUT bf16 weights (nn.Linear
       {out,in} matrices, un-split kv_b_proj, per-expert matrices) map into the
       decode layouts by movement + exact casts only -- every spot-checked
       tensor must equal the host-side transform of the same bf16 values
       EXACTLY, and the mapped state must decode against the host reference
       like the synthetic one *)
    TReset[];
    bf = a |-> TRealize @ TUOpCast[TTensorCreate @ NumericArray[a, "Real32"], "bf16"];
    SeedRandom[23];
    rnd = dims |-> RandomReal[{-0.05, 0.05}, dims];
    hf = Block[{d, hn, dn, dr, dv, kv, e, mi, si, sd},
        {d, hn, dn, dr, dv, kv, e, mi, si} = $cfg /@
            {"Dim", "NHeads", "DNope", "DRope", "DV", "KvLora", "NExperts", "MoeInter", "SharedInter"};
        sd = <|
            "model.embed_tokens.weight" -> rnd[{$cfg["Vocab"], d}],
            "model.norm.weight" -> RandomReal[{0.9, 1.1}, d],
            "lm_head.weight" -> rnd[{$cfg["Vocab"], d}]
        |>;
        Do[
            With[{p = "model.layers." <> ToString[l - 1] <> "."},
                sd[p <> "input_layernorm.weight"] = RandomReal[{0.9, 1.1}, d];
                sd[p <> "self_attn.q_proj.weight"] = rnd[{hn (dn + dr), d}];
                sd[p <> "self_attn.kv_a_proj_with_mqa.weight"] = rnd[{kv + dr, d}];
                sd[p <> "self_attn.kv_a_layernorm.weight"] = RandomReal[{0.9, 1.1}, kv];
                sd[p <> "self_attn.kv_b_proj.weight"] = rnd[{hn (dn + dv), kv}];
                sd[p <> "self_attn.o_proj.weight"] = rnd[{d, hn dv}];
                sd[p <> "post_attention_layernorm.weight"] = RandomReal[{0.9, 1.1}, d];
                If[ l <= $cfg["FirstDense"],
                    sd[p <> "mlp.gate_proj.weight"] = rnd[{$cfg["DenseInter"], d}];
                    sd[p <> "mlp.up_proj.weight"] = rnd[{$cfg["DenseInter"], d}];
                    sd[p <> "mlp.down_proj.weight"] = rnd[{d, $cfg["DenseInter"]}]
                    ,
                    sd[p <> "mlp.gate.weight"] = rnd[{e, d}];
                    Do[
                        sd[p <> "mlp.experts." <> ToString[i - 1] <> ".gate_proj.weight"] = rnd[{mi, d}];
                        sd[p <> "mlp.experts." <> ToString[i - 1] <> ".up_proj.weight"] = rnd[{mi, d}];
                        sd[p <> "mlp.experts." <> ToString[i - 1] <> ".down_proj.weight"] = rnd[{d, mi}],
                        {i, e}
                    ];
                    sd[p <> "mlp.shared_experts.gate_proj.weight"] = rnd[{si, d}];
                    sd[p <> "mlp.shared_experts.up_proj.weight"] = rnd[{si, d}];
                    sd[p <> "mlp.shared_experts.down_proj.weight"] = rnd[{d, si}]
                ]
            ],
            {l, $cfg["NLayers"]}
        ];
        sd
    ];
    hfT = bf /@ hf;                       (* bf16 TTerms in checkpoint layout *)
    hfQ = Normal /@ hfT;                  (* their exact bf16-quantized values *)
    stateHf = dsvHfInit[$cfg, hfT];
    lw2 = stateHf["Weights"]["Layers"][[2]];
    Block[{dn = $cfg["DNope"], dv = $cfg["DV"], hn = $cfg["NHeads"], kv = $cfg["KvLora"], kvb, checks},
        kvb = ArrayReshape[hfQ["model.layers.1.self_attn.kv_b_proj.weight"], {hn, dn + dv, kv}];
        checks = {
            Normal[stateHf["Weights"]["LmHead"]] === Transpose[hfQ["lm_head.weight"]],
            Normal[lw2["Wq"]] === Transpose[hfQ["model.layers.1.self_attn.q_proj.weight"]],
            Normal[lw2["Wuk"]] === Flatten[kvb[[All, 1 ;; dn, All]]],
            Normal[lw2["Wuv"]] === Flatten[kvb[[All, dn + 1 ;;, All]]],
            Normal[lw2["WGate"]] === Transpose[hfQ["model.layers.1.mlp.gate.weight"]],
            Normal[lw2["Wg"]] === Flatten[Table[hfQ["model.layers.1.mlp.experts." <> ToString[i - 1] <> ".gate_proj.weight"], {i, $cfg["NExperts"]}]],
            Normal[lw2["Swd"]] === Flatten[hfQ["model.layers.1.mlp.shared_experts.down_proj.weight"]],
            Normal[lw2["WIn"]] === hfQ["model.layers.1.input_layernorm.weight"]
        };
        refHf = dsvRefInit[stateHf];
        jitHf = dsvDecodeJit[stateHf];
        diffsHf = Table[
            Block[{tokIn, refOut, logits},
                tokIn = dsvToken[stateHf];
                refOut = dsvRefStep[refHf, tokIn];
                refHf = refOut["State"];
                logits = First @ Normal @ Last @ jitHf[];
                Max @ Abs[logits - refOut["Logits"]] / Max @ Abs[refOut["Logits"]]
            ],
            {3}
        ];
        Append[checks, Max[diffsHf] < 2.*^-2]
    ],
    ConstantArray[True, 9],
    TestID -> "deepseek-decode/hf-weight-mapping"
]

VerificationTest[
    TReset[];
    True,
    True,
    TestID -> "deepseek-decode/cleanup"
]
