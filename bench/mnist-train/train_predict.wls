#!/usr/bin/env wolframscript
(* MNIST trained FROM SCRATCH on the THVMLink tensor surface, then an
   example-picture prediction decoded with NetDecoder.

   A conv8 LeNet head (Conv2d[1->8,3x3] -> ReLU -> 2x2 maxpool -> Linear[10])
   is initialised random and trained by ONE TNetTrain call (the inert/
   realize-grads SGD one-liner).  TNetPredict + NetDecoder turn the logits
   into digit labels.  CPU by default; DEV=metal runs it on the GPU.

   Run:  wolframscript -file train_predict.wls
   Out:  prints train time + test accuracy; writes prediction.png *)

PacletDirectoryLoad[FileNameJoin[{DirectoryName[$InputFileName], "..", "..", "wl", "THVMLink"}]];
Needs["WolframInstitute`THVMLink`"];
SeedRandom[42];
B = 512;
tr = RandomSample[ResourceData["MNIST", "TrainingData"], B];
te = RandomSample[ResourceData["MNIST", "TestData"], B];
trImg = N @ ArrayReshape[Flatten[ImageData /@ Keys[tr]], {B, 1, 28, 28}];
trOH  = N @ Table[UnitVector[10, l + 1], {l, Values[tr]}];
teImg = N @ ArrayReshape[Flatten[ImageData /@ Keys[te]], {B, 1, 28, 28}];
teLab = Values[te];

(* from-scratch random init *)
net = NetInitialize[
    NetChain[{ConvolutionLayer[8, {3, 3}], Ramp, PoolingLayer[{2, 2}, {2, 2}],
        FlattenLayer[], LinearLayer[10]}, "Input" -> {1, 28, 28}],
    RandomSeeding -> 7];

dt = AbsoluteTiming[
    trained = TNetTrain[net, trImg, trOH, "MaxTrainingRounds" -> 250, "LearningRate" -> 0.2]][[1]];
Print["backend", " => ", Environment["DEV"]];
Print["train time (s)", " => ", Round[dt, 0.1]];

logits = TNetPredict[trained, teImg];
dec = NetDecoder[{"Class", Range[0, 9]}];
preds = dec /@ logits;
Print["test accuracy (512 held-out)", " => ", N @ Mean @ MapThread[Boole[#1 == #2] &, {preds, teLab}]];

(* example-picture prediction: 10 test digits, decoded label, green=correct *)
imgs = Keys[te];
cells = Table[
    Labeled[
        ImageResize[imgs[[i]], 56],
        Style[Row[{"\[Rule] ", preds[[i]]}],
            If[preds[[i]] === teLab[[i]], Darker @ Green, Red], Bold, 16]],
    {i, 10}];
viz = Labeled[
    Grid[Partition[cells, 5], Spacings -> {2.5, 1.5}, Frame -> All, Background -> White],
    Style["MNIST from scratch (THVMLink) -- NetDecoder predictions", 14], Top];
out = FileNameJoin[{DirectoryName[$InputFileName], "prediction.png"}];
Export[out, viz, ImageResolution -> 144];
Print["wrote", " => ", out];
