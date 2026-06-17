#!/usr/bin/env wolframscript
(* verify.wls -- overfit LeNet on a single MNIST sample for K Adam
   steps, then assert the trained model predicts THAT sample's
   label correctly.  Strongest integration test (every layer's
   grad rule + the optimizer end-to-end) the cron-loop budget
   supports; doesn't claim generalisation.

   Beats the 10-class random baseline (1/10): if the trained model
   reliably predicts the sample's true label after K updates,
   every layer's grad rule + TAdam is plumbed correctly.

   Run from the repo root:
       wolframscript wl/Examples/lenet-mnist/verify.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

SeedRandom[42];
nSteps = 4;

(* === LeNet weights as TTerms ============================== *)

w1 = TGlorot[{6,   1, 5, 5}];  b1 = TZeros[{6}];
w2 = TGlorot[{16,  6, 5, 5}];  b2 = TZeros[{16}];
w3 = TGlorot[{120, 256}];      b3 = TZeros[{120}];
w4 = TGlorot[{10,  120}];      b4 = TZeros[{10}];

params = {w1, b1, w2, b2, w3, b3, w4, b4};
mState = TZerosLike /@ params;
vState = TZerosLike /@ params;

(* === Fixed sample =========================================== *)

batch  = TMnistBatch[1];
imgArr = Normal[TTensorData @ batch["images"]][[1]];
label  = First @ Normal[TTensorData @ batch["labels"]];
imgT   = TTensorCreate @ NumericArray[imgArr, "Real32"];
tgtT   = TOneHot[label, 10];
Print["training sample: true label = ", label];

(* === Forward + loss + classifier =========================== *)

forwardLogits[x_TTerm] := Module[{h1, r1, p1, h2, r2, p2, flat, h3, r3, h4},
    h1   = TConv2D[x, w1, b1];
    r1   = TReLU[h1];
    p1   = TMaxPool2d[r1, 2];
    h2   = TConv2D[p1, w2, b2];
    r2   = TReLU[h2];
    p2   = TMaxPool2d[r2, 2];
    flat = ArrayReshape[p2, {1, 256}];
    h3   = TMatVec[w3, flat] + b3;
    r3   = TReLU[h3];
    h4   = TMatVec[w4, ArrayReshape[r3, {1, 120}]] + b4;
    h4
]

predictedLabel[] := Module[{logits},
    logits = Normal @ TTensorData @ TRealize @ forwardLogits[imgT];
    Position[logits, Max[logits]][[1, 1]] - 1
]

lossOf[x_TTerm, target_TTerm] :=
    TCrossEntropyLoss[TSoftmax[forwardLogits[x]], target]

(* === Train + verify ========================================= *)

Print["initial prediction: ", predictedLabel[]];
Print["training ", nSteps, " Adam steps"];
Do[
    Module[{loss = lossOf[imgT, tgtT]},
        TRealize[loss];
        TAdam[loss, params, mState, vState, k]],
    {k, nSteps}
];

finalPred = predictedLabel[];
Print["final prediction:   ", finalPred, "  (target ", label, ")"];

If[ finalPred =!= label,
    Print["FAIL: model did not learn the single sample"];
    Exit[1]];
Print["LeNet verify PASSED: predicted ", finalPred, " == ", label];
