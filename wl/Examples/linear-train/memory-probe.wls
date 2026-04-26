#!/usr/bin/env wolframscript
(* memory-probe.wls -- linear-train forward + multi-target backward
   in ONE realize.  Captures the post-realize TMemoryPlan + heap
   diagram so the reader sees what the fused scheduler actually
   produces (kernels, bufs, lifecycles) without per-grad re-realize
   noise.

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

(* === Setup: Glorot init + a single hand-picked sample. === *)
xHost = NumericArray[{1.0, 0.2, -0.1, 0.0}, "Real32"];
yLabel = 0;

wHost = NumericArray[
    RandomVariate[NormalDistribution[0., Sqrt[2.0 / 4]], {3, 4}],
    "Real32"];
bHost = NumericArray[ConstantArray[0., 3], "Real32"];

w = TTensorCreate @ wHost;
b = TTensorCreate @ bHost;
x = TTensorCreate @ xHost;
target = TTensorCreate @ NumericArray[
    UnitVector[3, yLabel + 1], "Real32"];

(* === Forward + multi-target backward as a single graph: the loss
   sink is a UOP graph; TGradMany builds ONE UOP_GRAD packing all
   parameter targets into a TAG_CTR; the single TRealize inside
   TGradMany fires forward + backward + the CTR descent in one shot.
   MSE loss instead of softmax+CE because the softmax+CE backward
   currently has a UOp pattern visit() can't compile (see CHANGELOG
   note about thvm_materialize bail/rewind); MSE keeps the same
   structural shape (Linear layer + per-element loss + reduce) and
   exercises the same materialize+grad path. *)
h    = TUOpAdd[TMatVec[w, x], b];
loss = TMSELoss[h, target];
grads = TGradMany[loss, {w, b}];

debugPrint["TGradMany[loss, {w, b}] returned ", Length[grads], " grads"];
debugPrint["Final totals after one realize:"];
debugPrint["  TenDescs : ", TTensCount[]];
debugPrint["  Buf bytes: ",
           Round[TTotalBufBytes[] / 1024.0, 0.1], " KiB"];
debugPrint["  Kernels  : ", TKernelCount[] - 1];

(* === TMemoryPlan summary + Gantt export. === *)
debugPrint[""];
debugPrint @ ToString[TMemoryPlanReport[TMemoryPlan[]], OutputForm];

Module[{plan = TMemoryPlan[], backend, path},
    backend = If[ Environment["THVM_BACKEND"] === "metal", "metal", "cpu"];
    path    = "wl/Examples/linear-train/memory-plan-" <> backend <> ".svg";
    Export[path, TMemoryPlanGantt[plan], Background -> White];
    debugPrint[""];
    debugPrint["TMemoryPlanGantt -> ", path]
]

(* === Heap diagram: DiagramNetwork view (one Diagram per agent),
   arranged on a Grid so the layout matches the IC string-diagram
   renderings under docs/images/diagrams/.  PortLabels off because
   the loss graph carries enough TenDescs that the per-port text
   overwhelms the agent shapes. *)
Module[{backend, path},
    backend = If[ Environment["THVM_BACKEND"] === "metal", "metal", "cpu"];
    path    = "wl/Examples/linear-train/heap-diagram-" <> backend <> ".svg";
    Export[path,
        THeapDiagram[loss]["Arrange"]["Grid", "PortLabels" -> False],
        Background -> White];
    debugPrint["THeapDiagram   -> ", path]
]
