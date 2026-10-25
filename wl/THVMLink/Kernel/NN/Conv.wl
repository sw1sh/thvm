(* ::Package:: *)
(* NN/Conv.wl - convolution (tinygrad's conv2d, src/nn/conv2d.c), the _pool
   strided-view and GEMM lowerings the FLUX / Krea VAEs call, max-pool, and
   nearest 2x upsample. *)

BeginPackage["WolframInstitute`THVMLink`", {"GeneralUtilities`"}];


SetUsage[TConv2DIm2ColPool, "TConv2DIm2ColPool[input$, weights$, bias$] is the strided-view _pool im2col lowering of a stride-1, no-padding rank-3 {C_in, H, W} convolution: the im2col operand stays a movement-only VIEW (RESHAPE/EXPAND/SHRINK/PERMUTE), broadcast against the weight and reduced over the (C_in, kh, kw) axes in one fused multiply-reduce -- no materialised im2col matrix. The FLUX / Krea VAEs call it; TConv2D gives the same output."];
SetUsage[TConv2DIm2ColBatchedPool, "TConv2DIm2ColBatchedPool[input$, weights$, bias$] is the rank-4 batched {B, C_in, H, W} analogue of TConv2DIm2ColPool: the _pool unfold stays a movement-only VIEW broadcast against the weight and reduced over (C_in, kh, kw) in one fused multiply-reduce, permuted back to {B, C_out, H_out, W_out}. TConv2D gives the same output."];
SetUsage[TConv2DIm2ColPoolGemm, "TConv2DIm2ColPoolGemm[input$, weights$, bias$] is the GEMM lowering of the strided-view _pool unfold: it realizes the im2col view as a contiguous {C_in*kh*kw, H_out*W_out} matrix and the weight as {C_out, C_in*kh*kw}, then out = w_flat @ xCol via TMatMul so the matmul takes the tensor-core GEMM path (the fused conv reduce is rejected by the TC recogniser). Same output as TConv2D, byte-faithful to the kh*kw partial-sum form; ~2x the fused reduce on the FLUX.2 VAE convs."];
SetUsage[TUpsample2x, "TUpsample2x[x$] is the nearest-neighbour 2x upsample of a {C, H, W} tensor to {C, 2H, 2W} via repeat-interleave (each pixel expanded to a 2x2 block). The VAE / U-Net decoder upsampler."];

SetUsage[TConv2D, "TConv2D[input$, weights$] and TConv2D[input$, weights$, bias$] are tinygrad's conv2d over the trailing spatial axes: input$ is {B, C_in, $$} or unbatched {C_in, $$}, weights$ {C_out, C_in/groups, $$}, bias$ {C_out} or None. Options \"Stride\", \"Dilation\" (an integer or one per axis), \"Padding\" (an integer, one per axis, or one per side with the last axis first, a negative side cropping) and \"Groups\"."];
SetUsage[TMaxPool2d, "TMaxPool2d[x$] and TMaxPool2d[x$, k$] run a non-overlapping k$ x k$ max-pool over the trailing two axes of a rank-3 {C, H, W} or rank-4 batched {B, C, H, W} channels-first tensor. Default k$ = 2."];

Begin["`Private`"];

(* TConv2D: one C body for every rank and option (docs/NN/nn.md); an unbatched
   input runs as a batch of 1. *)
Options[TConv2D] = {"Stride" -> 1, "Dilation" -> 1, "Padding" -> 0, "Groups" -> 1};
convTuple[v_Integer, n_] := ConstantArray[v, n]
convTuple[v_List, _] := v
(* (begin, end) per spatial axis in axis order, from tinygrad's padding forms. *)
convPads[p_Integer, n_] := ConstantArray[p, 2 n]
convPads[p_List, n_] /; Length[p] === n := Riffle[p, p]
convPads[p_List, n_] /; Length[p] === 2 n := Flatten[Transpose[{p[[-2 ;; 1 ;; -2]], p[[-1 ;; 1 ;; -2]]}]]
TConv2D[input_TTerm, weights_TTerm, bias : (_TTerm | None) : None, opts : OptionsPattern[]] := Module[{n, unbatched, x, ints, out},
    n = Length[tUopShape[weights]] - 2;
    unbatched = Length[tUopShape[input]] === n + 1;
    x = If[unbatched, TUOpReshape[input, Prepend[tUopShape[input], 1]], input];
    ints = Join[{OptionValue["Groups"]}, convTuple[OptionValue["Stride"], n],
        convTuple[OptionValue["Dilation"], n], convPads[OptionValue["Padding"], n]];
    out = nnCall[TConv2D, "conv2d", {x, weights, bias}, ints];
    If[unbatched && out =!= $Failed, TUOpReshape[out, Rest[tUopShape[out]]], out]
]

(* === tinygrad-`_pool` STRIDED-VIEW conv lowering ============
   TConv2DIm2ColPool / TConv2DIm2ColBatchedPool build the windowed
   im2col view `xCol6 : {cIn,kh,kw,...}` purely from movement ops
   (RESHAPE / EXPAND / SHRINK / PERMUTE) -- a strided VIEW over the
   input, exactly like tinygrad's `_pool` unfold, rather than a
   materialised im2col matrix.

   The conv is then `(xB * wB).sum` reducing the (cIn, kh, kw) axes
   SEPARATELY -- tinygrad's mixin conv2d (mixin/__init__.py:1420), which
   broadcasts x and weight to (cOut, cIn, kh, kw, hOut, wOut) and sums the
   trailing 1+len(HW) axes.  Keeping cIn/kh/kw as DISTINCT reduce axes is
   the load-bearing choice: each reduce range carries a plain linear stride
   so the codegen indexes the unfold view with affine address arithmetic.
   Flattening (cIn,kh,kw) into a single K axis (the prior `Reshape{cIn*kh*kw,
   hOut*wOut}` + TMatMul) forced the renderer to recover (cIn,kh,kw) via
   IDIV/IMOD on EVERY mac -- a ~20x slower per-mac address monster on the
   FLUX VAE convs (256-spatial, K=cIn*9).

   Construction (stride 1, no padding, rank-3 input {cIn,H,W}, weights
   {cOut,cIn,kh,kw}; verified: (ky*(H+1)+hh) mod H = ky+hh within the
   used range since ky+hh <= H-1, and matches TConv2DKhKw to f32
   tolerance):
     Rh = Ceiling[kh*(H+1)/H]; Rw = Ceiling[kw*(W+1)/W];
     x1   = Reshape(Expand(Reshape(x,{cIn,1,H,1,W}),{cIn,Rh,H,Rw,W}),
                    {cIn,Rh*H,Rw*W});
     x2   = Shrink(x1, .., {0,kh*(H+1)}, {0,kw*(W+1)});
     x3   = Reshape(x2, {cIn,kh,H+1,kw,W+1});
     x4   = Shrink(x3, .., {0,kh},{0,Hout},{0,kw},{0,Wout});
     xCol6= Permute(x4, {0,1,3,2,4});           -> {cIn,kh,kw,Hout,Wout}
   then broadcast x/w to {cOut,cIn,kh,kw,Hout,Wout}, MUL, SUM over the
   {cIn,kh,kw} axes -> {cOut,Hout,Wout}, add bias.

   FORWARD is bit-for-bit correct, and BACKWARD is too (matches
   TConv2DKhKw / the PAD-and-sum path to f32 tolerance).  This path is
   TConv2D's default lowering; THVM_CONV_POOL=0 reverts to PAD-and-sum.
   (`input` is pushed onto a contiguous buffer boundary first -- see the
   body -- so the strided-view chain composes correctly even when `input`
   is itself a movement / compute DAG.) *)
(* The conv reduce's f32 accumulation is sum_acc_dtype, applied generically
   by tSumAxes (Tensor.wl, the `.sum()` rule -- tinygrad mixin/reduce.py:43 +
   dtype.py:274): a bf16/f16 (x*w) is upcast to f32 for the cIn*kh*kw running
   sum then cast back, so the conv body is the plain idiomatic (x*w).sum and
   carries no conv-specific dtype hand-casts. *)

(* Shared rank-3 _pool unfold: x {cIn,H,W} -> strided VIEW {cIn,kh,kw,hOut,wOut}
   (movement ops only; see the construction note above).  Both the expand-mul-
   reduce body (TConv2DIm2ColPool) and the GEMM body (TConv2DIm2ColPoolGemm)
   build their im2col from this one windowing so they index identically. *)
conv2dPoolUnfold[input_, cIn_, h_, wd_, kh_, kw_, hOut_, wOut_] := Module[{rh, rw, x1, x2, x3, x4},
    rh = Ceiling[kh * (h + 1) / h];
    rw = Ceiling[kw * (wd + 1) / wd];
    x1 = TUOpReshape[TUOpExpand[TUOpReshape[input, {cIn, 1, h, 1, wd}], {cIn, rh, h, rw, wd}], {cIn, rh * h, rw * wd}];
    x2 = TUOpShrink[x1, {{0, cIn}, {0, kh * (h + 1)}, {0, kw * (wd + 1)}}];
    x3 = TUOpReshape[x2, {cIn, kh, h + 1, kw, wd + 1}];
    x4 = TUOpShrink[x3, {{0, cIn}, {0, kh}, {0, hOut}, {0, kw}, {0, wOut}}];
    TUOpPermute[x4, {0, 1, 3, 2, 4}]  (* {cIn,kh,kw,hOut,wOut} *)
]

TConv2DIm2ColPool[inputArg_TTerm, weights_TTerm, bias_TTerm] := Module[{input, inShape, wShape, cIn, cOut, h, wd, kh, kw, hOut, wOut, xCol6, xB, wB, outShaped, biasBcast},
    (* Push `input` onto a contiguous buffer boundary BEFORE the _pool
       unfold.  The unfold's first op RESHAPE[input,{cIn,1,h,1,wd}] (and the
       EXPAND/SHRINK windowing after it) assumes a contiguous row-major input
       layout: a RESHAPE that splits/merges axes is only a pure VIEW over
       contiguous memory.  When `input` is itself a deep movement/compute DAG
       (a chained conv -> GroupNorm -> SiLU -> conv, or the unpatchify PERMUTE),
       composing the unfold reshapes onto that non-contiguous view mis-indexes
       the strided gather and the fused reduce over-fuses into one mis-compiled
       kernel (NaN/Inf on CPU, washed-out garbage on Metal -- the FLUX.2 VAE
       decoded to flat gray).  Realizing `input` (the small {cIn,H,W} activation,
       NOT the big im2col view, which stays a view feeding the fused reduce)
       gives the unfold a contiguous leaf -- the boundary the GEMM lowering got
       for free by realizing xCol.  Matches TConv2DKhKw / TConv2DIm2ColPoolGemm
       to f32 tolerance on the full VAE chain. *)
    input = TRealize[inputArg];
    inShape = tUopShape[input];
    wShape = tUopShape[weights];
    {cIn, h, wd} = inShape;
    cOut = wShape[[1]];
    kh = wShape[[3]];
    kw = wShape[[4]];
    hOut = h - kh + 1;
    wOut = wd - kw + 1;
    xCol6 = conv2dPoolUnfold[input, cIn, h, wd, kh, kw, hOut, wOut];
    (* broadcast x over cOut, weight over (hOut,wOut); reduce cIn,kh,kw. *)
    xB = TUOpExpand[TUOpReshape[xCol6, {1, cIn, kh, kw, hOut, wOut}], {cOut, cIn, kh, kw, hOut, wOut}];
    wB = TUOpExpand[TUOpReshape[weights, {cOut, cIn, kh, kw, 1, 1}], {cOut, cIn, kh, kw, hOut, wOut}];
    (* (x*w).sum over (cIn,kh,kw) -> {cOut, hOut, wOut}.  tSumAxes carries the
       sum_acc_dtype f32 accumulation (cast up, reduce, cast back) for a
       bf16/f16 conv, matching tinygrad's (x*weight).sum (mixin/__init__.py:1432). *)
    outShaped = tSumAxes[TUOpMul[xB, wB], {1, 2, 3}, tUopDType[weights]];
    biasBcast = TUOpExpand[TUOpReshape[bias, {cOut, 1, 1}], {cOut, hOut, wOut}];
    outShaped + biasBcast
]

(* GEMM lowering of the (correct) _pool unfold: instead of the broadcast
   expand-mul-reduce (one fused strided reduce per conv -- the default), reshape
   the unfold VIEW xCol6 {cIn,kh,kw,hOut,wOut} to a 2D matrix {cIn*kh*kw,
   hOut*wOut} and the weight to {cOut, cIn*kh*kw}, then out = wFlat @ xCol via
   TMatMul.  REALIZING xCol + wFlat hands the matmul two contiguous 2D leaves so
   it takes the tensor-core GEMM path (the fused conv reduce is rejected by
   recognise_tc).  On the FLUX.2 VAE this is ~2x the fused reduce (TC GEMM >>
   strided bf16 reduce), and it reuses the _pool windowing so it is byte-faithful
   (the THVM_CONV_POOL=0 PAD-and-sum lowering mishandled a pre-padded input;
   this path does not).  TConv2DKhKw stays the partial-sum reference. *)
TConv2DIm2ColPoolGemm[input_TTerm, weights_TTerm, bias_TTerm] := Module[{inShape, wShape, cIn, cOut, h, wd, kh, kw, hOut, wOut, xCol6, xCol, wFlat, outFlat, outShaped, biasBcast},
    inShape = tUopShape[input];
    wShape = tUopShape[weights];
    {cIn, h, wd} = inShape;
    cOut = wShape[[1]];  kh = wShape[[3]];  kw = wShape[[4]];
    hOut = h - kh + 1;  wOut = wd - kw + 1;
    xCol6 = conv2dPoolUnfold[input, cIn, h, wd, kh, kw, hOut, wOut];
    (* Realize the unfold to a contiguous {cIn*kh*kw, hOut*wOut} matrix and the
       weight to {cOut, cIn*kh*kw} so TMatMul sees two 2D leaves (TC-eligible). *)
    xCol = TRealize @ TUOpReshape[xCol6, {cIn * kh * kw, hOut * wOut}];
    wFlat = TRealize @ TUOpReshape[weights, {cOut, cIn * kh * kw}];
    outFlat = TMatMul[wFlat, xCol];  (* {cOut, hOut*wOut} *)
    outShaped = TUOpReshape[outFlat, {cOut, hOut, wOut}];
    biasBcast = TUOpExpand[TUOpReshape[bias, {cOut, 1, 1}], {cOut, hOut, wOut}];
    outShaped + biasBcast
]

(* Rank-4 batched analogue of TConv2DIm2ColPool: input {B,cIn,H,W}.

   Like the rank-3 path, the (cIn, kh, kw) reduce axes stay SEPARATE
   (tinygrad mixin/__init__.py:1420) so the unfold view is indexed with
   affine strides instead of per-mac IDIV/IMOD K recovery.  x broadcasts
   over cOut, weight over (B,hOut,wOut); MUL then SUM the cIn/kh/kw axes
   -> {cOut, B, hOut, wOut}, permuted back to {B, cOut, hOut, wOut}.  The
   reshapes are contiguity-preserving so EXPAND just sets broadcast
   strides -- no extra materialization. *)
TConv2DIm2ColBatchedPool[inputArg_TTerm, weights_TTerm, bias_TTerm] := Module[{input, inShape, wShape, batch, cIn, cOut, h, wd, kh, kw, hOut, wOut, rh, rw, x1, x2, x3, x4, xCol6, xB, wB, out4, outShaped, biasBcast},
    (* Contiguous-input boundary before the unfold -- see TConv2DIm2ColPool. *)
    input = TRealize[inputArg];
    inShape = tUopShape[input];
    wShape = tUopShape[weights];
    {batch, cIn, h, wd} = inShape;
    cOut = wShape[[1]];
    kh = wShape[[3]];
    kw = wShape[[4]];
    hOut = h - kh + 1;
    wOut = wd - kw + 1;
    rh = Ceiling[kh * (h + 1) / h];
    rw = Ceiling[kw * (wd + 1) / wd];
    x1 = TUOpReshape[TUOpExpand[TUOpReshape[input, {batch, cIn, 1, h, 1, wd}], {batch, cIn, rh, h, rw, wd}], {batch, cIn, rh * h, rw * wd}];
    x2 = TUOpShrink[x1, {{0, batch}, {0, cIn}, {0, kh * (h + 1)}, {0, kw * (wd + 1)}}];
    x3 = TUOpReshape[x2, {batch, cIn, kh, h + 1, kw, wd + 1}];
    x4 = TUOpShrink[x3, {{0, batch}, {0, cIn}, {0, kh}, {0, hOut}, {0, kw}, {0, wOut}}];
    xCol6 = TUOpPermute[x4, {1, 2, 4, 0, 3, 5}];     (* {cIn,kh,kw,B,hOut,wOut} *)
    (* {cOut,cIn,kh,kw,B,hOut,wOut}: x over cOut, weight over (B,hOut,wOut). *)
    xB = TUOpExpand[TUOpReshape[xCol6, {1, cIn, kh, kw, batch, hOut, wOut}], {cOut, cIn, kh, kw, batch, hOut, wOut}];
    wB = TUOpExpand[TUOpReshape[weights, {cOut, cIn, kh, kw, 1, 1, 1}], {cOut, cIn, kh, kw, batch, hOut, wOut}];
    (* (x*w).sum over (cIn,kh,kw) with sum_acc_dtype f32 accumulation ->
       {cOut,B,hOut,wOut} (see TConv2DIm2ColPool). *)
    out4 = tSumAxes[TUOpMul[xB, wB], {1, 2, 3}, tUopDType[weights]];
    outShaped = TUOpPermute[out4, {1, 0, 2, 3}];           (* {B,cOut,hOut,wOut} *)
    biasBcast = TUOpExpand[TUOpReshape[bias, {1, cOut, 1, 1}], {batch, cOut, hOut, wOut}];
    outShaped + biasBcast
]

(* TMaxPool2d[x, k] -- non-overlapping k x k max-pool over the last two
   axes (tinygrad's max_pool2d, src/nn/max_pool2d.c): windows that tile the
   input are a reshape the max reduces. *)
TMaxPool2d[x_TTerm] := TMaxPool2d[x, 2]
TMaxPool2d[x_TTerm, k_Integer] := nnCall[TMaxPool2d, "max_pool2d", {x}, {k, k, k, k, 1, 1}]

(* TUpsample2x[x]: nearest-neighbour 2x upsample {C, H, W} -> {C, 2H, 2W}
   via repeat-interleave: insert unit axes after H and W, EXPAND each to 2,
   merge back so every pixel becomes a contiguous 2x2 block. *)
TUpsample2x[x_TTerm] := With[{shape = tUopShape[x]},
    Module[{c, h, w},
        {c, h, w} = shape;
        TUOpReshape[TUOpExpand[TUOpReshape[x, {c, h, 1, w, 1}], {c, h, 2, w, 2}], {c, 2 h, 2 w}]
    ]
]

End[];

EndPackage[];
