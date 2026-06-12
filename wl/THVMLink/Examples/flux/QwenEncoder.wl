(* QwenEncoder.wl: the faithful Qwen3-4B text encoder for FLUX.2-klein.

   Produces the {512, 7680} text embedding that conditions the FLUX.2-klein
   diffusion transformer.  Run the HF Qwen3 decoder layers 0..26 over the
   tokenized prompt, capture the hidden states after layers 8 / 17 / 26 (the
   diffusers text_encoder_out_layers), and concat the three {512, 2560} states
   per token along the feature axis to {512, 7680} in [layer9 | layer18 |
   layer27] order.

   Get-loaded after THVMLink` AND FluxForward.wl: it reuses the verified
   FluxForward primitives (fxLinear, fxRMSNorm, fxConcat, fxSiLU, fxBmm).
   Qwen3 differs from the FLUX DiT in two ways that matter: the rotary
   convention is the half-split (NEOX) one, not FluxForward's interleaved
   fxRoPE, and attention is grouped-query (GQA 4:1) with per-head q/k RMSNorm
   applied before the rotary.  Weights stay bf16; fxLinear runs the bf16-direct
   matmul, so there is no Real32 weight intermediate.

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

(* Half-split rotary (Qwen3 / NEOX rotate_half): t {S, H, D}; cos, sin {S, 1, D}
   broadcast over the head axis.  rotate_half(t) = concat[-t[D/2:], t[:D/2]];
   t_rot = t cos + rotate_half(t) sin.  Distinct from FluxForward's interleaved
   fxRoPE. *)
qwRoPE[x_, cos_, sin_] := Block[{s, h, d, half, lo, hi, rot, shape},
    {s, h, d} = Dimensions[x];  half = d/2;  shape = {s, h, d};
    lo = TUOpShrink[x, {{0, s}, {0, h}, {0, half}}];
    hi = TUOpShrink[x, {{0, s}, {0, h}, {half, d}}];
    rot = fxConcat[{-hi, lo}, 3];
    x*TUOpExpand[cos, shape] + rot*TUOpExpand[sin, shape]
]

(* GQA head expansion: a {S, Hkv, D} tensor whose Hkv kv heads each serve `rep`
   query heads, broadcast to {S, Hkv*rep, D} in HF repeat_interleave order (kv
   head j serves query heads rep*j .. rep*j+rep-1).  Insert a unit axis, expand
   it to rep, fold back: the repeats land contiguously per kv head. *)
qwExpandKV[x_, rep_] := Block[{s, hkv, d},
    {s, hkv, d} = Dimensions[x];
    ArrayReshape[TUOpExpand[ArrayReshape[x, {s, hkv, 1, d}], {s, hkv, rep, d}], {s, hkv*rep, d}]
]

(* Masked scaled-dot attention over head-split, rotated, GQA-expanded q/k/v
   {S, H, D}.  addMask {S, S} is the additive causal+padding bias added to the
   scores before softmax.  Heads ride the leading batch axis (one batched reduce
   per matmul). *)
qwAttention[q_, k_, v_, scale_, addMask_] := Block[
    {s, h, d, sk, qh, kh, vh, scores, attn, out},
    {s, h, d} = Dimensions[q];  sk = Dimensions[k][[1]];
    qh = Transpose[q, {2, 1, 3}];  kh = Transpose[k, {2, 1, 3}];  vh = Transpose[v, {2, 1, 3}];
    scores = fxBmm[qh, Transpose[kh, {1, 3, 2}], h, s, d, sk]*scale + TUOpExpand[ArrayReshape[addMask, {1, s, sk}], {h, s, sk}];
    attn = TSoftmax[scores, 2];
    out = fxBmm[attn, vh, h, s, sk, d];
    ArrayReshape[Transpose[out, {2, 1, 3}], {s, h*d}]
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
   before rotary; GQA-expand k/v; masked attention; SwiGLU MLP. *)
qwLayer[x_, cos_, sin_, addMask_, W_, cfg_] := Block[
    {h, hkv, dh, eps, scale, rep, s, xn, q, k, v, attnOut, hh, hn},
    h = cfg["heads"];  hkv = cfg["kv_heads"];  dh = cfg["head_dim"];
    eps = cfg["eps"];  scale = 1/Sqrt[N[dh]];  rep = h/hkv;  s = Dimensions[x][[1]];
    xn = fxRMSNorm[x, W["input_ln"], eps];
    q = fxRMSNorm[ArrayReshape[fxLinear[xn, W["q_proj"]], {s, h, dh}], W["q_norm"], eps];
    k = fxRMSNorm[ArrayReshape[fxLinear[xn, W["k_proj"]], {s, hkv, dh}], W["k_norm"], eps];
    v = ArrayReshape[fxLinear[xn, W["v_proj"]], {s, hkv, dh}];
    q = qwRoPE[q, cos, sin];  k = qwRoPE[k, cos, sin];
    attnOut = fxLinear[qwAttention[q, qwExpandKV[k, rep], qwExpandKV[v, rep], scale, addMask], W["o_proj"]];
    hh = x + attnOut;
    hn = fxRMSNorm[hh, W["post_ln"], eps];
    hh + fxLinear[fxSiLU[fxLinear[hn, W["gate_proj"]]]*fxLinear[hn, W["up_proj"]], W["down_proj"]]
]

(* Host-side rotary cos/sin for the half-split convention: inv_freq[i] =
   theta^(-2i/D); freqs = outer(pos, inv_freq); emb = concat[freqs, freqs].
   Returns {cos, sin} as {S, 1, D} so they broadcast over the head axis. *)
qwRopeCosSin[seq_Integer, headDim_Integer, theta_] := Block[{half, invFreq, emb},
    half = headDim/2;
    invFreq = Table[N[theta]^(-2 i/headDim), {i, 0, half - 1}];
    emb = Table[Join[#, #] &[p invFreq], {p, 0, seq - 1}];
    {
        ArrayReshape[TTensorCreate[N[Cos[emb]]], {seq, 1, headDim}],
        ArrayReshape[TTensorCreate[N[Sin[emb]]], {seq, 1, headDim}]
    }
]

(* Host-side additive attention mask {S, S}: 0 where token i may attend to j
   (j <= i causal AND attMask[j] real), a large negative elsewhere. *)
qwAddMask[attMask_List, neg_: -1.*^9] := With[{s = Length[attMask]},
    TTensorCreate[N @ Table[If[ j <= i && attMask[[j]] == 1, 0., neg], {i, s}, {j, s}]]
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
    {cos, sin} = qwRopeCosSin[s, dh, theta];
    addMask = qwAddMask[attMask];
    x = qwEmbed[wf["model.embed_tokens.weight"], inputIds];
    captured = <||>;
    Do[ x = TRealize @ qwLayer[x, cos, sin, addMask, qwLayerW[wf, i], lcfg];
        If[ MemberQ[caps, i], captured[i] = x],
        {i, 0, nL - 1}
    ];
    TRealize @ fxConcat[captured[#] & /@ caps, 2]
]
