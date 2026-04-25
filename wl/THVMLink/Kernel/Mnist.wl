(* ::Package:: *)
(* Mnist.wl - load MNIST as TTensor handles for training / eval.

   Wraps `ResourceData["MNIST", "TrainingData" / "TestData"]`.  Each
   resource entry is `Image[28x28] -> Integer label`; we batch them
   into channels-first tensors matching what TLeNet expects:

     trainImages : f32 {N_train, 1, 28, 28}
     trainLabels : i32 {N_train}
     testImages  : f32 {N_test, 1, 28, 28}
     testLabels  : i32 {N_test}

   `TMnistLoad[n]` takes the first `n` samples from each split.
   `TMnistLoad[]` defaults to All -- ~70 MB of f32 images, slow to
   load the first time but cached by ResourceData.

   Public surface
     TMnistLoad[n_:All]      -- returns Association of TTensors. *)

BeginPackage["THVMLink`"];

TMnistLoad::usage = "TMnistLoad[n] returns an Association with keys \"trainImages\", \"trainLabels\", \"testImages\", \"testLabels\" -- each a TTensor.  Images are f32 of shape {n, 1, 28, 28} (channels-first); labels are i32 of shape {n}.  TMnistLoad[] = TMnistLoad[All] loads the full 60K/10K splits.";

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

End[];

EndPackage[];
