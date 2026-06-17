#!/usr/bin/env wolframscript
(* forward.wls -- 2-layer fully-connected MLP forward inference on a
   single MNIST sample.  Pure forward; no backprop.  Validates that
   Flatten -> Linear -> ReLU -> Linear -> Softmax all materialize
   correctly and that TCrossEntropyLoss produces a finite scalar.

   Run from the repo root:
       wolframscript -f wl/Examples/mlp-mnist/forward.wls
       DEV=metal wolframscript -f wl/Examples/mlp-mnist/forward.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

debugPrint[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

TInit[];

net = NetInitialize @ NetChain[{
    FlattenLayer[],
    LinearLayer[32], ElementwiseLayer[Ramp],
    LinearLayer[10], SoftmaxLayer[]
}, "Input" -> {1, 28, 28}];
debugPrint["MLP built: ", Length[net], " layers, hidden=32"];

batch  = TMnistBatch[1];
img    = Normal @ TTensorData @ batch["images"];
label  = Normal[TTensorData @ batch["labels"]][[1]];
debugPrint["MNIST sample: true label = ", label];

x      = TTensorCreate @ NumericArray[img[[1]], "Real32"];
probs  = TRealize @ TFromNet[net, x];
pHost  = Normal @ TTensorData @ probs;

debugPrint["forward output shape: ", TTensorShape[probs]];
debugPrint["softmax sum: ", Total[pHost]];
debugPrint["argmax pred: ", Position[pHost, Max[pHost]][[1, 1]] - 1];

If[ Abs[Total[pHost] - 1.0] > 0.01,
    debugPrint["FAIL: softmax does not sum to 1 (sum=", Total[pHost], ")"];
    Exit[1]];
If[ ! AllTrue[pHost, NumericQ],
    debugPrint["FAIL: probs contain non-numeric values"];
    Exit[1]];

(* Cross-entropy loss against one-hot target. *)
target  = TTensorCreate @ NumericArray[
    UnitVector[10, label + 1], "Real32"];
loss    = TRealize @ TCrossEntropyLoss[probs, target];
lossRaw = Normal @ TTensorData @ loss;
(* REDUCE drops the axis but the materializer reports a {} shape as
   a length-1 list buffer, so unwrap. *)
lossVal = If[ ListQ[lossRaw], First @ lossRaw, lossRaw];
debugPrint["cross-entropy loss: ", lossVal];

If[ ! NumericQ[lossVal] || ! (lossVal > 0),
    debugPrint["FAIL: loss is not a positive finite scalar"];
    Exit[1]];

debugPrint["forward smoke test PASSED."];
