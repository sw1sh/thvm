(* nn.wlt -- high-level NN converter (Wolfram layers -> TUOp graph)
   plus Tensor-method helpers + autograd through compositions. *)

(* === Tensor-method helpers === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    Normal @ TTensorData[TRealize[TDot[a, b]]],
    {32.0},
    TestID -> "nn/dot"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    Normal @ TTensorData[TRealize[TSquare[a]]],
    {1.0, 4.0, 9.0},
    TestID -> "nn/square"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    Normal @ TTensorData[TRealize[TL2Loss[a]]],
    {14.0},
    TestID -> "nn/l2loss"
]

VerificationTest[
    TInit[];
    pred   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    target = TTensorCreate @ NumericArray[{1.5, 2.5, 3.5}, "Real32"];
    Normal @ TTensorData[TRealize[TMSELoss[pred, target]]],
    {0.75},
    TestID -> "nn/mse"
]

(* === gradients through pure ADD/MUL/REDUCE chains === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    Normal @ TTensorData[TRealize[TGrad[TDot[a, b], a]]],
    {4.0, 5.0, 6.0},
    TestID -> "nn/grad-dot-wrt-a"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{2.0, 3.0, 4.0}, "Real32"];
    Normal @ TTensorData[TRealize[TGrad[TL2Loss[a], a]]],
    {4.0, 6.0, 8.0},
    TestID -> "nn/grad-l2-equals-2a"
]

VerificationTest[
    TInit[];
    pred   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    target = TTensorCreate @ NumericArray[{1.5, 2.5, 3.5}, "Real32"];
    Normal @ TTensorData[TRealize[TGrad[TMSELoss[pred, target], pred]]],
    {-1.0, -1.0, -1.0},
    TestID -> "nn/grad-mse-wrt-pred"
]

