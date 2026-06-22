(* ::Package:: *)
(* NN/Conv.wl - 2-D convolution lowerings (tinygrad's _pool strided-view
   im2col, plus the kh*kw partial-sum reference), max-pool, and nearest 2x
   upsample. *)

BeginPackage["WolframInstitute`THVMLink`"];

GeneralUtilities`SetUsage[TConv2D, "TConv2D[input$, weights$, bias$] builds a stride-1, no-padding 2-D convolution. input$ is {C_in, H, W} or batched {B, C_in, H, W}; weights$ is {C_out, C_in, kh, kw}; bias$ is {C_out}. Rank-3 routes through TConv2DIm2Col, rank-4 through TConv2DIm2ColBatched."];
GeneralUtilities`SetUsage[TConv2DIm2Col, "TConv2DIm2Col[input$, weights$, bias$] is the im2col + matmul lowering of a stride-1, no-padding 2-D convolution, with the same signature and output shape as TConv2D. It builds the im2col operand xCol : {C_in*kh*kw, H_out*W_out}, then out = w_flat @ xCol via TMatMul (cblas_sgemm). TConv2D's default path."];
GeneralUtilities`SetUsage[TConv2DIm2ColBatched, "TConv2DIm2ColBatched[input$, weights$, bias$] is the rank-4 batched im2col lowering for input$ of shape {B, C, H, W}. It builds xCol : {C*kh*kw, B*H_out*W_out}, runs one matmul, then reshapes back to {B, C_out, H_out, W_out}."];
GeneralUtilities`SetUsage[TConv2DKhKw, "TConv2DKhKw[input$, weights$, bias$] is the kh*kw partial-sum convolution lowering, the explicit reference body for TConv2D."];
GeneralUtilities`SetUsage[TMaxPool2d, "TMaxPool2d[x$] and TMaxPool2d[x$, k$] run a non-overlapping k$ x k$ max-pool over the trailing two axes of a rank-3 {C, H, W} or rank-4 batched {B, C, H, W} channels-first tensor. Default k$ = 2."];
GeneralUtilities`SetUsage[TUpsample2x, "TUpsample2x[x$] is the nearest-neighbour 2x upsample of a {C, H, W} tensor to {C, 2H, 2W} via repeat-interleave (each pixel expanded to a 2x2 block). The VAE / U-Net decoder upsampler."];

Begin["`Private`"];

