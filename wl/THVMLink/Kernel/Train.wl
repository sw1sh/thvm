(* ::Package:: *)
(* Train.wl - TNetTrain[net_NetChain, data, prop, opts]: train a NetChain by
   lifting its batched forward and emitting the optimiser as an INERT
   interaction-net term.  A lifted TFromNet[net] term is inference-only.

   Following built-in NetTrain's `prop` argument:
     TNetTrain[net, data, "TrainingNet"]   returns the inert training-loop
                                            TTerm -- nothing has run yet;
                                            `TWnf` drives it.
     TNetTrain[net, data]                   convenience: TWnf's that term and
                                            returns the trained forward term.

   The step -- forward, categorical cross-entropy, the shared backward, and
   one in-place SGD ASSIGN per weight -- is materialised ONCE.  It is wrapped
   in a recursive lambda whose base case IS the forward term:
       loop(k) = if k == 0 then fwd else <fire every weight's step>; loop(k-1)
   so `TWnf @ loop(MaxTrainingRounds)` fires every step (training the weights
   in place) and reduces to the trained forward term.  The optimiser IS one
   interaction-net term; TWnf is the only driver.

   Data: a list of `input -> class` rules, or a dataset name string
   ("MNIST" -> ResourceData["MNIST", "TrainingData"]).  Inputs (Images /
   arrays) are reshaped to the net's input shape internally -- no host-side
   ImageData/Flatten ceremony at the call site.  Flat-input nets only for
   now (the batched forward keeps the batch axis for a rank-2 input;
   FlattenLayer / conv batching is a TFromNet follow-up). *)

BeginPackage["THVMLink`"];

TNetTrain::usage = "TNetTrain[net, data, \"TrainingNet\", opts] returns an INERT TTerm: the whole optimiser as a recursive interaction-net term whose base case is the forward.  TWnf drives it -- firing every training step in place and reducing to the trained forward term.  TNetTrain[net, data, opts] is the convenience form that TWnf's it for you.  `net` is a NetChain (e.g. NetModel[\"LeNet\"]), whose batched forward is lifted directly; `data` is a list of `input -> class` rules or a dataset name (\"MNIST\").  Inputs are reshaped to the net's input shape internally.  A lifted TFromNet[net] term is inference-only -- training it would need the batched forward re-lifted, which needs the NetChain -- so pass the NetChain instead.  Options: MaxTrainingRounds, \"LearningRate\", \"Momentum\" (0 = plain SGD; > 0 adds a persistent velocity per weight for SGD-with-momentum -- use a smaller LearningRate, the effective rate is lr/(1-momentum)), \"Method\" (\"SGD\" default, or \"Adam\" for uncorrected ADAM with persistent m/v moments and standard betas -- use a small LearningRate, ~0.001 on MNIST-scale problems; the adaptive step is roughly lr per element), \"WeightDecay\" (0 = none; > 0 adds L2 regularization by folding wd*p into each weight's gradient, for all optimizers), TargetDevice.";

TNetPredict::usage = "TNetPredict[trainedNet, inputs] runs the trained forward term on `inputs` (a list of the same length as the training batch) and returns the predicted integer classes.";

TNetTrain::needsnet = "Training a lifted TFromNet term needs a batched re-lift, which needs the NetChain; pass the NetChain (NetModel/NetChain) to TNetTrain. The lifted term is for inference.";

Begin["`Private`"];

Options[TNetTrain] = {
    MaxTrainingRounds -> 30,
    "LearningRate"    -> 0.05,
    "Momentum"        -> 0.0,
    "Method"          -> "SGD",
    "WeightDecay"     -> 0.0,
    "Loss"            -> Automatic,
    TargetDevice      -> "CPU"
};

$trainLoopCounter = 0;

freshTrainName[] := (
    $trainLoopCounter += 1;
    "thvm_train_loop_" <> ToString[$trainLoopCounter]
)

(* === data resolution === *)
resolveData[name_String] := ResourceData[name, "TrainingData"]
resolveData[data_List]   := data
resolveData[data_]       := Normal[data]      (* Dataset / EntityValue / ... *)