(* Polynomial f(x) = x^3 + 2x^2 + x; f'(x) = 3x^2 + 4x + 1. *)
VerificationTest[
    TInit[];
    x   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    two = TTensorCreate @ NumericArray[{2.0, 2.0, 2.0}, "Real32"];
    cubic = TUOpMul[TSquare[x], x];
    quad  = TUOpMul[TSquare[x], two];
    poly  = TUOpAdd[TUOpAdd[cubic, quad], x];
    Normal @ TTensorData[TRealize[TGrad[TSum[poly], x]]],
    {8.0, 21.0, 40.0},
    TestID -> "nn/grad-cubic-poly"
]

(* === Wolfram LinearLayer forward via TFromNet === *)

VerificationTest[
    TInit[];
    layer = NetReplacePart[
        LinearLayer[3, "Input" -> 2],
        {"Weights" -> NumericArray[{{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}},     "Real32"],
         "Biases"  -> NumericArray[{10.0, 20.0, 30.0},                        "Real32"]}
    ];
    x = TTensor[{1, 2}, {7.0, 8.0}, "f32"];
    Normal @ TTensorData[TRealize[TFromNet[layer, x]]],
    {33.0, 73.0, 113.0},
    TestID -> "nn/wolfram-linear-forward"
]

(* === Wolfram ElementwiseLayer (square) via TFromNet === *)

VerificationTest[
    TInit[];
    layer = ElementwiseLayer[# # &];
    x     = TTensorCreate @ NumericArray[{2.0, 3.0, 4.0}, "Real32"];
    Normal @ TTensorData[TRealize[TFromNet[layer, x]]],
    {4.0, 9.0, 16.0},
    TestID -> "nn/wolfram-eltwise-square"
]

(* === NetChain through TFromNet ===
   Build a tiny "polynomial layer": sum_i (W_i * x_i)^2  via
   LinearLayer[1] -> ElementwiseLayer[#^2 &]. *)

VerificationTest[
    TInit[];
    chain = NetReplacePart[
        NetChain[{LinearLayer[1, "Input" -> 3], ElementwiseLayer[# # &]}],
        {{1, "Weights"} -> NumericArray[{{1.0, 1.0, 1.0}}, "Real32"],
         {1, "Biases"}  -> NumericArray[{0.0},             "Real32"]}
    ];
    x = TTensor[{1, 3}, {2.0, 3.0, 4.0}, "f32"];
    (* (1*2 + 1*3 + 1*4)^2 = 81 *)
    Normal @ TTensorData[TRealize[TFromNet[chain, x]]],
    {81.0},
    TestID -> "nn/wolfram-netchain-linear-square"
]

(* === gradient through a NetChain (square only -- LinearLayer's
   EXPAND has no grad rule yet so the matmul backward path is a TODO).
   Test: d(sum(square(W . x + b)))/d(target) via the dot-product form
   (no EXPAND in the path).  A "fake" 1-output Linear via TDot. *)

VerificationTest[
    TInit[];
    a   = TTensorCreate @ NumericArray[{2.0, 3.0, 5.0}, "Real32"];
    w   = TTensorCreate @ NumericArray[{1.0, 1.0, 1.0}, "Real32"];
    out = TSquare[TDot[w, a]];                  (* (sum(w*a))^2 *)
    (* (sum(a))^2 = (10)^2 = 100; d/da = 2*sum(a)*w = 20*{1,1,1} *)
    {Normal @ TTensorData[TRealize[out]],
     Normal @ TTensorData[TRealize[TGrad[out, a]]]},
    {{100.0}, {20.0, 20.0, 20.0}},
    TestID -> "nn/grad-through-square-of-dot"
]

(* === multi-input loss: (sum(w*x) + sum(v*x))^2 ===
   d/dx = 2*(w.x + v.x)*(w + v).  Two dot-product "heads" combined
   into a single loss; checks that the chain rule sums correctly
   across multiple paths to the same target. *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    w = TTensorCreate @ NumericArray[{1.0, 0.0, 0.0}, "Real32"];
    v = TTensorCreate @ NumericArray[{0.0, 1.0, 0.0}, "Real32"];
    loss = TSquare[TUOpAdd[TDot[w, x], TDot[v, x]]];
    (* w.x = 1, v.x = 2, sum = 3, square = 9.  d/dx = 2*3*(w+v) = 6*{1,1,0} *)
    {Normal @ TTensorData[TRealize[loss]],
     Normal @ TTensorData[TRealize[TGrad[loss, x]]]},
    {{9.0}, {6.0, 6.0, 0.0}},
    TestID -> "nn/two-head-square-loss"
]

(* === MSE through a dot product ===
   pred = w.x; loss = (pred - t)^2.  d/dw = 2*(pred-t)*x; d/dx = 2*(pred-t)*w. *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{2.0, 3.0, 4.0}, "Real32"];
    w = TTensorCreate @ NumericArray[{1.0, 1.0, 1.0}, "Real32"];
    t = TTensorCreate @ NumericArray[{5.0},           "Real32"];
    pred = TDot[w, x];                       (* 2+3+4 = 9 *)
    loss = TMSELoss[pred, t];                (* (9-5)^2 = 16 *)
    {Normal @ TTensorData[TRealize[loss]],
     Normal @ TTensorData[TRealize[TGrad[loss, w]]],
     Normal @ TTensorData[TRealize[TGrad[loss, x]]]},
    {{16.0}, {16.0, 24.0, 32.0}, {8.0, 8.0, 8.0}},
    (* d/dw = 2*4*x = {16, 24, 32};  d/dx = 2*4*w = {8, 8, 8} *)
    TestID -> "nn/mse-grad-wrt-w-and-x"
]

(* === SGD step: take one gradient step, verify loss decreased ===
   For loss = (w.x - t)^2, stepping w by -lr * d(loss)/dw must
   strictly reduce the loss for small lr.  Pure numeric end-to-end
   sanity check that the gradient direction is correct. *)

VerificationTest[
    TInit[];
    xData = NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    wData = NumericArray[{0.5, 0.5, 0.5}, "Real32"];
    tData = NumericArray[{10.0},          "Real32"];

    x = TTensorCreate @ xData;
    w = TTensorCreate @ wData;
    t = TTensorCreate @ tData;

    pred0     = TDot[w, x];                          (* 0.5*1 + 0.5*2 + 0.5*3 = 3 *)
    loss0     = TMSELoss[pred0, t];                   (* (3-10)^2 = 49 *)
    loss0Num  = First @ Normal @ TTensorData @ TRealize[loss0];
    g         = TRealize @ TGrad[loss0, w];           (* 2*(-7)*x = {-14,-28,-42} *)
    gNorm     = Normal @ TTensorData[g];

    (* Manual SGD step on host arrays, then re-evaluate loss with the
       new weights.  Re-init so the new graph starts on a fresh heap. *)
    lr     = 0.01;
    wNext  = MapThread[Subtract,
        {Normal @ wData, lr * gNorm}];
    TInit[];
    xN = TTensorCreate @ xData;
    wN = TTensorCreate @ NumericArray[wNext, "Real32"];
    tN = TTensorCreate @ tData;
    loss1 = First @ Normal @ TTensorData @
        TRealize @ TMSELoss[TDot[wN, xN], tN];

    {gNorm, loss0Num, loss1 < loss0Num},
    {{-14.0, -28.0, -42.0}, 49.0, True},
    TestID -> "nn/sgd-step-reduces-loss"
]

(* === multi-step gradient descent: loss should monotonically drop ===
   Three SGD steps on (w.x - t)^2; just check loss[i] > loss[i+1]. *)

VerificationTest[
    TInit[];
    xData = NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    tData = NumericArray[{10.0},          "Real32"];
    wHost = {0.5, 0.5, 0.5};
    lr    = 0.005;
    losses = Reap[
        Do[
            TInit[];
            x  = TTensorCreate @ xData;
            wT = TTensorCreate @ NumericArray[wHost, "Real32"];
            t  = TTensorCreate @ tData;
            loss = TMSELoss[TDot[wT, x], t];
            Sow[First @ Normal @ TTensorData @ TRealize[loss]];
            g = Normal @ TTensorData @ TRealize @ TGrad[loss, wT];
            wHost = MapThread[Subtract, {wHost, lr * g}],
            {3}
        ]
    ][[2, 1]];
    {losses[[1]] > losses[[2]], losses[[2]] > losses[[3]]},
    {True, True},
    TestID -> "nn/gd-loss-monotonically-decreases"
]

(* === polynomial regression-ish: fit y = a*x^2 + b*x against a
   sample, take one SGD step on (a, b), verify direction. ===

   loss = ((a*x^2 + b*x) - target)^2.  Pick a=1, b=1, x=2, target=10.
       pred  = 1*4 + 1*2 = 6
       err   = pred - target = -4
       loss  = 16
       d/da  = 2 * err * x^2 = 2 * -4 * 4 = -32
       d/db  = 2 * err * x   = 2 * -4 * 2 = -16
*)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{1.0}, "Real32"];
    x = TTensorCreate @ NumericArray[{2.0}, "Real32"];
    t = TTensorCreate @ NumericArray[{10.0}, "Real32"];
    pred = TUOpAdd[TUOpMul[a, TSquare[x]], TUOpMul[b, x]];
    loss = TMSELoss[pred, t];
    {Normal @ TTensorData @ TRealize[loss],
     Normal @ TTensorData @ TRealize[TGrad[loss, a]],
     Normal @ TTensorData @ TRealize[TGrad[loss, b]]},
    {{16.0}, {-32.0}, {-16.0}},
    TestID -> "nn/poly-regression-gradients"
]

(* === ReLU forward (TReLU helper + ElementwiseLayer[Ramp] dispatch) === *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{-2.0, -0.5, 0.0, 1.5, 3.0}, "Real32"];
    Normal @ TTensorData @ TRealize[TReLU[x]],
    {0.0, 0.0, 0.0, 1.5, 3.0},
    TestID -> "nn/relu-forward-helper"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{-2.0, -0.5, 0.0, 1.5, 3.0}, "Real32"];
    Normal @ TTensorData @ TRealize @ TFromNet[ElementwiseLayer[Ramp], x],
    {0.0, 0.0, 0.0, 1.5, 3.0},
    TestID -> "nn/relu-via-ElementwiseLayer-Ramp"
]

(* === Tanh forward (TTanh helper + ElementwiseLayer[Tanh] dispatch) ===
   Reference values from Mathematica:
     tanh(-2) = -0.9640275801
     tanh(-1) = -0.7615941560
     tanh(0)  =  0.0
     tanh(1)  =  0.7615941560
     tanh(2)  =  0.9640275801
   The exp-via-EXP2 chain loses some f32 precision; tolerate ~1e-4. *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{-2.0, -1.0, 0.0, 1.0, 2.0}, "Real32"];
    Round[Normal @ TTensorData @ TRealize @ TTanh[x], 0.0001],
    Round[{-0.9640275801, -0.7615941560, 0.0, 0.7615941560, 0.9640275801}, 0.0001],
    TestID -> "nn/tanh-forward-helper"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{-1.0, 0.0, 1.0}, "Real32"];
    Round[Normal @ TTensorData @ TRealize @
        TFromNet[ElementwiseLayer[Tanh], x], 0.0001],
    Round[{-0.7615941560, 0.0, 0.7615941560}, 0.0001],
    TestID -> "nn/tanh-via-ElementwiseLayer-Tanh"
]

