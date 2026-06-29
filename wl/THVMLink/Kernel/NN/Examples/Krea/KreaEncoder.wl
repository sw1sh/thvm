(* KreaEncoder.wl - the Krea 2 text encoder (Qwen3-VL-4B language tower) for thvm.

   Krea 2 conditions its DiT on a STACK of tapped hidden states from the
   Qwen3-VL-4B text encoder.  For text-to-image only the LANGUAGE tower runs (no
   vision tower, no image tokens), and the multimodal rope (mrope) collapses to
   plain 1-D rope: with no image tokens the 3 grid position-id sections
   (temporal / height / width) are all the text arange(S), so
   apply_interleaved_mrope is a no-op and the rotary is byte-identical to the
   Qwen3 half-split rope (verified to 0.0 against transformers).  The decoder
   layer is therefore IDENTICAL to the thvm Qwen3-4B encoder (FLUX/QwenEncoder.wl
   qwLayer): input RMSNorm -> GQA attention with per-head q/k RMSNorm and
   half-split rope -> SwiGLU MLP, two residuals.

   Differences from the FLUX Qwen3-4B encoder (which this reuses where identical):
     - 36 layers (vs 27); rope_theta 5e6 (vs 1e6).
     - 12-layer TAP + STACK into {S, 12, 2560} (the DiT txtfusion's layer axis),
       NOT the FLUX 3-layer feature CONCAT into {S, 7680}.  The diffusers taps are
       hidden_states[2,5,8,...,35] (after layers 1,4,7,...,34, 0-indexed) stacked
       along a new layer axis.
     - configurable weight-name prefix (the standalone text model stores
       layers.* / embed_tokens.weight / norm.weight; inside the full Qwen3VLModel
       the language tower is under language_model.* -- the loader sets it).

   Reuses qwLayer / qwEmbed / qwenInputs from FLUX/QwenEncoder.wl (same
   Examples`Private` context) for the per-layer forward and host input prep, and
   the NN library (TRoPEHalfSplitTable, THeadAttention, etc.).  Loaded into the
   shared WolframInstitute`THVMLink`Examples` package.  Follows wl/GUIDE.md style. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

(* Public so the validation test can drive the encoder directly on a tiny CPU
   config (Get the Examples context, then call krQwenEncodeStackedForward). *)
krQwenEncodeStackedForward::usage = "krQwenEncodeStackedForward[x0, addMask, cos, sin, wf, cfg] runs the Qwen3-VL language-tower layers and returns the per-token STACK of the cfg[\"captureLayers\"] tapped hidden states as {S, L, dim} (the Krea DiT txtfusion input).";
krQwenTextConfig::usage = "krQwenTextConfig[] returns the Krea 2 text encoder (Qwen3-VL-4B language tower) config; krQwenTextConfig[overrides] merges overrides for the tiny CPU validation.";
krQwenDefaultTaps::usage = "krQwenDefaultTaps[nLayers, nTaps] returns the thvm capture-layer indices for the diffusers hidden_states taps (after-layer indices, 0-based).";

Begin["`Private`"];

(* Krea 2 text encoder config (Qwen3-VL-4B text_config).  hidden 2560, 32 q / 8
   kv heads (GQA 4:1), head_dim 128, intermediate 9728, 36 layers, rope_theta
   5e6, rms_norm_eps 1e-6, silu, no attention bias.  weightPrefix locates the
   language-tower weights; captureLayers are the thvm after-layer tap indices.
   actDType is the layer activation cast ("bf16" on a GPU for the tensor-core
   matmul path, "f32" on CPU for a clean forward). *)

$krQwenConfig = <|
    "layers" -> 36,
    "heads" -> 32,
    "kv_heads" -> 8,
    "head_dim" -> 128,
    "hidden" -> 2560,
    "intermediate" -> 9728,
    "eps" -> 1.*^-6,
    "theta" -> 5000000.,
    "txtLayers" -> 12,
    "weightPrefix" -> "",
    "actDType" -> "bf16"
|>;

krQwenTextConfig[] := Append[$krQwenConfig, "captureLayers" -> krQwenDefaultTaps[$krQwenConfig["layers"], $krQwenConfig["txtLayers"]]]
krQwenTextConfig[overrides_Association] := With[{merged = Join[$krQwenConfig, overrides]},
    Append[merged, "captureLayers" -> Lookup[merged, "captureLayers", krQwenDefaultTaps[merged["layers"], merged["txtLayers"]]]]]

(* diffusers text_encoder_select_layers taps hidden_states[k] for k in
   (2,5,8,...,35) -- evenly spaced by nLayers/nTaps, the k-th being step*(i+1)-1.
   hidden_states[k] is the state AFTER layer k-1 (0-based), which thvm captures as
   captureLayers index (k-1).  For Qwen3-VL-4B (36 layers, 12 taps) the diffusers
   ks are (2,5,8,...,35), so the thvm capture indices are (1,4,7,...,34). *)
krQwenDefaultTaps[nLayers_Integer, nTaps_Integer] := Module[{step},
    step = nLayers/nTaps;                                 (* 36/12 = 3 *)
    Table[Round[step (i + 1)] - 2, {i, 0, nTaps - 1}]     (* hidden_states[step*(i+1)-1] -> after-layer (that - 1) *)
]

(* --- a Krea Qwen3-VL decoder layer.  Identical to FLUX/QwenEncoder.wl qwLayer
       EXCEPT the activation realize is cfg-driven (qwLayer hard-casts bf16 for
       the GPU tensor-core path; CPU validation wants a clean f32 forward).  Op
       order is byte-identical: input RMSNorm -> GQA attention (per-head q/k
       RMSNorm, half-split rope, masked) -> SwiGLU MLP, two residuals. --- *)
krActRealize[t_, "f32"] := TRealize[t]
krActRealize[t_, dt_] := TRealize @ TUOpCast[t, dt]

krQwLayer[x_, cos_, sin_, addMask_, W_, cfg_] := Block[{h, hkv, dh, eps, scale, rep, s, act, xn, q, k, v, attnOut, hh, hn},
    h = cfg["heads"];  hkv = cfg["kv_heads"];  dh = cfg["head_dim"];
    eps = cfg["eps"];  scale = 1/Sqrt[N[dh]];  rep = h/hkv;  s = Dimensions[x][[1]];
    act = (krActRealize[#, cfg["actDType"]] &);
    xn = act @ TRMSNorm[x, W["input_ln"], eps];
    q = TRMSNorm[ArrayReshape[fxLinear[xn, W["q_proj"]], {s, h, dh}],   W["q_norm"], eps];
    k = TRMSNorm[ArrayReshape[fxLinear[xn, W["k_proj"]], {s, hkv, dh}], W["k_norm"], eps];
    v =          ArrayReshape[fxLinear[xn, W["v_proj"]], {s, hkv, dh}];
    q = TRoPEHalfSplit[q, cos, sin];  k = TRoPEHalfSplit[k, cos, sin];
    attnOut = fxLinear[act @ THeadAttention[q, TRepeatKV[k, rep], TRepeatKV[v, rep], scale, addMask], W["o_proj"]];
    hh = x + attnOut;
    hn = act @ TRMSNorm[hh, W["post_ln"], eps];
    hh + fxLinear[act[TSiLU[fxLinear[hn, W["gate_proj"]]] * fxLinear[hn, W["up_proj"]]], W["down_proj"]]
]

(* per-layer weight Association with the cfg's weightPrefix (HF Qwen3 names). *)
krQwLayerW[wf_, i_, prefix_] := With[{p = prefix <> "layers." <> ToString[i] <> "."}, <|
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

(* --- device forward: run the language-tower layers, capture the tapped states,
       and STACK them per token into {S, L, dim} (a new layer axis), NOT the FLUX
       feature concat.  captured[i] is the state AFTER layer i (0-based) =
       transformers hidden_states[i+1]; the diffusers taps select these.  cfg has
       layers / heads / kv_heads / head_dim / eps / weightPrefix / captureLayers /
       actDType.  Returns {S, L, dim}. --- *)
krQwenEncodeStackedForward[x0_, addMask_, cos_, sin_, wf_, cfg_] := Block[{nL, caps, prefix, lcfg, x, captured, stacked},
    nL = cfg["layers"];  caps = cfg["captureLayers"];  prefix = cfg["weightPrefix"];
    lcfg = <|"heads" -> cfg["heads"], "kv_heads" -> cfg["kv_heads"], "head_dim" -> cfg["head_dim"], "eps" -> cfg["eps"], "actDType" -> cfg["actDType"]|>;
    x = x0;  captured = <||>;
    Do[ x = TRealize @ krQwLayer[x, cos, sin, addMask, krQwLayerW[wf, i, prefix], lcfg];
        If[ MemberQ[caps, i], captured[i] = x],
        {i, 0, nL - 1}
    ];
    (* stack the tapped {S, dim} states along a NEW layer axis -> {S, L, dim}:
       each capture reshaped to {S, 1, dim}, joined on the middle axis (WL Join
       level 2 = the unit layer axis). *)
    stacked = Join[Sequence @@ (
        With[{cap = captured[#], d = Dimensions[captured[#]]},
            TUOpReshape[cap, {First[d], 1, Last[d]}]] & /@ caps), 2];
    TRealize[stacked]
]

(* host-prep of the device inputs (cos/sin/mask/x), reusing the FLUX qwenInputs
   path but with the Qwen3-VL theta + the cfg's embed-weight prefix.  Returns
   <|"cos","sin","addMask","x"|> device TTerms. *)
krQwenInputs[inputIds_List, attMask_List, wf_, cfg_] := Block[{dh, theta, s, prefix, dev, toDev, cos, sin},
    dh = cfg["head_dim"];  theta = cfg["theta"];  s = Length[inputIds];  prefix = cfg["weightPrefix"];
    dev = TDevice[wf[prefix <> "layers.0.self_attn.q_proj.weight"]];
    toDev = If[ dev === None || dev === "cpu", # &, TToDevice[#, dev] &];
    {cos, sin} = TRoPEHalfSplitTable[s, dh, theta];
    <|"cos" -> toDev[cos], "sin" -> toDev[sin], "addMask" -> toDev @ TPaddingCausalMask[attMask], "x" -> toDev @ qwEmbed[wf[prefix <> "embed_tokens.weight"], inputIds]|>
]

(* full encoder: inputIds {S} host int list (0-indexed); attMask {S} host list
   (1 real / 0 pad); wf a name->TTerm loader; cfg as above.  Returns the
   {S, L, dim} stacked tapped states the Krea DiT txtfusion consumes. *)
krQwenEncodeStack[inputIds_List, attMask_List, wf_, cfg_] := With[{in = krQwenInputs[inputIds, attMask, wf, cfg]},
    krQwenEncodeStackedForward[in["x"], in["addMask"], in["cos"], in["sin"], wf, cfg]
]

End[];

EndPackage[];
