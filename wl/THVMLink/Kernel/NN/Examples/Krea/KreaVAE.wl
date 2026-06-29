(* KreaVAE.wl - the AutoencoderKLQwenImage decoder for thvm, single image.

   Decodes the Krea 2 packed image latent into an RGB image.  The diffusers
   AutoencoderKLQwenImage is a 3-D causal-conv (Wan-style) VIDEO VAE, but for a
   single still image the temporal axis is one frame, and every 3-D op collapses
   to a 2-D op (verified to ~5e-7 against the reference decode):

     - a causal Conv3d on a SINGLE frame (cache fresh) prepends 2 zero time-frames
       before a valid time-3 conv, so only the LAST temporal kernel slice
       w[:,:,kT-1] acts -> a plain Conv2d with that slice.
     - the temporal upsample (upsample3d) on a single frame takes the "Rep"
       cache branch: time_conv never runs, the frame count stays 1, and only the
       spatial path runs -> nearest-exact 2x upsample + a channel-halving Conv2d.
     - QwenImageRMS_norm is an L2-normalize over the CHANNEL axis (per spatial
       location) rescaled by sqrt(C)*gamma -- NOT the transformer RMSNorm.

   Pipeline (AutoencoderKLQwenImage decode, T=1, dim_mult {1,2,4,4}, base_dim 96):
     z {16, gH, gW}
       -> denorm           z = z * latents_std + latents_mean (per channel)
       -> post_quant_conv  (1x1, 16->16)
       -> decoder.conv_in  (3x3 pad1, 16->384)
       -> mid: ResBlock(384) + self-attn + ResBlock(384)
       -> up_blocks[0..3]: 3 ResBlocks each; up_blocks 0,1,2 then a 2x upsample
          (channels halve in the upsample conv), up_block 3 has no upsample
       -> norm_out (L2-channel) -> SiLU -> conv_out (3x3 pad1, ->3)
       -> clip(x, -1, 1) -> image = (x + 1) * 0.5 -> {3, H, W}

   Reuses the NN-library TConv2DIm2ColPool / TSiLU / TUpsample2x / TSoftmax /
   TMatMul and the FLUX VAE conv-pad idiom.  Loaded into the shared
   WolframInstitute`THVMLink`Examples` package.  Follows wl/GUIDE.md style. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

(* Public so the validation test can drive the decoder on a tiny CPU config. *)
krVaeDecode::usage = "krVaeDecode[z, W, wsub, cfg] decodes a packed Krea 2 image latent z {z_dim, gH, gW} into an RGB image {3, H, W} in [0,1]; W is a name->TTerm VAE-decoder loader, wsub[prefix] slices a sub-Association, cfg has eps/latentsMean/latentsStd.";

Begin["`Private`"];

(* === conv: 3-D causal Conv3d on one frame == Conv2d with the LAST temporal
   kernel slice.  w is the diffusers {Co, Ci, kT, kh, kw} weight; for T=1 (cache
   fresh) only slice kT-1 acts.  pad is the spatial H/W zero-pad (1 for 3x3,
   0 for 1x1).  TConv2D is no-padding, so pad H,W first; the bias folds into the
   lowering.  Reuses the FLUX VAE fused strided-view conv (footprint flat in
   resolution). *)

(* last-time-slice extractor: a {Co,Ci,kT,kh,kw} causal conv weight -> the 2-D
   {Co,Ci,kh,kw} slice at time index kT (1-based) -- the only temporal slice that
   acts on a single frame.  A 1x1x1 conv collapses to {Co,Ci,1,1}. *)
krVaeLastSlice[w_] := With[{d = Dimensions[w]},
    TUOpReshape[w[[All, All, d[[3]]]], {d[[1]], d[[2]], d[[4]], d[[5]]}]
]

krVaeConv[x_, w_, b_, pad_] := With[{w2 = krVaeLastSlice[w]},
    With[{xp = If[pad === 0, x, TUOpPad[x, {{0, 0}, {pad, pad}, {pad, pad}}]]},
        TConv2DIm2ColPool[xp, w2, b]
    ]
]

(* a 1x1 conv given an ALREADY-2D {Co,Ci,1,1} weight (the attention to_qkv/proj
   stored {Co,Ci,1,1}; no temporal slice). *)
krVaeConv1x1[x_, w_, b_] := TConv2DIm2ColPool[x, w, b]

(* === QwenImageRMS_norm: L2-normalize over the CHANNEL axis (per spatial
   location), rescaled by sqrt(C)*gamma.  x {C,H,W}; gamma {C} (the stored
   gamma is {C,1,1} or {C,1,1,1}; reshape to {C}).  eps 1e-12 (F.normalize
   default).  y[c,h,w] = x[c,h,w]/sqrt(sum_c x^2 + eps) * sqrt(C) * gamma[c]. *)

krVaeNorm[x_, gamma_, eps_] := Module[{c, hh, ww, sq, denom, g},
    {c, hh, ww} = Dimensions[x];
    sq = TUOpReduce[TUOpMul[x, x], 0, "SUM"];                          (* {1,H,W} reduce over C *)
    sq = TUOpReshape[sq, {1, hh, ww}];
    denom = TUOpSqrt[TUOpAdd[sq, TUOpConst[N[eps]]]];                  (* {1,H,W} *)
    g = TUOpReshape[gamma, {c, 1, 1}];
    TUOpMul[
        TUOpMul[TUOpMul[x, TUOpExpand[denom, {c, hh, ww}] // TUOpRecip], TUOpConst[N[Sqrt[c]]]],
        TUOpExpand[g, {c, hh, ww}]
    ]
]

(* === resnet block: shortcut + (norm1 -> SiLU -> conv3x3 -> norm2 -> SiLU ->
   conv3x3).  W is this block's sub-Association (norm1.gamma / conv1.* /
   norm2.gamma / conv2.* [/ conv_shortcut.* when channels change]). *)

krVaeResBlock[x_, W_, eps_] := Module[{h, sc},
    h = krVaeConv[TSiLU @ krVaeNorm[x, W["norm1.gamma"], eps], W["conv1.weight"], W["conv1.bias"], 1];
    h = krVaeConv[TSiLU @ krVaeNorm[h, W["norm2.gamma"], eps], W["conv2.weight"], W["conv2.bias"], 1];
    sc = If[ KeyExistsQ[W, "conv_shortcut.weight"],
        (* conv_shortcut is a 1x1x1 causal conv -> last-slice {Co,Ci,1,1} 1x1 conv. *)
        krVaeConv[x, W["conv_shortcut.weight"], W["conv_shortcut.bias"], 0]
        ,
        x
    ];
    TUOpAdd[sc, h]
]

(* === mid-block self-attention (QwenImageAttentionBlock), single head, T=1.
   GN(L2-channel) -> to_qkv (1x1) -> flatten {C,N} -> softmax(q^T k / sqrt(C))
   over keys -> v @ attn -> proj (1x1) -> +x.  to_qkv/proj weights are
   {Co,Ci,1,1} (true Conv2d, no temporal axis). *)

krVaeAttn[x_, W_, eps_] := Module[{c, hh, ww, n, hn, qkv, q, k, v, scores, attn, o, proj},
    {c, hh, ww} = Dimensions[x];  n = hh ww;
    hn = krVaeNorm[x, W["norm.gamma"], eps];
    qkv = krVaeConv1x1[hn, W["to_qkv.weight"], W["to_qkv.bias"]];      (* {3C,H,W} *)
    qkv = TUOpReshape[qkv, {3 c, n}];
    q = qkv[[1 ;; c]];  k = qkv[[c + 1 ;; 2 c]];  v = qkv[[2 c + 1 ;; 3 c]];   (* {C,N} *)
    (* scores[i,j] = sum_c q[c,i] k[c,j] / sqrt(C); softmax over keys (j). *)
    scores = TUOpMul[TMatMul[Transpose[q], k], TUOpConst[1./Sqrt[N[c]]]];        (* {N,N} *)
    attn = TSoftmax[scores, 1];
    o = TMatMul[v, Transpose[attn]];                                  (* {C,N} *)
    proj = krVaeConv1x1[TUOpReshape[o, {c, hh, ww}], W["proj.weight"], W["proj.bias"]];
    TUOpAdd[x, proj]
]

(* === up-block: num_res_blocks+1 ResBlocks, then optional 2x upsample.  The
   upsample (QwenImageResample upsample3d, T=1 Rep path) is a nearest-exact 2x
   spatial upsample + a channel-halving Conv2d (resample.1, a true {Co,Ci,kh,kw}
   2-D conv with pad 1).  time_conv is skipped (Rep path). *)

krVaeUpBlock[x_, resBlocks_List, upConv_, eps_] := Module[{h},
    h = Fold[krVaeResBlock[#1, #2, eps] &, x, resBlocks];
    If[ upConv === None,
        h
        ,
        (* resample.1.weight is a 2-D Conv2d {Co,Ci,kh,kw} (NOT 3-D); pad 1. *)
        With[{up = TUpsample2x[h]},
            TConv2DIm2ColPool[TUOpPad[up, {{0, 0}, {1, 1}, {1, 1}}], upConv["weight"], upConv["bias"]]
        ]
    ]
]

(* === latent denorm: z {z_dim, gH, gW} *= latents_std (per channel) += mean.
   The diffusers pipeline does latents = latents * std + mean before decode. *)

krVaeDenorm[z_, mean_List, std_List] := Module[{c, hh, ww, m, s},
    {c, hh, ww} = Dimensions[z];
    m = TUOpReshape[TTensorCreate[N[mean]], {c, 1, 1}];
    s = TUOpReshape[TTensorCreate[N[std]], {c, 1, 1}];
    TUOpAdd[TUOpMul[z, TUOpExpand[s, {c, hh, ww}]], TUOpExpand[m, {c, hh, ww}]]
]

(* === full decoder.  z {z_dim, gH, gW}; W the name->TTerm decoder loader;
   wsub[prefix] slices a sub-Association; cfg has eps / latentsMean / latentsStd /
   dimMult / numResBlocks.  Returns the RGB image {3, H, W} in [0,1].

   dim_mult {1,2,4,4} -> decoder dims [base*4, base*4, base*4, base*2, base*1];
   3 up-blocks upsample (the first three), the last does not -> /8 spatial
   upsampling on the (already /8 VAE-downsampled) latent recovers full res. *)

krVaeDecode[z_, W_, wsub_, cfg_] := Module[{eps, dimMult, nRes, nUp, z2, h, i, up, resB},
    eps = cfg["eps"];
    dimMult = cfg["dimMult"];
    nRes = cfg["numResBlocks"];
    nUp = Length[dimMult];                                            (* number of up-blocks *)
    (* denorm + post_quant (1x1) + conv_in (3x3). *)
    z2 = krVaeDenorm[z, cfg["latentsMean"], cfg["latentsStd"]];
    h = krVaeConv[z2, W["post_quant_conv.weight"], W["post_quant_conv.bias"], 0];  (* 16->16 *)
    h = krVaeConv[h, W["decoder.conv_in.weight"], W["decoder.conv_in.bias"], 1];   (* ->384 *)
    (* mid block: ResBlock -> self-attn -> ResBlock. *)
    h = krVaeResBlock[h, wsub["decoder.mid_block.resnets.0."], eps];
    h = krVaeAttn[h, wsub["decoder.mid_block.attentions.0."], eps];
    h = krVaeResBlock[h, wsub["decoder.mid_block.resnets.1."], eps];
    (* up-blocks: each has nRes+1 ResBlocks; all but the last upsample 2x. *)
    Do[ resB = Table[wsub["decoder.up_blocks." <> ToString[i] <> ".resnets." <> ToString[#] <> "."] &[m], {m, 0, nRes}];
        up = If[ i < nUp - 1, wsub["decoder.up_blocks." <> ToString[i] <> ".upsamplers.0.resample.1."], None];
        h = TRealize @ krVaeUpBlock[h, resB, up, eps],
        {i, 0, nUp - 1}];
    (* head: norm_out (L2-channel) -> SiLU -> conv_out (3x3 ->3) -> clip(-1,1) -> [0,1]. *)
    h = TSiLU @ krVaeNorm[h, W["decoder.norm_out.gamma"], eps];
    h = krVaeConv[h, W["decoder.conv_out.weight"], W["decoder.conv_out.bias"], 1];  (* ->3 *)
    TUOpMul[TUOpAdd[TClip[h, -1., 1.], TUOpConst[1.]], TUOpConst[0.5]]
]

End[];

EndPackage[];
