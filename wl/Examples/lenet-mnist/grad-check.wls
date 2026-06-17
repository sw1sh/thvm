#!/usr/bin/env wolframscript
(* grad-check.wls -- LeNet end-to-end forward + grad smoke test.
   Verifies that the FULL chain (Conv -> ReLU -> MaxPool -> Conv ->
   ReLU -> MaxPool -> Flatten -> Linear -> ReLU -> Linear -> Softmax
   + CrossEntropyLoss) materializes correctly forward AND that
   TGrad[loss, input] produces a finite, correctly-shaped gradient.

   Why grad-wrt-input rather than grad-wrt-each-weight: a single
   TGrad call exercises the entire backward chain (CONV2D
   grad_input, Pool grad via REDUCE_MAX, ReLU grad via CMPLT/MUL,
   Linear grad via MUL+REDUCE_SUM+EXPAND, Softmax via EXP2+RECIP+
   EXPAND, CrossEntropy via LOG2+MUL+REDUCE_SUM+NEG).  Per-weight
   handles aren't directly accessible from TFromNet anyway.

   Run from the repo root:
       wolframscript -f wl/Examples/lenet-mnist/grad-check.wls
       DEV=metal wolframscript -f wl/Examples/lenet-mnist/grad-check.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

debugPrint[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

TInit[];

net = NetInitialize @ NetModel["LeNet"];
debugPrint["LeNet built (random init): ", Length[net], " layers"];

batch  = TMnistBatch[1];
img    = Normal @ TTensorData @ batch["images"];
label  = Normal[TTensorData @ batch["labels"]][[1]];
debugPrint["MNIST sample: true label = ", label];

x      = TTensorCreate @ NumericArray[img[[1]], "Real32"];
probs  = TFromNet[net, x];
target = TTensorCreate @ NumericArray[
    UnitVector[10, label + 1], "Real32"];
loss   = TCrossEntropyLoss[probs, target];

debugPrint["materializing forward + loss..."];
lossRaw = Normal @ TTensorData @ TRealize @ loss;
lossVal = If[ListQ[lossRaw], First @ lossRaw, lossRaw];
debugPrint["  loss = ", Round[lossVal, 0.0001]];
If[ ! NumericQ[lossVal] || ! (lossVal > 0),
    debugPrint["FAIL: loss is not a positive finite scalar (",
               lossVal, ")"];
    Exit[1]];

debugPrint["materializing TGrad[loss, x]..."];
gx     = TRealize @ TGrad[loss, x];
gxHost = Normal @ TTensorData @ gx;
gxFlat = Flatten @ gxHost;
debugPrint["  shape = ", TTensorShape[gx], " (expected {1, 28, 28})"];
debugPrint["  min = ", Round[Min[gxFlat], 0.000001],
           "  max = ", Round[Max[gxFlat], 0.000001]];

If[ TTensorShape[gx] =!= {1, 28, 28},
    debugPrint["FAIL: grad shape mismatch"]; Exit[1]];
If[ ! AllTrue[gxFlat, NumericQ],
    debugPrint["FAIL: grad contains non-numeric values"]; Exit[1]];
If[ AnyTrue[gxFlat, (Abs[#] > 1.0*^10) &],
    debugPrint["FAIL: grad contains NaN/Inf or huge values"]; Exit[1]];

debugPrint["LeNet forward+grad smoke test PASSED."];
debugPrint["  Backward chain through every LeNet layer materializes"];
debugPrint["  to a finite, correctly-shaped gradient."];
