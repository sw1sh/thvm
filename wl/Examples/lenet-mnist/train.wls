#!/usr/bin/env wolframscript
(* train.wls -- K Adam steps on LeNet over a fixed MNIST sample.
   Uses TAdamHostStep (multi-tensor host-side Adam) to update the
   8 LeNet weight tensors per iteration.

   Run from the repo root:
       wolframscript -f wl/Examples/lenet-mnist/train.wls
       THVM_BACKEND=metal wolframscript -f wl/Examples/lenet-mnist/train.wls

   Note: each step does 1 forward + 8 backward materializations of
   a fairly deep LeNet chain.  Step count is conservative so the
   script finishes in a reasonable time.
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

debugPrint[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

SeedRandom[42];
TInit[];

(* === Glorot-style init for each LeNet weight tensor === *)
glorot[shape_List] := With[{
    fanIn = If[Length[shape] >= 2, Times @@ Drop[shape, 1], shape[[1]]]
},
    NumericArray[
        RandomVariate[NormalDistribution[0., Sqrt[2.0 / fanIn]], shape],
        "Real32"]
]
zerosArr[shape_List] := NumericArray[
    ConstantArray[0., shape], "Real32"]

w1Host = glorot[{6, 1, 5, 5}];   b1Host = zerosArr[{6}];
w2Host = glorot[{16, 6, 5, 5}];  b2Host = zerosArr[{16}];
w3Host = glorot[{120, 256}];      b3Host = zerosArr[{120}];
w4Host = glorot[{10, 120}];       b4Host = zerosArr[{10}];

(* Fix one MNIST sample. *)
batch = TMnistBatch[1];
img   = Normal[TTensorData @ batch["images"]][[1]];
label = Normal[TTensorData @ batch["labels"]][[1]];
debugPrint["fixed sample: true label = ", label];

(* === Forward LeNet inline holding tensor handles. === *)
buildLeNetForward[w1H_, b1H_, w2H_, b2H_, w3H_, b3H_, w4H_, b4H_] :=
    Module[{W1, b1, W2, b2, W3, b3, W4, b4,
            x, h1, r1, p1, h2, r2, p2,
            flat, h3, r3, h4, probs, target, loss},
        W1 = TTensorCreate @ w1H;  b1 = TTensorCreate @ b1H;
        W2 = TTensorCreate @ w2H;  b2 = TTensorCreate @ b2H;
        W3 = TTensorCreate @ w3H;  b3 = TTensorCreate @ b3H;
        W4 = TTensorCreate @ w4H;  b4 = TTensorCreate @ b4H;

        x  = TTensorCreate @ NumericArray[img, "Real32"];

        h1 = TConv2D[x, W1, b1];
        r1 = TReLU[h1];
        p1 = TUOpReduce[
                TUOpReduce[
                    TUOpReshape[r1, {6, 12, 2, 12, 2}],
                    2, "MAX"],
                3, "MAX"];

        h2 = TConv2D[p1, W2, b2];
        r2 = TReLU[h2];
        p2 = TUOpReduce[
                TUOpReduce[
                    TUOpReshape[r2, {16, 4, 2, 4, 2}],
                    2, "MAX"],
                3, "MAX"];

        flat = TUOpReshape[p2, {1, 256}];
        h3   = TUOpAdd[TMatVec[W3, flat], b3];
        r3   = TReLU[h3];
        h4   = TUOpAdd[TMatVec[W4, TUOpReshape[r3, {1, 120}]], b4];
        probs = TSoftmax[h4];

        target = TTensorCreate @ NumericArray[
            UnitVector[10, label + 1], "Real32"];
        loss = TCrossEntropyLoss[probs, target];
        {loss, {W1, b1, W2, b2, W3, b3, W4, b4}}
    ]

(* === One step: realize loss + 8 grads. === *)
stepGrads[w1H_, b1H_, w2H_, b2H_, w3H_, b3H_, w4H_, b4H_] :=
    Module[{loss, weights, lossR, grads},
        TInit[];
        {loss, weights} = buildLeNetForward[w1H, b1H, w2H, b2H,
                                             w3H, b3H, w4H, b4H];
        lossR = First @ Normal @ TTensorData @ TRealize @ loss;
        grads = Map[
            NumericArray[Normal @ TTensorData @ TRealize @ TGrad[loss, #],
                         "Real32"] &,
            weights];
        {lossR, grads}
    ]

(* === Adam state. === *)
hostsAll = {w1Host, b1Host, w2Host, b2Host,
            w3Host, b3Host, w4Host, b4Host};
{mAll, vAll} = TAdamHostInit[hostsAll];

lr     = 0.001;
beta1  = 0.9;
beta2  = 0.999;
eps    = 1.0*^-8;
nSteps = 4;
losses = {};

Do[
    Module[{lossVal, grads, hostsNew, mNew, vNew},
        {lossVal, grads} = stepGrads[
            hostsAll[[1]], hostsAll[[2]], hostsAll[[3]], hostsAll[[4]],
            hostsAll[[5]], hostsAll[[6]], hostsAll[[7]], hostsAll[[8]]];
        AppendTo[losses, lossVal];
        debugPrint["  step ", k, ": loss = ", Round[lossVal, 0.0001]];
        {hostsNew, mNew, vNew} = TAdamHostStep[hostsAll, grads, mAll, vAll,
                                                lr, beta1, beta2, eps, k + 1];
        hostsAll = hostsNew;  mAll = mNew;  vAll = vNew;
    ],
    {k, 0, nSteps - 1}
];

(* Final loss after the last update. *)
Module[{finalPair},
    finalPair = stepGrads[hostsAll[[1]], hostsAll[[2]], hostsAll[[3]],
                          hostsAll[[4]], hostsAll[[5]], hostsAll[[6]],
                          hostsAll[[7]], hostsAll[[8]]];
    AppendTo[losses, finalPair[[1]]];
];
debugPrint["  final  : loss = ", Round[Last[losses], 0.0001]];
debugPrint["loss curve: ", Map[Round[#, 0.0001] &, losses]];

If[ Last[losses] >= First[losses],
    debugPrint["FAIL: loss did not decrease (",
               Round[First[losses], 0.0001], " -> ",
               Round[Last[losses], 0.0001], ")"];
    Exit[1]];

debugPrint["LeNet training PASSED: loss ",
           Round[First[losses], 0.0001], " -> ",
           Round[Last[losses], 0.0001]];
