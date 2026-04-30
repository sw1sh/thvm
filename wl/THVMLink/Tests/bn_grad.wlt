(* bn_grad.wlt -- TBatchNorm gradient verification via numerical
   finite-difference grad-check.

   M2 of the beautiful-mnist parity arc.  TBatchNorm is built from
   Plus / Times / Sqrt / Reshape / Expand primitives whose chain
   rule already exists; the test confirms that the combined
   gradient through the (gamma * (x-mean)/sqrt(var+eps) + beta)
   pattern matches what a finite-difference probe of the SAME
   forward computes.  Errors are bounded by O(h) where h = 0.001;
   tolerance 0.01 is comfortably above that for the f32 path.

   Probes d/dx, d/dgamma, d/dbeta against an L2-loss against a
   random target.  Skip d/dmean and d/dvar -- the inference-form
   BN treats them as constants, so the trainer-side running stats
   (TAssign, host-side update) handle their evolution. *)

bnGrad[which_String, xData_, gData_, bData_, mData_, vData_, tData_] :=
    Module[{x, g, b, m, v, t, loss, target, fwdLoss},
        TInit[];
        x = TTensorCreate @ NumericArray[xData, "Real32"];
        g = TTensorCreate @ NumericArray[gData, "Real32"];
        b = TTensorCreate @ NumericArray[bData, "Real32"];
        m = TTensorCreate @ NumericArray[mData, "Real32"];
        v = TTensorCreate @ NumericArray[vData, "Real32"];
        t = TTensorCreate @ NumericArray[tData, "Real32"];
        fwdLoss[xT_, gT_, bT_, mT_, vT_, tT_] :=
            TUOpReduce[TUOpReduce[TUOpReduce[
                (TBatchNorm[xT, gT, bT, mT, vT] - tT)^2,
                0, "SUM"], 0, "SUM"], 0, "SUM"];
        target = Switch[which, "x", x, "g", g, "b", b];
        Normal @ TTensorData @ TRealize @ TGrad[
            fwdLoss[x, g, b, m, v, t], target]
    ];

bnEvalLoss[xData_, gData_, bData_, mData_, vData_, tData_] :=
    Module[{x, g, b, m, v, t},
        TInit[];
        x = TTensorCreate @ NumericArray[xData, "Real32"];
        g = TTensorCreate @ NumericArray[gData, "Real32"];
        b = TTensorCreate @ NumericArray[bData, "Real32"];
        m = TTensorCreate @ NumericArray[mData, "Real32"];
        v = TTensorCreate @ NumericArray[vData, "Real32"];
        t = TTensorCreate @ NumericArray[tData, "Real32"];
        First @ Normal @ TTensorData @ TRealize @ TUOpReduce[
            TUOpReduce[TUOpReduce[
                (TBatchNorm[x, g, b, m, v] - t)^2,
                0, "SUM"], 0, "SUM"], 0, "SUM"]
    ];

(* === d(L2(BN - target))/d(gamma) and d/d(beta) === *)

VerificationTest[
    SeedRandom[7];
    cShape = {3, 4, 4};
    xD = N @ RandomReal[{-1, 1}, cShape];
    gD = N @ RandomReal[{0.5, 1.5}, {3}];
    bD = N @ RandomReal[{-0.5, 0.5}, {3}];
    mD = N @ RandomReal[{-0.2, 0.2}, {3}];
    vD = N @ RandomReal[{0.5, 1.5}, {3}];
    tD = N @ RandomReal[{-1, 1}, cShape];
    h  = 0.001;
    sym = bnGrad["g", xD, gD, bD, mD, vD, tD];
    fd  = Table[
        ((bnEvalLoss[xD, ReplacePart[gD, i -> gD[[i]] + h], bD, mD, vD, tD]
          - bnEvalLoss[xD, ReplacePart[gD, i -> gD[[i]] - h], bD, mD, vD, tD])
         / (2 h)),
        {i, 3}];
    Max @ Abs @ Flatten[sym - fd] < 0.01,
    True,
    TestID -> "bn-grad/dgamma-finite-diff"
]

VerificationTest[
    SeedRandom[7];
    cShape = {3, 4, 4};
    xD = N @ RandomReal[{-1, 1}, cShape];
    gD = N @ RandomReal[{0.5, 1.5}, {3}];
    bD = N @ RandomReal[{-0.5, 0.5}, {3}];
    mD = N @ RandomReal[{-0.2, 0.2}, {3}];
    vD = N @ RandomReal[{0.5, 1.5}, {3}];
    tD = N @ RandomReal[{-1, 1}, cShape];
    h  = 0.001;
    sym = bnGrad["b", xD, gD, bD, mD, vD, tD];
    fd  = Table[
        ((bnEvalLoss[xD, gD, ReplacePart[bD, i -> bD[[i]] + h], mD, vD, tD]
          - bnEvalLoss[xD, gD, ReplacePart[bD, i -> bD[[i]] - h], mD, vD, tD])
         / (2 h)),
        {i, 3}];
    Max @ Abs @ Flatten[sym - fd] < 0.01,
    True,
    TestID -> "bn-grad/dbeta-finite-diff"
]

(* === d/dx, smaller {2, 2, 2} for FD-eval cost === *)

VerificationTest[
    SeedRandom[11];
    cShape = {2, 2, 2};
    cChan  = 2;
    xD = N @ RandomReal[{-1, 1}, cShape];
    gD = N @ RandomReal[{0.5, 1.5}, {cChan}];
    bD = N @ RandomReal[{-0.5, 0.5}, {cChan}];
    mD = N @ RandomReal[{-0.2, 0.2}, {cChan}];
    vD = N @ RandomReal[{0.5, 1.5}, {cChan}];
    tD = N @ RandomReal[{-1, 1}, cShape];
    h  = 0.001;
    sym = bnGrad["x", xD, gD, bD, mD, vD, tD];
    fd  = Table[
        ((bnEvalLoss[ReplacePart[xD, {ci,hi,wi} -> xD[[ci,hi,wi]] + h], gD, bD, mD, vD, tD]
          - bnEvalLoss[ReplacePart[xD, {ci,hi,wi} -> xD[[ci,hi,wi]] - h], gD, bD, mD, vD, tD])
         / (2 h)),
        {ci, cShape[[1]]}, {hi, cShape[[2]]}, {wi, cShape[[3]]}];
    Max @ Abs @ Flatten[sym - fd] < 0.01,
    True,
    TestID -> "bn-grad/dx-finite-diff"
]
