#!/usr/bin/env wolframscript
(* Train tinygrad's beautiful_mnist EXACT architecture from scratch on
   MNIST via the THVMLink tensor surface.

   Arch (examples/beautiful_mnist.py): Conv[32,5] ReLU Conv[32,5] ReLU
   BatchNorm MaxPool Conv[64,3] ReLU Conv[64,3] ReLU BatchNorm MaxPool
   Flatten Linear[10].  TFromNet lifts the BatchNormalizationLayer via
   fromLayer -> TBatchNormTrain (batch-statistics normalisation); all 14
   params train through the BN-training backward.  Mini-batch SGD (the
   input/target slots are re-fed per batch with TAssign).

   CPU only: this is the net the project flags as over-fusing the backward
   into 1300+ kernels and orphaning the Metal GPU on dispatch (reboot to
   clear) -- prescreen TKernelCount[] on DEV=cpu before any Metal train.

   Run:  wolframscript -file beautiful_mnist_train.wls *)

PacletDirectoryLoad[FileNameJoin[{DirectoryName[$InputFileName], "..", "..", "wl", "THVMLink"}]];
Needs["THVMLink`"];
print[l_, v_] := WriteString["stdout", l, " => ", ToString[v, InputForm], "\n"];

SeedRandom[42]; Ntr = 1024; B = 64;
tr = RandomSample[ResourceData["MNIST", "TrainingData"], Ntr];
te = RandomSample[ResourceData["MNIST", "TestData"], 512];
trImg = N @ ArrayReshape[Flatten[ImageData /@ Keys[tr]], {Ntr, 1, 28, 28}];
trOH  = N @ Table[UnitVector[10, l + 1], {l, Values[tr]}];
teImg = N @ ArrayReshape[Flatten[ImageData /@ Keys[te]], {512, 1, 28, 28}];
teLab = Values[te];

bm = NetInitialize[NetChain[{
    ConvolutionLayer[32, {5, 5}], Ramp, ConvolutionLayer[32, {5, 5}], Ramp,
    BatchNormalizationLayer[], PoolingLayer[{2, 2}, {2, 2}],
    ConvolutionLayer[64, {3, 3}], Ramp, ConvolutionLayer[64, {3, 3}], Ramp,
    BatchNormalizationLayer[], PoolingLayer[{2, 2}, {2, 2}],
    FlattenLayer[], LinearLayer[10]}, "Input" -> {1, 28, 28}], RandomSeeding -> 7];

xS = TTensorCreate[trImg[[1 ;; B]]]; yT = TTensorCreate[trOH[[1 ;; B]]]; lr = 0.1;
{fwd, sown} = Quiet @ Reap[TFromNet[bm, xS], "thvmNetParam"];
params = Flatten[sown];
print["beautiful_mnist params lifted (4 conv + 2 BN + linear = 14)", Length @ params];
loss = TCategoricalCrossEntropy[fwd, yT]; nB = Ntr/B;
Do[
    Do[
        TRealize @ TAssign[xS, TTensorCreate[trImg[[(b - 1)*B + 1 ;; b*B]]]];
        TRealize @ TAssign[yT, TTensorCreate[trOH[[(b - 1)*B + 1 ;; b*B]]]];
        TClearGrad /@ params;
        gs = TRealize[TGrad[loss, params]];
        MapThread[TRealize @ TAssign[#1, #1 + (-lr)*#2] &, {params, gs}],
        {b, nB}],
    {ep, 8}];

preds = Join @@ Table[
    TRealize @ TAssign[xS, TTensorCreate[teImg[[(k - 1)*B + 1 ;; k*B]]]];
    (Ordering[#, -1][[1]] - 1) & /@ Normal @ TRealize @ fwd, {k, 8}];
print["thvm-trained beautiful_mnist (1024 imgs, from scratch) test acc",
    N @ Mean @ MapThread[Boole[#1 == #2] &, {preds, teLab}]];
