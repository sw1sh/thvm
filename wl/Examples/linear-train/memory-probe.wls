#!/usr/bin/env wolframscript
(* memory-probe.wls -- linear-train forward + multi-target backward,
   captured at THREE distinct stages so the reader sees how the
   graph evolves through the IC + scheduler:

     1. Pure forward  -- just the loss UOp graph.
     2. With backward -- after building UOP_GRAD_MULTI and TNf-
                         reducing it (interact_grad walks the
                         chain rule; result is a graph of plain
                         elementwise + reduce + movement UOps,
                         no kernels yet, no kernels emitted).
     3. With kernels  -- after TMaterialize compiles the lazy
                         compute into UOP_KERNEL terms.

   Plus a TMemoryPlan Gantt at stage 3 (only stage where any
   kernels exist).

   Run from the repo root:
       wolframscript -f wl/Examples/linear-train/memory-probe.wls
       THVM_BACKEND=metal wolframscript -f wl/Examples/linear-train/memory-probe.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

debugPrint[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

$renderBudget = 90;

(* Try Grid arrange first (matches docs/images/diagrams/ style);
   fall back to the default layout if Grid takes too long.  Grid
   is O(n^2) in heap cells; for the post-grad chain (~150 cells)
   it doesn't finish in any reasonable budget, but the default
   layout still produces a usable (if uglier) diagram. *)
renderHeapDiagram[seed_, label_] := Module[{backend, path, gridOk},
    backend = If[ Environment["THVM_BACKEND"] === "metal", "metal", "cpu"];
    path    = "wl/Examples/linear-train/heap-diagram-" <> label
                <> "-" <> backend <> ".svg";
    gridOk = TimeConstrained[
        Export[path,
            THeapDiagram[seed]["Arrange"]["Grid", "PortLabels" -> False],
            Background -> White];
        True,
        $renderBudget,
        False];
    If[ gridOk,
        debugPrint[label, " heap diagram (Grid) -> ", path],
        (* Grid timed out; fall back to default layout. *)
        TimeConstrained[
            Export[path, THeapDiagram[seed], Background -> White];
            debugPrint[label, " heap diagram (default layout) -> ", path],
            $renderBudget,
            debugPrint[label, " heap diagram TIMED OUT (>", 2 $renderBudget, "s) -- skipped"]
        ]
    ]
]

SeedRandom[7];
TInit[];

(* === Setup.  Small dimensions so stages 2 and 3 stay renderable
   under the IC string-diagram layout: post-grad, the heap holds
   the unfolded chain rule for every weight scalar (~20x cells per
   element for this loss), and Wolfram's Grid arranger doesn't
   scale past ~50 agents in reasonable time.  3x4 weights would
   produce a ~400-cell heap which the renderer can't lay out. *)
xHost  = NumericArray[{1.0, 0.5}, "Real32"];
wHost  = NumericArray[{0.3, -0.2}, "Real32"];
target = NumericArray[{0.0}, "Real32"];

w = TTensorCreate @ wHost;
x = TTensorCreate @ xHost;
tgt = TTensorCreate @ target;

(* === Stage 1: pure forward.  Loss is a UOp graph -- no grads
   built, nothing materialized.  Diagram captures the unfused AST
   that the scheduler will compile. *)
loss = TMSELoss[TDot[w, x], tgt];

debugPrint["stage 1 (pure forward): heap=", THeapPos[],
           " kernels=", TKernelCount[] - 1];
renderHeapDiagram[loss, "1-forward"];

(* === Stage 2: forward + backward (still no kernels).  Build
   UOP_GRAD directly (bypass TGrad's auto-realize) and reduce via
   TNf so interact_grad walks the chain rule, emitting the
   backward UOps inline.  Single-target grad keeps the heap small
   enough for the IC string-diagram renderer.  thvm_materialize
   hasn't been called yet. *)
gy       = TUOpConst[1.0, "f32"];
gradTerm = TUOpGrad[loss, gy, w];
gradReduced = TNf[gradTerm];

debugPrint["stage 2 (after grad+nf): heap=", THeapPos[],
           " kernels=", TKernelCount[] - 1];
renderHeapDiagram[gradReduced, "2-with-backward"];

(* === Stage 3: kernels emitted.  TRealize (the wnf+nf+materialize
   fixed-point loop) compiles every reachable lazy-compute UOp into
   UOP_KERNEL terms and fires them.  TMaterialize alone wouldn't
   work here: thvm_materialize is all-or-nothing per call (a single
   bailing boundary rewinds the whole emission) and the post-grad
   chain has UOp patterns visit() can't compile in one pass; the
   loop in TRealize retries until convergence. *)
gradRealized = TRealize[gradReduced];

debugPrint["stage 3 (after realize): heap=", THeapPos[],
           " kernels=", TKernelCount[] - 1];
renderHeapDiagram[gradRealized, "3-with-kernels"];

(* === TMemoryPlan summary + Gantt export.  Only meaningful at
   stage 3 since stages 1-2 emit no KernelEntrys. *)
debugPrint[""];
debugPrint @ ToString[TMemoryPlanReport[TMemoryPlan[]], OutputForm];

Module[{plan = TMemoryPlan[], backend, path},
    backend = If[ Environment["THVM_BACKEND"] === "metal", "metal", "cpu"];
    path    = "wl/Examples/linear-train/memory-plan-" <> backend <> ".svg";
    Export[path, TMemoryPlanGantt[plan], Background -> White];
    debugPrint[""];
    debugPrint["TMemoryPlanGantt -> ", path]
]
