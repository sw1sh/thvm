#!/usr/bin/env wolframscript
(* memory-probe.wls -- the linear-train counterpart of
   wl/Examples/lenet-mnist/memory-probe.wls.  Runs ONE training
   step (forward + loss + backward + Adam update) and prints
   the per-phase tensor / kernel / buf deltas, then captures a
   TMemoryPlan and exports a Gantt PNG.

   The graph is small enough (~9 kernels) that the Gantt is
   readable end-to-end -- exactly the example to use when
   working on lifetime-aware scheduling and slot reuse.

   Run from the repo root:
       wolframscript -f wl/Examples/linear-train/memory-probe.wls
       THVM_BACKEND=metal wolframscript -f wl/Examples/linear-train/memory-probe.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

debugPrint[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

SeedRandom[7];
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
]

(* === Setup: Glorot init + a single hand-picked sample. === *)
xHost = NumericArray[{1.0, 0.2, -0.1, 0.0}, "Real32"];
yLabel = 0;

wHost = NumericArray[
    RandomVariate[NormalDistribution[0., Sqrt[2.0 / 4]], {3, 4}],
    "Real32"];
bHost = NumericArray[ConstantArray[0., 3], "Real32"];

probe["After TInit", Function[Null]];

(* === Forward pass. === *)
w = TTensorCreate @ wHost;
b = TTensorCreate @ bHost;
x = TTensorCreate @ xHost;
target = TTensorCreate @ NumericArray[
    UnitVector[3, yLabel + 1], "Real32"];

probe["After tensor upload (W, b, x, target)", Function[Null]];

probe["After forward + loss materialize",
    Function[
        h    = TUOpAdd[TMatVec[w, x], b];
        p    = TSoftmax[h];
        loss = TCrossEntropyLoss[p, target];
        TRealize[loss]
    ]
];

probe["After TGrad[loss, w] materialize",
    Function[
        TRealize @ TGrad[loss, w]
    ]
];

probe["After TGrad[loss, b] materialize",
    Function[
        TRealize @ TGrad[loss, b]
    ]
];

debugPrint[""];
debugPrint["Final totals:"];
debugPrint["  TenDescs : ", TTensCount[]];
debugPrint["  Buf bytes: ",
           Round[TTotalBufBytes[] / 1024.0, 0.1], " KiB"];
debugPrint["  Kernels  : ", TKernelCount[]];

(* === TMemoryPlan summary + Gantt export. === *)
debugPrint[""];
debugPrint @ ToString[TMemoryPlanReport[TMemoryPlan[]], OutputForm];

Module[{plan = TMemoryPlan[], backend, path},
    backend = If[ Environment["THVM_BACKEND"] === "metal", "metal", "cpu"];
    path    = "wl/Examples/linear-train/memory-plan-" <> backend <> ".png";
    Export[path, TMemoryPlanGantt[plan],
        ImageResolution -> 144, Background -> White];
    debugPrint[""];
    debugPrint["TMemoryPlanGantt -> ", path]
]
