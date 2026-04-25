#!/usr/bin/env wolframscript
(* grad-check.wls -- per-weight gradient sanity check for the
   MLP-on-MNIST chain.  Builds the forward pass inline so we hold
   handles to each weight tensor (W1, b1, W2, b2), computes
   TGrad[loss, W] for each, materializes each, and asserts:

     - gradient is finite (no NaN, no Inf)
     - gradient shape matches the weight shape

   Pure structural sanity; no parameter updates.  This is the gate
   between the forward smoke test and the actual training loop.

   Run from the repo root:
       wolframscript -f wl/Examples/mlp-mnist/grad-check.wls
       THVM_BACKEND=metal wolframscript -f wl/Examples/mlp-mnist/grad-check.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

debugPrint[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

(* Build the forward pass inline holding tensor handles, then realise
   TGrad[loss, w] for each weight.  Returns an Association keyed by
   the weight's name with the realised gradient TTerm. *)
runOnce[label_Integer, img_] := Module[{
    net, w1H, b1H, w2H, b2H,
    W1, b1, W2, b2, x, xFlat, xRow1, h1, r1, rRow1, h2, probs,
    target, loss
},
    TInit[];
    net = NetInitialize @ NetChain[{
        FlattenLayer[],
        LinearLayer[32], ElementwiseLayer[Ramp],
        LinearLayer[10], SoftmaxLayer[]
    }, "Input" -> {1, 28, 28}];

    {w1H, b1H} = TLayerWeights[net[[2]]];
    {w2H, b2H} = TLayerWeights[net[[4]]];

    W1 = TTensorCreate @ w1H;
    b1 = TTensorCreate @ b1H;
    W2 = TTensorCreate @ w2H;
    b2 = TTensorCreate @ b2H;

    x      = TTensorCreate @ NumericArray[img, "Real32"];
    xFlat  = TUOpReshape[x, {784}];
    xRow1  = TUOpReshape[xFlat, {1, 784}];
    h1     = TUOpAdd[TMatVec[W1, xRow1], b1];
    r1     = TReLU[h1];
    rRow1  = TUOpReshape[r1, {1, 32}];
    h2     = TUOpAdd[TMatVec[W2, rRow1], b2];
    probs  = TSoftmax[h2];
    target = TTensorCreate @ NumericArray[
        UnitVector[10, label + 1], "Real32"];
    loss   = TCrossEntropyLoss[probs, target];

    <|
      "W1" -> {W1, TRealize @ TGrad[loss, W1]},
      "b1" -> {b1, TRealize @ TGrad[loss, b1]},
      "W2" -> {W2, TRealize @ TGrad[loss, W2]},
      "b2" -> {b2, TRealize @ TGrad[loss, b2]}
    |>
]

batch = TMnistBatch[1];
img   = Normal @ TTensorData @ batch["images"];
label = Normal[TTensorData @ batch["labels"]][[1]];
debugPrint["MNIST sample: true label = ", label];

grads = runOnce[label, img[[1]]];

flatHost[t_TTerm] := Flatten @ Normal @ TTensorData @ t

KeyValueMap[Function[{name, pair},
    Module[{w, g, wShape, gShape, host},
        {w, g}  = pair;
        wShape  = TTensorShape[w];
        gShape  = TTensorShape[g];
        host    = flatHost[g];
        debugPrint["  ", name, ": shape=", gShape,
                   " (expected ", wShape, ")",
                   "  min=", Round[Min[host], 0.0001],
                   "  max=", Round[Max[host], 0.0001]];
        If[ gShape =!= wShape,
            debugPrint["    FAIL: shape mismatch"]; Exit[1]];
        If[ ! AllTrue[host, NumericQ],
            debugPrint["    FAIL: gradient contains non-numeric values"];
            Exit[1]];
        If[ AnyTrue[host, (Abs[#] > 1.0*^10) &],
            debugPrint["    FAIL: gradient contains NaN/Inf or huge values"];
            Exit[1]];
    ]],
    grads
];

debugPrint["grad-check PASSED."];
debugPrint["NOTE: random NetInitialize occasionally produces"];
debugPrint["      h1 < 0 for every hidden unit; ReLU then masks"];
debugPrint["      every upstream gradient to zero (mathematically"];
debugPrint["      correct -- d(ReLU)/dh = mask = 0).  b2's grad"];
debugPrint["      stays non-zero because it sits before the"];
debugPrint["      ReLU in the backward chain.  Re-run to draw"];
debugPrint["      a different init for non-zero W1/b1/W2 grads."];
