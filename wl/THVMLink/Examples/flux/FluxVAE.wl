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
   THVMLink`s NN library (TGroupNorm / TSiLU / TUpsample2x / TClip); this file
   keeps the FLUX-specific glue: the VAE-resolution conv (im2col with realized
   operands so the big gemm hits BLAS), the spatial self-attention, and the
   decoder block assembly.  All convs are stride-1; TConv2D is no-padding so
   3x3 convs pad H,W by 1 first.  Weights are bf16 in the diffusers vae
   safetensors; cast to f32. *)

Get[FileNameJoin[{DirectoryName[$InputFileName], "FluxForward.wl"}]];

(* === primitives ===================================================== *)

(* conv: x {C_in,H,W}, w {C_out,C_in,kh,kw}, b {C_out}; pad is the symmetric
   H/W zero-pad (1 for 3x3 same-conv, 0 for 1x1).  im2col + matmul, but with
   xCol AND wFlat realized before TMatMul: left lazy (as builtin TConv2D does
   them) the gemm classifier declines and the {512,4608}.{4608,16384} matmul
   runs as a scalar cpu_jit loop -- 43s/conv at VAE resolution.  Realizing both
   operands routes it to blas_gemm (~0.05s).  [thvm follow-up: TConv2DIm2Col
   should protect its matmul operands at high resolution.]  The input is also
   realized so chained convs' 9x im2col SHRINKs hit a leaf, not a 9^depth nest.
   FLUX-local (not the library TConv2D) precisely for these realize barriers. *)
vaeConv[x_, w_, b_, pad_] := Module[
    {xp, cIn, hp, wp, cOut, kh, kw, hOut, wOut, kSpat, patches, summed, xCol, wFlat, out},
    xp = TRealize @ If[ pad === 0, x, TUOpPad[x, {{0, 0}, {pad, pad}, {pad, pad}}]];
    {cIn, hp, wp} = Dimensions[xp];  {cOut, cIn, kh, kw} = Dimensions[w];
    hOut = hp - kh + 1;  wOut = wp - kw + 1;  kSpat = kh kw;
    patches = Flatten @ Table[ With[{slot = ki kw + kj},
        TUOpPad[TUOpReshape[TUOpShrink[xp, {{0, cIn}, {ki, ki + hOut}, {kj, kj + wOut}}],
                            {cIn, 1, hOut wOut}], {{0, 0}, {slot, kSpat - 1 - slot}, {0, 0}}]],
        {ki, 0, kh - 1}, {kj, 0, kw - 1}];
    summed = Fold[TUOpAdd, First @ patches, Rest @ patches];
    xCol  = TRealize @ TUOpReshape[summed, {cIn kSpat, hOut wOut}];
    wFlat = TRealize @ TUOpReshape[w, {cOut, cIn kSpat}];
    out = TMatMul[wFlat, xCol];                                     (* blas_gemm *)
    TRealize @ TUOpAdd[TUOpReshape[out, {cOut, hOut, wOut}],
                       TUOpExpand[TUOpReshape[b, {cOut, 1, 1}], {cOut, hOut, wOut}]]]

(* === resnet block ==================================================== *)

(* ResBlock: GN -> SiLU -> conv3x3 -> GN -> SiLU -> conv3x3, plus shortcut
   (1x1 conv_shortcut when C_in != C_out, else identity).  W is an
   Association of this block's weights (norm1/conv1/norm2/conv2[/conv_shortcut]). *)
vaeResBlock[x_, W_, eps_] := Module[{h, sc},
    h = vaeConv[TSiLU[TGroupNorm[x, W["norm1.weight"], W["norm1.bias"], 32, eps]],
                W["conv1.weight"], W["conv1.bias"], 1];
    h = vaeConv[TSiLU[TGroupNorm[h, W["norm2.weight"], W["norm2.bias"], 32, eps]],
                W["conv2.weight"], W["conv2.bias"], 1];
    sc = If[ KeyExistsQ[W, "conv_shortcut.weight"],
            vaeConv[x, W["conv_shortcut.weight"], W["conv_shortcut.bias"], 0], x];
    TUOpAdd[sc, h]]

(* === mid-block self-attention ======================================== *)

(* spatial self-attention: GN -> q,k,v (1x1 conv) -> flatten {C, H*W} ->
   softmax(q^T k / sqrt(C)) over keys -> v @ attn -> 1x1 proj_out -> +x. *)
vaeAttn[x_, W_, eps_] := Module[{c, hh, ww, n, hn, lin, q, k, v, scores, attn, o, proj},
    {c, hh, ww} = Dimensions[x];  n = hh ww;
    (* q/k/v/to_out are diffusers Linear {C,C}; a 1x1 conv with the weight
       reshaped to {C,C,1,1} is the same channel-mixing projection. *)
    lin[name_, src_] := TUOpReshape[
        vaeConv[src, TUOpReshape[W[name <> ".weight"], {c, c, 1, 1}], W[name <> ".bias"], 0], {c, n}];
    hn = TGroupNorm[x, W["group_norm.weight"], W["group_norm.bias"], 32, eps];
    q = lin["to_q", hn];  k = lin["to_k", hn];  v = lin["to_v", hn];            (* {C,N} *)
    (* scores[i,j] = sum_c q[c,i] k[c,j] / sqrt(C) ; softmax over j (keys).
       BLAS matmuls (q/k/v are realized leaves) -- no expand-reduce. *)
    scores = TMatMul[Transpose[q], k] * (1./Sqrt[c]);                            (* {N,N} *)
    attn = TSoftmax[scores, 1];                                                  (* over keys *)
    o = TMatMul[v, Transpose[attn]];                                            (* {C,N} *)
    proj = vaeConv[TUOpReshape[o, {c, hh, ww}],
                   TUOpReshape[W["to_out.0.weight"], {c, c, 1, 1}], W["to_out.0.bias"], 0];
    TUOpAdd[x, proj]]

(* === block assemblies =============================================== *)

(* mid block: ResBlock -> self-attention -> ResBlock (all at the deepest
   channel count).  Wm/Wa keyed by the per-sublayer Associations. *)
vaeMidBlock[x_, Wr0_, Wa_, Wr1_, eps_] :=
    vaeResBlock[vaeAttn[vaeResBlock[x, Wr0, eps], Wa, eps], Wr1, eps]

(* up block: `nres` ResBlocks then optional (nearest-2x upsample + 3x3 conv).
   Ws is a list of per-ResBlock weight Associations; Wup the upsampler conv
   Association (or None). *)
vaeUpBlock[x_, Ws_List, Wup_, eps_] := Module[{h},
    h = Fold[vaeResBlock[#1, #2, eps] &, x, Ws];
    If[ Wup === None, h,
        vaeConv[TUpsample2x[h], Wup["conv.weight"], Wup["conv.bias"], 1]]]

(* === unpatchify ===================================================== *)

(* BatchNorm denorm: z {256,128} *= sqrt(var + epsBn), += mean, per channel. *)
vaeDenorm[z_, mean_, var_, epsBn_] := With[{
        m = TUOpReshape[mean, {1, 128}], v = TUOpReshape[var, {1, 128}]},
    TUOpAdd[TUOpMul[z, TUOpExpand[TUOpSqrt[TUOpAdd[v, TUOpConst[N[epsBn]]]], {256, 128}]],
            TUOpExpand[m, {256, 128}]]]

(* unpatchify: z {256,128} -> {32,32,32}.  Diffusers pipeline:
     view {16,16,128} -> permute(2,0,1) {128,16,16}
     -> reshape {32,2,2,16,16} -> permute(0,3,1,4,2) {32,16,2,16,2}
     -> reshape {32,32,32}.  TUOpPermute is numpy-style (0-indexed axes);
     validated to 2.4e-7 against the reference latent_unpatch. *)
vaeUnpatchify[z_] := TUOpReshape[
    TUOpPermute[
        TUOpReshape[TUOpPermute[TUOpReshape[z, {16, 16, 128}], {2, 0, 1}], {32, 2, 2, 16, 16}],
        {0, 3, 1, 4, 2}],
    {32, 32, 32}]

(* === full decoder =================================================== *)

(* W is the flattened name->TTerm VAE loader (diffusers `decoder.*` etc.);
   helper wsub[prefix] slices an Association of the keys under a prefix.
   cfg has eps (GroupNorm 1e-6) + epsBn (1e-4). *)
vaeDecoder[zPacked_, W_, wsub_, cfg_] := Module[{eps, epsBn, z, h},
    eps = cfg["eps"];  epsBn = cfg["epsBn"];
    z = vaeUnpatchify[vaeDenorm[zPacked, W["bn_running_mean"], W["bn_running_var"], epsBn]];
    h = vaeConv[z, W["post_quant_conv.weight"], W["post_quant_conv.bias"], 0];   (* 1x1 32->32 *)
    h = vaeConv[h, W["decoder.conv_in.weight"], W["decoder.conv_in.bias"], 1];   (* 3x3 32->512 *)
    h = vaeMidBlock[h,
            wsub["decoder.mid_block.resnets.0."], wsub["decoder.mid_block.attentions.0."],
            wsub["decoder.mid_block.resnets.1."], eps];
    Do[ With[{up = "decoder.up_blocks." <> ToString[i] <> "."},
            h = TRealize @ vaeUpBlock[h,
                Table[wsub[up <> "resnets." <> ToString[m] <> "."], {m, 0, 2}],
                If[ i < 3, wsub[up <> "upsamplers.0."], None], eps]],
        {i, 0, 3}];
    h = TSiLU @ TGroupNorm[h, W["decoder.conv_norm_out.weight"], W["decoder.conv_norm_out.bias"], 32, eps];
    h = vaeConv[h, W["decoder.conv_out.weight"], W["decoder.conv_out.bias"], 1]; (* 3x3 128->3 *)
    (* image = clip((x+1)/2, 0, 1) *)
    TClip[(h + 1) * 0.5, 0., 1.]]
