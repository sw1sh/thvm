#!/usr/bin/env wolframscript
(* Warm per-step wall time + training-memory delta for the Train.md
   LeNet-head benchmark: Conv2d[1->8,3x3] -> ReLU -> 2x2 max-pool ->
   Linear[1352->10], batch 64, softmax cross-entropy, eager SGD.

   Pairs with tinygrad_step.py (same net) for the Train.md "Time and
   memory against tinygrad" table.  Run:  wolframscript -file thvm_step.wls *)

PacletDirectoryLoad[FileNameJoin[{DirectoryName[$InputFileName], "..", "..", "wl", "THVMLink"}]];
Needs["THVMLink`"];

print[l_, v_] := WriteString["stdout", l, " => ", ToString[v, InputForm], "\n"];
(* The compute runs in this kernel process; ps on $ProcessID gives its
   own RSS (the wolframscript wrapper is a separate, idle process). *)
rss[] := ToExpression @ StringTrim @ ReadString["!ps -o rss= -p " <> ToString[$ProcessID]];

SeedRandom[7];
B = 64;
xb = TTensorCreate[N @ RandomReal[1, {B, 1, 28, 28}]];
yb = TTensorCreate[N @ Table[UnitVector[10, RandomInteger[{1, 10}]], B]];
w1 = TRequiresGrad @ TGlorot[{8, 1, 3, 3}]; b1 = TRequiresGrad @ TZeros[{8}];
w2 = TRequiresGrad @ TGlorot[{8*13*13, 10}]; b2 = TRequiresGrad @ TZeros[{10}];
params = {w1, b1, w2, b2};
logits = TLinear[ArrayReshape[TMaxPool2d[TReLU[TConv2D[xb, w1, b1]]], {B, 8*13*13}], w2, b2];

step := (
    TClearGrad /@ params;
    grads = TRealize @ TGrad[TCategoricalCrossEntropy[logits, yb], params];
    MapThread[TSet[#1, #1 + (-0.1)*#2] &, {params, grads}]
);

Do[step, {3}];                 (* warmup *)
base = rss[];
t = AbsoluteTiming[Do[step, {30}]][[1]];
print["thvm warm ms/step (eager, B=64, conv8+relu+maxpool+linear)", Round[1000.*t/30, 0.1]];
print["thvm training RSS delta MB", Round[(rss[] - base)/1024., 1]];
print["thvm WL-heap MaxMemoryUsed MB", Round[MaxMemoryUsed[]/1.*^6, 1]];