(* === ReshapeLayer forward via TFromNet === *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, "Real32"];
    r = TRealize @ TFromNet[ReshapeLayer[{2, 3}], x];
    {TTensorShape[r], Normal @ TTensorData[r]},
    {{2, 3}, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}},
    TestID -> "nn/reshape-layer-1d-to-2d"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}}, "Real32"];
    r = TRealize @ TFromNet[ReshapeLayer[{6}], x];
    {TTensorShape[r], Normal @ TTensorData[r]},
    {{6}, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}},
    TestID -> "nn/reshape-layer-2d-to-1d-flatten"
]

(* === FlattenLayer forward via TFromNet === *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}, "Real32"];
    r = TRealize @ TFromNet[FlattenLayer[], x];
    {TTensorShape[r], Normal @ TTensorData[r]},
    {{6}, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}},
    TestID -> "nn/flatten-layer-2d-to-1d"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    r = TRealize @ TFromNet[FlattenLayer[], x];
    {TTensorShape[r], Normal @ TTensorData[r]},
    {{4}, {1.0, 2.0, 3.0, 4.0}},
    TestID -> "nn/flatten-layer-1d-passthrough"
]

(* === PoolingLayer Max non-overlap (Stride == KernelSize) === *)

VerificationTest[
    TInit[];
    data = ArrayReshape[Range[16] * 1.0, {1, 4, 4}];
    x = TTensorCreate @ NumericArray[data, "Real32"];
    r = TRealize @ TFromNet[
        PoolingLayer[{2, 2}, "Stride" -> {2, 2}, "Function" -> Max], x];
    {TTensorShape[r], Normal @ TTensorData[r]},
    {{1, 2, 2}, {{{6.0, 8.0}, {14.0, 16.0}}}},
    TestID -> "nn/pool-max-2x2-non-overlap-1ch"
]

