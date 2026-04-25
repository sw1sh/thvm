#!/usr/bin/env wolframscript
(* train.wls -- 20 Adam steps on a tiny synthetic 4-feature
   3-class classification problem.  Architecture: a single
   linear layer (W:{3,4} @ x:{1,4} + b:{3}) followed by softmax
   and cross-entropy loss.  Run from the repo root.

   The dataset is 3 hand-picked samples, each with a clean
   class-1 / class-2 / class-3 signal.  20 Adam steps overfit
   trivially, demonstrating end-to-end gradient flow on the
   smallest training graph the runtime supports.
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

debugPrint[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

SeedRandom[7];
TInit[];

(* === Synthetic dataset: 3 samples, 3 classes, 4 features. === *)
samples = NumericArray[
    {{1.0,  0.2, -0.1,  0.0},
     {-0.3, 1.0,  0.1, -0.2},
     {0.1, -0.2,  1.0,  0.3}}, "Real32"];
labels  = {0, 1, 2};

(* === Glorot-init the weights. === *)
fanIn = 4;
wHost = NumericArray[
    RandomVariate[NormalDistribution[0., Sqrt[2.0 / fanIn]], {3, 4}],
    "Real32"];
bHost = NumericArray[ConstantArray[0., 3], "Real32"];

(* === One forward pass of the linear+softmax+CE chain. ===
   Returns {loss_TTerm, {weight_TTerms_in_order}}. *)
linearForward[wH_, bH_, xH_, yLabel_] := Block[{w, b, x, h, p, target, loss},
    w  = TTensorCreate @ wH;
    b  = TTensorCreate @ bH;
    x  = TTensorCreate @ NumericArray[xH, "Real32"];
    h  = TUOpAdd[TMatVec[w, x], b];
    p  = TSoftmax[h];
    target = TTensorCreate @ NumericArray[
        UnitVector[3, yLabel + 1], "Real32"];
    loss = TCrossEntropyLoss[p, target];
    {loss, {w, b}}
]

(* === One Adam step: forward, realize loss, realize each grad, then
       hand off to TAdamHostStep for the parameter update. *)
linearStep[hosts_, t_] := Block[{
    wH, bH, mAll, vAll,
    sampleIdx, x, label, loss, weights, lossR, grads,
    wHnew, bHnew, mNew, vNew
},
    {wH, bH, mAll, vAll} = hosts;
    sampleIdx = Mod[t - 1, Length[labels]] + 1;
    x         = Normal[samples][[sampleIdx]];
    label     = labels[[sampleIdx]];
    TInit[];   (* fresh runtime state per step keeps the heap small *)
    TReset[];
    {loss, weights} = linearForward[wH, bH, x, label];
    lossR = First @ Normal @ TTensorData @ TRealize @ loss;
    grads = Map[
        NumericArray[Normal @ TTensorData @ TRealize @ TGrad[loss, #],
                     "Real32"] &,
        weights];
    {{wHnew, bHnew}, mNew, vNew} = TAdamHostStep[
        {wH, bH}, grads, mAll, vAll,
        0.1, 0.9, 0.999, 1.0*^-8, t];
    {lossR, {wHnew, bHnew, mNew, vNew}}
]

(* === Run 20 Adam steps. === *)
{m0, v0} = TAdamHostInit[{wHost, bHost}];
state    = {wHost, bHost, m0, v0};

debugPrint["step  loss"];
Do[
    Module[{lossR, newState},
        {lossR, newState} = linearStep[state, t];
        state = newState;
        debugPrint[StringPadLeft[ToString[t], 4],
                   "  ",
                   ToString @ NumberForm[lossR, {6, 4}]]
    ],
    {t, 1, 20}
];
