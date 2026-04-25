#!/usr/bin/env wolframscript
(* train.wls -- K manual SGD steps on the MLP-on-MNIST chain.
   Verifies that backprop through the full chain (Linear -> ReLU
   -> Linear -> Softmax + CrossEntropyLoss) actually converges:
   the loss curve must trend down on a fixed batch.

   Why not TOptim["Adam"]: Adam threads (m, v, b1pow, b2pow, k)
   through a SINGLE weight tensor's recursion.  An MLP has 4
   independent weight tensors (W1, b1, W2, b2), each needing its
   own optimizer state.  Plumbing that through TOptim is a
   separate task; for the validation milestone here, plain
   manual SGD in WL is sufficient.

   Run from the repo root:
       wolframscript -f wl/Examples/mlp-mnist/train.wls
       THVM_BACKEND=metal wolframscript -f wl/Examples/mlp-mnist/train.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

debugPrint[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

(* Deterministic seed so the loss curve is reproducible.  Glorot
   init + positive bias avoids the all-negative-h1 ReLU saturation
   that NetInitialize occasionally produces. *)
SeedRandom[42];

TInit[];

glorotInit[out_, in_] := With[{scale = Sqrt[2.0 / in]},
    NumericArray[
        RandomVariate[NormalDistribution[0., scale], {out, in}],
        "Real32"]
]

w1Host = glorotInit[32, 784];
b1Host = NumericArray[ConstantArray[0.1, 32], "Real32"];
w2Host = glorotInit[10, 32];
b2Host = NumericArray[ConstantArray[0., 10], "Real32"];

(* Fix one MNIST sample as the training "batch". *)
batch = TMnistBatch[1];
img   = Normal[TTensorData @ batch["images"]][[1]];
label = Normal[TTensorData @ batch["labels"]][[1]];
debugPrint["fixed sample: true label = ", label];

(* === Build forward + grads each iteration with current weight
   hosts.  Returns {lossScalar, {gW1, gb1, gW2, gb2}} as Normal'd
   host arrays. === *)
stepGrads[w1H_, b1H_, w2H_, b2H_] := Module[{
    W1, b1, W2, b2, x, xFlat, xRow1, h1, r1, rRow1, h2, probs,
    target, loss
},
    TInit[];
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
    {
        First @ Normal @ TTensorData @ TRealize @ loss,
        {Normal @ TTensorData @ TRealize @ TGrad[loss, W1],
         Normal @ TTensorData @ TRealize @ TGrad[loss, b1],
         Normal @ TTensorData @ TRealize @ TGrad[loss, W2],
         Normal @ TTensorData @ TRealize @ TGrad[loss, b2]}
    }
]

sgdUpdate[wH_, gradHost_, lr_] :=
    NumericArray[Normal[wH] - lr * gradHost, "Real32"]

lr      = 0.05;
nSteps  = 10;
losses  = {};

Do[
    Module[{lossVal, grads},
        {lossVal, grads} = stepGrads[w1Host, b1Host, w2Host, b2Host];
        AppendTo[losses, lossVal];
        debugPrint["  step ", k, ": loss = ", Round[lossVal, 0.0001]];
        w1Host = sgdUpdate[w1Host, grads[[1]], lr];
        b1Host = sgdUpdate[b1Host, grads[[2]], lr];
        w2Host = sgdUpdate[w2Host, grads[[3]], lr];
        b2Host = sgdUpdate[b2Host, grads[[4]], lr];
    ],
    {k, 0, nSteps - 1}
];

(* Final loss after the last update. *)
Module[{finalPair},
    finalPair = stepGrads[w1Host, b1Host, w2Host, b2Host];
    AppendTo[losses, finalPair[[1]]];
];
debugPrint["  final  : loss = ", Round[Last[losses], 0.0001]];

debugPrint["loss curve: ", Map[Round[#, 0.0001] &, losses]];

If[ Last[losses] >= First[losses],
    debugPrint["FAIL: loss did not decrease (",
               Round[First[losses], 0.0001], " -> ",
               Round[Last[losses], 0.0001], ")"];
    Exit[1]];

debugPrint["training PASSED: loss decreased ",
           Round[First[losses], 0.0001], " -> ",
           Round[Last[losses], 0.0001]];
