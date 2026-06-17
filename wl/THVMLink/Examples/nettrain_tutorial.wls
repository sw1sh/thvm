#!/usr/bin/env wolframscript
(* nettrain_tutorial.wls - showcase thvm's inert-loop NetTrain and compare
   it to built-in NetTrain on MNIST.

   thvm trains by emitting the ENTIRE optimiser as a single inert
   interaction-net term: the step (forward + cross-entropy + backward +
   per-weight SGD assign) is materialised once, wrapped in a recursive
   lambda, and a single TWnf dispatches every round.  No host-side Do
   loop -- the training run IS one normalisation.

       trained = TNetTrain[net, data, MaxTrainingRounds -> n]   (net a NetChain)

   Run:  wolframscript -file wl/THVMLink/Examples/nettrain_tutorial.wls *)

PacletDirectoryLoad[FileNameJoin[{DirectoryName[$InputFileName], ".."}]];
Get["WolframInstitute`THVMLink`"];

SeedRandom[7];
nTrain = 512; nTest = 512;
all = RandomSample @ ResourceData["MNIST", "TrainingData"];
tr  = Take[all, nTrain];
te  = Take[all, {nTrain + 1, nTrain + nTest}];
flat = Flatten[ImageData[#[[1]]]] &;
teLabels = #[[2]] & /@ te;

arch = {LinearLayer[64], ElementwiseLayer[Ramp], LinearLayer[10]};

Print["=== thvm NetTrain (inert recursive loop) ==="];
(* train at increasing round counts (fresh each) to trace progress;
   each call is ONE TWnf dispatch of the whole optimiser term. *)
roundsList = {25, 50, 100, 200, 400};
thvmCurve = Table[
    SeedRandom[7];
    With[{net = NetInitialize @ NetChain[arch, "Input" -> 784]},
        Module[{trained, t0, dt, acc},
            t0 = AbsoluteTime[];
            trained = TNetTrain[net, tr, MaxTrainingRounds -> r, "LearningRate" -> 0.05];
            dt = AbsoluteTime[] - t0;
            acc = N[Count[MapThread[#1 === #2 &, {TNetPredict[trained, te[[All, 1]]], teLabels}], True] / nTest];
            Print["  rounds=", r, "  testAcc=", Round[acc, 0.001], "  (", Round[dt, 0.01], "s)"];
            {r, acc}]],
    {r, roundsList}];
thvmBest = thvmCurve[[-1, 2]];

Print["=== built-in NetTrain (reference, same hidden arch) ==="];
SeedRandom[7];
clsNet = NetInitialize @ NetChain[
    Append[arch, SoftmaxLayer[]], "Input" -> 784,
    "Output" -> NetDecoder[{"Class", Range[0, 9]}]];
biTrained = NetTrain[clsNet, (flat[#] -> #[[2]]) & /@ tr,
    MaxTrainingRounds -> 20, TrainingProgressReporting -> None, "Method" -> "ADAM"];
biAcc = N[Count[MapThread[#1 === #2 &, {biTrained[flat /@ te], teLabels}], True] / nTest];
Print["  built-in testAcc=", Round[biAcc, 0.001]];

Print["=== summary ==="];
Print["  thvm  (inert loop, 400 rounds SGD lr0.05): ", Round[thvmBest, 0.001]];
Print["  built-in (20 rounds ADAM):          ", Round[biAcc, 0.001]];

(* === optimizer suite: SGD vs momentum vs ADAM, same net, 100 rounds ===
   The inert loop carries per-weight optimiser state (a velocity for momentum,
   first/second moments for ADAM) as extra in-place ASSIGNs fired each round;
   a single TWnf still drives the whole thing.  Adaptive optimisers reach a
   given accuracy in far fewer rounds (use a smaller LearningRate -- momentum's
   effective rate is lr/(1-momentum), ADAM's adaptive step is ~lr per weight). *)
Print["=== thvm optimizer suite (same net, 100 rounds) ==="];
optAcc[label_, opts_List] := (
    SeedRandom[7];
    With[{net = NetInitialize @ NetChain[arch, "Input" -> 784]},
        With[{trained = TNetTrain[net, tr, Sequence @@ opts]},
            Print["  ", label, "  testAcc=",
                Round[N[Count[MapThread[#1 === #2 &,
                    {TNetPredict[trained, te[[All, 1]]], teLabels}], True] / nTest], 0.001]]]]);
optAcc["SGD       lr0.05       ", {MaxTrainingRounds -> 100, "LearningRate" -> 0.05}];
optAcc["Momentum  lr0.01 b0.9  ", {MaxTrainingRounds -> 100, "LearningRate" -> 0.01, "Momentum" -> 0.9}];
optAcc["ADAM      lr0.001      ", {MaxTrainingRounds -> 100, "LearningRate" -> 0.001, "Method" -> "Adam"}];

(* === conv net, trained DIRECTLY from a Wolfram NetChain ===
   TNetTrain dispatches on a NetChain too, with no TFromNet round-trip: it
   builds the logits forward over a fresh input slot, drops a trailing
   SoftmaxLayer, and emits the same inert SGD loop.  A convolutional net (with
   the rank-4 batch axis kept through ConvolutionLayer / PoolingLayer /
   FlattenLayer) trains in one TWnf dispatch.  Here we overfit a small batch to
   show the loss curve descend. *)
Print["=== thvm NetTrain on a conv NetChain (direct, no TFromNet) ==="];
SeedRandom[7];
convBatch = Take[all, 32];
convNet = NetInitialize @ NetChain[
    {ConvolutionLayer[8, {3, 3}], ElementwiseLayer[Ramp], PoolingLayer[{2, 2}, {2, 2}],
     FlattenLayer[], LinearLayer[10], SoftmaxLayer[]},
    "Input" -> {1, 28, 28}];
convCurve = Table[
    SeedRandom[7];
    Module[{trained, t0, dt, acc, net},
        net = NetInitialize @ convNet;
        t0 = AbsoluteTime[];
        trained = TNetTrain[net, convBatch, MaxTrainingRounds -> r, "LearningRate" -> 0.1];
        dt = AbsoluteTime[] - t0;
        acc = N[Count[MapThread[#1 === #2 &, {TNetPredict[trained, convBatch[[All, 1]]], convBatch[[All, 2]]}], True] / Length[convBatch]];
        Print["  rounds=", r, "  trainBatchAcc=", Round[acc, 0.001], "  (", Round[dt, 0.01], "s)"];
        {r, acc}],
    {r, {5, 10, 20, 40}}];
Print["  conv net (40 rounds SGD lr0.1) overfits 32-batch to acc ", Round[convCurve[[-1, 2]], 0.001]];

(* === plot: accuracy-vs-rounds curve + built-in reference line === *)
plot = ListLinePlot[
    {thvmCurve},
    PlotMarkers -> Automatic,
    PlotStyle -> StandardBlue,
    Frame -> True,
    FrameLabel -> {"training rounds (TWnf dispatches)", "held-out test accuracy"},
    PlotLabel -> "thvm inert-loop NetTrain on MNIST (512/512)",
    GridLines -> {None, {{biAcc, Directive[StandardRed, Dashed]}}},
    Epilog -> {StandardRed, Text["built-in NetTrain", {200, biAcc + 0.03}]},
    PlotRange -> {0, 1},
    ImageSize -> 560,
    Background -> LightDarkSwitched[White, Black]];
pngPath = FileNameJoin[{DirectoryName[$InputFileName], "nettrain_tutorial.png"}];
Export[pngPath, plot];
Print["plot saved: ", pngPath];