VerificationTest[
    TInit[];
    (* 2 channels, 2x4 input.  2x2 pool with 2x2 stride -> 1x2 output. *)
    data = {{{1.0, 2.0, 3.0, 4.0}, {5.0, 6.0, 7.0, 8.0}},
            {{9.0, 10.0, 11.0, 12.0}, {13.0, 14.0, 15.0, 16.0}}};
    x = TTensorCreate @ NumericArray[data, "Real32"];
    r = TRealize @ TFromNet[
        PoolingLayer[{2, 2}, "Stride" -> {2, 2}, "Function" -> Max], x];
    {TTensorShape[r], Normal @ TTensorData[r]},
    (* ch0: max{1,2,5,6}=6, max{3,4,7,8}=8 -> {{6, 8}}
       ch1: max{9,10,13,14}=14, max{11,12,15,16}=16 -> {{14, 16}} *)
    {{2, 1, 2}, {{{6.0, 8.0}}, {{14.0, 16.0}}}},
    TestID -> "nn/pool-max-2x2-non-overlap-multichannel"
]

(* Refusal cases -- documented surface so callers see the gap clearly. *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{{{1.0, 2.0}, {3.0, 4.0}}}, "Real32"];
    Head @ TFromNet[
        PoolingLayer[{2, 2}, "Stride" -> {1, 1}, "Function" -> Max], x],
    Failure,
    TestID -> "nn/pool-overlapping-returns-Failure"
]

(* === SoftmaxLayer forward (TSoftmax helper + dispatch) ===
   softmax({1,2,3}) ≈ {0.0900306, 0.244728, 0.665241}.  Tolerance
   to 0.0001 swallows the EXP2/RECIP precision wobble. *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    Round[Normal @ TTensorData @ TRealize @ TSoftmax[x], 0.0001],
    Round[{0.0900306, 0.244728, 0.665241}, 0.0001],
    TestID -> "nn/softmax-helper-3-elements"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    Round[Normal @ TTensorData @ TRealize @
        TFromNet[SoftmaxLayer[], x], 0.0001],
    Round[{0.0900306, 0.244728, 0.665241}, 0.0001],
    TestID -> "nn/softmax-via-SoftmaxLayer"
]

(* Sanity: outputs sum to 1. *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{0.5, -1.0, 2.0, 0.0}, "Real32"];
    Round[Total @ Normal @ TTensorData @ TRealize @ TSoftmax[x], 0.0001],
    1.0,
    TestID -> "nn/softmax-sums-to-one"
]

(* === TLog + TCrossEntropyLoss === *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, E*1.0, E^2*1.0}, "Real32"];
    Round[Normal @ TTensorData @ TRealize @ TLog[x], 0.0001],
    Round[{0.0, 1.0, 2.0}, 0.0001],
    TestID -> "nn/log-natural-base"
]

(* CE with one-hot target: loss = -log(pred[true_class]). *)
VerificationTest[
    TInit[];
    pred = TTensorCreate @ NumericArray[{0.1, 0.2, 0.7}, "Real32"];
    tgt  = TTensorCreate @ NumericArray[{0.0, 0.0, 1.0}, "Real32"];
    Round[Normal @ TTensorData @ TRealize @
        TCrossEntropyLoss[pred, tgt], 0.0001],
    {Round[-Log[0.7], 0.0001]},
    TestID -> "nn/ce-loss-one-hot-correct-class"
]

(* CE with uniform target: loss = -mean(log(pred)) * n_classes... no,
   actually -sum(target * log(pred)) where target = {1/3, 1/3, 1/3}
   over predictions {0.5, 0.25, 0.25} = -(1/3)(log .5 + log .25 + log .25). *)
VerificationTest[
    TInit[];
    pred = TTensorCreate @ NumericArray[{0.5, 0.25, 0.25}, "Real32"];
    tgt  = TTensorCreate @ NumericArray[{1.0/3, 1.0/3, 1.0/3}, "Real32"];
    Round[Normal @ TTensorData @ TRealize @
        TCrossEntropyLoss[pred, tgt], 0.0001],
    {Round[-(Log[0.5] + Log[0.25] + Log[0.25])/3, 0.0001]},
    TestID -> "nn/ce-loss-uniform-target"
]

(* === ConvolutionLayer 2-D forward via TConv2D + dispatch ===
   Hand-verified on a 1-channel 3x3 input, 2 output channels, 2x2
   kernel (sum + diagonal masks).  NetApply cross-checks would be
   nice but the local Wolfram NeuralNetworks runtime errors with
   NetChain::badbackend, so the numeric reference here is a
   hand-derived expected matrix instead. *)

VerificationTest[
    TInit[];
    input = TTensorCreate @ NumericArray[
        {{{1.0,2.0,3.0},{4.0,5.0,6.0},{7.0,8.0,9.0}}}, "Real32"];
    weights = TTensorCreate @ NumericArray[
        {{{{1.,1.},{1.,1.}}}, {{{1.,0.},{0.,1.}}}}, "Real32"];
    bias = TTensorCreate @ NumericArray[{0.5, -0.5}, "Real32"];
    Round[Normal @ TTensorData @ TRealize @ TConv2D[input, weights, bias], 0.001],
    {{{12.5, 16.5}, {24.5, 28.5}}, {{5.5, 7.5}, {11.5, 13.5}}},
    TestID -> "nn/conv2d-helper-1ch-2outch-2x2-kernel"
]

