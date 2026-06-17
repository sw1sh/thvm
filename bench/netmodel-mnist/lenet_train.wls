#!/usr/bin/env wolframscript
(* Train the Wolfram NetModel["LeNet"] ARCHITECTURE from scratch on MNIST
   via the THVMLink tensor surface, and compare to the pretrained
   NetModel["LeNet Trained on MNIST Data"].

   NetModel["LeNet"] returns the (uninitialised) architecture -- on a WL
   older than the model's version its weights come back Automatic, so
   NetExtract is empty and TFromNet has nothing to lift.  NetInitialize
   gives concrete random weights (= "from scratch") on the SAME net, no
   rebuild; TFromNet then lifts it directly.  The trailing SoftmaxLayer
   is skipped (layers 1..10) so the loss sees logits.

   Run:  wolframscript -file lenet_train.wls *)

PacletDirectoryLoad[FileNameJoin[{DirectoryName[$InputFileName], "..", "..", "wl", "THVMLink"}]];
Needs["WolframInstitute`THVMLink`"];
SeedRandom[42]; Btr = 256;
tr = RandomSample[ResourceData["MNIST", "TrainingData"], Btr];
te = RandomSample[ResourceData["MNIST", "TestData"], 512];
trImg = N @ ArrayReshape[Flatten[ImageData /@ Keys[tr]], {Btr, 1, 28, 28}];
trOH  = N @ Table[UnitVector[10, l + 1], {l, Values[tr]}];
teImg = N @ ArrayReshape[Flatten[ImageData /@ Keys[te]], {512, 1, 28, 28}];
teLab = Values[te];

(* NetModel["LeNet"] as-is, just initialised; lift its own layers 1..10 (logits) *)
init = Quiet @ NetInitialize[NetModel["LeNet"], RandomSeeding -> 7];
xS = TTensorCreate[trImg]; yT = TTensorCreate[trOH]; lr = 0.05;
{logits, sown} = Quiet @ Reap[Fold[TFromLayer[#2, #1] &, xS, Table[init[[i]], {i, 10}]], "thvmNetParam"];
params = Flatten[sown];
Print["LeNet params lifted (2 conv + 2 FC = 8)", " => ", Length @ params];
loss = TCategoricalCrossEntropy[logits, yT];
Do[
    TClearGrad /@ params;
    gs = TRealize[TGrad[loss, params]];
    MapThread[TRealize @ TAssign[#1, #1 + (-lr)*#2] &, {params, gs}],
    {150}];

(* inference through the TRAINED logits via the input slot (512 = 2x256) *)
preds = Join @@ Table[
    TRealize @ TAssign[xS, TTensorCreate[teImg[[(k - 1)*256 + 1 ;; k*256]]]];
    (Ordering[#, -1][[1]] - 1) & /@ Normal @ TRealize @ logits, {k, 2}];
Print["thvm-trained LeNet (256 imgs, from scratch) test acc", " => ", N @ Mean @ MapThread[Boole[#1 == #2] &, {preds, teLab}]];
Print["pretrained Wolfram LeNet (60k) test acc", " => ", N @ Mean @ MapThread[Boole[#1 == #2] &,
    {NetModel["LeNet Trained on MNIST Data"][Keys[te]], teLab}]];
