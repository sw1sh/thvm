#!/usr/bin/env wolframscript
(* baseline.wls -- bm3 of the bench arc.

   Runs the lenet-mnist + beautiful-mnist baseline benches on the
   ACTIVE backend (CPU by default; THVM_BACKEND=metal for Metal),
   prints the TBench reports to stdout, and saves
   wl/Examples/_bench/baseline-{backend}-{net}.gantt.png snapshots
   alongside each result.

   Run twice (CPU + Metal) to populate docs/bench-baseline.md;
   for now we just emit the raw bench Associations and PNG paths.
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

debugPrint[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

backendLabel = If[ Environment["THVM_BACKEND"] === "metal",
                   "metal", "cpu"];

SeedRandom[42];

glorot[shape_List] := With[{
    fanIn = If[Length[shape] >= 2, Times @@ Drop[shape, 1], shape[[1]]]
},
    NumericArray[
        RandomVariate[NormalDistribution[0., Sqrt[2.0 / fanIn]], shape],
        "Real32"]
]
zerosArr[shape_List] := NumericArray[ConstantArray[0., shape], "Real32"]

(* === lenet-mnist: full Adam step (forward + backward + update) === *)
batch  = TMnistBatch[1];
img    = Normal[TTensorData @ batch["images"]][[1]];
label  = Normal[TTensorData @ batch["labels"]][[1]];

lenetInit[] := Block[{w1H, b1H, w2H, b2H, w3H, b3H, w4H, b4H},
    TInit[];
    TReset[];
    w1H = glorot[{6, 1, 5, 5}];   b1H = zerosArr[{6}];
    w2H = glorot[{16, 6, 5, 5}];  b2H = zerosArr[{16}];
    w3H = glorot[{120, 256}];      b3H = zerosArr[{120}];
    w4H = glorot[{10, 120}];       b4H = zerosArr[{10}];
    {w1H, b1H, w2H, b2H, w3H, b3H, w4H, b4H,
     TAdamHostInit[{w1H, b1H, w2H, b2H, w3H, b3H, w4H, b4H}]}
]

lenetForward[wH_, img_, label_] := Block[{
    w1, b1, w2, b2, w3, b3, w4, b4, x, h1, r1, p1, h2, r2, p2,
    flat, h3, r3, h4, probs, target, loss
},
    w1 = TTensorCreate @ wH[[1]];   b1 = TTensorCreate @ wH[[2]];
    w2 = TTensorCreate @ wH[[3]];   b2 = TTensorCreate @ wH[[4]];
    w3 = TTensorCreate @ wH[[5]];   b3 = TTensorCreate @ wH[[6]];
    w4 = TTensorCreate @ wH[[7]];   b4 = TTensorCreate @ wH[[8]];
    x  = TTensorCreate @ NumericArray[img, "Real32"];
    h1 = TConv2D[x, w1, b1];   r1 = TReLU[h1];
    p1 = TUOpReduce[TUOpReduce[
            TUOpReshape[r1, {6, 12, 2, 12, 2}], 2, "MAX"], 3, "MAX"];
    h2 = TConv2D[p1, w2, b2];  r2 = TReLU[h2];
    p2 = TUOpReduce[TUOpReduce[
            TUOpReshape[r2, {16, 4, 2, 4, 2}], 2, "MAX"], 3, "MAX"];
    flat = TUOpReshape[p2, {1, 256}];
    h3   = TUOpAdd[TMatVec[w3, flat], b3];  r3 = TReLU[h3];
    h4   = TUOpAdd[TMatVec[w4, TUOpReshape[r3, {1, 120}]], b4];
    probs = TSoftmax[h4];
    target = TTensorCreate @ NumericArray[
        UnitVector[10, label + 1], "Real32"];
    loss = TCrossEntropyLoss[probs, target];
    {loss, {w1, b1, w2, b2, w3, b3, w4, b4}}
]

lenetStep[hosts_, t_] := Block[{
    weightsH, mAll, vAll, loss, weights, lossR, gradTerms, grads,
    hostsNew, mNew, vNew
},
    weightsH = Take[hosts, 8];
    {mAll, vAll} = hosts[[9]];
    TInit[];
    TReset[];
    {loss, weights} = lenetForward[weightsH, img, label];
    lossR = First @ Normal @ TTensorData @ TRealize @ loss;
    (* TGradMany builds one UOP_GRAD covering all 8 weights; the
       multi-target chain rule lowers to a TAG_CTR of unary grads
       sharing the forward DAG via heap-loc identity, so the
       per-realize memo emits ONE set of forward kernels for the
       whole pack.  Each result still gets per-grad TTermUnpin
       after host extract so transient backward bufs are freed. *)
    gradTerms = TGradMany[loss, weights];
    grads = Map[
        Module[{hostArr},
            hostArr = NumericArray[Normal @ TTensorData @ TRealize @ #,
                                   "Real32"];
            TTermUnpin[#];
            hostArr
        ] &,
        gradTerms];
    {hostsNew, mNew, vNew} = TAdamHostStep[weightsH, grads, mAll, vAll,
                                            0.05, 0.9, 0.999, 1.0*^-8, t + 1];
    Append[hostsNew, {mNew, vNew}]
]

(* === beautiful-mnist: forward-only (backward exceeds KERNELS_CAP
       at this arch size; bm4's slot allocator unblocks training). *)
beautifulInit[] := Block[{w1H, b1H, w2H, b2H, w3H, b3H},
    TInit[];
    TReset[];
    w1H = glorot[{32, 1, 5, 5}];     b1H = zerosArr[{32}];
    w2H = glorot[{64, 32, 5, 5}];    b2H = zerosArr[{64}];
    w3H = glorot[{10, 6400}];         b3H = zerosArr[{10}];
    {w1H, b1H, w2H, b2H, w3H, b3H, img}
]

beautifulStep[hosts_, t_] := Block[{
    w1, b1, w2, b2, w3, b3, x, h1, r1, h2, r2, p2, flat, h3, probs
},
    TInit[];
    TReset[];
    w1 = TTensorCreate @ hosts[[1]];   b1 = TTensorCreate @ hosts[[2]];
    w2 = TTensorCreate @ hosts[[3]];   b2 = TTensorCreate @ hosts[[4]];
    w3 = TTensorCreate @ hosts[[5]];   b3 = TTensorCreate @ hosts[[6]];
    x  = TTensorCreate @ NumericArray[hosts[[7]], "Real32"];
    h1 = TConv2D[x,  w1, b1];   r1 = TReLU[h1];
    h2 = TConv2D[r1, w2, b2];   r2 = TReLU[h2];
    p2 = TUOpReduce[TUOpReduce[
            TUOpReshape[r2, {64, 10, 2, 10, 2}], 2, "MAX"], 3, "MAX"];
    flat = TUOpReshape[p2, {1, 6400}];
    h3   = TUOpAdd[TMatVec[w3, flat], b3];
    probs = TSoftmax[h3];
    TRealize @ probs;
    hosts
]

(* === Run the benches and emit Gantt PNGs. === *)
benches = {
    <|"Name"     -> "lenet-mnist (Adam step)",
      "InitFn"   -> lenetInit,
      "StepFn"   -> lenetStep,
      "NumSteps" -> 4|>,
    <|"Name"     -> "beautiful-mnist (forward only)",
      "InitFn"   -> beautifulInit,
      "StepFn"   -> beautifulStep,
      "NumSteps" -> 4|>
};

debugPrint["=== bm3 baseline / backend = ", backendLabel, " ==="];
Do[
    Module[{result, ganttPath, slug},
        result = TBench[spec];
        debugPrint[""];
        debugPrint @ ToString[TBenchReport[result], OutputForm];
        slug = StringReplace[
            ToLowerCase @ StringSplit[result["name"]][[1]],
            {"(" -> "", ")" -> "", "," -> ""}];
        ganttPath = "wl/Examples/_bench/baseline-" <>
                    backendLabel <> "-" <> slug <> ".svg";
        Export[ganttPath, TMemoryPlanGantt[TMemoryPlan[]],
            Background -> White];
        debugPrint["  TMemoryPlanGantt -> ", ganttPath]
    ],
    {spec, benches}
];
