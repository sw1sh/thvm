(* QwenEncoder.wl: the faithful Qwen3-4B text encoder for FLUX.2-klein.

   Produces the {512, 7680} text embedding that conditions the FLUX.2-klein
   diffusion transformer.  Run the HF Qwen3 decoder layers 0..26 over the
   tokenized prompt, capture the hidden states after layers 8 / 17 / 26 (the
   diffusers text_encoder_out_layers), and concat the three {512, 2560} states
   per token along the feature axis to {512, 7680} in [layer9 | layer18 |
   layer27] order.

   Get-loaded after WolframInstitute`THVMLink` AND FluxForward.wl: it reuses fxLinear (the
   BLAS-friendly diffusers linear).  All the model-agnostic attention pieces
   come from WolframInstitute`THVMLink`s NN library.  Qwen3 differs from the FLUX DiT in two
   ways that matter: the rotary convention is the half-split (NEOX) one
   (TRoPEHalfSplit, not the FLUX TRoPEInterleaved), and attention is
   grouped-query (GQA 4:1, TRepeatKV) with per-head q/k RMSNorm applied before
   the rotary.  Weights stay bf16; fxLinear runs the bf16-direct matmul, so
   there is no Real32 weight intermediate.

   Qwen3-4B config: hidden 2560, head_dim 128, 32 query heads, 8 kv heads
   (GQA 4:1), intermediate 9728, rms_norm_eps 1e-6, rope_theta 1e6, SiLU MLP,
   no attention bias. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

Begin["`Private`"];

(* Embedding lookup.  thvm has no true gather (TEmbedding/TGather lower to a
   one-hot matmul, infeasible over the 151936-long vocabulary), so select the
   token rows of the frozen bf16 table on the host: Normal returns the raw
   uint16 bf16 words, pick the (0-based) id rows, decode just those to f32 (a
   bf16 word is the high half of the f32, low half zero, exactly the device
   cast).  One-time input prep on a frozen table, not a per-step roundtrip;
   peak stays at the {S, dim} output. *)
(* Normal[TTensorData[table]] (raw UBit16 words, NOT the decoding Normal[t_TTerm]
   sugar) so we materialise the frozen table once as words, slice the id rows,
   and decode only those to f32 -- decoding the whole 151936-row table would
   cost 1.5GB for ~512 rows. *)
(* Materialise the frozen bf16 embedding-table words ONCE per table (the whole
   151936x2560 table is ~778MB of UBit16 words).  Memoised: qwEmbed is called per
   prompt, and re-reading TTensorData[table] each time dominated the host input
   prep (~1.2s/call).  The table is a frozen weight, so the cached words are
   reused for every prompt in the session; only the per-id row slice + decode
   (~512 rows) runs per call. *)
qwEmbedWords[table_] := qwEmbedWords[table] = Normal[TTensorData[table]]
qwEmbed[table_, ids_List] :=
    TTensorCreate @ qwBf16ToF32 @ qwEmbedWords[table][[ids + 1]]

qwBf16ToF32[u16_] := With[{shape = Dimensions[u16], flat = Flatten[u16]},
    ArrayReshape[
        Flatten @ ImportByteArray[
            ByteArray @ Flatten @ Transpose @ {
                ConstantArray[0, Length[flat]], ConstantArray[0, Length[flat]],
                Mod[flat, 256], Quotient[flat, 256]},
            {"Binary", "Real32"}],
        shape]]

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
(* bf16Act: realize an activation as bf16 so the downstream fxLinear matmul is
   bf16(act) x bf16(W) -- recognised as a tensor-core matmul and lowered to the
   TILED simdgroup path.  The Qwen embed + RMSNorm keep f32, so without this the
   projections feed a MIXED f32xbf16 matmul that is NOT recognised and falls to the
   generic scalar accumulator (~7x slower).  The cast must be REALIZED (not a lazy
   CAST(INDEX_E) operand): the recogniser peels one cast but then sees the f32 buffer
   underneath and declines; a realized cast gives a clean bf16 INDEX_E.  bf16(act)
   matches how HF Qwen3 runs the linears (bf16 weights AND activations). *)
bf16Act[t_] := TRealize @ TUOpCast[t, "bf16"]

qwLayer[x_, cos_, sin_, addMask_, W_, cfg_] := Block[
    {h, hkv, dh, eps, scale, rep, s, xn, q, k, v, attnOut, hh, hn},
    h = cfg["heads"];  hkv = cfg["kv_heads"];  dh = cfg["head_dim"];
    eps = cfg["eps"];  scale = 1/Sqrt[N[dh]];  rep = h/hkv;  s = Dimensions[x][[1]];
    xn = bf16Act @ TRMSNorm[x, W["input_ln"], eps];
    q = TRMSNorm[ArrayReshape[fxLinear[xn, W["q_proj"]], {s, h, dh}], W["q_norm"], eps];
    k = TRMSNorm[ArrayReshape[fxLinear[xn, W["k_proj"]], {s, hkv, dh}], W["k_norm"], eps];
    v = ArrayReshape[fxLinear[xn, W["v_proj"]], {s, hkv, dh}];
    q = TRoPEHalfSplit[q, cos, sin];  k = TRoPEHalfSplit[k, cos, sin];
    attnOut = fxLinear[bf16Act @ THeadAttention[q, TRepeatKV[k, rep], TRepeatKV[v, rep], scale, addMask], W["o_proj"]];
    hh = x + attnOut;
    hn = bf16Act @ TRMSNorm[hh, W["post_ln"], eps];
    hh + fxLinear[bf16Act[TSiLU[fxLinear[hn, W["gate_proj"]]]*fxLinear[hn, W["up_proj"]]], W["down_proj"]]]

(* DEVICE forward over the 27 decoder layers.  x {S,dim} embedded token rows;
   addMask {S,S} additive causal+padding mask; cos/sin {S,1,head_dim} the rotary
   table; wf the weight loader; cfg as below.  Captures the hidden states after
   layers in cfg["captureLayers"] and returns their per-token concat
   {S, 3*hidden}.  Split out from qwenEncode so it can be TJit-captured with x +
   addMask as the only per-prompt rebound inputs (cos/sin + weights are closed
   over, constant across prompts of the same length).  Every shape is concrete
   (kvar-free): S comes from the host-prepped x, dim/head_dim from cfg. *)
qwenForward[x0_, addMask_, cos_, sin_, wf_, cfg_] := Block[
    {nL, caps, lcfg, x, captured},
    nL = cfg["layers"];  caps = cfg["captureLayers"];
    lcfg = <|"heads" -> cfg["heads"], "kv_heads" -> cfg["kv_heads"],
             "head_dim" -> cfg["head_dim"], "eps" -> cfg["eps"]|>;
    x = x0;  captured = <||>;
    Do[ x = TRealize @ qwLayer[x, cos, sin, addMask, qwLayerW[wf, i], lcfg];
        If[ MemberQ[caps, i], captured[i] = x],
        {i, 0, nL - 1}];
    TRealize @ Join[Sequence @@ (captured[#] & /@ caps), 2]]

(* host-prep of the device inputs (cos/sin/mask/x) for qwenForward.  Returns
   <|"cos","sin","addMask","x"|> as device TTerms.  cos/sin depend only on S
   (cacheable per length); addMask + x are per-prompt. *)
qwenInputs[inputIds_List, attMask_List, wf_, cfg_] := Block[
    {dh, theta, s, dev, toDev, cos, sin},
    dh = cfg["head_dim"];  theta = cfg["theta"];  s = Length[inputIds];
    (* place x/cos/sin/mask on the device the layer weights live on: otherwise the
       additive mask (host) drags the attention-scores reduce onto the CPU, where
       bf16 has no gemm and the {H,S,D,S} expand materialises (2GB at S=512)
       instead of routing through the device batched gemm. *)
    dev = TDevice[wf["model.layers.0.self_attn.q_proj.weight"]];
    toDev = If[ dev === None || dev === "cpu", # &, TToDevice[#, dev] &];
    {cos, sin} = TRoPEHalfSplitTable[s, dh, theta];
    <|"cos" -> toDev[cos], "sin" -> toDev[sin],
      "addMask" -> toDev @ TPaddingCausalMask[attMask],
      "x" -> toDev @ qwEmbed[wf["model.embed_tokens.weight"], inputIds]|>]

(* Full encoder.  inputIds {S} host int list (0-indexed); attMask {S} host list
   (1 real / 0 pad); wf a name -> TTerm loader (HF names, both shards merged);
   cfg has heads/kv_heads/head_dim/eps/theta/layers/captureLayers.  Returns the
   {S, 3*hidden} per-token concat of the captured hidden states. *)
qwenEncode[inputIds_List, attMask_List, wf_, cfg_] := With[
    {in = qwenInputs[inputIds, attMask, wf, cfg]},
    qwenForward[in["x"], in["addMask"], in["cos"], in["sin"], wf, cfg]]

End[];

EndPackage[];
