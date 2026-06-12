(* QwenEncoder.wl -- the faithful Qwen3-4B text encoder for FLUX.2-klein.

   Produces the {512, 7680} text embedding that feeds the FLUX.2-klein
   diffusion transformer: run the HF Qwen3 decoder layers 0..26 over the
   tokenized prompt, capture the hidden states after layers 8 / 17 / 26
   (hs_9 / hs_18 / hs_27), and concat the three {512,2560} states per token
   along the feature axis -> {512, 7680} in [layer9 | layer18 | layer27]
   order.

   Get-loaded after THVMLink` AND FluxForward.wl -- it reuses the verified
   FluxForward primitives (fxLinear, fxRMSNorm, fxConcat, fxSiLU, fxBmm) and
   the lazy-shape alias fxShape.  Qwen3 differs from the FLUX DiT in two ways
   that matter here: the RoPE convention is the half-split (NEOX) one (NOT
   FluxForward's interleaved fxRoPE) and attention is grouped-query (GQA 4:1)
   with per-head q/k RMSNorm BEFORE rope.

   Qwen3-4B config: hidden_size 2560, head_dim 128, 32 q heads, 8 kv heads
   (GQA 4:1), intermediate_size 9728, rms_norm_eps 1e-6, rope_theta 1e6,
   silu MLP, no attention bias.  Weights are bf16; compute is f32 (fxLinear
   realizes the f32 weight cast, like the FLUX forward). *)

(* --- embedding lookup by host-side bf16 row selection.  Read the {vocab, dim}
       bf16 table to the host (Normal returns the raw uint16 bit patterns), pick
       the (0-based) id rows, decode just those {S, dim} bf16 values to f32, and
       wrap the result as one buffer.  Faithful: a bf16 value's f32 equivalent is
       its 16 bits placed in the high half of an f32 (low 16 zero), exactly the
       device cast, so this equals f32(table)[[ids]].  Decoding only the selected
       rows keeps peak memory at the {S, dim} output (~5 MB) -- it never
       materialises the 1.55 GB full f32 table (whose buffer would linger and,
       stacked with the per-layer weight casts, trip the host memory killer), and
       sidesteps the two stitch paths that blow up at seq 512: the 512-deep
       pad+sum tree of TEmbeddingMatrix (recursive-lift explosion) and the
       {S, vocab} one-hot matmul over the 151936-long contraction. --- *)
qwEmbed[table_, ids_List] := With[{raw = Normal[table]},
    TTensorCreate @ qwBf16ToF32 @ raw[[ids + 1]]]

(* --- decode an array of bf16 bit patterns (uint16 ints, as Normal returns a
       bf16 TTerm) to f32: a bf16 word is the high 16 bits of the f32, low 16
       zero.  Pack little-endian {0, 0, lo, hi} bytes and reinterpret Real32.
       Exact (the same value the device bf16->f32 cast produces). --- *)
qwBf16ToF32[u16arr_] := With[{shape = Dimensions[u16arr], u16 = Flatten[u16arr]},
    ArrayReshape[
        Flatten @ ImportByteArray[
            ByteArray @ Flatten @ Transpose @ {
                ConstantArray[0, Length[u16]], ConstantArray[0, Length[u16]],
                Mod[u16, 256], Quotient[u16, 256]},
            {"Binary", "Real32"}],
        shape]]

(* --- per-layer weights as host f32 leaves, decoded transiently from the bf16
       shards (Normal reads just that tensor's bytes; qwBf16ToF32 converts; the
       host array is wrapped as a leaf and the bf16 read is GC'd).  Use this
       instead of qwLayerW on a memory-pressured host: it never lets the 8 GB of
       mmap'd bf16 weights accumulate resident across 27 layers, since each
       layer's bytes are read on demand and freed -- peak stays at one layer's
       f32 weights (~400 MB) plus the activations.  Faithful (same bf16->f32
       values as the device cast). --- *)
qwLayerWHost[wf_, i_] := With[{p = "model.layers." <> ToString[i] <> ".",
    f32 = (TTensorCreate @ qwBf16ToF32 @ Normal @ wf[#] &)}, <|
    "input_ln"  -> f32[p <> "input_layernorm.weight"],
    "q_proj"    -> f32[p <> "self_attn.q_proj.weight"],
    "k_proj"    -> f32[p <> "self_attn.k_proj.weight"],
    "v_proj"    -> f32[p <> "self_attn.v_proj.weight"],
    "o_proj"    -> f32[p <> "self_attn.o_proj.weight"],
    "q_norm"    -> f32[p <> "self_attn.q_norm.weight"],
    "k_norm"    -> f32[p <> "self_attn.k_norm.weight"],
    "post_ln"   -> f32[p <> "post_attention_layernorm.weight"],
    "gate_proj" -> f32[p <> "mlp.gate_proj.weight"],
    "up_proj"   -> f32[p <> "mlp.up_proj.weight"],
    "down_proj" -> f32[p <> "mlp.down_proj.weight"]|>]

(* --- half-split rotary embedding (Qwen3 / NEOX use_real_unbind_dim, the
       HF rotate_half convention): t {S, H, D}; cos,sin {S, 1, D}.
       rotate_half(t) = concat[ -t[..., D/2:D], t[..., 0:D/2] ];
       t_rot = t*cos + rotate_half(t)*sin.  cos/sin broadcast over the H axis.
       Distinct from FluxForward's fxRoPE (the interleaved pair convention). --- *)
qwRoPE[x_, cos_, sin_] := Module[{s, h, d, half, lo, hi, rot},
    {s, h, d} = fxShape[x];  half = d/2;
    lo  = TUOpShrink[x, {{0, s}, {0, h}, {0, half}}];       (* t[..., 0:D/2]   *)
    hi  = TUOpShrink[x, {{0, s}, {0, h}, {half, d}}];       (* t[..., D/2:D]   *)
    rot = fxConcat[{TUOpNeg[hi], lo}, 3];                   (* {S, H, D}       *)
    TUOpAdd[TUOpMul[x, TUOpExpand[cos, {s, h, d}]],
            TUOpMul[rot, TUOpExpand[sin, {s, h, d}]]]]

(* --- GQA head expansion: a {S, Hkv, D} k/v tensor with Hkv kv heads, each
       serving `rep` query heads, broadcast to {S, Hkv*rep, D} in the HF
       repeat_interleave order (kv head j -> q heads rep*j .. rep*j+rep-1).
       Insert a unit axis after the kv-head axis, EXPAND it to rep, reshape:
       this lays the repeats out contiguously per kv head (interleave). --- *)
qwExpandKV[x_, rep_] := Module[{s, hkv, d},
    {s, hkv, d} = fxShape[x];
    TUOpReshape[
        TUOpExpand[TUOpReshape[x, {s, hkv, 1, d}], {s, hkv, rep, d}],
        {s, hkv*rep, d}]]

(* --- masked scaled-dot attention over already head-split + rope'd + GQA-
       expanded q,k,v {S, H, D} (k/v expanded to the full H).  addMask is the
       {S, S} additive bias (causal + padding) added to scores before softmax.
       Heads -> leading batch axis, one batched reduce per matmul. --- *)
qwAttention[q_, k_, v_, scale_, addMask_] := Module[
    {s, h, d, sk, qh, kh, vh, scores, attn, out, maskB},
    {s, h, d} = fxShape[q];  sk = fxShape[k][[1]];
    qh = Transpose[q, {2, 1, 3}];  kh = Transpose[k, {2, 1, 3}];  vh = Transpose[v, {2, 1, 3}];
    scores = TUOpMul[fxBmm[qh, Transpose[kh, {1, 3, 2}], h, s, d, sk], TUOpConst[N[scale]]];
    maskB  = TUOpExpand[TUOpReshape[addMask, {1, s, sk}], {h, s, sk}];
    scores = TUOpAdd[scores, maskB];
    attn   = TSoftmax[scores, 2];                          (* over key axis *)
    out    = fxBmm[attn, vh, h, s, sk, d];
    TUOpReshape[Transpose[out, {2, 1, 3}], {s, h*d}]]

(* per-layer weight Association from a name->TTerm loader wf (HF names).  The
   shards store bf16; cast every weight to a lazy f32 (TUOpCast) so fxLinear's
   TRealize materialises an f32 buffer cblas_sgemm can read -- a raw bf16
   weight forces the matmul down the scalar EXPAND-MUL-REDUCE fallback (~200 s
   per projection). *)
qwLayerW[wf_, i_] := With[{p = "model.layers." <> ToString[i] <> ".",
    f32 = (TUOpCast[wf[#], "f32"] &)}, <|
    "input_ln"  -> f32[p <> "input_layernorm.weight"],
    "q_proj"    -> f32[p <> "self_attn.q_proj.weight"],
    "k_proj"    -> f32[p <> "self_attn.k_proj.weight"],
    "v_proj"    -> f32[p <> "self_attn.v_proj.weight"],
    "o_proj"    -> f32[p <> "self_attn.o_proj.weight"],
    "q_norm"    -> f32[p <> "self_attn.q_norm.weight"],
    "k_norm"    -> f32[p <> "self_attn.k_norm.weight"],
    "post_ln"   -> f32[p <> "post_attention_layernorm.weight"],
    "gate_proj" -> f32[p <> "mlp.gate_proj.weight"],
    "up_proj"   -> f32[p <> "mlp.up_proj.weight"],
    "down_proj" -> f32[p <> "mlp.down_proj.weight"]|>]

(* --- one HF Qwen3 decoder layer.  x {S, dim}; cos/sin {S, 1, head_dim};
       addMask {S, S}; W the layer weights; cfg has heads/kv_heads/head_dim/
       eps.  qk-norm BEFORE rope; GQA-expand k/v; masked attention; SwiGLU
       MLP.  Returns the layer output {S, dim}. --- *)
qwLayer[x_, cos_, sin_, addMask_, W_, cfg_] := Module[
    {h, hkv, dh, eps, scale, rep, s, dim, xn, q, k, v, ke, ve, ctx, attnOut,
     hh, h2},
    h = cfg["heads"];  hkv = cfg["kv_heads"];  dh = cfg["head_dim"];
    eps = cfg["eps"];  scale = 1/Sqrt[N[dh]];  rep = h/hkv;
    s = fxShape[x][[1]];  dim = h*dh;
    (* attention block *)
    xn = fxRMSNorm[x, W["input_ln"], eps];
    q  = fxRMSNorm[TUOpReshape[fxLinear[xn, W["q_proj"]], {s, h,   dh}], W["q_norm"], eps];
    k  = fxRMSNorm[TUOpReshape[fxLinear[xn, W["k_proj"]], {s, hkv, dh}], W["k_norm"], eps];
    v  =          TUOpReshape[fxLinear[xn, W["v_proj"]], {s, hkv, dh}];
    q  = qwRoPE[q, cos, sin];  k = qwRoPE[k, cos, sin];
    ke = qwExpandKV[k, rep];  ve = qwExpandKV[v, rep];
    ctx     = qwAttention[q, ke, ve, scale, addMask];       (* {S, dim} *)
    attnOut = fxLinear[ctx, W["o_proj"]];                   (* {S, dim} *)
    hh = TUOpAdd[x, attnOut];
    (* SwiGLU MLP block *)
    h2 = fxRMSNorm[hh, W["post_ln"], eps];
    h2 = fxLinear[
        TUOpMul[fxSiLU[fxLinear[h2, W["gate_proj"]]], fxLinear[h2, W["up_proj"]]],
        W["down_proj"]];
    TUOpAdd[hh, h2]]

(* --- host-side rope cos/sin for Qwen3 half-split: inv_freq[i] = theta^(-2i/D),
       i=0..D/2-1; freqs = outer(pos, inv_freq); emb = concat[freqs, freqs];
       cos = cos(emb), sin = sin(emb).  Returns {cos, sin} as {S, 1, D} TTerms
       (the unit head axis so they broadcast over the H axis in qwRoPE). --- *)
qwRopeCosSin[seq_Integer, headDim_Integer, theta_] := Module[
    {half, invFreq, freqs, emb, cos, sin},
    half    = headDim/2;
    invFreq = Table[N[theta]^(-2 i/headDim), {i, 0, half - 1}];
    freqs   = Table[p invFreq, {p, 0, seq - 1}];            (* {seq, half} *)
    emb     = Join[#, #] & /@ freqs;                        (* {seq, headDim} *)
    cos     = TTensorCreate[N[Cos[emb]]];
    sin     = TTensorCreate[N[Sin[emb]]];
    {TUOpReshape[cos, {seq, 1, headDim}], TUOpReshape[sin, {seq, 1, headDim}]}]

(* --- host-side additive attention mask {S, S}: mask[i,j] = 0 if (j<=i AND
       attMask[j]==1) else big-negative.  Causal + padding.  attMask is the
       {S} host list (1 real / 0 pad). --- *)
qwAddMask[attMask_List, neg_:-1.*^9] := With[{s = Length[attMask]},
    TTensorCreate[N @ Table[
        If[j <= i && attMask[[j]] == 1, 0., neg], {i, s}, {j, s}]]]

(* --- full encoder.  inputIds {S} host int list (0-indexed token ids);
       attMask {S} host list (1/0); wf a name->TTerm loader (HF names, both
       shards merged); cfg has hidden/heads/kv_heads/head_dim/eps/theta/
       layers/captureLayers.  Returns {S, 3*hidden} = the [hs@cap1 | hs@cap2 |
       hs@cap3] per-token concat.

       Each layer realizes, then re-enters as a FRESH host leaf
       (TTensorCreate[Normal[...]]): detaching the running state drops every
       reference to the prior layer's sub-graph, so its realized f32 weight
       casts (q/k/v/o + the three 9728-wide MLP buffers, ~400 MB/layer) free and
       the next layer's same-shape casts recycle them off the free-list.
       Without the detach those buffers stay reachable through the hidden-state
       chain and pile up across 27 layers (~12 GB of dirty allocations) until a
       memory-pressured host swap-thrashes and the OS kills the kernel.  The
       captures are likewise detached so only the three {S, dim} arrays persist
       to the final concat. --- *)
qwenEncode[inputIds_List, attMask_List, wf_, cfg_] := Module[
    {h, hkv, dh, eps, theta, nL, caps, s, cos, sin, addMask, x, captured, lcfg},
    h = cfg["heads"];  hkv = cfg["kv_heads"];  dh = cfg["head_dim"];
    eps = cfg["eps"];  theta = cfg["theta"];  nL = cfg["layers"];
    caps = cfg["captureLayers"];  s = Length[inputIds];
    lcfg = <|"heads" -> h, "kv_heads" -> hkv, "head_dim" -> dh, "eps" -> eps|>;
    {cos, sin} = qwRopeCosSin[s, dh, theta];
    addMask = qwAddMask[attMask];
    x = qwEmbed[wf["model.embed_tokens.weight"], inputIds];   (* hs_0 {S, dim} host leaf *)
    captured = <||>;
    Do[ x = TTensorCreate[Normal @ TRealize @ qwLayer[x, cos, sin, addMask, qwLayerW[wf, i], lcfg]];
        If[MemberQ[caps, i], captured[i] = x],
        {i, 0, nL - 1}];
    TTensorCreate[Normal @ TRealize @ fxConcat[captured[#] & /@ caps, 2]]]
