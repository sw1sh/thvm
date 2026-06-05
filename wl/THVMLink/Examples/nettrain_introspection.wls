#!/usr/bin/env wolframscript
(* nettrain_introspection.wls - inspect an inert-loop training run end to end:
   net introspection, kernel/memory/buffer stats, codegen across CPU/Metal/CUDA
   plus the PTX + native-dylib compiled artifacts, a loss-trace callback that
   prints live during the single TWnf, and held-out eval + a train profile.

   Pure codegen for the GPU backends -- TKernelSource renders source/PTX with
   no dispatch, so it runs on a CPU-only box.

   Run:  wolframscript -file wl/THVMLink/Examples/nettrain_introspection.wls *)

PacletDirectoryLoad[FileNameJoin[{DirectoryName[$InputFileName], ".."}]];
Get["THVMLink`"];
banner[s_] := (Print[""]; Print["################ ", s, " ################"]);
dims[t_] := Dimensions[Normal @ TTensorData @ TRealize @ t];

SeedRandom[7];
all = RandomSample @ ResourceData["MNIST", "TrainingData"];
tr = Take[all, 256]; te = Take[all, {257, 384}]; teLabels = te[[All, 2]];
arch = {LinearLayer[64, "Input" -> 784], ElementwiseLayer[Ramp], LinearLayer[10]};

(* ===================================================================== *)
banner["1. NET INTROSPECTION"];
SeedRandom[7];
net = TFromNet[NetInitialize @ NetChain[arch, "Input" -> 784]];
Print["net = TFromNet[...]  ->  Head: ", Head[net]];
info = TNetParamInfo[net];
Print["TNetParamInfo[net]  (", Length[info], " trainable params, in layer order):"];
Do[ With[{e = info[[i]]},
    Print["  param ", i, ":  Layer=", e["Layer"], "  Param=", e["Param"], "  shape=", dims[e["Term"]]]],
    {i, Length[info]}];