(* === TConv2D: kh*kw-unrolled chain of primitives === *)
(* Sub-item (a) of the conv2d-lowering arc: forward parity with the
   bespoke TConv2D for the same hand-verified case above. *)

VerificationTest[
    TInit[];
    input = TTensorCreate @ NumericArray[
        {{{1.0,2.0,3.0},{4.0,5.0,6.0},{7.0,8.0,9.0}}}, "Real32"];
    weights = TTensorCreate @ NumericArray[
        {{{{1.,1.},{1.,1.}}}, {{{1.,0.},{0.,1.}}}}, "Real32"];
    bias = TTensorCreate @ NumericArray[{0.5, -0.5}, "Real32"];
    Round[Normal @ TTensorData @ TRealize @ TConv2D[input, weights, bias],
          0.001],
    {{{12.5, 16.5}, {24.5, 28.5}}, {{5.5, 7.5}, {11.5, 13.5}}},
    TestID -> "nn/conv2d-lowered-1ch-2outch-2x2-parity"
]

(* fromLayer dispatch path -- TFromNet[ConvolutionLayer[...], x] uses
   the same TConv2D with weights/bias pulled from the layer. *)
VerificationTest[
    TInit[];
    Module[{net, conv, x},
        net = NetInitialize @ NetChain[
            {ConvolutionLayer[2, {2, 2}, "Stride" -> {1, 1}]},
            "Input" -> {1, 3, 3}];
        conv = net[[1]];
        x = TTensorCreate @ NumericArray[
            {{{1.0,2.0,3.0},{4.0,5.0,6.0},{7.0,8.0,9.0}}}, "Real32"];
        TTensorShape @ TRealize @ TFromNet[conv, x]
    ],
    {2, 2, 2},
    TestID -> "nn/conv2d-dispatch-shape-correct"
]

(* Refusal cases. *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[
        {{{1.0,2.0},{3.0,4.0}}}, "Real32"];
    Head @ TFromNet[
        ConvolutionLayer[1, {2, 2}, "Stride" -> {2, 2}], x],
    Failure,
    TestID -> "nn/conv2d-non-1-stride-returns-Failure"
]

(* === NetModel["LeNet"] -- canonical LeNet architecture from
       Mathematica's network model registry, NetInitialize'd
       to give it concrete weights. === *)

VerificationTest[
    TInit[];
    net = NetInitialize @ Quiet[NetModel["LeNet"], Import::nnincmpb];
    {Length[net], Head /@ Table[net[[i]], {i, Length[net]}]},
    {11, {ConvolutionLayer, ElementwiseLayer, PoolingLayer,
          ConvolutionLayer, ElementwiseLayer, PoolingLayer,
          FlattenLayer, LinearLayer, ElementwiseLayer,
          LinearLayer, SoftmaxLayer}},
    TestID -> "nn/lenet-architecture-11-layers"
]

VerificationTest[
    TInit[];
    net = NetInitialize @ Quiet[NetModel["LeNet"], Import::nnincmpb];
    Head @ NetExtract[net, {1, "Weights"}],
    NumericArray,
    TestID -> "nn/lenet-conv1-weights-concrete"
]

(* === LeNet end-to-end forward === *)

VerificationTest[
    TInit[];
    net = NetInitialize @ Quiet[NetModel["LeNet"], Import::nnincmpb];
    x = TTensorCreate @ NumericArray[ConstantArray[0.5, {1, 28, 28}], "Real32"];
    res = TRealize @ TFromNet[net, x];
    {TTensorShape[res], Round[Total @ Normal @ TTensorData[res], 0.0001]},
    {{10}, 1.0},
    TestID -> "nn/lenet-end-to-end-forward-shape-and-softmax-sum"
]

(* === Phase-5 NN building blocks ===
   TMaxPool2d / TLayerNorm / TSoftmaxAxis / TAttention.  Pin both
   the numerics and the kernel-count claim so the scheduler can't
   silently regress fusion. *)

(* TMaxPool2d 2x2 on a {1, 4, 4} channels-first input.  Memory layout
   reshape {1,4,4} -> {1,2,2,2,2} is contiguity-preserving, then two
   REDUCE_MAX (one per inserted k-axis) collapse to {1,2,2}. *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[N @ {Partition[Range[16], 4]}, "Real32"];
    r = TRealize @ TMaxPool2d[x, 2];
    {TTensorShape[r], Normal @ TTensorData[r]},
    {{1, 2, 2}, {{{6., 8.}, {14., 16.}}}},
    TestID -> "nn/maxpool2d-2x2-on-1x4x4"
]

