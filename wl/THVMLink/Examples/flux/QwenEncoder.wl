(* QwenEncoder.wl: the faithful Qwen3-4B text encoder for FLUX.2-klein.

   Produces the {512, 7680} text embedding that conditions the FLUX.2-klein
   diffusion transformer.  Run the HF Qwen3 decoder layers 0..26 over the
   tokenized prompt, capture the hidden states after layers 8 / 17 / 26 (the
   diffusers text_encoder_out_layers), and concat the three {512, 2560} states
   per token along the feature axis to {512, 7680} in [layer9 | layer18 |
   layer27] order.

   Get-loaded after THVMLink` AND FluxForward.wl: it reuses fxLinear (the
   BLAS-friendly diffusers linear).  All the model-agnostic attention pieces
   come from THVMLink`s NN library.  Qwen3 differs from the FLUX DiT in two
   ways that matter: the rotary convention is the half-split (NEOX) one
   (TRoPEHalfSplit, not the FLUX TRoPEInterleaved), and attention is
   grouped-query (GQA 4:1, TRepeatKV) with per-head q/k RMSNorm applied before
   the rotary.  Weights stay bf16; fxLinear runs the bf16-direct matmul, so
   there is no Real32 weight intermediate.

   Qwen3-4B config: hidden 2560, head_dim 128, 32 query heads, 8 kv heads
   (GQA 4:1), intermediate 9728, rms_norm_eps 1e-6, rope_theta 1e6, SiLU MLP,
   no attention bias. *)

(* Embedding lookup.  thvm has no true gather (TEmbedding/TGather lower to a
   one-hot matmul, infeasible over the 151936-long vocabulary), so select the
   token rows of the frozen bf16 table on the host: Normal returns the raw
   uint16 bf16 words, pick the (0-based) id rows, decode just those to f32 (a
   bf16 word is the high half of the f32, low half zero, exactly the device
   cast).  One-time input prep on a frozen table, not a per-step roundtrip;
   peak stays at the {S, dim} output. *)
qwEmbed[table_, ids_List] :=
    TTensorCreate @ qwBf16ToF32 @ Normal[table][[ids + 1]]

qwBf16ToF32[u16_] := With[{shape = Dimensions[u16], flat = Flatten[u16]},
    ArrayReshape[
        Flatten @ ImportByteArray[
            ByteArray @ Flatten @ Transpose @ {
                ConstantArray[0, Length[flat]], ConstantArray[0, Length[flat]],
                Mod[flat, 256], Quotient[flat, 256]
            },
            {"Binary", "Real32"}
        ],
        shape
    ]
]

(* Per-layer weight Association from a name -> TTerm loader wf (HF names).
   Weights stay bf16; fxLinear's bf16-direct matmul reads them as-is. *)
qwLayerW[wf_, i_] := With[{p = "model.layers." <> ToString[i] <> "."}, <|
    "input_ln" -> wf[p <> "input_layernorm.weight"],
    "q_proj" -> wf[p <> "self_attn.q_proj.weight"],
    "k_proj" -> wf[p <> "self_attn.k_proj.weight"],
    "v_proj" -> wf[p <> "self_attn.v_proj.weight"],
    "o_proj" -> wf[p <> "self_attn.o_proj.weight"],
    "q_norm" -> wf[p <> "self_attn.q_norm.weight"],
    "k_norm" -> wf[p <> "self_attn.k_norm.weight"],
    "post_ln" -> wf[p <> "post_attention_layernorm.weight"],
    "gate_proj" -> wf[p <> "mlp.gate_proj.weight"],
    "up_proj" -> wf[p <> "mlp.up_proj.weight"],
    "down_proj" -> wf[p <> "mlp.down_proj.weight"]
|>]

(* One HF Qwen3 decoder layer.  x {S, dim}; cos/sin {S, 1, head_dim}; addMask
   {S, S}; W the layer weights; cfg has heads/kv_heads/head_dim/eps.  qk-norm
   before rotary (TRMSNorm); GQA-expand k/v (TRepeatKV); masked attention
   (THeadAttention); SwiGLU MLP. *)
qwLayer[x_, cos_, sin_, addMask_, W_, cfg_] := Block[
    {h, hkv, dh, eps, scale, rep, s, xn, q, k, v, attnOut, hh, hn},
    h = cfg["heads"];  hkv = cfg["kv_heads"];  dh = cfg["head_dim"];
    eps = cfg["eps"];  scale = 1/Sqrt[N[dh]];  rep = h/hkv;  s = Dimensions[x][[1]];
    xn = TRMSNorm[x, W["input_ln"], eps];
    q = TRMSNorm[ArrayReshape[fxLinear[xn, W["q_proj"]], {s, h, dh}], W["q_norm"], eps];
    k = TRMSNorm[ArrayReshape[fxLinear[xn, W["k_proj"]], {s, hkv, dh}], W["k_norm"], eps];
    v = ArrayReshape[fxLinear[xn, W["v_proj"]], {s, hkv, dh}];
    q = TRoPEHalfSplit[q, cos, sin];  k = TRoPEHalfSplit[k, cos, sin];
    attnOut = fxLinear[THeadAttention[q, TRepeatKV[k, rep], TRepeatKV[v, rep], scale, addMask], W["o_proj"]];
    hh = x + attnOut;
    hn = TRMSNorm[hh, W["post_ln"], eps];
    hh + fxLinear[TSiLU[fxLinear[hn, W["gate_proj"]]]*fxLinear[hn, W["up_proj"]], W["down_proj"]]
]

(* Full encoder.  inputIds {S} host int list (0-indexed); attMask {S} host list
   (1 real / 0 pad); wf a name -> TTerm loader (HF names, both shards merged);
   cfg has heads/kv_heads/head_dim/eps/theta/layers/captureLayers.  Returns the
   {S, 3*hidden} per-token concat of the captured hidden states. *)
qwenEncode[inputIds_List, attMask_List, wf_, cfg_] := Block[
    {dh, eps, theta, nL, caps, s, cos, sin, addMask, x, captured, lcfg},
    dh = cfg["head_dim"];  eps = cfg["eps"];  theta = cfg["theta"];
    nL = cfg["layers"];  caps = cfg["captureLayers"];  s = Length[inputIds];
    lcfg = <|"heads" -> cfg["heads"], "kv_heads" -> cfg["kv_heads"], "head_dim" -> dh, "eps" -> eps|>;
    {cos, sin} = TRoPEHalfSplitTable[s, dh, theta];
    addMask = TPaddingCausalMask[attMask];
    x = qwEmbed[wf["model.embed_tokens.weight"], inputIds];
    captured = <||>;
    Do[ x = TRealize @ qwLayer[x, cos, sin, addMask, qwLayerW[wf, i], lcfg];
        If[ MemberQ[caps, i], captured[i] = x],
        {i, 0, nL - 1}
    ];
    TRealize @ Join[Sequence @@ (captured[#] & /@ caps), 2]
]