Print["TNetParams[net]  ->  ", Length[TNetParams[net]], " weight handles (TAG_TEN, TRequiresGrad)"];
rec = TNetOf[net];
Print["TNetOf[net]  (faithful reconstruction from the term graph, registry-free):"];
Print["  Head: ", Head[rec]];
Print["  layers: ", Head /@ (NetExtract[rec, #] & /@ Range[Length[arch]])];
Print["  forward is bit-faithful to the term graph (pinned by training.wlt tnetof-reconstructs)"];

(* ===================================================================== *)
banner["2. KERNEL + MEMORY + BUFFER STATS"];
tn = NetTrain[net, tr, "TrainingNet", MaxTrainingRounds -> 100, "LearningRate" -> 0.05];
Print["NetTrain[net, data, \"TrainingNet\"]  ->  Head: ", Head[tn], "  (INERT: nothing has run)"];
Print["-- before TWnf --"];
Print["  TKernelCount   = ", TKernelCount[]];
Print["  TTensCount     = ", TTensCount[]];
Print["  TTotalBufBytes = ", TTotalBufBytes[], " bytes"];
profBefore = TProfileAll[];
t0 = AbsoluteTime[];
TWnf[tn];
trainWall = AbsoluteTime[] - t0;
$trainWall = trainWall;
Print["-- single TWnf drove the whole 100-round optimiser (", Round[trainWall, 0.01], " s) --"];
Print["  TKernelCount   = ", TKernelCount[]];
Print["  TTensCount     = ", TTensCount[]];
Print["  TTotalBufBytes = ", TTotalBufBytes[], " bytes (live)"];
mp = TMemoryPlan[][[1]];
Print["  TMemoryPlan: ", Length[mp["Kernels"]], " kernels, ", Length[mp["Bufs"]], " bufs, peak-concurrent-live = ", mp["Peak"], " bytes"];
ob = TKernelOutputBytes[];
Print["  TKernelOutputBytes: total ", Total[Values[ob]], " bytes across ", Length[ob], " kernels"];
ps = TPoolStats[];
Print["  TPoolStats: ", ps["Workers"], " workers, ", ps["TotalFires"], " total fires, ", ps["DrainRounds"], " drain rounds"];
delta = TProfileDelta[profBefore, TProfileAll[]];
Print["  TProfileDelta: ", Length[delta], " kernels fired during the TWnf window"];
topUs = Take[SortBy[Values[delta], -#["TotalUs"] &], UpTo[6]];
Print["  top kernels by wallclock (this training window):"];
Do[ Print["    kid ", k["Kid"], "  ", k["DispatchKind"], "  fires=", k["DispatchCount"],
        "  totalUs=", Round[k["TotalUs"], 0.1], "  GFLOP/s=", Round[k["GFlopsPerSec"], 0.1]], {k, topUs}];

(* ===================================================================== *)
banner["3. CODEGEN: CPU (C99) + METAL (MSL)"];
showHead[lbl_, src_, n_] := (Print[""]; Print["---- ", lbl, "  (", StringLength[src], " chars) ----"];
    Print[StringTake[src, UpTo[StringLength @ StringRiffle[Take[StringSplit[src, "\n"], UpTo[n]], "\n"]]]]);
gemmKids = Select[Range[0, TKernelCount[] - 1], TKernelDispatchKind[#] === "blas-gemm" &];
Print["the ", Length[gemmKids], " dense matmuls dispatch to a tuned CPU BLAS gemm (see section 2's top kernels);"];
Print["every other kernel is rendered by thvm's own codegen.  pick the heaviest codegen kernel:"];
(* heaviest jit (codegen) kernel that renders a non-empty C body *)
pickable[kid_] := (TKernelDispatchKind[kid] === "jit" && TKernelFlops[kid] > 0 && StringLength[TKernelSource[kid, "C"]] > 0);
jitKids = SortBy[Select[Range[0, TKernelCount[] - 1], pickable], -TKernelFlops[#] &];
kid = First[jitKids];
Print["representative codegen kernel: kid ", kid, "  (", TKernelDispatchKind[kid], ", flops=", TKernelFlops[kid], ")"];
showHead["TKernelSource[kid, \"C\"]      (C99, CPU)", TKernelSource[kid, "C"], 9];
showHead["TKernelSource[kid, \"Metal\"]  (MSL -- pure codegen, no GPU dispatch)", TKernelSource[kid, "Metal"], 7];
Print[""]; Print["TKernelJitDylibPath[kid] -> ", TKernelJitDylibPath[kid]];
Print["  (this net's small kernels run on the interpreter; hot kernels cross the JIT warmup"];
Print["   gate and clang -O3 compiles the rendered C to a native dylib)"];

(* ===================================================================== *)
banner["4. TRAINING LOSS-TRACE CALLBACK FIRING ON TWnf"];
Begin["THVMLink`Private`"];
SeedRandom[7];
net2 = stripSoftmax @ NetInitialize @ NetChain[arch];
{xS, tgt} = batchTensors[tr, netInputShape[net2], 10];
{fwd2, ps2} = {#1, Lookup[#2, "Term"]} & @@ reapParams[TFromNet[net2, xS]];
loss2 = TCategoricalCrossEntropy[fwd2, tgt];
g2 = TGrad[loss2, ps2]; lr2 = TUOpConst[0.05];
nm2 = freshTrainName[];
sr2 = MapThread[Function[{p, gr, i}, With[{s = nm2 <> "_p" <> ToString[i]},
    TDef[s, TMaterialize[TNf[TAssign[p, TUOpAdd[p, TUOpNeg[TUOpMul[lr2, gr]]]]]]]; TRef[s]]],
    {ps2, g2, Range[Length[ps2]]}];
(* materialize the loss in-graph so the forced PRI value is a readable buffer; the
   callback reads it directly -- a re-entrant host TRealize would corrupt the loop *)
lossSlot = nm2 <> "_loss"; TDef[lossSlot, TMaterialize[TNf[loss2]]];
round = 0;
lossCb = Function[v, round++;
    If[ round <= 3 || Mod[round, 10] == 0,
        Print["    [TWnf  round ", IntegerString[round, 10, 3], "]   loss = ",
            Round[First @ Normal @ TTensorData @ v, 0.0001]]]];
TDef[nm2, TLam[k, TIfZero[k, TUOpConst[0.0],
    Fold[TPriForce[#2, #1] &, TPri[lossCb, TRef[lossSlot], TApp[TRef[nm2], TOp2["-", k, TNum[1]]]], Reverse[sr2]]]]];
Print["the inert loop fires N rounds under ONE TWnf; a TPri callback prints each round's loss live:"];
Print[""];
TWnf[TApp[TRef[nm2], TNum[100]]];
Print[""];
Print["  ", round, " callbacks fired DURING the single TWnf (loss read from an in-graph"];
Print["  materialized buffer -- never a re-entrant host TRealize, which would corrupt the loop)"];
End[];

(* ===================================================================== *)
banner["5. EVAL METRICS + TRAIN PROFILE"];
preds = TNetPredict[net, te[[All, 1]]];
acc = N[Count[MapThread[#1 === #2 &, {preds, teLabels}], True] / Length[teLabels]];
Print["TNetPredict[net, testImages]  (net trained in section 2, in place):"];
Print["  held-out test accuracy (", Length[teLabels], " ex) = ", Round[acc, 0.001]];
Print[""];
Print["train profile (the section-2 TWnf):"];
Print["  wall time          = ", Round[$trainWall, 0.01], " s for 100 rounds (", Round[$trainWall/100*1000, 0.1], " ms/round)"];
Print["  kernels fired      = ", Length[delta]];
Print["  total kernel us    = ", Round[Total[#["TotalUs"] & /@ Values[delta]], 0.1]];
Print["  total dispatches   = ", Total[#["DispatchCount"] & /@ Values[delta]]];
Print["  aggregate GFLOP/s  = ", Round[Total[#["Flops"] * #["DispatchCount"] & /@ Values[delta]] / (Total[#["TotalUs"] & /@ Values[delta]] * 1000), 0.1]];
Print[""]; Print["################ DONE ################"];
