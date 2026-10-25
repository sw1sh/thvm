(* conv_im2col.wlt -- TConv2DIm2ColPool vs TConv2D numerical + gradient
   equivalence: the _pool view lowering the FLUX / Krea VAEs call against
   TConv2D's C body (src/nn/conv2d.c).

   The beautiful-mnist parity arc:
   im2col + matmul lowering of stride-1 / no-padding 2D conv.  Both
   forms must agree on:
     (a) forward output bit-for-bit (within f32 tolerance)
     (b) gradient w.r.t. input
     (c) gradient w.r.t. weights
     (d) gradient w.r.t. bias
   so either lowering trains the same. *)

approxEqualQ[a_, b_, tol_ : 1.0*^-4] :=
    Max @ Abs[Flatten[a - b]] <= tol

(* === Tiny case: cIn=1, cOut=2, 5x5 input, 3x3 kernel ============ *)

VerificationTest[
    TInit[];
    SeedRandom[7];
    x = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {1, 5, 5}], "Real32"];
    w = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {2, 1, 3, 3}], "Real32"];
    b = TTensorCreate @ NumericArray[{0.1, -0.2}, "Real32"];
    yA = Normal @ TTensorData @ TRealize @ TConv2D     [x, w, b];
    yB = Normal @ TTensorData @ TRealize @ TConv2DIm2ColPool[x, w, b];
    approxEqualQ[yA, yB],
    True,
    TestID -> "conv-im2col/forward-tiny"
]

(* === Multi-channel: cIn=3, cOut=4, 6x6 input, 3x3 kernel ============ *)

VerificationTest[
    TInit[];
    SeedRandom[11];
    x = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {3, 6, 6}], "Real32"];
    w = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {4, 3, 3, 3}], "Real32"];
    b = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {4}], "Real32"];
    yA = Normal @ TTensorData @ TRealize @ TConv2D     [x, w, b];
    yB = Normal @ TTensorData @ TRealize @ TConv2DIm2ColPool[x, w, b];
    approxEqualQ[yA, yB],
    True,
    TestID -> "conv-im2col/forward-multichan"
]

(* === Asymmetric kernel: kh=2, kw=3 ================================== *)

VerificationTest[
    TInit[];
    SeedRandom[13];
    x = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {2, 5, 7}], "Real32"];
    w = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {3, 2, 2, 3}], "Real32"];
    b = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
    yA = Normal @ TTensorData @ TRealize @ TConv2D     [x, w, b];
    yB = Normal @ TTensorData @ TRealize @ TConv2DIm2ColPool[x, w, b];
    approxEqualQ[yA, yB],
    True,
    TestID -> "conv-im2col/forward-asym-kernel"
]

(* === Gradient w.r.t. weights ======================================== *)

VerificationTest[
    TInit[];
    SeedRandom[17];
    x = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {2, 5, 5}], "Real32"];
    w = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {3, 2, 3, 3}], "Real32"];
    b = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
    sumOf[net_] := TUOpReduce[TUOpReduce[TUOpReduce[
        net, 0, "SUM"], 0, "SUM"], 0, "SUM"];
    gWa = Normal @ TTensorData @ TRealize @ TGrad[
        sumOf @ TConv2D     [x, w, b], w];
    gWb = Normal @ TTensorData @ TRealize @ TGrad[
        sumOf @ TConv2DIm2ColPool[x, w, b], w];
    approxEqualQ[gWa, gWb],
    True,
    TestID -> "conv-im2col/grad-w"
]

(* === Gradient w.r.t. input ========================================== *)

VerificationTest[
    TInit[];
    SeedRandom[19];
    x = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {2, 5, 5}], "Real32"];
    w = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {3, 2, 3, 3}], "Real32"];
    b = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
    sumOf[net_] := TUOpReduce[TUOpReduce[TUOpReduce[
        net, 0, "SUM"], 0, "SUM"], 0, "SUM"];
    gXa = Normal @ TTensorData @ TRealize @ TGrad[
        sumOf @ TConv2D     [x, w, b], x];
    gXb = Normal @ TTensorData @ TRealize @ TGrad[
        sumOf @ TConv2DIm2ColPool[x, w, b], x];
    approxEqualQ[gXa, gXb],
    True,
    TestID -> "conv-im2col/grad-x"
]

(* === Gradient w.r.t. bias =========================================== *)

VerificationTest[
    TInit[];
    SeedRandom[23];
    x = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {2, 5, 5}], "Real32"];
    w = TTensorCreate @ NumericArray[
            RandomReal[{-1, 1}, {3, 2, 3, 3}], "Real32"];
    b = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
    sumOf[net_] := TUOpReduce[TUOpReduce[TUOpReduce[
        net, 0, "SUM"], 0, "SUM"], 0, "SUM"];
    gBa = Normal @ TTensorData @ TRealize @ TGrad[
        sumOf @ TConv2D     [x, w, b], b];
    gBb = Normal @ TTensorData @ TRealize @ TGrad[
        sumOf @ TConv2DIm2ColPool[x, w, b], b];
    approxEqualQ[gBa, gBb],
    True,
    TestID -> "conv-im2col/grad-b"
]
