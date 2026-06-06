#!/usr/bin/env wolframscript
(* train.wls -- end-to-end beautiful_mnist training on real MNIST.

   Mirrors tinygrad's examples/beautiful_mnist.py exactly: same
   architecture (Conv5x5 1->32 -> ReLU -> Conv5x5 32->32 -> ReLU -> BN ->
   MaxPool -> Conv3x3 32->64 -> ReLU -> Conv3x3 64->64 -> ReLU -> BN ->
   MaxPool -> Flatten -> Linear 576->10), Adam at BS=512, sparse-
   categorical-CE on int labels, test-acc eval every TEST_EVERY steps.

   The trainStep is wrapped with TJit so the first step pays capture
   cost and subsequent steps replay the kernel sequence.  Real MNIST
   samples are fed via TSet'ing a pre-allocated input/target slot
   pair each step -- the buf_id stays stable so the captured DAG
   remains valid.

   Run from the repo root:
       wolframscript -f wl/Examples/beautiful-mnist/train.wls
       DEV=metal THVM_TILE=1 N_STEPS=70 TEST_EVERY=10 BS=512 MNIST_LIMIT=All wolframscript -f wl/Examples/beautiful-mnist/train.wls

   Env:
     BS          (default 512)  -- batch size
     N_STEPS     (default 2)    -- training steps (one Adam update each)
     TEST_EVERY  (default 0)    -- run test-acc every N steps; 0 disables
     MNIST_LIMIT (default 2048) -- # of train/test samples to load;
                                   set to All for a real ~99 % run. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

envValue[name_String, default_String] := Module[{v = Environment[name]},
    If[ StringQ[v] && StringLength[StringTrim[v]] > 0, v, default]]

parseInt[name_String, default_Integer] := Module[{
    raw = StringTrim @ envValue[name, ToString[default]]
},
    If[ StringMatchQ[raw, DigitCharacter ..], ToExpression[raw], default]]

bs        = parseInt["BS",          512];
nSteps    = parseInt["N_STEPS",     2];
testEvery = parseInt["TEST_EVERY",  0];
mnistLim  = envValue["MNIST_LIMIT", "2048"];
mnistN    = If[ mnistLim === "All", All, ToExpression @ mnistLim];

SeedRandom[42];
TInit[];

Print["beautiful_mnist full-training (cold + warm JIT smoke test)"];
Print["  BS=", bs, "  N_STEPS=", nSteps, "  TEST_EVERY=", testEvery,
      "  MNIST_LIMIT=", mnistLim];
Print["  DEV=", envValue["DEV", "default-cpu"],
      "  THVM_TILE=", envValue["THVM_TILE", "off"]];

(* === Load MNIST once, materialise to host arrays for fast slicing.
       TMnistLoad returns TTensors; we pull them back via TTensorData
       so per-step batch construction is a host array slice + a fresh
       TTensorCreate (which feeds TSet into the stable slot). *)
mnistT0 = AbsoluteTime[];
mnist = TMnistLoad[mnistN];
trainImgsHost = Normal @ TTensorData @ mnist["trainImages"];
trainLabsHost = Normal @ TTensorData @ mnist["trainLabels"];
testImgsHost  = Normal @ TTensorData @ mnist["testImages"];
testLabsHost  = Normal @ TTensorData @ mnist["testLabels"];
nTrain = Length[trainImgsHost];
nTest  = Length[testImgsHost];
mnistDt = (AbsoluteTime[] - mnistT0) * 1000.0;
Print["MNIST loaded: nTrain=", nTrain, " nTest=", nTest,
      " (", NumberForm[mnistDt, {8, 1}], "ms)"];

(* === Model parameters -- same init as bench-train.wls. ============ *)
randTensor[shape_List, scale_ : 0.05] :=
    TTensorCreate @ NumericArray[
        RandomVariate[NormalDistribution[0., scale], shape], "Real32"]
zerosTensor[shape_List] := TTensorCreate @ NumericArray[
    ConstantArray[0., shape], "Real32"]
onesTensor[shape_List]  := TTensorCreate @ NumericArray[
    ConstantArray[1., shape], "Real32"]

W1 = randTensor[{32, 1, 5, 5}];   b1 = zerosTensor[{32}];
W2 = randTensor[{32, 32, 5, 5}];  b2 = zerosTensor[{32}];
g1 = onesTensor[{32}];            bn1 = zerosTensor[{32}];
W3 = randTensor[{64, 32, 3, 3}];  b3 = zerosTensor[{64}];
W4 = randTensor[{64, 64, 3, 3}];  b4 = zerosTensor[{64}];
g2 = onesTensor[{64}];            bn2 = zerosTensor[{64}];
W5 = randTensor[{576, 10}];       b5 = zerosTensor[{10}];

params = {W1, b1, W2, b2, g1, bn1, W3, b3, W4, b4, g2, bn2, W5, b5};
mState = TZerosLike /@ params;
vState = TZerosLike /@ params;
(* beta^t accumulators (shape {1}, seeded to 1.0).  TAdam advances
   them in-graph each step so bias correction tracks t under TJit
   replay rather than freezing at capture. *)
b1Pow = TOnes[{1}];
b2Pow = TOnes[{1}];
Print["params=", Total[Times @@ TTensorShape[#] & /@ params]];

(* === Pre-allocated input/target slots.  Same buf_id every step --
       the captured DAG reads from these; per-step batch data lands
       via TSet, no DAG re-capture needed.  Labels are int32 class
       indices, NOT one-hot -- matches tinygrad's
       sparse_categorical_crossentropy convention. *)
xSlot   = zerosTensor[{bs, 1, 28, 28}];
tgtSlot = TTensorCreate @ NumericArray[ConstantArray[0, bs], "Integer32"];

feedBatch[] := Module[{idx, batchImg, batchLab},
    idx      = RandomSample[Range[nTrain], UpTo[bs]];
    batchImg = trainImgsHost[[idx]];
    batchLab = trainLabsHost[[idx]];
    TSet[xSlot,   TTensorCreate @ NumericArray[batchImg, "Real32"]];
    TSet[tgtSlot, TTensorCreate @ NumericArray[batchLab, "Integer32"]];
]

(* === forward + loss + Adam step.  Identical structure to
       bench-train.wls; reading from xSlot/tgtSlot so the captured
       JIT-replay path is the same we benchmarked. *)
forwardLoss[x_TTerm, target_TTerm] := Module[{
    h1, r1, h2, r2, n1, p1, h3, r3, h4, r4, n2, p2, flat, logits
},
    h1     = TConv2D[x, W1, b1];
    r1     = TReLU[h1];
    h2     = TConv2D[r1, W2, b2];
    r2     = TReLU[h2];
    n1     = TBatchNormTrain[r2, g1, bn1];
    p1     = TMaxPool2d[n1, 2];
    h3     = TConv2D[p1, W3, b3];
    r3     = TReLU[h3];
    h4     = TConv2D[r3, W4, b4];
    r4     = TReLU[h4];
    n2     = TBatchNormTrain[r4, g2, bn2];
    p2     = TMaxPool2d[n2, 2];
    flat   = TUOpReshape[p2, {bs, 576}];
    logits = TLinear[flat, W5, b5];
    TSparseCategoricalCrossEntropy[logits, target]
]

(* `lossSlot` is the readback channel for the per-step loss.  TJit's
   replay return value is unreliable, so trainStep TAssigns loss into
   lossSlot in its own bundled TRealize before TAdam updates the
   weights.  The `0.0 * lossSlot + loss` wrapper on the RHS forces an
   add to be emitted -- a bare TAssign[lossSlot, loss] with a UOP RHS
   gets short-circuited and the assign never fires. *)
lossSlot = TTensorCreate @ NumericArray[{0.0}, "Real32"];

trainStep = Function[{stepIdx}, Module[{loss = forwardLoss[xSlot, tgtSlot]},
    TRealize @ {TAssign[lossSlot, 0.0 * lossSlot + loss]};
    TAdam[loss, params, mState, vState, b1Pow, b2Pow]; Null]];
jittedStep = TJit[trainStep];

runStep[step_Integer, label_String] := Module[{
    t0, dt, kBefore, kAfter, mem, lossVal
},
    kBefore = TKernelCount[];
    t0      = AbsoluteTime[];
    feedBatch[];
    jittedStep[step];
    dt      = (AbsoluteTime[] - t0) * 1000.0;
    kAfter  = TKernelCount[];
    mem     = TMetalMemoryProfile[];
    lossVal = First @ Normal @ TTensorData @ lossSlot;
    Print[label, " step ", step,
          ": loss=", NumberForm[lossVal, {6, 3}],
          " wall=", NumberForm[dt, {8, 1}], "ms",
          " kernels(new)=", kAfter - kBefore,
          " jit-ops=", TJitOpCount[jittedStep],
          " heap=", THeapPos[],
          " peak_live=", NumberForm[
              N[Lookup[mem, "PeakLiveBytes", 0] / 1024.0^2], {8, 1}], "MB"];
    dt
]

(* === Test-accuracy eval (when TEST_EVERY > 0).
       Captures a separate forward-only DAG that reads from xSlot and
       TAssigns the {bs, 10} logits into a stable `logitsSlot` -- TJit
       captures the kernel sequence + the assign, but the *return*
       value of a JIT'd function is unreliable (the replay path
       returns Null), so the slot is the readback channel.  Argmax is
       done host-side on {bs, 10} (cheap) -- no dedicated argmax op
       needed. *)
logitsSlot  = TTensorCreate @ NumericArray[ConstantArray[0., {bs, 10}], "Real32"];
evalForward = Function[{}, Module[{
    h1, r1, h2, r2, n1, p1, h3, r3, h4, r4, n2, p2, flat
},
    h1   = TConv2D[xSlot, W1, b1];
    r1   = TReLU[h1];
    h2   = TConv2D[r1, W2, b2];
    r2   = TReLU[h2];
    n1   = TBatchNormTrain[r2, g1, bn1];
    p1   = TMaxPool2d[n1, 2];
    h3   = TConv2D[p1, W3, b3];
    r3   = TReLU[h3];
    h4   = TConv2D[r3, W4, b4];
    r4   = TReLU[h4];
    n2   = TBatchNormTrain[r4, g2, bn2];
    p2   = TMaxPool2d[n2, 2];
    flat = TUOpReshape[p2, {bs, 576}];
    TAssign[logitsSlot, TLinear[flat, W5, b5]]; Null
]];
jittedEval = TJit[evalForward];

(* TestImages have been pulled to host as `testImgsHost` (rank-4
   real32 array) and `testLabsHost` (int list).  Pad the trailing
   partial batch with zeros and ignore those predictions when
   tallying. *)
testAcc[] := Module[{nBatches, correct, total, predicted, batchImg, gold},
    nBatches  = Ceiling[nTest / bs];
    correct   = 0;
    total     = 0;
    Do[
        Module[{b0 = (i - 1) * bs + 1, b1 = Min[i * bs, nTest], pad},
            batchImg = Take[testImgsHost, {b0, b1}];
            gold     = Take[testLabsHost, {b0, b1}];
            pad      = bs - Length[batchImg];
            If[ pad > 0,
                batchImg = Join[batchImg, ConstantArray[0., {pad, 1, 28, 28}]] ];
            TSet[xSlot, TTensorCreate @ NumericArray[batchImg, "Real32"]];
            jittedEval[];
            predicted = Map[First @ Ordering[#, -1] - 1 &,
                            Normal @ TTensorData @ logitsSlot];
            predicted = Take[predicted, Length[gold]];
            correct  += Count[MapThread[Equal, {predicted, gold}], True];
            total    += Length[gold];
        ],
        {i, nBatches}];
    100.0 * correct / total
]

(* === Train loop ================================================== *)
Do[ Module[{},
        runStep[step, If[ step == 1, "cold ", "warm "]];
        If[ testEvery > 0 && Mod[step, testEvery] == 0,
            Print["    test_acc=", NumberForm[testAcc[], {6, 2}], "%"] ]],
    {step, 1, nSteps}];

If[ testEvery == 0 && nSteps > 2,
    Print["final test_acc=", NumberForm[testAcc[], {6, 2}], "%"] ];

Print["done."];