(* one example -> a flat-or-shaped real array matching the net's input. *)
toInput[img_Image, inShape_List]      := ArrayReshape[ImageData[img], inShape]
toInput[a_NumericArray, inShape_List] := ArrayReshape[Normal[a], inShape]
toInput[a_, inShape_List]             := ArrayReshape[a, inShape]

batchTensors[data_List, inShape_List, nClasses_Integer] := {
    TTensorCreate @ NumericArray[toInput[#[[1]], inShape] & /@ data, "Real32"],
    TTensorCreate @ NumericArray[(UnitVector[nClasses, # + 1] &) /@ data[[All, 2]], "Real32"]
}

(* === the inert recursive SGD loop ===
   TWnf @ loop(rounds) fires every weight's step (training in place) and
   reduces to the 0 base case; the trained forward is the registered `fwd`
   (its weight buffers are updated in place).  Each weight's step is
   TMaterialize[TNf[...]] (re-fireable -- the kernel is not baked to its
   output; see the redex_fire UOP_KERNEL carve-out). *)
buildLoop[params_List, grads_List, lr_TTerm, name_String] := Module[{stepRefs},
    stepRefs = MapThread[
        Function[{p, g, i},
            With[{nm = name <> "_p" <> ToString[i]},
                TDef[nm, TMaterialize[TNf[TAssign[p, TUOpAdd[p, TUOpNeg[TUOpMul[lr, g]]]]]]];
                TRef[nm]]],
        {params, grads, Range[Length[params]]}];
    TDef[name,
        TLam[k,
            TIfZero[k, TUOpConst[0.0],
                Fold[
                    TPriForce[#2, #1] &,
                    TApp[TRef[name], TOp2["-", k, TNum[1]]],
                    Reverse[stepRefs]]]]];
    name
]

(* === inert recursive SGD-with-MOMENTUM loop ===
   Same shape as buildLoop, but each weight carries a persistent velocity
   buffer `v` (TZeros init).  Each step is TWO in-place assigns fired in
   order: first  v <- beta*v + g,  then  p <- p - lr*v  (reading the just-
   updated v).  The step worklist fires stepRefs in order, so listing the
   v-assign before the p-assign per weight gives the correct read-after-write
   (p sees the new v).  Converges markedly faster than plain SGD. *)
buildLoopMomentum[params_List, grads_List, vels_List, lr_TTerm, beta_TTerm, name_String] :=
    Module[{stepRefs},
        stepRefs = Flatten @ MapThread[
            Function[{p, g, v, i},
                With[{vnm = name <> "_v" <> ToString[i], pnm = name <> "_p" <> ToString[i]},
                    TDef[vnm, TMaterialize[TNf[TAssign[v, TUOpAdd[TUOpMul[beta, v], g]]]]];
                    TDef[pnm, TMaterialize[TNf[TAssign[p, TUOpAdd[p, TUOpNeg[TUOpMul[lr, v]]]]]]];
                    {TRef[vnm], TRef[pnm]}]],
            {params, grads, vels, Range[Length[params]]}];
        TDef[name,
            TLam[k,
                TIfZero[k, TUOpConst[0.0],
                    Fold[
                        TPriForce[#2, #1] &,
                        TApp[TRef[name], TOp2["-", k, TNum[1]]],
                        Reverse[stepRefs]]]]];
        name
    ]

(* === trained-model registry (slot for TNetPredict) ===
   keyed by the forward term's id, plus a `$thvmLastTrained` fallback: a
   recursive loop returns a reduced COPY of the base-case forward whose id
   differs from the one we registered, but it shares the (in-place trained)
   weights and input slot, so the most-recent registration still predicts. *)
$thvmTrainRegistry = <||>;
$thvmLastTrained   = Missing[];

registerTrained[fwd_TTerm, xSlot_TTerm, inShape_List, nBatch_Integer] := With[{
    entry = <|"Fwd" -> fwd, "Input" -> xSlot, "InShape" -> inShape, "Batch" -> nBatch|>
},
    $thvmTrainRegistry[TTermVal[fwd]] = entry;
    $thvmLastTrained = entry;
    fwd
]

argmaxRows[out_] := (First @ Ordering[#, -1] - 1) & /@ out

(* === strip a trailing SoftmaxLayer (train on logits) ===
   On the LeNet NetModel the output NetDecoder dangles, so NetDrop[net,-1],
   NetTake[net,{1,-2}], NetChain[Most@..] and NetReplacePart all fail with an
   internal error.  Rebuilding each weight-bearing layer from its arrays into a
   fresh NetChain detaches that decoder; we fold over the layers, dropping a
   trailing SoftmaxLayer, into a fresh input-shaped chain.  Non-softmax nets
   round-trip unchanged (same weights), so this is safe to always apply. *)
rebuildLayer[l_ConvolutionLayer] := ConvolutionLayer[
    NetExtract[l, "OutputChannels"], NetExtract[l, "KernelSize"],
    "Weights" -> NetExtract[l, "Weights"], "Biases" -> NetExtract[l, "Biases"]]
rebuildLayer[l_LinearLayer] := LinearLayer[
    NetExtract[l, "OutputDimensions"],
    "Weights" -> NetExtract[l, "Weights"], "Biases" -> NetExtract[l, "Biases"]]
rebuildLayer[l_] := l

stripSoftmax[net_NetChain] := Module[{n, keep, layers},
    n      = Length[net];
    keep   = If[ Head[NetExtract[net, n]] === SoftmaxLayer, n - 1, n];
    layers = Table[rebuildLayer[NetExtract[net, i]], {i, keep}];
    NetChain[layers, "Input" -> netInputShape[net]]
]

(* === inert recursive ADAM loop (uncorrected) ===
   Per weight, persistent first/second-moment buffers m, v (TZeros init).
   Each step is THREE in-place assigns fired in order m, v, then p:
     m <- b1*m + (1-b1)*g
     v <- b2*v + (1-b2)*g*g
     p <- p - lr*m/(sqrt(v) + eps)   (reads the just-updated m, v)
   Standard betas (0.9, 0.999) + eps 1e-8.  Bias correction is omitted: the
   step kernels are fixed and carry no step counter, and the m/sqrt(v) ratio
   keeps the step well-scaled (~lr per element) regardless.  Use a small
   LearningRate (~0.005); ADAM's adaptive step is roughly lr in magnitude. *)
buildLoopAdam[params_List, grads_List, ms_List, vs_List, lr_TTerm, name_String] :=
    Module[{stepRefs, b1, b2, om1, om2, eps},
        b1 = TUOpConst[0.9];  b2 = TUOpConst[0.999];
        om1 = TUOpConst[0.1]; om2 = TUOpConst[0.001];  eps = TUOpConst[1.*^-8];
        stepRefs = Flatten @ MapThread[
            Function[{p, g, m, v, i},
                With[{mnm = name <> "_m" <> ToString[i], vnm = name <> "_v" <> ToString[i],
                      pnm = name <> "_p" <> ToString[i]},
                    TDef[mnm, TMaterialize[TNf[TAssign[m,
                        TUOpAdd[TUOpMul[b1, m], TUOpMul[om1, g]]]]]];
                    TDef[vnm, TMaterialize[TNf[TAssign[v,
                        TUOpAdd[TUOpMul[b2, v], TUOpMul[om2, TUOpMul[g, g]]]]]]];
                    TDef[pnm, TMaterialize[TNf[TAssign[p,
                        TUOpAdd[p, TUOpNeg[TUOpMul[lr,
                            TUOpMul[m, TUOpRecip[TUOpAdd[TUOpSqrt[v], eps]]]]]]]]]];
                    {TRef[mnm], TRef[vnm], TRef[pnm]}]],
            {params, grads, ms, vs, Range[Length[params]]}];
        TDef[name,
            TLam[k,
                TIfZero[k, TUOpConst[0.0],
                    Fold[
                        TPriForce[#2, #1] &,
                        TApp[TRef[name], TOp2["-", k, TNum[1]]],
                        Reverse[stepRefs]]]]];
        name
    ]

(* === build the inert training-loop term from a Wolfram net (shared core) ===
   Strip a trailing SoftmaxLayer (train on logits), derive the input shape,
   build the forward over a fresh input slot, then the cross-entropy +
   shared-backward + inert SGD loop.  A lifted LAM is inference-only; the
   NetChain entry point lifts the batched forward here directly. *)
inertTrainFrom[net_NetChain, dataSpec_, rounds_Integer, opt_Association] := Module[{
    logitNet, inShape, nClasses, data, xSlot, tgtSlot, fwd, params
},
    logitNet = stripSoftmax[net];
    inShape  = netInputShape[logitNet];
    If[ inShape === $Failed,
        Message[TFromNet::noinput, net];
        Return[$Failed]
    ];
    data     = resolveData[dataSpec];
    nClasses = Max[data[[All, 2]]] + 1;
    {xSlot, tgtSlot} = batchTensors[data, inShape, nClasses];
    (* The param terms are the float-leaf TTerms baked into THIS batched
       forward -- the very tensors the inert SGD loop updates in place.
       Read them straight off the graph (TNetParams), MINUS the input slot
       (itself a float TEN leaf of the forward, but not a weight). *)
    fwd    = TFromNet[logitNet, xSlot];
    params = trainableParams[fwd, xSlot];
    inertTrainCore[fwd, params, xSlot, tgtSlot, inShape, Length[data], rounds, opt]
]

(* the trainable weights of a forward built over a concrete input slot: the
   float leaves of `fwd` minus the input slot `xSlot` (a float TEN leaf too,
   but the data to feed, not a weight).  The forward-UOP TNetParams[fwd, x]
   form does exactly this. *)
trainableParams[fwd_, xSlot_TTerm] := TNetParams[fwd, xSlot]

(* === inert training-loop core over a prebuilt batched forward ===
   `fwd` is the batched forward TTerm (logits), `params` its trainable
   float-leaf handles, `xSlot` the input slot, `tgtSlot` the one-hot target.
   The NetChain entry point (inertTrainFrom) lifts the batched forward and
   hands it here. *)
inertTrainCore[fwd_, params_, xSlot_, tgtSlot_, inShape_, nData_,
               rounds_Integer, opt_Association] := Module[{
    methodVal, momentumVal, wdVal, loss, grads, effGrads, lr, name, vels, ms, vs
},
    methodVal = opt["Method"];  momentumVal = opt["Momentum"];  wdVal = opt["WeightDecay"];
    lr     = TUOpConst[N[opt["LearningRate"]]];
    loss   = TCategoricalCrossEntropy[fwd, tgtSlot];
    grads  = TGrad[loss, params];
    effGrads = If[ TrueQ[wdVal > 0],
        With[{wd = TUOpConst[N[wdVal]]},
            MapThread[TUOpAdd[#1, TUOpMul[wd, #2]] &, {grads, params}]],
        grads];
    name = freshTrainName[];
    Which[
        methodVal === "Adam",
            ms = (TZeros[TTensorShape[#]] &) /@ params;
            vs = (TZeros[TTensorShape[#]] &) /@ params;
            buildLoopAdam[params, effGrads, ms, vs, lr, name],
        TrueQ[momentumVal > 0],
            vels = (TZeros[TTensorShape[#]] &) /@ params;
            buildLoopMomentum[params, effGrads, vels, lr, TUOpConst[N[momentumVal]], name],
        True,
            buildLoop[params, effGrads, lr, name]
    ];
    registerTrained[fwd, xSlot, inShape, nData];
    TApp[TRef[name], TNum[rounds]]
]

(* === lifted LAM is inference-only ===
   A lifted TFromNet[net] LAM bakes its input shape into the forward, so
   training over a (different) batch size needs the forward re-lifted at the
   batch shape -- and that re-lift needs the NetChain (nothing stores the
   net).  Guide the caller to pass the NetChain to TNetTrain instead, which
   lifts the batched forward directly (inertTrainFrom). *)
inertTrainTerm[_TTerm, _, _Integer, _Association] := (
    Message[TNetTrain::needsnet]; $Failed)

(* === public dispatch ===
   `prop` argument like built-in NetTrain: "TrainingNet" returns the inert
   loop term (nothing has run); the no-prop form TWnf's it and returns the
   trained forward (the in-place-trained registered term, not the reduced
   copy TWnf hands back).  A NetChain (e.g. NetModel["LeNet"]) builds the
   logits forward directly -- no round trip.  A lifted LAM (_TTerm) is
   inference-only and routes to a guiding Message (inertTrainTerm).  The
   optimiser config is bundled into one `opt` association. *)
TNetTrain[net : (_TTerm | _NetChain), dataSpec_, "TrainingNet", opts : OptionsPattern[]] := With[{
    rounds = OptionValue[MaxTrainingRounds],
    opt    = <|
        "LearningRate" -> OptionValue["LearningRate"],
        "Momentum"     -> OptionValue["Momentum"],
        "Method"       -> OptionValue["Method"],
        "WeightDecay"  -> OptionValue["WeightDecay"]
    |>
},
    If[ MatchQ[net, _TTerm],
        inertTrainTerm[net, dataSpec, rounds, opt],
        inertTrainFrom[net, dataSpec, rounds, opt]
    ]
]

TNetTrain[net : (_TTerm | _NetChain), dataSpec_, opts : OptionsPattern[]] := With[{
    t = TNetTrain[net, dataSpec, "TrainingNet", opts]
},
    If[ t === $Failed,
        $Failed,
        TWnf[t];                       (* drive the inert loop -> trains in place *)
        $thvmLastTrained["Fwd"]        (* the trained forward term *)
    ]
]

(* === backward-compatible {x, y} overload ===
   The earlier surface took explicit host arrays of inputs and one-hot
   targets and returned a handle association <|"Forward", "Input", "Params"|>
   that TNetPredict[handle, x] consumed.  Kept so existing callers (Tests,
   docs/Tutorials/Train.md, bench/mnist-train) and the same inert-loop
   semantics keep working: lift over an input slot, build the inert SGD loop,
   TWnf-drive it, and hand back the same association.  `net` is a raw
   NeuralNetworks net (NetChain / layer); `x`, `y` are rank-2 host arrays. *)
TNetTrain[net_, x_List, y_List, opts : OptionsPattern[]] := Module[{
    xS, yT, lr, rounds, lossFn, fwd, params, loss, grads, name
},
    xS     = TTensorCreate[N @ x];
    yT     = TTensorCreate[N @ y];
    lr     = TUOpConst[N @ OptionValue[TNetTrain, {opts}, "LearningRate"]];
    rounds = OptionValue[TNetTrain, {opts}, MaxTrainingRounds];
    lossFn = OptionValue[TNetTrain, {opts}, "Loss"] /. Automatic -> ({f, t} |-> TCategoricalCrossEntropy[f, t]);
    fwd    = TFromNet[net, xS];
    params = trainableParams[fwd, xS];
    loss   = lossFn[fwd, yT];
    grads  = TGrad[loss, params];
    name   = freshTrainName[];
    buildLoop[params, grads, lr, name];
    TWnf @ TApp[TRef[name], TNum[rounds]];
    <|"Forward" -> fwd, "Input" -> xS, "Params" -> params|>
]

(* The forward is built for a fixed batch size (the training batch), so the
   input slot has that leading dimension.  Predict in chunks of that size,
   padding the final partial chunk with the last input (those rows are
   discarded), so any number of inputs works. *)
TNetPredict[net_TTerm, inputs_List] := Module[{
    entry, xSlot, inShape, fwd, nBatch, n, padded, chunkPreds
},
    entry = Lookup[$thvmTrainRegistry, TTermVal[net], $thvmLastTrained];
    If[ MissingQ[entry], Return[$Failed]];
    xSlot   = entry["Input"];
    inShape = entry["InShape"];
    fwd     = entry["Fwd"];
    nBatch  = entry["Batch"];
    n       = Length[inputs];
    padded  = Join[inputs, ConstantArray[Last[inputs], Mod[-n, nBatch]]];
    chunkPreds = Function[chunk,
        TSet[xSlot, TTensorCreate @ NumericArray[toInput[#, inShape] & /@ chunk, "Real32"]];
        argmaxRows[Normal @ TTensorData @ TRealize @ fwd]
    ] /@ Partition[padded, nBatch];
    Take[Join @@ chunkPreds, n]
]

(* === backward-compatible {handle, x} overload ===
   Consumes the <|"Forward", "Input", "Params"|> handle the {x, y} TNetTrain
   overload returns: overwrite the input slot with a fresh same-shape batch
   and re-realise the forward, returning the raw logits rows (the caller takes
   the per-row argmax for a class). *)
TNetPredict[trained_Association, x_List] := (
    TRealize @ TAssign[trained["Input"], TTensorCreate[N @ x]];
    Normal @ TRealize @ trained["Forward"]
)

(* === UpValues: NetTrain[net_TTerm, ...] delegates to TNetTrain === *)
TTerm /: NetTrain[net_TTerm, dataSpec_, "TrainingNet", opts : OptionsPattern[]] :=
    TNetTrain[net, dataSpec, "TrainingNet", opts]
TTerm /: NetTrain[net_TTerm, dataSpec_, opts : OptionsPattern[]] :=
    TNetTrain[net, dataSpec, opts]

End[];
EndPackage[];
