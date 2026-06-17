(* ::Package:: *)
(* Mnist.wl - load MNIST as TTensor handles for training / eval.

   This file lives under wl/Examples/_lib/ rather than the THVMLink
   library proper -- MNIST loading is example-data plumbing, not core
   runtime.  Consumers Get this file directly, e.g.
       Get @ FileNameJoin[{Directory[], "wl/Examples/_lib/Mnist.wl"}]
   from the repo root before calling TMnistBatch / TMnistLoad.

   Wraps `ResourceData["MNIST", "TrainingData" / "TestData"]`.  Each
   resource entry is `Image[28x28] -> Integer label`; we batch them
   into channels-first tensors of shape {N, 1, 28, 28}:

     trainImages : f32 {N_train, 1, 28, 28}
     trainLabels : i32 {N_train}
     testImages  : f32 {N_test, 1, 28, 28}
     testLabels  : i32 {N_test}

   `TMnistLoad[n]` takes the first `n` samples from each split.
   `TMnistLoad[]` defaults to All -- ~70 MB of f32 images, slow to
   load the first time but cached by ResourceData.

   Public surface
     TMnistLoad[n_:All]       -- whole-dataset Association of TTensors.
     TMnistBatch[n, split]    -- random minibatch as TTerm pair. *)

BeginPackage["WolframInstitute`THVMLink`"];

TMnistLoad::usage = "TMnistLoad[n] returns an Association with keys \"trainImages\", \"trainLabels\", \"testImages\", \"testLabels\" -- each a TTensor.  Images are f32 of shape {n, 1, 28, 28} (channels-first); labels are i32 of shape {n}.  TMnistLoad[] = TMnistLoad[All] loads the full 60K/10K splits.";

TMnistBatch::usage = "TMnistBatch[n] returns a random training-split minibatch as <|\"images\" -> TTerm{n, 1, 28, 28}, \"labels\" -> TTerm{n}|>.  TMnistBatch[n, \"test\"] samples from the test split instead.  Underlying ResourceData is cached, so repeated calls are fast after the first.";

Begin["`Private`"];

mnistImageBatch[imgs_List] := TTensorCreate @ NumericArray[
    (* {N, 28, 28} -> {N, 1, 28, 28} via ArrayReshape. *)
    ArrayReshape[
        ImageData[#, "Real32"] & /@ imgs,
        {Length[imgs], 1, 28, 28}
    ],
    "Real32"
]

mnistLabelBatch[labels_List] := TTensorCreate @ NumericArray[
    labels,
    "Integer32"
]

TMnistLoad[n_:All] := Module[{train, test, take, trainImg, trainLab, testImg, testLab},
    train = ResourceData["MNIST", "TrainingData"];
    test  = ResourceData["MNIST", "TestData"];
    take = If[n === All, Identity, Take[#, UpTo[n]] &];
    trainImg = take @ Keys[train];
    trainLab = take @ Values[train];
    testImg  = take @ Keys[test];
    testLab  = take @ Values[test];
    <|
        "trainImages" -> mnistImageBatch[trainImg],
        "trainLabels" -> mnistLabelBatch[trainLab],
        "testImages"  -> mnistImageBatch[testImg],
        "testLabels"  -> mnistLabelBatch[testLab]
    |>
]

(* Sample n random items from "TrainingData" or "TestData" and
   pack them as fresh TTensors.  ResourceData caches across calls,
   so repeated invocations are fast after the first. *)
TMnistBatch[n_Integer]                := TMnistBatch[n, "train"]
TMnistBatch[n_Integer, split_String] := Module[{
    resKey, src, total, idx, samples
},
    resKey = Switch[split,
        "train", "TrainingData",
        "test",  "TestData",
        _,        None];
    If[ resKey === None,
        Message[TMnistBatch::badsplit, split];
        Return @ $Failed
    ];
    src = ResourceData["MNIST", resKey];
    total = Length[src];
    idx = RandomSample[Range[total], UpTo[n]];
    samples = src[[idx]];
    <|
        "images" -> mnistImageBatch @ Keys[samples],
        "labels" -> mnistLabelBatch @ Values[samples]
    |>
]

TMnistBatch::badsplit = "TMnistBatch split must be \"train\" or \"test\", not `1`.";

End[];

EndPackage[];
