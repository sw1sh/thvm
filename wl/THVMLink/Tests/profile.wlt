(* profile.wlt -- baseline regression tests for runtime allocation
   counts.  These guard against accidental bloat in heap usage,
   kernel size, or interaction count for known-shape grad workloads.

   Tolerances are generous (~20-30%) -- the goal is to catch a
   regression that doubles allocations, not pin exact numbers.  When
   intentional changes shift the baselines, update the expected
   ranges here.

   Baseline numbers were captured at commit cf52924 (Newton example
   landed; CONST cache + dynamic kernel arrays in place). *)

(* withinPct[actual, expected, pct] -- True iff actual is within
   pct% of expected (or below). *)
withinPct[actual_, expected_, pct_] := actual <= expected * (1 + pct/100)

(* TGrad on a^4 1st-order: small-ish kernel, used as a sanity floor. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{2.0, 3.0}, "Real32"];
    y = TUOpMul[TUOpMul[TUOpMul[a, a], a], a];
    seed = TUOpExpand[TUOpConst[1.0, "f32"], {2}];
    TRealize @ TGrad[y, a, seed];
    p = TProfile[];
    {withinPct[p["HeapCells"],       100, 25],
     withinPct[p["MaxKernelInputs"],   8, 30],
     withinPct[p["MaxKernelOps"],     12, 30],
     p["Kernels"] >= 1},
    {True, True, True, True},
    TestID -> "profile/grad-a4-first"
]

(* TGrad on a^4 4th-order (was previously a hard-fail before dynamic
   kernel arrays).  Lock in that it stays well under the prior
   KERNEL_MAX_INPUT=64 bail point. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{2.0, 3.0}, "Real32"];
    y = TUOpMul[TUOpMul[TUOpMul[a, a], a], a];
    seed = TUOpExpand[TUOpConst[1.0, "f32"], {2}];
    TRealize @ TGrad[TGrad[TGrad[TGrad[y, a, seed], a, seed], a, seed], a, seed];
    p = TProfile[];
    {p["HeapCells"]       <= 12000,
     p["MaxKernelInputs"] <= 150,
     p["MaxKernelOps"]    <= 250,
     p["Kernels"] >= 1},
    {True, True, True, True},
    TestID -> "profile/grad-a4-fourth"
]

(* MSE-style loss gradient on small linear-regression: tracks the
   train.wls workload.  This kernel must stay small for SGD loop
   training to be viable. *)
VerificationTest[
    TInit[];
    w   = TTensorCreate @ NumericArray[{{0.1, 0.2}, {0.3, 0.4}}, "Real32"];
    x   = TTensorCreate @ NumericArray[{1.0, 0.5}, "Real32"];
    tgt = TTensorCreate @ NumericArray[{0.0, 0.0}, "Real32"];
    pred = TUOpReduce[TUOpMul[w, TUOpExpand[x, {2, 2}]], 1, "SUM"];
    diff = TUOpAdd[pred, TUOpNeg[tgt]];
    loss = TUOpReduce[TUOpMul[diff, diff], 0, "SUM"];
    TRealize @ TGrad[loss, w];
    p = TProfile[];
    {p["HeapCells"]       <= 3000,
     p["MaxKernelInputs"] <= 12,
     p["MaxKernelOps"]    <= 40,
     p["Kernels"] >= 1},
    {True, True, True, True},
    TestID -> "profile/mse-linear-regression"
]

(* Non-uniform gradient through SHRINK + chain rule.  Shape numbers
   stay small. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0, 5.0}, "Real32"];
    expr = TUOpReduce[TUOpShrink[TUOpMul[a, a], {{1, 4}}], 0, "SUM"];
    TRealize @ TGrad[expr, a];
    p = TProfile[];
    {p["HeapCells"]       <= 200,
     p["MaxKernelInputs"] <= 10,
     p["MaxKernelOps"]    <= 16,
     p["Kernels"] >= 1},
    {True, True, True, True},
    TestID -> "profile/shrink-chain-rule"
]
