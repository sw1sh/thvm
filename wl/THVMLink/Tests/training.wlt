(* training.wlt -- regression tests for the inert-loop NetTrain feature
   (TNetTrain / NetTrain[_TTerm] / TNetOf / TNetPredict).  The whole
   optimiser is emitted as one inert interaction-net term and dispatched
   by a single TWnf; these pins assert it actually TRAINS a net end to
   end (flat MLP + conv), not just fires once.  A broken inert loop (firing
   the step a single time) leaves the net barely trained, so it cannot
   overfit a tiny set and the >= 0.75 train-batch accuracy gate fails.
   Synthetic + data-free so they stay fast and offline. *)

(* === flat MLP: the inert SGD loop overfits a tiny classification set === *)
VerificationTest[
    TInit[];
    SeedRandom[1];
    data = {{1., 0., 0.} -> 0, {0., 1., 0.} -> 1, {0., 0., 1.} -> 0, {1., 1., 1.} -> 1};
    net  = TFromNet[NetInitialize @ NetChain[
        {LinearLayer[8, "Input" -> 3], ElementwiseLayer[Ramp], LinearLayer[2]}]];
    trained = NetTrain[net, data, MaxTrainingRounds -> 150, "LearningRate" -> 0.2];
    With[{preds = TNetPredict[trained, data[[All, 1]]]},
        N[Count[MapThread[#1 === #2 &, {preds, data[[All, 2]]}], True] / Length[data]] >= 0.75
    ],
    True,
    TestID -> "training/flat-mlp-overfits-tiny-set"
]

(* === conv net: the WL-net TNetTrain form (im2col conv + inert loop)
       overfits a tiny image set, exercising the conv forward + backward === *)
VerificationTest[
    TInit[];
    SeedRandom[2];
    imgs   = RandomReal[{0, 1}, {4, 1, 6, 6}];
    labels = {0, 1, 0, 1};
    data   = MapThread[Rule, {imgs, labels}];
    net = NetInitialize @ NetChain[
        {ConvolutionLayer[4, {3, 3}], ElementwiseLayer[Ramp],
         PoolingLayer[{2, 2}, {2, 2}], FlattenLayer[], LinearLayer[2]},
        "Input" -> {1, 6, 6}];
    trained = TNetTrain[net, data, MaxTrainingRounds -> 120, "LearningRate" -> 0.15];
    With[{preds = TNetPredict[trained, imgs]},
        N[Count[MapThread[#1 === #2 &, {preds, labels}], True] / 4] >= 0.75
    ],
    True,
    TestID -> "training/conv-net-overfits-tiny-set"
]

(* === TNetOf reconstructs an equivalent NetChain from the term graph
       (registry-free): same structure + bit-equal forward === *)
VerificationTest[
    TInit[];
    SeedRandom[3];
    net0 = NetInitialize @ NetChain[
        {LinearLayer[6, "Input" -> 5], ElementwiseLayer[Ramp], LinearLayer[4]}];
    rec  = TNetOf[TFromNet[net0]];
    xv   = TTensorCreate @ NumericArray[RandomReal[{-1, 1}, {2, 5}], "Real32"];
    With[{
        diff = Max @ Abs[Flatten[
            Normal @ TTensorData @ TRealize @ TFromNet[net0, xv]
          - Normal @ TTensorData @ TRealize @ TFromNet[rec, xv]]]
    },
        {Head[rec] === NetChain, diff < 1.*^-5}
    ],
    {True, True},
    TestID -> "training/tnetof-reconstructs-equivalent-netchain"
]

(* === TNetInitialize re-initialises the weights in place -- Glorot for
       weight matrices (non-zero), zero for biases -- with no round-trip
       through a Wolfram net === *)
VerificationTest[
    TInit[];
    SeedRandom[4];
    net = TFromNet[NetInitialize @ NetChain[
        {LinearLayer[4, "Input" -> 3], ElementwiseLayer[Ramp], LinearLayer[2]}]];
    NetInitialize[net];
    With[{vals = (Normal @ TTensorData @ TRealize @ # &) /@ TNetParams[net]},
        {AllTrue[Flatten[vals], NumberQ[#] && Abs[#] < 100 &],
         AllTrue[Select[vals, MatrixQ], Max @ Abs @ Flatten[#] > 0 &],
         AllTrue[Select[vals, VectorQ], Max @ Abs @ # == 0 &]}
    ],
    {True, True, True},
    TestID -> "training/tnetinitialize-glorot-weights-zero-biases"
]

(* === the "TrainingNet" prop returns an INERT term (nothing has run); a
       single TWnf drives the whole optimiser loop and trains the net === *)
VerificationTest[
    TInit[];
    SeedRandom[5];
    data = {{1., 0., 0.} -> 0, {0., 1., 0.} -> 1, {0., 0., 1.} -> 0, {1., 1., 1.} -> 1};
    net  = TFromNet[NetInitialize @ NetChain[
        {LinearLayer[8, "Input" -> 3], ElementwiseLayer[Ramp], LinearLayer[2]}]];
    tn   = NetTrain[net, data, "TrainingNet", MaxTrainingRounds -> 150, "LearningRate" -> 0.2];
    TWnf[tn];
    With[{preds = TNetPredict[tn, data[[All, 1]]]},
        {Head[tn] === TTerm,
         N[Count[MapThread[#1 === #2 &, {preds, data[[All, 2]]}], True] / Length[data]] >= 0.75}
    ],
    {True, True},
    TestID -> "training/trainingnet-prop-inert-term-driven-by-twnf"
]

(* === SGD-with-momentum (Momentum option) trains: the inert loop carries a
       persistent velocity per weight (v <- beta*v + g, then p <- p - lr*v)
       and overfits the tiny set.  Effective rate is lr/(1-momentum), so the
       LearningRate is scaled down accordingly. === *)
VerificationTest[
    TInit[];
    SeedRandom[6];
    data = {{1., 0., 0.} -> 0, {0., 1., 0.} -> 1, {0., 0., 1.} -> 0, {1., 1., 1.} -> 1};
    net  = TFromNet[NetInitialize @ NetChain[
        {LinearLayer[8, "Input" -> 3], ElementwiseLayer[Ramp], LinearLayer[2]}]];
    trained = NetTrain[net, data, MaxTrainingRounds -> 150, "LearningRate" -> 0.02, "Momentum" -> 0.9];
    With[{preds = TNetPredict[trained, data[[All, 1]]]},
        N[Count[MapThread[#1 === #2 &, {preds, data[[All, 2]]}], True] / Length[data]] >= 0.75
    ],
    True,
    TestID -> "training/momentum-overfits-tiny-set"
]

(* === ADAM (Method -> "Adam") trains: the inert loop carries persistent
       first/second-moment buffers m, v per weight and the adaptive update
       p <- p - lr*m/(sqrt(v)+eps), overfitting the tiny set === *)
VerificationTest[
    TInit[];
    SeedRandom[8];
    data = {{1., 0., 0.} -> 0, {0., 1., 0.} -> 1, {0., 0., 1.} -> 0, {1., 1., 1.} -> 1};
    net  = TFromNet[NetInitialize @ NetChain[
        {LinearLayer[8, "Input" -> 3], ElementwiseLayer[Ramp], LinearLayer[2]}]];
    trained = NetTrain[net, data, MaxTrainingRounds -> 150, "LearningRate" -> 0.005, "Method" -> "Adam"];
    With[{preds = TNetPredict[trained, data[[All, 1]]]},
        N[Count[MapThread[#1 === #2 &, {preds, data[[All, 2]]}], True] / Length[data]] >= 0.75
    ],
    True,
    TestID -> "training/adam-overfits-tiny-set"
]

(* === WeightDecay (L2) regularizes: with wd > 0 the inert loop still fits the
       tiny set but reaches a SMALLER trained weight L1-norm (wd folds wd*p into
       each gradient).  Both runs share the seed/init for a fair comparison. === *)
VerificationTest[
    TInit[];
    SeedRandom[9];
    data = {{1., 0., 0.} -> 0, {0., 1., 0.} -> 1, {0., 0., 1.} -> 0, {1., 1., 1.} -> 1};
    t0 = (SeedRandom[9]; NetTrain[
        TFromNet[NetInitialize @ NetChain[{LinearLayer[8, "Input" -> 3], ElementwiseLayer[Ramp], LinearLayer[2]}]],
        data, MaxTrainingRounds -> 200, "LearningRate" -> 0.1, "WeightDecay" -> 0.0]);
    tw = (SeedRandom[9]; NetTrain[
        TFromNet[NetInitialize @ NetChain[{LinearLayer[8, "Input" -> 3], ElementwiseLayer[Ramp], LinearLayer[2]}]],
        data, MaxTrainingRounds -> 200, "LearningRate" -> 0.1, "WeightDecay" -> 0.1]);
    With[{wn = (t |-> Total[Map[(Total[Flatten[Abs @ Normal @ TTensorData @ TRealize @ #]] &), TNetParams[t]]])},
        {N[Count[MapThread[#1 === #2 &, {TNetPredict[tw, data[[All, 1]]], data[[All, 2]]}], True] / 4] >= 0.75,
         wn[tw] < wn[t0]}
    ],
    {True, True},
    TestID -> "training/weight-decay-shrinks-weights"
]
