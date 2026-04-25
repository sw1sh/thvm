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

(* === ConvolutionLayer 2-D forward via TUOpConv2D + dispatch ===
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
    Round[Normal @ TTensorData @ TRealize @ TUOpConv2D[input, weights, bias], 0.001],
    {{{12.5, 16.5}, {24.5, 28.5}}, {{5.5, 7.5}, {11.5, 13.5}}},
    TestID -> "nn/conv2d-helper-1ch-2outch-2x2-kernel"
]

(* fromLayer dispatch path -- TFromNet[ConvolutionLayer[...], x] uses
   the same TUOpConv2D with weights/bias pulled from the layer. *)
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
