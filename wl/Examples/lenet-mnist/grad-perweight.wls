#!/usr/bin/env wolframscript
(* grad-perweight.wls -- diagnostic.  For each of LeNet's 8 weight
   tensors, materialize TGrad[loss, w] in isolation and print its
   shape, min, max + a flag if it contains $Failed / NaN / Inf /
   absurd magnitudes.  Identifies which weight's chain breaks
   through the LeNet stack; surfaces issues the unit tests miss.

   Run from the repo root:
       wolframscript -f wl/Examples/lenet-mnist/grad-perweight.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

debugPrint[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

SeedRandom[42];

(* Hand-init weight hosts (mirror train.wls draft). *)
glorot[shape_List] := With[{
    fanIn = If[Length[shape] >= 2, Times @@ Drop[shape, 1], shape[[1]]]
},
    NumericArray[
        RandomVariate[NormalDistribution[0., Sqrt[2.0 / fanIn]], shape],
        "Real32"]
]
zerosArr[shape_List] := NumericArray[
    ConstantArray[0., shape], "Real32"]

w1Host = glorot[{6, 1, 5, 5}];   b1Host = zerosArr[{6}];
w2Host = glorot[{16, 6, 5, 5}];  b2Host = zerosArr[{16}];
w3Host = glorot[{120, 256}];      b3Host = zerosArr[{120}];
w4Host = glorot[{10, 120}];       b4Host = zerosArr[{10}];

(* === Build the chain inline holding tensor handles. ===
   Returns {loss, {W1,b1,W2,b2,W3,b3,W4,b4}}. *)
buildLeNet[w1H_, b1H_, w2H_, b2H_, w3H_, b3H_, w4H_, b4H_, img_, label_] :=
    Module[{W1, b1, W2, b2, W3, b3, W4, b4,
            x, h1, r1, p1, h2, r2, p2, flat, h3, r3, h4, probs, target, loss},
        W1 = TTensorCreate @ w1H;  b1 = TTensorCreate @ b1H;
        W2 = TTensorCreate @ w2H;  b2 = TTensorCreate @ b2H;
        W3 = TTensorCreate @ w3H;  b3 = TTensorCreate @ b3H;
        W4 = TTensorCreate @ w4H;  b4 = TTensorCreate @ b4H;
        x  = TTensorCreate @ NumericArray[img, "Real32"];
        h1 = TUOpConv2D[x, W1, b1];
        r1 = TReLU[h1];
        p1 = TUOpReduce[
                TUOpReduce[
                    TUOpReshape[r1, {6, 12, 2, 12, 2}],
                    2, "MAX"],
                3, "MAX"];
        h2 = TUOpConv2D[p1, W2, b2];
        r2 = TReLU[h2];
        p2 = TUOpReduce[
                TUOpReduce[
                    TUOpReshape[r2, {16, 4, 2, 4, 2}],
                    2, "MAX"],
                3, "MAX"];
        flat = TUOpReshape[p2, {1, 256}];
        h3   = TUOpAdd[TMatVec[W3, flat], b3];
        r3   = TReLU[h3];
        h4   = TUOpAdd[TMatVec[W4, TUOpReshape[r3, {1, 120}]], b4];
        probs = TSoftmax[h4];
        target = TTensorCreate @ NumericArray[
            UnitVector[10, label + 1], "Real32"];
        loss = TCrossEntropyLoss[probs, target];
        {loss, {W1, b1, W2, b2, W3, b3, W4, b4}}
    ]

batch = TMnistBatch[1];
img   = Normal[TTensorData @ batch["images"]][[1]];
label = Normal[TTensorData @ batch["labels"]][[1]];
debugPrint["MNIST sample: true label = ", label];

(* Verify forward first. *)
TInit[];
{loss0, _} = buildLeNet[w1Host, b1Host, w2Host, b2Host,
                         w3Host, b3Host, w4Host, b4Host, img, label];
lossVal0 = First @ Normal @ TTensorData @ TRealize @ loss0;
debugPrint["forward loss: ", Round[lossVal0, 0.0001]];

(* Per-weight grad probe.  Re-init each time to keep heap
   pressure small + isolate any state leak. *)
weightNames = {"W1", "b1", "W2", "b2", "W3", "b3", "W4", "b4"};

probe[idx_Integer, name_String] := Module[{loss, weights, w, g, host, flat},
    TInit[];
    {loss, weights} = buildLeNet[w1Host, b1Host, w2Host, b2Host,
                                   w3Host, b3Host, w4Host, b4Host,
                                   img, label];
    w = weights[[idx]];
    g = TRealize @ TGrad[loss, w];
    host = TTensorData @ g;
    If[ host === $Failed,
        debugPrint["  ", name, ": FAILED -- TTensorData returned $Failed"];
        Return[]];
    flat = Flatten @ Normal @ host;
    debugPrint["  ", name,
        ": shape=", TTensorShape[g],
        "  min=", Round[Min[flat], 0.000001],
        "  max=", Round[Max[flat], 0.000001],
        If[ AnyTrue[flat, (! NumericQ[#] || ! NumberQ[#] ||
                             # === Indeterminate || Abs[#] > 1.0*^10) &],
            "  ** FLAGGED **",
            ""]]
]

debugPrint["per-weight TGrad probe:"];
Do[probe[i, weightNames[[i]]], {i, 8}];
debugPrint["done."];
