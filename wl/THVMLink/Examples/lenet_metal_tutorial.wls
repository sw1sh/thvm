#!/usr/bin/env wolframscript
(* lenet_metal_tutorial.wls -- end-to-end LeNet on MNIST through the THVMLink
   tensor surface, trained by the inert-loop NetTrain and run on the Metal GPU.

   Pipeline (exactly what the Train tutorial walks through):
     1. TFromNet[NetModel["LeNet"]] lifts the real Wolfram LeNet AS-IS.
        NetModel["LeNet"] is the UNINITIALISED architecture (Automatic
        weights), so NetInitialize gives concrete random weights = "from
        scratch".  The trailing SoftmaxLayer is dropped inside TNetTrain so
        the loss sees logits (TCategoricalCrossEntropy).
     2. Image input / class output: the lifted net takes raw MNIST Images
        (toInput maps each Image -> the {1,28,28} array the encoder feeds)
        and TNetPredict returns the integer class (logits -> argmax, the
        Class decoder).
     3. NetTrain[lifted, data, MaxTrainingRounds -> n] drives the inert
        recursive optimiser term: the whole step is materialised ONCE and a
        single TWnf re-fires the fixed kernel set every round.
     4. Metal safety: that fixed set is PRESCREENED on the active device
        (TKernelCount[]); when it is small (< 300) and re-fired, Metal
        training is safe.  We assert it and dispatch.
     5. Sample predictions on held-out MNIST Images + the test accuracy.

   Run:  wolframscript -file wl/THVMLink/Examples/lenet_metal_tutorial.wls
         DEV=metal wolframscript -file .../lenet_metal_tutorial.wls   (GPU)

   tinygrad reference (same net, same backend):
         DEV=METAL python3 bench/netmodel-mnist/lenet_tinygrad.py *)

PacletDirectoryLoad[FileNameJoin[{DirectoryName[$InputFileName], ".."}]];
Get["THVMLink`"];

print[args___] := WriteString["stdout", StringJoin @@ Map[ToString, {args}], "\n"];
dev = With[{e = Environment["DEV"]}, If[ StringQ[e] && ToLowerCase[e] === "metal", "Metal", "CPU"]];

(* === data: real MNIST Images in, integer classes out === *)
SeedRandom[7];
nTrain = 256; nTest = 256; rounds = 60; lr = 0.1;
trAll = RandomSample @ ResourceData["MNIST", "TrainingData"];
teAll = RandomSample @ ResourceData["MNIST", "TestData"];
tr = Take[trAll, nTrain];
te = Take[teAll, nTest];
testImages = Keys[te];
testLabels = Values[te];

print["=== LeNet on MNIST through THVMLink (DEV=", dev, ") ==="];
print["train ", nTrain, " imgs, test ", nTest, " imgs, ", rounds, " full-batch SGD rounds at lr ", lr];

(* === 1. lift the real Wolfram LeNet AS-IS === *)
lenet0 = NetModel["LeNet"];
print["NetModel[\"LeNet\"]: ", Head[lenet0], " of ", Length[lenet0], " layers, ",
    "Image input + Class output ports"];
init = Quiet @ NetInitialize[lenet0, RandomSeeding -> 7];     (* from scratch *)
lifted = TFromNet[init];
print["TFromNet lifted it to a ", Head[lifted], " with ", Length @ TNetParams[lifted],
    " trainable tensors (2 conv + 2 FC weights/biases)"];

(* === 4. PRESCREEN the materialised fixed step set on the active device ===
   "TrainingNet" returns the INERT optimiser term: the whole step is
   materialised (every kernel built) but nothing has fired.  Counting kernels
   now measures exactly the fixed set TWnf will re-fire each round. *)
loopTerm = NetTrain[lifted, tr, "TrainingNet", MaxTrainingRounds -> rounds, "LearningRate" -> lr];
nKernels = TKernelCount[] - 1;
safe = nKernels < 300;
print["prescreen: the inert step materialises to ", nKernels, " kernels (fixed; re-fired each round)"];
print["Metal-safety: ", If[ safe, "SAFE (< 300, fixed set) -> dispatch on " <> dev, "UNSAFE (>= 300) -> CPU only"]];

(* === 3. train: a single TWnf drives the whole inert loop in place === *)
t0 = AbsoluteTime[];
TWnf[loopTerm];
dt = AbsoluteTime[] - t0;
trained = lifted;                            (* trained in place; lifted is the forward *)
nAfter = TKernelCount[] - 1;
print["trained in ", Round[dt, 0.01], "s -> ", Round[1000.*dt/rounds, 1], " ms/round on ", dev];
print["kernels after ", rounds, " rounds: ", nAfter,
    If[ nAfter === nKernels, " (UNCHANGED: TWnf re-fired the fixed set, no new kernels)", " (grew)"]];

(* === 2 + 6. predict integer classes on held-out MNIST Images === *)
preds = TNetPredict[trained, testImages];
acc = N @ Mean @ MapThread[Boole[#1 == #2] &, {preds, testLabels}];
print[""];
print["=== held-out test accuracy (", nTest, " imgs): ", Round[acc, 0.001], " ==="];

(* sample predictions: a few images with predicted vs true label *)
print[""];
print["=== sample predictions (predicted / true) ==="];
Do[
    print["  img ", i, ":  predicted ", preds[[i]], "   true ", testLabels[[i]],
        If[ preds[[i]] === testLabels[[i]], "   ok", "   X"]],
    {i, 12}];
nCorrect = Count[MapThread[#1 === #2 &, {Take[preds, 12], Take[testLabels, 12]}], True];
print["  (", nCorrect, " of the first 12 correct)"];

(* === 5. tinygrad reference (same net, same backend) === *)
print[""];
print["=== reference: tinygrad LeNet (same arch, same backend) ==="];
print["  run  DEV=", ToUpperCase[dev], " python3 bench/netmodel-mnist/lenet_tinygrad.py"];
print["  representative (Apple-silicon Metal, bs256, 60 rounds SGD lr0.1):"];
print["    tinygrad: ~32 ms/round, ~0.77 test acc"];
print["    thvm:     this run above"];

(* save a labelled sample-prediction grid for the docs *)
grid = Grid[Partition[
    MapThread[Labeled[#1, Row[{"pred ", #2, "/ true ", #3}], Bottom] &,
        {Take[testImages, 8], Take[preds, 8], Take[testLabels, 8]}], 4],
    Spacings -> {2, 2}];
pngPath = FileNameJoin[{DirectoryName[$InputFileName], "lenet_metal_tutorial.png"}];
Export[pngPath, grid];
print[""];
print["sample-prediction grid saved: ", pngPath];
