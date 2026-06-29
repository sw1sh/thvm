(* FluxVAE.wl -- the FLUX.2-klein-4B VAE decoder forward for thvm.

   Turns the transformer's packed latent {256, 128} into an RGB image
   {3, 256, 256}.  Pipeline (diffusers Flux2 AutoencoderKL decode):

     z_packed {256,128}
       -> BatchNorm denorm  (z * sqrt(var + eps_bn) + mean, per feature)
       -> unpatchify        ({256,128} -> {1,32,32,32} -> {32,32,32})
       -> post_quant_conv   (1x1, 32->32)
       -> conv_in           (3x3 pad1, 32->512)
       -> mid: ResBlock(512) + self-attn(GroupNorm32) + ResBlock(512)
       -> up_blocks[512,512,512->256,256->128]: 3 ResBlock + 2x upsample
          (the last up-block has NO upsample)
       -> GroupNorm(32) -> SiLU -> conv_out (3x3 pad1, 128->3)
       -> clip((x+1)/2, 0, 1) -> {3,256,256}

   The model-agnostic GroupNorm / SiLU / 2x-upsample / clip come from
   WolframInstitute`THVMLink`s NN library (TGroupNorm / TSiLU / TUpsample2x / TClip); this file
   keeps the FLUX-specific glue: the VAE-resolution conv (the fused strided-view
   im2col, so the spatial gather stays a VIEW and the footprint is flat in
   resolution), the spatial self-attention, and the decoder block assembly.  All
   convs are stride-1; the conv lowering is no-padding so 3x3 convs pad H,W by 1
   first.  Weights are bf16 in the diffusers vae safetensors; the loader
   (FluxGenerate.wl fxVaeLoader) keeps them bf16 on a GPU (half the bytes, and the
   conv reduce accumulates in f32 regardless) and f32 on CPU. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

Begin["`Private`"];


(* === primitives ===================================================== *)

(* conv: x {C_in,H,W}, w {C_out,C_in,kh,kw}, b {C_out}; pad is the symmetric
   H/W zero-pad (1 for 3x3 same-conv, 0 for 1x1).  TConv2D is no-padding, so 3x3
   convs pad H,W first; bias is folded in by the lowering.

   Lowering (default): TConv2DIm2ColPool -- the _pool strided unfold kept as a
   movement-only VIEW, broadcast against the weight and reduced over (Ci,kh,kw)
   in one fused multiply-reduce.  No materialised im2col matrix -- the GEMM
   lowering (TConv2DIm2ColPoolGemm) realizes a contiguous {Ci*kh*kw, Ho*Wo}
   im2col + {Co, Ci*kh*kw} weight so the matmul takes the tensor-core path, which
   is ~2x faster on the VAE conv shapes but materialises the 302/151 MB spatial
   gather (~64 GB at ImageSize 1024).  The fused view keeps the footprint flat in
   resolution, so it is the default; THVM_VAE_CONV_GEMM=1 opts back into the
   faster-but-heavy GEMM for small sizes.  Both reuse the same _pool windowing and
   are byte-faithful to TConv2DKhKw (validated to 1e-6 vs the correlation
   reference; pool vs gemm bit-identical).  Rank-4 inputs route to the batched
   _pool body. *)
vaeConvGemmQ[] := Environment["THVM_VAE_CONV_GEMM"] === "1"
vaeConvLower[x_, w_, b_] := With[{r = Length[Dimensions[x]]},
    Which[
        vaeConvGemmQ[], TConv2DIm2ColPoolGemm[x, w, b],
        r === 4, TConv2DIm2ColBatchedPool[x, w, b],
        True, TConv2DIm2ColPool[x, w, b]
    ]
]
vaeConv[x_, w_, b_, pad_] := vaeConvLower[If[pad === 0, x, TUOpPad[x, {{0, 0}, {pad, pad}, {pad, pad}}]], w, b]

(* === resnet block ==================================================== *)

(* ResBlock: GN -> SiLU -> conv3x3 -> GN -> SiLU -> conv3x3, plus shortcut
   (1x1 conv_shortcut when C_in != C_out, else identity).  W is an
   Association of this block's weights (norm1/conv1/norm2/conv2[/conv_shortcut]). *)
vaeResBlock[x_, W_, eps_] := Module[{h, sc},
    h = vaeConv[TSiLU[TGroupNorm[x, W["norm1.weight"], W["norm1.bias"], 32, eps]], W["conv1.weight"], W["conv1.bias"], 1];
    h = vaeConv[TSiLU[TGroupNorm[h, W["norm2.weight"], W["norm2.bias"], 32, eps]], W["conv2.weight"], W["conv2.bias"], 1];
    sc = If[KeyExistsQ[W, "conv_shortcut.weight"], vaeConv[x, W["conv_shortcut.weight"], W["conv_shortcut.bias"], 0], x];
    TUOpAdd[sc, h]
]

(* === mid-block self-attention ======================================== *)

(* spatial self-attention: GN -> q,k,v (1x1 conv) -> flatten {C, H*W} ->
   softmax(q^T k / sqrt(C)) over keys -> v @ attn -> 1x1 proj_out -> +x. *)
vaeAttn[x_, W_, eps_] := Module[{c, hh, ww, n, hn, lin, q, k, v, scores, attn, o, proj},
    {c, hh, ww} = Dimensions[x];  n = hh ww;
    (* q/k/v/to_out are diffusers Linear {C,C}; a 1x1 conv with the weight
       reshaped to {C,C,1,1} is the same channel-mixing projection. *)
    lin[name_, src_] := TUOpReshape[vaeConv[src, TUOpReshape[W[name <> ".weight"], {c, c, 1, 1}], W[name <> ".bias"], 0], {c, n}];
    hn = TGroupNorm[x, W["group_norm.weight"], W["group_norm.bias"], 32, eps];
    q = lin["to_q", hn];  k = lin["to_k", hn];  v = lin["to_v", hn];            (* {C,N} *)
    (* scores[i,j] = sum_c q[c,i] k[c,j] / sqrt(C) ; softmax over j (keys).
       BLAS matmuls (q/k/v are realized leaves) -- no expand-reduce. *)
    scores = TMatMul[Transpose[q], k] * (1. / Sqrt[c]);                            (* {N,N} *)
    attn = TSoftmax[scores, 1];                                                  (* over keys *)
    o = TMatMul[v, Transpose[attn]];                                            (* {C,N} *)
    proj = vaeConv[TUOpReshape[o, {c, hh, ww}], TUOpReshape[W["to_out.0.weight"], {c, c, 1, 1}], W["to_out.0.bias"], 0];
    TUOpAdd[x, proj]
]

(* === block assemblies =============================================== *)

(* mid block: ResBlock -> self-attention -> ResBlock (all at the deepest
   channel count).  Wm/Wa keyed by the per-sublayer Associations. *)
vaeMidBlock[x_, Wr0_, Wa_, Wr1_, eps_] := vaeResBlock[vaeAttn[vaeResBlock[x, Wr0, eps], Wa, eps], Wr1, eps]

(* up block: `nres` ResBlocks then optional (nearest-2x upsample + 3x3 conv).
   Ws is a list of per-ResBlock weight Associations; Wup the upsampler conv
   Association (or None). *)
vaeUpBlock[x_, Ws_List, Wup_, eps_] := Module[{h},
    h = Fold[vaeResBlock[#1, #2, eps]&, x, Ws];
    If[Wup === None, h, vaeConv[TUpsample2x[h], Wup["conv.weight"], Wup["conv.bias"], 1]]
]

(* === unpatchify ===================================================== *)

(* BatchNorm denorm: z {S,128} *= sqrt(var + epsBn), += mean, per channel.
   S = gridH*gridW image tokens (256 at 16x16); the per-feature mean/var
   broadcast over the token axis. *)
vaeDenorm[z_, mean_, var_, epsBn_] := With[{s = Dimensions[z], m = TUOpReshape[mean, {1, 128}], v = TUOpReshape[var, {1, 128}]},
    TUOpAdd[TUOpMul[z, TUOpExpand[TUOpSqrt[TUOpAdd[v, TUOpConst[N[epsBn]]]], s]], TUOpExpand[m, s]]
]

(* unpatchify: z {gridH*gridW,128} -> {32, 2*gridH, 2*gridW}.  Diffusers pipeline:
     view {gridH,gridW,128} -> permute(2,0,1) {128,gridH,gridW}
     -> reshape {32,2,2,gridH,gridW} -> permute(0,3,1,4,2) {32,gridH,2,gridW,2}
     -> reshape {32,2*gridH,2*gridW}.  TUOpPermute is numpy-style (0-indexed
     axes); at gridH=gridW=16 this is {16,16,128}->{32,32,32}, validated to
     2.4e-7 against the reference latent_unpatch. *)
vaeUnpatchify[z_, gridH_ : 16, gridW_ : 16] := TUOpReshape[
    TUOpPermute[
        TUOpReshape[TUOpPermute[TUOpReshape[z, {gridH, gridW, 128}], {2, 0, 1}], {32, 2, 2, gridH, gridW}],
        {0, 3, 1, 4, 2}
    ],
    {32, 2 gridH, 2 gridW}
]

(* === full decoder =================================================== *)

(* W is the flattened name->TTerm VAE loader (diffusers `decoder.*` etc.);
   helper wsub[prefix] slices an Association of the keys under a prefix.
   cfg has eps (GroupNorm 1e-6) + epsBn (1e-4). *)
vaeDecoder[zPacked_, W_, wsub_, cfg_] := Module[{eps, epsBn, gridH, gridW, z, h},
    eps = cfg["eps"];  epsBn = cfg["epsBn"];
    gridH = Lookup[cfg, "gridH", 16];  gridW = Lookup[cfg, "gridW", 16];
    z = vaeUnpatchify[vaeDenorm[zPacked, W["bn_running_mean"], W["bn_running_var"], epsBn], gridH, gridW];
    h = vaeConv[z, W["post_quant_conv.weight"], W["post_quant_conv.bias"], 0];   (* 1x1 32->32 *)
    h = vaeConv[h, W["decoder.conv_in.weight"], W["decoder.conv_in.bias"], 1];   (* 3x3 32->512 *)
    h = vaeMidBlock[h, wsub["decoder.mid_block.resnets.0."], wsub["decoder.mid_block.attentions.0."], wsub["decoder.mid_block.resnets.1."], eps];
    Do[ With[{up = "decoder.up_blocks." <> ToString[i] <> "."},
            h = TRealize @ vaeUpBlock[h, Table[wsub[up <> "resnets." <> ToString[m] <> "."], {m, 0, 2}], If[i < 3, wsub[up <> "upsamplers.0."], None], eps]],
        {i, 0, 3}];
    h = TSiLU @ TGroupNorm[h, W["decoder.conv_norm_out.weight"], W["decoder.conv_norm_out.bias"], 32, eps];
    h = vaeConv[h, W["decoder.conv_out.weight"], W["decoder.conv_out.bias"], 1]; (* 3x3 128->3 *)
    (* image = clip((x+1)/2, 0, 1) *)
    TClip[(h + 1) * 0.5, 0., 1.]
]

End[];

EndPackage[];
