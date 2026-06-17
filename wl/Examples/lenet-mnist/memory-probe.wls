#!/usr/bin/env wolframscript
(* memory-probe.wls -- report the memory + KernelEntry footprint
   of one LeNet training step (forward + backward + 1 Adam update).

   Mirrors verify.wls's setup but doesn't run all 4 steps -- the
   goal is to characterise per-step memory usage so we know what
   reuse / lifetime tracking would need to handle.  Numbers feed
   into docs/memory.md.

   Run from the repo root:
       wolframscript -f wl/Examples/lenet-mnist/memory-probe.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

debugPrint[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

SeedRandom[42];
TInit[];

probe[label_, fn_] := Module[{tensBefore, bufBefore, kBefore,
                              tensAfter, bufAfter, kAfter, result},
    tensBefore = TTensCount[];
    bufBefore  = TTotalBufBytes[];
    kBefore    = TKernelCount[];
    result     = fn[];
    tensAfter  = TTensCount[];
    bufAfter   = TTotalBufBytes[];
    kAfter     = TKernelCount[];
    debugPrint[label, ":"];
    debugPrint["  TenDescs added : ", tensAfter - tensBefore,
               " (cumulative ", tensAfter, ")"];
    debugPrint["  CPU buf bytes  : +",
               Round[(bufAfter - bufBefore) / 1024.0, 0.1], " KiB",
               " (cumulative ", Round[bufAfter / 1024.0, 0.1], " KiB)"];
    debugPrint["  KernelEntries  : +", kAfter - kBefore,
               " (cumulative ", kAfter, ")"];
    result
];

(* === Setup: load one MNIST sample + init LeNet weights. === *)
glorot[shape_List] := With[{
    fanIn = If[Length[shape] >= 2, Times @@ Drop[shape, 1], shape[[1]]]
},
    NumericArray[
        RandomVariate[NormalDistribution[0., Sqrt[2.0 / fanIn]], shape],
        "Real32"]
];
zerosArr[shape_List] := NumericArray[ConstantArray[0., shape], "Real32"];

probe["After TInit", Function[Null]];

batch = TMnistBatch[1];
img   = Normal @ TTensorData @ batch["images"];
label = Normal[TTensorData @ batch["labels"]][[1]];

probe["After loading 1-sample MNIST batch", Function[Null]];

(* LeNet init: 8 host arrays for the trainable weights. *)
hosts = <|
    "W1" -> glorot[{6, 1, 5, 5}],   "b1" -> zerosArr[{6}],
    "W2" -> glorot[{16, 6, 5, 5}],  "b2" -> zerosArr[{16}],
    "W3" -> glorot[{120, 256}],      "b3" -> zerosArr[{120}],
    "W4" -> glorot[{10, 120}],       "b4" -> zerosArr[{10}]
|>;

probe["After Glorot host-init (8 weight arrays, host-only)",
    Function[Null]];

(* === Forward pass: TFromNet through NetModel["LeNet"]. === *)
net = NetInitialize @ Quiet[NetModel["LeNet"], Import::nnincmpb];
x   = TTensorCreate @ NumericArray[img[[1]], "Real32"];

probe["After tensor upload (input + LeNet weights)",
    Function[
        TFromNet[net, x]
    ]
];

(* === Forward + loss (TCrossEntropyLoss). === *)
probe["After forward + loss materialize",
    Function[
        target = TTensorCreate @ NumericArray[
            UnitVector[10, label + 1], "Real32"];
        loss = TCrossEntropyLoss[TFromNet[net, x], target];
        TRealize[loss]
    ]
];

(* === Backward pass: TGrad wrt input. === *)
probe["After TGrad[loss, x] materialize",
    Function[
        TRealize @ TGrad[loss, x]
    ]
];

debugPrint[""];
debugPrint["Final totals:"];
debugPrint["  TenDescs : ", TTensCount[]];
debugPrint["  Buf bytes: ",
           Round[TTotalBufBytes[] / 1024.0, 0.1], " KiB ",
           "(", Round[TTotalBufBytes[] / 1024.0 / 1024.0, 0.01], " MiB)"];
debugPrint["  Kernels  : ", TKernelCount[]];

(* === TMemoryPlan summary (mp4 of the visualization arc) ===
   Top-5-largest / top-5-longest-lived bufs + status histogram +
   active-backend label, derived from the live KERNELS / TENS /
   {CPU,Metal}_BUFS state via static topological-depth analysis.
   No new runtime probes -- this is a snapshot of state already
   exposed by mp1's bridge tables. *)
debugPrint[""];
debugPrint @ ToString[TMemoryPlanReport[TMemoryPlan[]], OutputForm];

(* Render the Gantt to a sibling SVG so the visualization is
   browsable outside a notebook session AND zoom-friendly (the
   391-buf LeNet plot needs vector scaling to be legible).
   Filename encodes the active backend so CPU + Metal runs land
   at distinct paths. *)
Module[{plan = TMemoryPlan[], backend, path},
    backend = If[ Environment["DEV"] === "metal", "metal", "cpu"];
    path    = "wl/Examples/lenet-mnist/memory-plan-" <> backend <> ".svg";
    Export[path, TMemoryPlanGantt[plan], Background -> White];
    debugPrint[""];
    debugPrint["TMemoryPlanGantt -> ", path]
]
