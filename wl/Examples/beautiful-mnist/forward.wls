#!/usr/bin/env wolframscript
(* forward.wls -- forward pass over the tinygrad-style
   "beautiful_mnist" architecture (bm2 of the bench arc).

   Reference architecture (simplified version of tinygrad's beautiful
   _mnist; see TASKS.md bm-arc):

       input          {1, 28, 28}
       Conv2D 1->32, 5x5     -> {32, 24, 24}
       ReLU
       Conv2D 32->64, 5x5    -> {64, 20, 20}
       ReLU
       MaxPool 2x2            -> {64, 10, 10}
       Flatten                -> {1, 6400}
       Linear 6400 -> 10      -> {10}
       Softmax

   ~120k parameters, vs LeNet's ~1M (mostly FC).  Designed to stress
   the conv path more, the FC path less.

   Run from repo root:
       wolframscript -f wl/Examples/beautiful-mnist/forward.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

debugPrint[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

SeedRandom[42];
TInit[];

glorot[shape_List] := With[{
    fanIn = If[Length[shape] >= 2, Times @@ Drop[shape, 1], shape[[1]]]
},
    NumericArray[
        RandomVariate[NormalDistribution[0., Sqrt[2.0 / fanIn]], shape],
        "Real32"]
]
zerosArr[shape_List] := NumericArray[ConstantArray[0., shape], "Real32"]

w1Host = glorot[{32, 1, 5, 5}];     b1Host = zerosArr[{32}];
w2Host = glorot[{64, 32, 5, 5}];    b2Host = zerosArr[{64}];
w3Host = glorot[{10, 6400}];         b3Host = zerosArr[{10}];

debugPrint["beautiful_mnist params: ",
    Times @@ Dimensions @ Normal @ w1Host, " + ",
    Times @@ Dimensions @ Normal @ w2Host, " + ",
    Times @@ Dimensions @ Normal @ w3Host,
    " conv/fc weights"]

(* Pre-trained weights would go here; we use random init so the
   prediction shape is what we're checking, not accuracy. *)

batch  = TMnistBatch[3];
imgs   = Normal @ TTensorData @ batch["images"];   (* {3, 1, 28, 28} *)
labels = Normal @ TTensorData @ batch["labels"];

buildForward[wH1_, bH1_, wH2_, bH2_, wH3_, bH3_, img_] :=
    Module[{W1, b1, W2, b2, W3, b3, x, h1, r1, h2, r2, p2, flat, h3, probs},
        W1 = TTensorCreate @ wH1;  b1 = TTensorCreate @ bH1;
        W2 = TTensorCreate @ wH2;  b2 = TTensorCreate @ bH2;
        W3 = TTensorCreate @ wH3;  b3 = TTensorCreate @ bH3;
        x  = TTensorCreate @ NumericArray[img, "Real32"];
        h1 = TUOpConv2D[x,  W1, b1];           (* {32, 24, 24} *)
        r1 = TReLU[h1];
        h2 = TUOpConv2D[r1, W2, b2];           (* {64, 20, 20} *)
        r2 = TReLU[h2];
        (* MaxPool 2x2: reshape to {64, 10, 2, 10, 2}, reduce-MAX
           on axes 2 and (post-shrink) 3.  Same pattern as LeNet's
           verify.wls. *)
        p2 = TUOpReduce[
                TUOpReduce[
                    TUOpReshape[r2, {64, 10, 2, 10, 2}],
                    2, "MAX"],
                3, "MAX"];                     (* {64, 10, 10} *)
        flat = TUOpReshape[p2, {1, 6400}];
        h3   = TUOpAdd[TMatVec[W3, flat], b3]; (* {10} *)
        probs = TSoftmax[h3];
        probs
    ]

debugPrint["per-sample forward pass (random init):"];
Do[
    Module[{probs, pred, conf},
        TInit[];
        probs = Normal @ TTensorData @ TRealize @ buildForward[
            w1Host, b1Host, w2Host, b2Host, w3Host, b3Host, imgs[[i]]];
        pred  = Position[probs, Max[probs]][[1, 1]] - 1;
        conf  = Round[Max[probs], 0.001];
        debugPrint["  sample ", i, ": true=", labels[[i]],
                   " pred=", pred, " confidence=", conf]
    ],
    {i, 3}
];

debugPrint["done."];