(* TConv2D[input, weights, bias] -- stride-1, no-padding 2-D
   convolution, lowered into a chain of kh*kw partial sums:

       For each (ki, kj) in [0, kh) x [0, kw):
         x_slice = SHRINK(input,   all C_in,  [ki, ki + H_out), [kj, kj + W_out))
         w_slice = SHRINK(weights, all C_out, all C_in,
                                   [ki, ki + 1), [kj, kj + 1))
         x_b     = EXPAND(RESHAPE(x_slice, {1, C_in, H_out, W_out}),
                                 {C_out, C_in, H_out, W_out})
         w_b     = EXPAND(w_slice, {C_out, C_in, H_out, W_out})
         partial = REDUCE_SUM(x_b * w_b, axis = 1)
                                                     {C_out, H_out, W_out}
       sum the kh*kw partials, broadcast bias, return.

   Why kh*kw partials (rather than tinygrad's _pool unfold): the
   unfolded tensor would have shape {C_in, H_out, W_out, kh, kw}
   which for a 28x28 -> 24x24 conv with kh=kw=5 is 24*24*25 = 14400
   elements per channel; the partial-sum form only allocates a few
   {C_out, C_in, H_out, W_out} intermediates per kernel position,
   which fits the per-op-allocates-a-buffer materializer better. *)
(* TConv2D routes through im2col + sgemm; TConv2DKhKw keeps the kh*kw path. *)
TConv2D[input_TTerm, weights_TTerm, bias_TTerm] := With[{
    inShape = tUopShape[input]
},
    Which[
        Length[inShape] === 3, TConv2DIm2Col[input, weights, bias],
        Length[inShape] === 4, TConv2DIm2ColBatched[input, weights, bias],
        True, Failure["NotImplemented",
            <|"Message" -> "TConv2D expects rank-3 {C,H,W} or rank-4 {B,C,H,W}",
              "InputShape" -> inShape|>]
    ]
]

TConv2DKhKw[input_TTerm, weights_TTerm, bias_TTerm] := Module[{
    inShape, wShape, cIn, cOut, h, wd, kh, kw, hOut, wOut,
    partials, xSlice, wSlice, xB, wB, summed, biasBcast
},
    inShape = tUopShape[input];
    wShape  = tUopShape[weights];
    {cIn, h, wd}        = inShape;
    {cOut, cIn, kh, kw} = wShape;
    hOut = h  - kh + 1;
    wOut = wd - kw + 1;
    partials = Flatten @ Table[
        xSlice = TUOpShrink[input,
            {{0, cIn}, {ki, ki + hOut}, {kj, kj + wOut}}];
        wSlice = TUOpShrink[weights,
            {{0, cOut}, {0, cIn}, {ki, ki + 1}, {kj, kj + 1}}];
        xB = TUOpExpand[
            TUOpReshape[xSlice, {1, cIn, hOut, wOut}],
            {cOut, cIn, hOut, wOut}];
        wB = TUOpExpand[wSlice, {cOut, cIn, hOut, wOut}];
        TUOpReduce[xB * wB, 1, "SUM"],
        {ki, 0, kh - 1}, {kj, 0, kw - 1}
    ];
    summed = Fold[Plus, First[partials], Rest[partials]];
    biasBcast = TUOpExpand[
        TUOpReshape[bias, {cOut, 1, 1}],
        {cOut, hOut, wOut}];
    summed + biasBcast
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
TConv2DIm2ColPool[inputArg_TTerm, weights_TTerm, bias_TTerm] := Module[{
    input, inShape, wShape, cIn, cOut, h, wd, kh, kw, hOut, wOut,
    rh, rw, x1, x2, x3, x4, xCol6, xB, wB, outShaped, biasBcast
},
    input = inputArg;
    inShape = tUopShape[input];
    wShape  = tUopShape[weights];
    {cIn, h, wd}      = inShape;
    cOut = wShape[[1]];
    kh   = wShape[[3]];
    kw   = wShape[[4]];
    hOut  = h  - kh + 1;
    wOut  = wd - kw + 1;
    rh = Ceiling[kh * (h + 1) / h];
    rw = Ceiling[kw * (wd + 1) / wd];
    x1 = TUOpReshape[
        TUOpExpand[TUOpReshape[input, {cIn, 1, h, 1, wd}],
                   {cIn, rh, h, rw, wd}],
        {cIn, rh * h, rw * wd}];
    x2 = TUOpShrink[x1, {{0, cIn}, {0, kh * (h + 1)}, {0, kw * (wd + 1)}}];
    x3 = TUOpReshape[x2, {cIn, kh, h + 1, kw, wd + 1}];
    x4 = TUOpShrink[x3,
        {{0, cIn}, {0, kh}, {0, hOut}, {0, kw}, {0, wOut}}];
    xCol6 = TUOpPermute[x4, {0, 1, 3, 2, 4}];        (* {cIn,kh,kw,hOut,wOut} *)
    (* broadcast x over cOut, weight over (hOut,wOut); reduce cIn,kh,kw. *)
    xB = TUOpExpand[TUOpReshape[xCol6, {1, cIn, kh, kw, hOut, wOut}],
                    {cOut, cIn, kh, kw, hOut, wOut}];
    wB = TUOpExpand[TUOpReshape[weights, {cOut, cIn, kh, kw, 1, 1}],
                    {cOut, cIn, kh, kw, hOut, wOut}];
    (* SUM kw (axis 3), kh (axis 2), cIn (axis 1) -- high axis first so the
       remaining axis ids stay valid -> {cOut, hOut, wOut}. *)
    outShaped = Fold[TUOpReduce[#1, #2, "SUM"] &, TUOpMul[xB, wB], {3, 2, 1}];
    biasBcast = TUOpExpand[
        TUOpReshape[bias, {cOut, 1, 1}],
        {cOut, hOut, wOut}];
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
TConv2DIm2ColBatchedPool[inputArg_TTerm, weights_TTerm, bias_TTerm] := Module[{
    input, inShape, wShape, batch, cIn, cOut, h, wd, kh, kw, hOut, wOut,
    rh, rw, x1, x2, x3, x4, xCol6, xB, wB, out4, outShaped, biasBcast
},
    input = inputArg;
    inShape = tUopShape[input];
    wShape  = tUopShape[weights];
    {batch, cIn, h, wd} = inShape;
    cOut = wShape[[1]];
    kh   = wShape[[3]];
    kw   = wShape[[4]];
    hOut  = h  - kh + 1;
    wOut  = wd - kw + 1;
    rh = Ceiling[kh * (h + 1) / h];
    rw = Ceiling[kw * (wd + 1) / wd];
    x1 = TUOpReshape[
        TUOpExpand[TUOpReshape[input, {batch, cIn, 1, h, 1, wd}],
                   {batch, cIn, rh, h, rw, wd}],
        {batch, cIn, rh * h, rw * wd}];
    x2 = TUOpShrink[x1,
        {{0, batch}, {0, cIn}, {0, kh * (h + 1)}, {0, kw * (wd + 1)}}];
    x3 = TUOpReshape[x2, {batch, cIn, kh, h + 1, kw, wd + 1}];
    x4 = TUOpShrink[x3,
        {{0, batch}, {0, cIn}, {0, kh}, {0, hOut}, {0, kw}, {0, wOut}}];
    xCol6 = TUOpPermute[x4, {1, 2, 4, 0, 3, 5}];     (* {cIn,kh,kw,B,hOut,wOut} *)
    (* {cOut,cIn,kh,kw,B,hOut,wOut}: x over cOut, weight over (B,hOut,wOut). *)
    xB = TUOpExpand[TUOpReshape[xCol6, {1, cIn, kh, kw, batch, hOut, wOut}],
                    {cOut, cIn, kh, kw, batch, hOut, wOut}];
    wB = TUOpExpand[TUOpReshape[weights, {cOut, cIn, kh, kw, 1, 1, 1}],
                    {cOut, cIn, kh, kw, batch, hOut, wOut}];
    (* SUM kw (3), kh (2), cIn (1) -- high axis first -> {cOut,B,hOut,wOut}. *)
    out4      = Fold[TUOpReduce[#1, #2, "SUM"] &, TUOpMul[xB, wB], {3, 2, 1}];
    outShaped = TUOpPermute[out4, {1, 0, 2, 3}];           (* {B,cOut,hOut,wOut} *)
    biasBcast = TUOpExpand[
        TUOpReshape[bias, {1, cOut, 1, 1}],
        {batch, cOut, hOut, wOut}];
    outShaped + biasBcast
]

(* TConv2DIm2Col / TConv2DIm2ColBatched route to the strided-view
   `_pool` bodies above by DEFAULT: forward is bit-for-bit correct and
   backward is correct too.  The `_pool` lowering keeps the im2col
   operand a ShapeTracker VIEW chain instead of a materialised matrix --
   the dispatch pre-mat gathers it once into the matmul operand; no kh*kw
   partial-sum kernels and no giant zero-padded `repeat` intermediate.
   TConv2DKhKw stays as the explicit partial-sum reference body.
   THVM_CONV_POOL=0 forces the PAD-and-sum im2col path (escape hatch for
   bisection). *)
convPoolEnabled[] := Environment["THVM_CONV_POOL"] =!= "0"

(* TConv2DIm2Col[input, weights, bias] -- im2col + matmul lowering.
   Same input/weight/bias signature and output shape as TConv2D.

   Builds the im2col matrix `xCol : {cIn*kh*kw, hOut*wOut}` so the
   convolution becomes one MatMul:
       out_flat[cOut, p] = sum_q w_flat[cOut, q] * xCol[q, p]
   where q indexes (cIn, ki, kj) and p indexes (i_out, j_out).

   xCol slots are filled by SHRINK'ing each (ki, kj) spatial patch
   from the input and PAD'ing the patch into its kh*kw-axis slot of
   a zero {cIn, kh*kw, hOut*wOut} tensor.  Slots don't overlap, so
   summing all kh*kw padded patches (Fold[Plus]) yields xCol_3d.
   Reshape collapses the (kh*kw) axis into the cIn axis.

   The final TMatMul dispatches via cpu_blas_dispatch's MUL +
   REDUCE_SUM pattern -> cblas_sgemm.  Net effect: one sgemm per
   conv layer (vs kh*kw partial-sum kernels in TConv2D), which is
   the ceiling lift needed to scale beautiful-mnist past BS=1. *)
TConv2DIm2Col[input_TTerm, weights_TTerm, bias_TTerm] /; convPoolEnabled[] :=
    TConv2DIm2ColPool[input, weights, bias]

TConv2DIm2Col[input_TTerm, weights_TTerm, bias_TTerm] := Module[{
    inShape, wShape, cIn, cOut, h, wd, kh, kw, hOut, wOut, kSpat,
    patches, summed, xCol, wFlat, outFlat, outShaped, biasBcast
},
    inShape = tUopShape[input];
    wShape  = tUopShape[weights];
    {cIn, h, wd}      = inShape;
    cOut = wShape[[1]];
    kh   = wShape[[3]];
    kw   = wShape[[4]];
    hOut  = h  - kh + 1;
    wOut  = wd - kw + 1;
    kSpat = kh * kw;
    patches = Flatten @ Table[
        With[{slot = ki * kw + kj},
            TUOpPad[
                TUOpReshape[
                    TUOpShrink[input,
                        {{0, cIn}, {ki, ki + hOut}, {kj, kj + wOut}}],
                    {cIn, 1, hOut * wOut}],
                {{0, 0}, {slot, kSpat - 1 - slot}, {0, 0}}]
        ],
        {ki, 0, kh - 1}, {kj, 0, kw - 1}
    ];
    (* Use Fold[TUOpAdd, ...] instead of Total[]: Mathematica's
       Plus[] dispatch on n-ary TTerm args goes pathological at
       n>~16 (Orderless attribute triggers an expensive arg sort
       that runs for minutes on a 25-element list).  TUOpAdd
       direct fold builds the same left-leaning tree in 1ms. *)
    summed    = Fold[TUOpAdd, First[patches], Rest[patches]];
    xCol      = TUOpReshape[summed,  {cIn * kSpat, hOut * wOut}];
    wFlat     = TUOpReshape[weights, {cOut, cIn * kSpat}];
    outFlat   = TMatMul[wFlat, xCol];
    outShaped = TUOpReshape[outFlat, {cOut, hOut, wOut}];
    biasBcast = TUOpExpand[
        TUOpReshape[bias, {cOut, 1, 1}],
        {cOut, hOut, wOut}];
    outShaped + biasBcast
]

TConv2DIm2ColBatched[input_TTerm, weights_TTerm, bias_TTerm] /; convPoolEnabled[] :=
    TConv2DIm2ColBatchedPool[input, weights, bias]

TConv2DIm2ColBatched[input_TTerm, weights_TTerm, bias_TTerm] := Module[{
    inShape, wShape, batch, cIn, cOut, h, wd, kh, kw, hOut, wOut, kSpat,
    patches, xPatch, summed, xCol, wFlat, outFlat, outObp, outShaped,
    biasBcast
},
    inShape = tUopShape[input];
    wShape  = tUopShape[weights];
    {batch, cIn, h, wd} = inShape;
    cOut = wShape[[1]];
    kh   = wShape[[3]];
    kw   = wShape[[4]];
    hOut  = h  - kh + 1;
    wOut  = wd - kw + 1;
    kSpat = kh * kw;
    patches = Flatten @ Table[
        With[{slot = ki * kw + kj},
            xPatch = TUOpPermute[
                TUOpShrink[input,
                    {{0, batch}, {0, cIn}, {ki, ki + hOut}, {kj, kj + wOut}}],
                {1, 0, 2, 3}];
            TUOpPad[
                TUOpReshape[xPatch, {cIn, 1, batch * hOut * wOut}],
                {{0, 0}, {slot, kSpat - 1 - slot}, {0, 0}}]
        ],
        {ki, 0, kh - 1}, {kj, 0, kw - 1}
    ];
    summed    = Fold[TUOpAdd, First[patches], Rest[patches]];
    xCol      = TUOpReshape[summed,  {cIn * kSpat, batch * hOut * wOut}];
    wFlat     = TUOpReshape[weights, {cOut, cIn * kSpat}];
    outFlat   = TMatMul[wFlat, xCol];
    outObp    = TUOpReshape[outFlat, {cOut, batch, hOut, wOut}];
    outShaped = TUOpPermute[outObp, {1, 0, 2, 3}];
    biasBcast = TUOpExpand[
        TUOpReshape[bias, {1, cOut, 1, 1}],
        {batch, cOut, hOut, wOut}];
    outShaped + biasBcast
]

(* TMaxPool2d[x, k] -- non-overlapping kxk max-pool over the last
   two axes of {C, H, W}.  Reshape {C, H, W} -> {C, H/k, k, W/k, k}
   is contiguity-preserving (memory layout of {C, H, W} row-major
   IS {C, H/k, k, W/k, k} row-major when H = (H/k)*k and W likewise),
   so EXPAND/PERMUTE aren't needed -- a plain RESHAPE plus two
   REDUCE_MAX on the inserted k-axes does it.  Reduce axis 2 first
   (so axis 3 in the rank-5 shape -- the second k -- shifts down to
   axis 3 in the resulting rank-4 shape, ready for the second reduce). *)
TMaxPool2d[x_TTerm] := TMaxPool2d[x, 2]
TMaxPool2d[x_TTerm, k_Integer] := With[{shape = tUopShape[x]},
    Module[{rank, b, c, h, w},
        rank = Length[shape];
        Which[
            rank === 3,
                c = shape[[1]];
                h = shape[[2]];
                w = shape[[3]];
                TUOpReduce[
                    TUOpReduce[
                        TUOpReshape[x, {c, h/k, k, w/k, k}],
                        2, "MAX"],
                    3, "MAX"],
            rank === 4,
                b = shape[[1]];
                c = shape[[2]];
                h = shape[[3]];
                w = shape[[4]];
                TUOpReduce[
                    TUOpReduce[
                        TUOpReshape[x, {b, c, h/k, k, w/k, k}],
                        3, "MAX"],
                    4, "MAX"],
            True,
                Failure["NotImplemented",
                    <|"Message" -> "TMaxPool2d expects rank-3 or rank-4 input",
                      "InputShape" -> shape|>]
        ]
    ]
]

(* TUpsample2x[x]: nearest-neighbour 2x upsample {C, H, W} -> {C, 2H, 2W}
   via repeat-interleave: insert unit axes after H and W, EXPAND each to 2,
   merge back so every pixel becomes a contiguous 2x2 block. *)
TUpsample2x[x_TTerm] := With[{shape = tUopShape[x]},
    Module[{c, h, w},
        {c, h, w} = shape;
        TUOpReshape[
            TUOpExpand[TUOpReshape[x, {c, h, 1, w, 1}], {c, h, 2, w, 2}],
            {c, 2 h, 2 w}]
    ]
]

End[];

EndPackage[];
