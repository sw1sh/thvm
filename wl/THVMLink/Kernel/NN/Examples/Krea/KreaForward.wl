(* KreaForward.wl - the Krea 2 single-stream MMDiT velocity forward for thvm.

   Faithful port of the krea-ai/krea-2 mmdit.py SingleStreamDiT (the
   diffusers Krea2Transformer2DModel; the released FP8 weights use the mmdit.py
   parameter names, so this file ports from there).  Predicts the flow-match
   velocity for the packed image-latent tokens, conditioned on a stacked,
   tapped text-encoder hidden state and the flow time.

   Pipeline (mmdit.py SingleStreamDiT.forward):
     img {Simg, ch*patch^2}  -> first (Linear+bias) -> {Simg, F}
     t (sigma)               -> sinusoid(tdim) -> tmlp (Lin GELU Lin) -> t {1,F}
                             -> tproj (GELU Lin) -> tvec {1, 6F}  (shared modulation)
     context {Stxt, L, txtD}  -> txtfusion (layerwise / projector / refiner)
                             -> txtmlp (RMSNorm Lin GELU Lin) -> {Stxt, F}
     combined = [context ; img]  (text-first)
     pos {Stxt+Simg, 3}      -> per-axis interleaved RoPE (axes [t,h,w])
     for block in 28 blocks: single-stream (modulate -> GQA gated attn ->
                             SwiGLU MLP, two gated residuals)
     last (modulation -> RMSNorm -> Linear+bias) over the IMAGE tokens
       -> velocity {Simg, ch*patch^2}

   NEW vs the FLUX DiT: zero-centered RMSNorm (weight is scale+1), a sigmoid
   output GATE on attention, grouped-query attention (48 q / 12 kv heads), the
   text-fusion sub-transformer that collapses the tapped-layer axis, a single
   [text; image] stream (no double-stream), GELU-tanh MLPs in the timestep /
   text projections, RoPE over 3 axes (t,h,w) with per-axis dims, patch 2, 16
   latent channels.

   Reuses fxLinear (the diffusers x.W^T matmul) from FluxForward.wl and the NN
   library (TRoPEInterleaved, THeadAttention, TRepeatKV, TRMSNorm, TGELU,
   TSiLU, TSigmoid, TSoftmax).  Loaded into the shared
   WolframInstitute`THVMLink`Examples` package.  Follows wl/GUIDE.md style. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

(* Public so the validation test can drive the transformer + sub-pieces directly
   on a tiny CPU config (Get the Examples context, then call krTransformer). *)
krTransformer::usage = "krTransformer[img, context, sigma, pos, mask, wf, cfg] runs the Krea 2 single-stream MMDiT velocity forward and returns the {Simg, ch*patch^2} velocity; wf is a name->TTerm weight loader (mmdit.py names) and cfg the architecture config.";
krRopeTable::usage = "krRopeTable[pos, axesDims, theta] builds the interleaved RoPE cos/sin {S,1,headDim} tables for the (t,h,w) position ids pos and per-axis dims axesDims.";
krRMSNorm::usage = "krRMSNorm[x, scale, eps] is the Krea zero-centered RMSNorm: RMSNorm with effective weight (scale + 1).";

Begin["`Private`"];

(* --- zero-centered RMSNorm: the Krea RMSNorm stores the scale centered at 0,
       so the effective gain is (scale + 1).  The NN-library TRMSNorm multiplies
       by the raw weight, so add 1 first.  Activations upcast to f32 inside
       TRMSNorm's reduce. --- *)
krRMSNorm[x_, scale_, eps_] := TRMSNorm[x, scale + 1, eps]

(* --- linear with bias: a diffusers Linear {out,in} with a {out} bias.  fxLinear
       is the x.W^T matmul (no resident transpose); the bias broadcasts over the
       sequence axis (EXPAND'd so a {S,out}*{out} numel-cycle does not misalign,
       per the fxModulate caveat). --- *)
krLinearBias[x_, w_, b_] := Module[{y, s},
    y = fxLinear[x, w];                          (* fxLinear realizes -> concrete shape *)
    s = Dimensions[y];
    TUOpAdd[y, TUOpExpand[TUOpReshape[b, {1, Last[s]}], s]]
]

(* --- GELU-tanh: the mmdit.py timestep / text projections use
       GELU(approximate="tanh"); the NN-library TGELU is exactly that tanh
       approximation. --- *)

(* --- SwiGLU MLP: down(silu(gate(x)) * up(x)); all three Linears no-bias.  The
       mlp hidden dim is a config detail folded into the weights. --- *)
krMLP[x_, W_] := fxLinear[TUOpMul[TSiLU @ fxLinear[x, W["gate"]], fxLinear[x, W["up"]]], W["down"]]

(* --- gated grouped-query attention with q/k RMSNorm and a sigmoid output gate.
       x {S,F}; W has wq/wk/wv/wo/gate (no-bias Linears) + qknorm.qnorm/.knorm
       (zero-centered head-dim RMSNorm scales); rope is <|cos,sin|> or None
       (the text-fusion layerwise/refiner blocks pass None).  Op-order per
       mmdit.py Attention.forward:
         q,k,v,gate = wq(x),wk(x),wv(x),gate(x)
         split heads (q: heads, k/v: kvheads); per-head RMSNorm(q), RMSNorm(k)
         rope q,k (interleaved) if rope
         attn = sdpa(q, repeat_kv(k), repeat_kv(v))            (GQA)
         out  = wo(attn * sigmoid(gate)) --- *)
krAttention[x_, s_Integer, rope_, mask_, W_, cfg_] := Module[{h, hkv, dh, eps, scale, rep, q, k, v, gate, attn},
    h = cfg["heads"];  hkv = cfg["kvHeads"];  dh = cfg["headDim"];
    eps = cfg["eps"];  scale = 1/Sqrt[N[dh]];  rep = h/hkv;
    q = krRMSNorm[ArrayReshape[fxLinear[x, W["wq"]], {s, h, dh}],   W["qnorm"], eps];
    k = krRMSNorm[ArrayReshape[fxLinear[x, W["wk"]], {s, hkv, dh}], W["knorm"], eps];
    v =          ArrayReshape[fxLinear[x, W["wv"]], {s, hkv, dh}];
    gate = fxLinear[x, W["gate"]];                                  (* {s, F} *)
    If[ rope =!= None,
        q = TRoPEInterleaved[q, rope["cos"], rope["sin"]];
        k = TRoPEInterleaved[k, rope["cos"], rope["sin"]]
    ];
    attn = THeadAttention[q, TRepeatKV[k, rep], TRepeatKV[v, rep], scale, mask];  (* {s,F} *)
    fxLinear[TUOpMul[attn, TSigmoid[gate]], W["wo"]]
]

(* --- per-block weight Associations from a name->TTerm loader wf (mmdit.py
       names).  prefix is the block path (blocks.i. / txtfusion.*_blocks.j.). --- *)
krAttnW[wf_, p_] := <|
    "wq" -> wf[p <> "attn.wq.weight"],
    "wk" -> wf[p <> "attn.wk.weight"],
    "wv" -> wf[p <> "attn.wv.weight"],
    "wo" -> wf[p <> "attn.wo.weight"],
    "gate" -> wf[p <> "attn.gate.weight"],
    "qnorm" -> wf[p <> "attn.qknorm.qnorm.scale"],
    "knorm" -> wf[p <> "attn.qknorm.knorm.scale"]
|>
krMlpW[wf_, p_] := <|
    "gate" -> wf[p <> "mlp.gate.weight"],
    "up" -> wf[p <> "mlp.up.weight"],
    "down" -> wf[p <> "mlp.down.weight"]
|>

(* --- text-fusion block (TextFusionBlock): pre-norm transformer block, NO rope,
       NO time modulation, gated GQA attention + SwiGLU MLP, two residuals.  s is
       the (host-concrete) sequence length of x.
       mmdit.py: x = x + attn(prenorm(x)); x = x + mlp(postnorm(x)). --- *)
krFusionBlock[x_, s_Integer, mask_, wf_, p_, cfg_] := Module[{a, m},
    a = krAttention[krRMSNorm[x, wf[p <> "prenorm.scale"], cfg["eps"]], s, None, mask, krAttnW[wf, p], cfg];
    m = TUOpAdd[x, a];
    TUOpAdd[m, krMLP[krRMSNorm[m, wf[p <> "postnorm.scale"], cfg["eps"]], krMlpW[wf, p]]]
]

(* --- text fusion (TextFusionTransformer): collapse the tapped-layer axis of the
       stacked text states.  context {Stxt, L, txtD} (the concrete dims stxt/nl/td
       are taken from the leaf context ONCE and threaded as host integers; never
       re-queried from a derived TTerm, whose shape may be symbolic):
         2 layerwise_blocks attend across the L axis per token (mask None)
         permute to {Stxt, txtD, L}; projector Linear(L->1); squeeze -> {Stxt, txtD}
         2 refiner_blocks attend across the Stxt token axis (text mask)
       The batch dim is 1 here (one prompt), so b*l = Stxt and the layerwise
       attention runs over the L (tapped-layer) axis per token. --- *)
krTextFusion[context_, txtMask_, wf_, cfg_] := Module[{stxt, nl, td, fcfg, h, proj, refined, i},
    {stxt, nl, td} = Dimensions[context];                          (* leaf -> concrete *)
    fcfg = <|"heads" -> cfg["txtHeads"], "kvHeads" -> cfg["txtKvHeads"], "headDim" -> td/cfg["txtHeads"], "eps" -> cfg["eps"]|>;
    h = context;                                                   (* {stxt, L, td} *)
    Do[ h = krFusionBlockBatched[h, stxt, nl, td, wf, "txtfusion.layerwise_blocks." <> ToString[i] <> ".", fcfg], {i, 0, 1}];
    (* projector: a Linear(L -> 1) over the tapped-layer axis -> proj[t,d] =
       sum_l h[t,l,d] * W[0,l].  projector.weight is {1, L}; a 3-D fxLinear matmul
       mis-batches here, so do the weighted sum explicitly: broadcast W {L} over
       the L axis (axis 1) of h {stxt,L,td}, multiply, reduce axis 1. *)
    proj = TUOpReshape[
        TUOpReduce[
            TUOpMul[h, TUOpExpand[TUOpReshape[wf["txtfusion.projector.weight"], {1, nl, 1}], {stxt, nl, td}]],
            1, "SUM"],
        {stxt, td}];
    refined = proj;
    Do[ refined = krFusionBlock[refined, stxt, txtMask, wf, "txtfusion.refiner_blocks." <> ToString[i] <> ".", fcfg], {i, 0, 1}];
    refined
]

(* layerwise fusion block over a {stxt, L, td} tensor: run the fusion block per
   token row (seq = L).  thvm's attention is 2-D {S,F}; fold the stxt rows into
   the batch by looping (stxt small) -- a faithful per-token attention over the L
   tapped-layer axis.  All dims (stxt/nl/td) are host integers (threaded in), so
   the per-row slice + reshape never queries a derived TTerm's shape. *)
krFusionBlockBatched[h_, stxt_Integer, nl_Integer, td_Integer, wf_, p_, cfg_] := Module[{x, W, mlpW, heads, dh, eps, scale, xf, xn, q4, k4, v4, gate, fold, attn, m},
    (* TextFusionBlock applied to ALL stxt tokens at once: pre-norm gated attention
       over the nl (tapped-layer) axis + SwiGLU MLP, two residuals.  The attention is
       batched over the stxt tokens by FOLDING stxt into THeadAttention's head batch
       axis (S=nl, H=stxt*heads) -- ONE batched attention, vs the old per-token
       loop+Join that lowered to ~512 pad-adds ({stxt,nl,td} each ~= 27 GB) AND hit a
       3-D-slice+matmul correctness bug (x[[t]] . w returns garbage in thvm).  No GQA
       here (txtKvHeads==txtHeads), no rope.  Fold validated cosine 1.0 vs a per-token
       reference; peak ~286 MB at full Krea dims. *)
    x = TRealize[h];                                                  (* {stxt, nl, td} *)
    W = krAttnW[wf, p];  mlpW = krMlpW[wf, p];
    heads = cfg["heads"];  dh = td/heads;  eps = cfg["eps"];  scale = 1/Sqrt[N[dh]];
    xf = TUOpReshape[x, {stxt*nl, td}];                               (* flatten for rank-2 fxLinear *)
    xn = krRMSNorm[xf, wf[p <> "prenorm.scale"], eps];
    q4 = krRMSNorm[TUOpReshape[fxLinear[xn, W["wq"]], {stxt, nl, heads, dh}], W["qnorm"], eps];
    k4 = krRMSNorm[TUOpReshape[fxLinear[xn, W["wk"]], {stxt, nl, heads, dh}], W["knorm"], eps];
    v4 =          TUOpReshape[fxLinear[xn, W["wv"]], {stxt, nl, heads, dh}];
    gate = fxLinear[xn, W["gate"]];                                   (* {stxt*nl, td} *)
    fold = t4 |-> TUOpReshape[Transpose[t4, {2, 1, 3, 4}], {nl, stxt*heads, dh}];
    attn = THeadAttention[fold[q4], fold[k4], fold[v4], scale, None]; (* {nl, stxt*heads*dh} *)
    attn = TUOpReshape[Transpose[TUOpReshape[attn, {nl, stxt, heads, dh}], {2, 1, 3, 4}], {stxt*nl, td}];
    attn = fxLinear[TUOpMul[attn, TSigmoid[gate]], W["wo"]];          (* {stxt*nl, td} *)
    m = TUOpAdd[xf, attn];                                            (* residual 1 *)
    m = TUOpAdd[m, krMLP[krRMSNorm[m, wf[p <> "postnorm.scale"], eps], mlpW]];   (* residual 2 *)
    TUOpReshape[m, {stxt, nl, td}]
]

(* --- single-stream DiT block (SingleStreamBlock): shared time modulation
       chunked into 6, zero-centered prenorm/postnorm, gated GQA attention (with
       rope), SwiGLU MLP, two gated residuals.  mmdit.py:
         prescale,preshift,pregate,postscale,postshift,postgate = chunk6(tvec + mod.lin)
         x = x + pregate  * attn((1+prescale)*prenorm(x) + preshift, rope, mask)
         x = x + postgate * mlp ((1+postscale)*postnorm(x) + postshift)
       tvec {1,6F} broadcasts over the sequence; mod.lin {6F} is the per-block
       learned modulation offset. --- *)
krBlock[x_, s_Integer, tvec_, rope_, mask_, wf_, i_, cfg_] := Module[{p, f, modLin, mod, pre, post, prescale, preshift, pregate, postscale, postshift, postgate, a, m},
    p = "blocks." <> ToString[i] <> ".";  f = cfg["features"];
    modLin = wf[p <> "mod.lin"];                                   (* {6F} *)
    (* tvec {1,6F} + mod.lin {6F} -> chunk 6 into {1,F} modulation vectors. *)
    mod = TUOpAdd[tvec, TUOpReshape[modLin, {1, 6 f}]];
    prescale  = mod[[All, 1 ;; f]];        preshift  = mod[[All, f + 1 ;; 2 f]];
    pregate   = mod[[All, 2 f + 1 ;; 3 f]]; postscale = mod[[All, 3 f + 1 ;; 4 f]];
    postshift = mod[[All, 4 f + 1 ;; 5 f]]; postgate  = mod[[All, 5 f + 1 ;; 6 f]];
    pre  = krModulate[krRMSNorm[x, wf[p <> "prenorm.scale"], cfg["eps"]], prescale, preshift, s, f];
    a    = krAttention[pre, s, rope, mask, krAttnW[wf, p], cfg];
    m    = krGateAdd[x, pregate, a, s, f];
    post = krModulate[krRMSNorm[m, wf[p <> "postnorm.scale"], cfg["eps"]], postscale, postshift, s, f];
    krGateAdd[m, postgate, krMLP[post, krMlpW[wf, p]], s, f]
]

(* (1+scale)*x + shift; scale/shift {1,F} EXPAND'd over the seq axis to {s,F}.
   The shape {s,f} is threaded as host integers (never queried from the derived
   x, whose tracked shape may be symbolic). *)
krModulate[x_, scale_, shift_, s_Integer, f_Integer] := With[{sh = {s, f}},
    TUOpAdd[TUOpMul[x, TUOpAdd[TUOpExpand[scale, sh], TUOpConst[1.]]], TUOpExpand[shift, sh]]
]

(* x + gate * y; gate {1,F} EXPAND'd over the seq axis to {s,F}. *)
krGateAdd[x_, gate_, y_, s_Integer, f_Integer] := With[{sh = {s, f}},
    TUOpAdd[x, TUOpMul[TUOpExpand[gate, sh], y]]
]

(* === RoPE table ====================================================
   Per-axis interleaved RoPE for the (t,h,w) position ids.  For axis a with dim
   d_a, omega_j = 1/theta^(2j/d_a) for j in 0..d_a/2-1; angle = pos[a]*omega_j;
   cos/sin are repeat-interleaved per pair (repeat_interleave_real=True) so the
   TRoPEInterleaved pair (2j,2j+1) shares angle j.  Concatenate over the axes to
   head_dim = sum(axesDims).  Returns <|cos,sin|> each {S,1,head_dim}. *)

krRopeTable[pos_List, axesDims_List, theta_, dev_] := Module[{angRows, cosT, sinT, hd},
    hd = Total[axesDims];
    angRows = Map[
        Function[p,
            Flatten @ MapThread[
                Function[{posA, dA},
                    Module[{omega = Table[1./N[theta]^(2 j/dA), {j, 0, dA/2 - 1}]},
                        Flatten[{#, #} & /@ (posA omega)]]   (* repeat-interleave per pair *)
                ],
                {p, axesDims}
            ]
        ],
        pos
    ];                                                       (* {S, head_dim} *)
    (* cos/sin ride the activation dtype (bf16 on GPU): a f32 rope table would
       upcast q/k at TRoPEInterleaved and cascade f32 through the whole DiT stream
       -- doubling both the captured-intermediate footprint and matmul bandwidth. *)
    With[{wdt = If[dev === "cpu", "f32", "bf16"]},
        cosT = TToDevice[TUOpCast[ArrayReshape[TTensorCreate[N[Cos[angRows]]], {Length[pos], 1, hd}], wdt], dev];
        sinT = TToDevice[TUOpCast[ArrayReshape[TTensorCreate[N[Sin[angRows]]], {Length[pos], 1, hd}], wdt], dev]
    ];
    <|"cos" -> cosT, "sin" -> sinT|>
]

(* === timestep embedding ============================================
   sigma -> sinusoid(tdim, cos-first, scaled 1e3) -> tmlp (Lin GELU Lin) -> t {1,F}.
   tvec = tproj (GELU Lin) over t -> {1, 6F}.  The sinusoid is built host-side
   (mmdit.py temb: half = tdim/2; freqs = exp(-log(1e4)*arange(half)/half);
   args = (sigma*1e3)*freqs; concat[cos, sin]) -- the same cos-first convention
   as the NN-library TSinusoidalEmbedding (which scales the input by 1e3 too). *)

krTimeEmbed[sigma_, wf_, cfg_, dev_] := Module[{tdim, sinus, t, tvec},
    tdim = cfg["tDim"];
    sinus = TToDevice[TUOpCast[TTensorCreate[{Normal @ TSinusoidalEmbedding[N[sigma*1000.], tdim]}], If[dev === "cpu", "f32", "bf16"]], dev];
    t = krLinearBias[TGELU @ krLinearBias[sinus, wf["tmlp.0.weight"], wf["tmlp.0.bias"]], wf["tmlp.2.weight"], wf["tmlp.2.bias"]];          (* {1, F} *)
    tvec = krLinearBias[TGELU[t], wf["tproj.1.weight"], wf["tproj.1.bias"]];  (* {1, 6F} *)
    <|"t" -> t, "tvec" -> tvec|>
]

(* === full transformer forward ======================================
   img {Simg, ch*patch^2}; context {Stxt, L, txtD}; sigma the flow time; pos
   {Stxt+Simg, 3} the (t,h,w) ids; mask {Stxt+Simg} the key-padding mask (1 real,
   0 pad), or None; wf the name->TTerm loader; cfg the architecture config.
   Returns the velocity {Simg, ch*patch^2} over the image tokens. *)

(* the DiT CORE: takes a PRECOMPUTED timestep embedding (t, tvec) so it is a
   pure velocity net over (img, t, tvec), the JIT-capturable unit.  No per-block
   TRealize -- the whole 28-block graph stays lazy so the sampler can TJit-capture
   it ONCE and replay it (rebinding img/t/tvec) each step; that bounds the
   tensor-descriptor count to a single forward (the non-JIT loop re-dispatches the
   entire net every step, which both blows the descriptor pool and is slow). *)
(* the FIXED precompute (independent of img/t/tvec): the text-fusion context, the
   per-axis RoPE table, and the additive key-padding mask.  krRopeTable / krAddMask
   do host->device builds (TTensorCreate), which MUST stay OUTSIDE the JIT-captured
   velocity net -- a host op inside the capture makes TJit silently fall back to
   re-running the whole forward every step (the descriptor blowup + the wait).
   Computed ONCE per generation; also avoids recomputing the text-fusion
   sub-transformer 8 times. *)
krTransformerPre[context_, stxt_, simg_, pos_, mask_, wf_, cfg_] := Module[{addMask, txtAddMask, ctx, rope, dev},
    dev = Lookup[cfg, "device", "cpu"];
    addMask = krAddMask[If[ mask === None, None, Join[mask, ConstantArray[1, simg]]], stxt + simg, dev];
    txtAddMask = krAddMask[If[ mask === None, None, Take[mask, stxt]], stxt, dev];
    ctx = TRealize @ krTextFusion[context, txtAddMask, wf, cfg];   (* {Stxt, txtD} *)
    ctx = TRealize @ krLinearBias[TGELU @ krLinearBias[krRMSNorm[ctx, wf["txtmlp.0.scale"], cfg["eps"]], wf["txtmlp.1.weight"], wf["txtmlp.1.bias"]], wf["txtmlp.3.weight"], wf["txtmlp.3.bias"]];   (* {Stxt, F} *)
    rope = krRopeTable[pos, cfg["axesDims"], cfg["theta"], dev];
    <|"ctx" -> ctx, "rope" -> rope, "addMask" -> addMask|>
]

(* the velocity net: PURE device ops over (img, t, tvec) with the precomputed
   ctx/rope/addMask + the fp8-resident weights closed over.  patch-embed img,
   prepend the fixed text context, run the 28 blocks (per-block realized), drop the
   text prefix, final layer.  The sampler calls it directly each step. *)
krTransformerBlocks[img_, ctx_, t_, tvec_, rope_, addMask_, stxt_, simg_, wf_, cfg_] := Module[{hidden, combined, i, out},
    hidden = krLinearBias[img, wf["first.weight"], wf["first.bias"]];   (* {Simg, F} *)
    combined = Join[ctx, hidden, 1];                                   (* {Stxt+Simg, F} *)
    (* Realize each block: the fp8 weights decode to a per-matmul bf16 transient
       (bufferize_fp8_cast_feeds_matmul) freed at end-of-realize, so per-block
       TRealize bounds the DiT working set to ~one block instead of pinning the
       whole 28-block activation set at once.  (A TJit-captured lazy stack is the
       speed follow-up; per-block is the correct-and-simple baseline.) *)
    Do[ combined = TRealize @ krBlock[combined, stxt + simg, tvec, rope, addMask, wf, i, cfg], {i, 0, cfg["layers"] - 1}];
    out = combined[[stxt + 1 ;; stxt + simg]];                         (* {Simg, F} *)
    krLast[out, simg, t, wf, cfg]
]

(* core = precompute + blocks (the non-JIT path, e.g. the tiny CPU validation). *)
krTransformerCore[img_, context_, t_, tvec_, pos_, mask_, wf_, cfg_] := Module[{stxt, simg, pre},
    stxt = Dimensions[context][[1]];  simg = Dimensions[img][[1]];
    pre = krTransformerPre[context, stxt, simg, pos, mask, wf, cfg];
    krTransformerBlocks[img, pre["ctx"], t, tvec, pre["rope"], pre["addMask"], stxt, simg, wf, cfg]
]

(* sigma-driven wrapper (the CPU validation path): build the timestep embedding,
   then run the core. *)
krTransformer[img_, context_, sigma_, pos_, mask_, wf_, cfg_] := Module[{dev, te},
    dev = TDevice[wf["first.weight"]];
    If[dev === None, dev = "cpu"];
    te = krTimeEmbed[sigma, wf, cfg, dev];
    krTransformerCore[img, context, te["t"], te["tvec"], pos, mask, wf, cfg]
]

(* additive key-padding mask {S,S} from a {S} 0/1 mask: 0 on valid keys,
   -inf (a large negative) on padded keys, broadcast over query rows.  None
   gives None (no mask). *)
krAddMask[mask_, s_, dev_] := If[ mask === None,
    None
    ,
    With[{neg = -1.*^9},
        (* TToDevice the host-built mask onto the compute device (else it stays
           CPU-resident and term_device_in routes the whole masked-attention output
           onto the CPU interpreter -- a 122s scalar walk); cast to the activation
           dtype so the additive mask does not upcast the attention scores.  -1e9
           is representable in bf16 (f32-width exponent) and still masks to -inf. *)
        TToDevice[TUOpCast[TTensorCreate[N @ Table[If[mask[[j]] == 1 || mask[[j]] == 1., 0., neg], {i, s}, {j, s}]], If[dev === "cpu", "f32", "bf16"]], dev]
    ]
]

(* last layer: SimpleModulation(t) -> (1+scale)*RMSNorm(x) + shift -> Linear+bias.
   modulation.lin {2,F}: scale,shift = chunk2(t + lin).  norm is zero-centered. *)
krLast[x_, simg_Integer, t_, wf_, cfg_] := Module[{f, modLin, mod, scale, shift, normed},
    f = cfg["features"];
    modLin = wf["last.modulation.lin"];                               (* {2, F} *)
    mod = TUOpAdd[TUOpExpand[TUOpReshape[t, {1, 1, f}], {1, 2, f}], TUOpReshape[modLin, {1, 2, f}]];                     (* {1,2,F} *)
    scale = TUOpReshape[mod[[All, 1]], {1, f}];
    shift = TUOpReshape[mod[[All, 2]], {1, f}];
    normed = krModulate[krRMSNorm[x, wf["last.norm.scale"], cfg["eps"]], scale, shift, simg, f];
    krLinearBias[normed, wf["last.linear.weight"], wf["last.linear.bias"]]
]

End[];

EndPackage[];