(* TLayerNorm on a rank-1 {N} input: zero-mean, unit-variance output.
   Two REDUCEs (mean + variance) prevent the Phase-3 single-REDUCE
   fusion from collapsing the whole graph; the variance + normalise
   tail still fuses through the broadcast-chain relaxation. *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    r = TRealize @ TLayerNorm[x];
    Normal @ TTensorData[r],
    {-1.34164, -0.447212, 0.447212, 1.34164},
    SameTest -> (Max[Abs[#1 - #2]] < 1.0*^-4 &),
    TestID -> "nn/layernorm-rank1-zero-mean-unit-var"
]

(* TSoftmaxAxis along the last axis of a rank-2 input.  Each row
   independently sums to 1.  Same KProgOp[] shape as the row-wise
   softmax inside attention. *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[N @ {{1, 2, 3}, {0, 0, 0}}, "Real32"];
    r = TRealize @ TSoftmaxAxis[x, 1];
    Total /@ Normal @ TTensorData[r],
    {1.0, 1.0},
    SameTest -> (Max[Abs[#1 - #2]] < 1.0*^-4 &),
    TestID -> "nn/softmaxaxis-rows-sum-to-one"
]

(* TAttention with an identity-like Q against an axis-aligned K
   selects rows of V softly via the scaled scores.  The exact values
   are computed offline (scores = Q@K^T / sqrt(4) -> softmax over the
   last axis -> @ V); pin them so a regression in TMatMul or the
   axis-1 softmax broadcast pattern is caught. *)
VerificationTest[
    TInit[];
    q = TTensorCreate @ NumericArray[N @ {{1, 0, 0, 0}, {0, 1, 0, 0}}, "Real32"];
    k = TTensorCreate @ NumericArray[N @ {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}, "Real32"];
    v = TTensorCreate @ NumericArray[N @ {{0.1, 0.2}, {0.3, 0.4}, {0.5, 0.6}}, "Real32"];
    r = TRealize @ TAttention[q, k, v];
    {TTensorShape[r], Normal @ TTensorData[r]},
    {{2, 2}, {{0.264441, 0.364441}, {0.300000, 0.400000}}},
    SameTest -> (#1[[1]] === #2[[1]]
              && Max[Abs[Flatten[#1[[2]] - #2[[2]]]]] < 1.0*^-4 &),
    TestID -> "nn/attention-identity-q-row-selection"
]

(* === Phase-6 NN building blocks ===
   TBatchNorm / TCategoricalCrossEntropy / TSparseCategoricalCrossEntropy
   / TAdam (TAssign-form). *)

VerificationTest[
    TInit[];
    c = 2; h = 3; w = 3;
    xData  = N @ Table[ic + 0.1 (ih w + iw), {ic, c}, {ih, h}, {iw, w}];
    gammaH = N @ {2.0, 1.5};
    betaH  = N @ {0.1, -0.2};
    meanH  = N @ {1.5, 2.5};
    varH   = N @ {0.5, 0.7};
    x = TTensorCreate @ NumericArray[xData,  "Real32"];
    g = TTensorCreate @ NumericArray[gammaH, "Real32"];
    b = TTensorCreate @ NumericArray[betaH,  "Real32"];
    mu  = TTensorCreate @ NumericArray[meanH, "Real32"];
    sg2 = TTensorCreate @ NumericArray[varH,  "Real32"];
    r   = TRealize @ TBatchNorm[x, g, b, mu, sg2, 1.0*^-5];
    ref = Table[
        gammaH[[ic]] (xData[[ic, ih, iw]] - meanH[[ic]]) / Sqrt[varH[[ic]] + 1.0*^-5]
            + betaH[[ic]],
        {ic, c}, {ih, h}, {iw, w}];
    {TTensorShape[r], Max @ Abs @ Flatten[Normal @ TTensorData[r] - ref]},
    {{c, h, w}, _ ? (# < 1.0*^-5 &)},
    SameTest -> MatchQ,
    TestID -> "nn/batchnorm-rank3-matches-formula"
]

VerificationTest[
    TInit[];
    logitsH = N @ {1.0, 2.0, 3.0};
    targetH = N @ {0.0, 1.0, 0.0};       (* one-hot at index 1 *)
    logits  = TTensorCreate @ NumericArray[logitsH, "Real32"];
    target  = TTensorCreate @ NumericArray[targetH, "Real32"];
    loss    = First @ Normal @ TTensorData @ TRealize @
                TCategoricalCrossEntropy[logits, target];
    expected = Log[Total[Exp[logitsH]]] - logitsH[[2]];
    Abs[loss - expected],
    _ ? (# < 1.0*^-5 &),
    SameTest -> MatchQ,
    TestID -> "nn/categorical-ce-rank1-matches-logsumexp"
]

VerificationTest[
    TInit[];
    logitsH  = N @ {1.0, 2.0, 3.0};
    logits   = TTensorCreate @ NumericArray[logitsH, "Real32"];
    labels   = TTensorCreate @ NumericArray[{1}, "Integer32"];
    loss     = First @ Normal @ TTensorData @ TRealize @
                 TSparseCategoricalCrossEntropy[logits, labels];
    expected = Log[Total[Exp[logitsH]]] - logitsH[[2]];
    Abs[loss - expected],
    _ ? (# < 1.0*^-5 &),
    SameTest -> MatchQ,
    TestID -> "nn/sparse-ce-rank1-int-label"
]

VerificationTest[
    TInit[];
    logitsH  = N @ {{1, 2, 3}, {2, 1, 0}};
    labelsH  = {1, 0};              (* int class indices, 0-based *)
    logits   = TTensorCreate @ NumericArray[logitsH, "Real32"];
    labels   = TTensorCreate @ NumericArray[labelsH, "Integer32"];
    loss     = First @ Normal @ TTensorData @ TRealize @
                 TSparseCategoricalCrossEntropy[logits, labels];
    perRow   = Table[
        Log[Total[Exp[logitsH[[i]]]]] - logitsH[[i, labelsH[[i]] + 1]],
        {i, Length[logitsH]}];
    Abs[loss - Mean[perRow]],
    _ ? (# < 1.0*^-5 &),
    SameTest -> MatchQ,
    TestID -> "nn/sparse-ce-rank2-int-labels-batch-mean"
]

VerificationTest[
    TInit[];
    xH = N @ {
        {{{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}}},
        {{{2, 1, 0, -1}, {3, 2, 1, 0}, {4, 3, 2, 1}, {5, 4, 3, 2}}}
    };
    wH = N @ {
        {{{1, 0}, {0, 1}}},
        {{{1, -1}, {1, -1}}}
    };
    bH = N @ {0.5, -1.0};
    xb = TTensorCreate @ NumericArray[xH, "Real32"];
    w  = TTensorCreate @ NumericArray[wH, "Real32"];
    b  = TTensorCreate @ NumericArray[bH, "Real32"];
    yb = Normal @ TTensorData @ TRealize @ TConv2D[xb, w, b];
    y0 = Normal @ TTensorData @ TRealize @
        TConv2D[TTensorCreate @ NumericArray[xH[[1]], "Real32"], w, b];
    y1 = Normal @ TTensorData @ TRealize @
        TConv2D[TTensorCreate @ NumericArray[xH[[2]], "Real32"], w, b];
    Max @ Abs @ Flatten[yb - {y0, y1}],
    _ ? (# < 1.0*^-5 &),
    SameTest -> MatchQ,
    TestID -> "nn/conv2d-rank4-batch-matches-rank3-slices"
]

(* Conv weight-gradient FD-parity guard.  The rank-changing reshape
   inside TConv2DIm2ColBatchedPool's matmul lowering used to silently
   drop cOut differentiation: gW[cOut=1] came back identical to
   gW[cOut=0].  Root cause was in kernel_lift's S_RESHAPE_V flat-idx
   builder, which read out_ext = 0 for expression-emitted size-1
   axis refs (S_IMOD(0,1)) and let that 0 poison every UPSTREAM
   axis's stride product (stride = product of out_ext[e] for e > d),
   silently dropping those axes' contributions from flat_idx.  Fixed
   by falling back to scalar_ref_extent on non-S_RANGE out refs. *)
VerificationTest[
    TInit[];
    SeedRandom[42];
    xH = N @ RandomReal[{-1, 1}, {2, 1, 5, 5}];
    wH = N @ RandomVariate[NormalDistribution[0., 0.5], {2, 1, 3, 3}];
    bH = N @ RandomReal[{-0.2, 0.2}, {2}];
    tH = N @ RandomReal[{-1, 1}, {2, 2, 3, 3}];
    fwdLoss[wD_] := Module[{x, w, b, t, diff},
        TInit[];
        x = TTensorCreate @ NumericArray[xH, "Real32"];
        w = TTensorCreate @ NumericArray[wD, "Real32"];
        b = TTensorCreate @ NumericArray[bH, "Real32"];
        t = TTensorCreate @ NumericArray[tH, "Real32"];
        diff = TConv2D[x, w, b] - t;
        First @ Normal @ TTensorData @ TRealize @ TUOpReduce[TUOpReduce[
            TUOpReduce[TUOpReduce[diff * diff, 3, "SUM"], 2, "SUM"],
            1, "SUM"], 0, "SUM"]];
    TInit[];
    xT = TTensorCreate @ NumericArray[xH, "Real32"];
    wT = TTensorCreate @ NumericArray[wH, "Real32"];
    bT = TTensorCreate @ NumericArray[bH, "Real32"];
    tT = TTensorCreate @ NumericArray[tH, "Real32"];
    diff = TConv2D[xT, wT, bT] - tT;
    loss = TUOpReduce[TUOpReduce[TUOpReduce[TUOpReduce[diff * diff,
        3, "SUM"], 2, "SUM"], 1, "SUM"], 0, "SUM"];
    gW = Normal @ TTensorData @ TRealize @ TGrad[loss, wT];
    fdGW = Table[
        (fwdLoss[ReplacePart[wH, {co, ci, ki, kj} -> wH[[co, ci, ki, kj]] + 0.001]]
       - fwdLoss[ReplacePart[wH, {co, ci, ki, kj} -> wH[[co, ci, ki, kj]] - 0.001]]) / 0.002,
        {co, 2}, {ci, 1}, {ki, 3}, {kj, 3}];
    Max @ Abs @ Flatten[gW - fdGW],
    _ ? (# < 0.01 &),
    SameTest -> MatchQ,
    TestID -> "nn/conv2d-grad-w-matches-finite-difference"
]

VerificationTest[
    TInit[];
    xH = N @ {
        {{{1, 5, 2, 4}, {7, 3, 8, 6}, {0, 9, 1, 2}, {4, 6, 3, 5}}},
        {{{-1, -2, -3, -4}, {4, 3, 2, 1}, {8, 7, 6, 5}, {0, 1, 2, 3}}}
    };
    x = TTensorCreate @ NumericArray[xH, "Real32"];
    y = Normal @ TTensorData @ TRealize @ TMaxPool2d[x, 2];
    y,
    N @ {
        {{{7, 8}, {9, 5}}},
        {{{4, 2}, {8, 6}}}
    },
    TestID -> "nn/maxpool2d-rank4-batch"
]

VerificationTest[
    TInit[];
    xH = N @ {
        {{{1, 2}, {3, 4}}, {{2, 4}, {6, 8}}},
        {{{5, 6}, {7, 8}}, {{1, 3}, {5, 7}}}
    };
    x = TTensorCreate @ NumericArray[xH, "Real32"];
    g = TTensorCreate @ NumericArray[{1.0, 1.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{0.0, 0.0}, "Real32"];
    y = Normal @ TTensorData @ TRealize @ TBatchNormTrain[x, g, b, 0.0];
    perChan = Table[Flatten[y[[All, c, All, All]]], {c, 2}];
    Max @ Abs @ Flatten @ Table[
        {Mean[perChan[[c]]], Mean[perChan[[c]]^2] - 1.0},
        {c, 2}],
    _ ? (# < 1.0*^-4 &),
    SameTest -> MatchQ,
    TestID -> "nn/batchnorm-train-rank4-normalizes-channel-stats"
]

VerificationTest[
    TInit[];
    logitsH = N @ {{1, 2, 3}, {2, 1, 0}};
    targetsH = N @ {{0, 1, 0}, {1, 0, 0}};
    logits   = TTensorCreate @ NumericArray[logitsH, "Real32"];
    target   = TTensorCreate @ NumericArray[targetsH, "Real32"];
    loss     = First @ Normal @ TTensorData @ TRealize @
                 TCategoricalCrossEntropy[logits, target];
    perRow   = Table[Log[Total[Exp[logitsH[[i]]]]] - logitsH[[i]] . targetsH[[i]],
                     {i, Length[logitsH]}];
    Abs[loss - Mean[perRow]],
    _ ? (# < 1.0*^-5 &),
    SameTest -> MatchQ,
    TestID -> "nn/categorical-ce-rank2-batch-mean"
]

(* In-backend TAdam at lr=0.001, beta1=0.9, beta2=0.999, against
   loss = (w - 0.05)^2 -- a degenerate per-element regression whose
   gradient is 2(w - 0.05) = 2(1 - 0.05) = 1.9 at w = 1.
       grad   = 1.9
       m_new  = 0.9 * 0 + 0.1 * 1.9       = 0.19
       v_new  = 0.999 * 0 + 0.001 * 1.9^2 = 0.00361
       m_hat  = 0.19 / (1 - 0.9)          = 1.9
       v_hat  = 0.00361 / (1 - 0.999)     = 3.61
       w_new  = 1 - 0.001 * 1.9 / (sqrt(3.61) + 1e-8) = 0.999
   Previously failing as `pending_adam_dp1.wlt`: TAdam's v-update
   `(1-beta2) * (gTen * gTen)` lowered through WL's `Times` UpValue
   which dedups the duplicate `gTen` factor and emits an IC-level
   OP2 instead of a UOP node, and `OP2(DP1_x, DP1_x)` (from the
   chain-rule grad) had no IC reduction rule -- v silently stayed at
   zero.  Fix: TAdam writes `gTen^2` instead of `gTen * gTen`, which
   routes through Power's UpValue to a TUOpMul call that yields a
   proper TAG_UOP. *)
VerificationTest[
    TInit[];
    w   = TTensorCreate @ NumericArray[{1.0}, "Real32"];
    tgt = TTensorCreate @ NumericArray[{0.05}, "Real32"];
    m   = TTensorCreate @ NumericArray[{0.0}, "Real32"];
    v   = TTensorCreate @ NumericArray[{0.0}, "Real32"];
    loss = TL2Loss[w - tgt];
    TAdam[loss, {w}, {m}, {v}, 1];
    Max @ Abs @ Flatten @ {
        Normal @ TTensorData[w] - {0.999},
        Normal @ TTensorData[m] - {0.19},
        Normal @ TTensorData[v] - {0.00361}
    },
    _ ? (# < 1.0*^-5 &),
    SameTest -> MatchQ,
    TestID -> "nn/adam-rank1-l2-loss-matches-textbook"
]
