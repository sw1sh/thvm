#!/usr/bin/env wolframscript
(* train.wls -- LeNet on a fixed MNIST sample, K Adam steps in
   the in-backend lazy surface.  Every weight stays a TTerm
   handle from init through the loop; gradients come from
   TGradMany (one realize, shared forward DAG); the Adam update
   is TAdam (lazy TAssign per buffer).  No host-side NumericArray
   round-trip per step.

   Run from the repo root:
       wolframscript wl/Examples/lenet-mnist/train.wls
       N_STEPS=8 wolframscript wl/Examples/lenet-mnist/train.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

SeedRandom[42];

nSteps = With[{n = Interpreter["Integer"][Environment["N_STEPS"]]},
    If[ FailureQ[n] || n === $Failed, 4, n]
];

(* === LeNet weights, initialised once as TTerms ============= *)

w1 = TGlorot[{6,   1, 5, 5}];  b1 = TZeros[{6}];
w2 = TGlorot[{16,  6, 5, 5}];  b2 = TZeros[{16}];
w3 = TGlorot[{120, 256}];      b3 = TZeros[{120}];
w4 = TGlorot[{10,  120}];      b4 = TZeros[{10}];

params = {w1, b1, w2, b2, w3, b3, w4, b4};
mState = TZerosLike /@ params;
vState = TZerosLike /@ params;

(* === Pre-allocated input + target slots ==================== *)

batch = TMnistBatch[1];
imgArr = Normal[TTensorData @ batch["images"]][[1]];
label  = First @ Normal[TTensorData @ batch["labels"]];

imgT = TTensorCreate @ NumericArray[imgArr, "Real32"];
tgtT = TOneHot[label, 10];

print["fixed MNIST sample: true label = ", label];
print["LeNet params: ", Times @@@ Map[TTensorShape, params],
      " = ", Total[Times @@@ Map[TTensorShape, params]], " weights"];

(* === Forward + loss ======================================== *)

forwardLoss[x_TTerm, target_TTerm] := Module[{
    h1, r1, p1, h2, r2, p2, flat, h3, r3, h4, probs
},
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
    probs = TSoftmax[h4];
    TCrossEntropyLoss[probs, target]
]

(* === One step: realize loss, hand to TAdam (TGradMany internal) *)

trainStep[step_Integer] := Module[{loss, lossVal},
    loss    = forwardLoss[imgT, tgtT];
    lossVal = First @ Normal @ TTensorData @ TRealize @ loss;
    TAdam[loss, params, mState, vState, step];
    lossVal
]

print[""];
print["training ", nSteps, " steps:"];
losses = Table[
    Module[{l = trainStep[k]},
        print["  step ", k, ": loss = ", Round[l, 0.0001]];
        l],
    {k, nSteps}];

finalLoss = First @ Normal @ TTensorData @ TRealize @
                forwardLoss[imgT, tgtT];
AppendTo[losses, finalLoss];
print["  final  : loss = ", Round[finalLoss, 0.0001]];
print["loss curve: ", Map[Round[#, 0.0001] &, losses]];

If[ Last[losses] >= First[losses],
    print["FAIL: loss did not decrease (",
          Round[First[losses], 0.0001], " -> ",
          Round[Last[losses], 0.0001], ")"];
    Exit[1]];

print["LeNet training PASSED: loss ",
      Round[First[losses], 0.0001], " -> ",
      Round[Last[losses], 0.0001]];
