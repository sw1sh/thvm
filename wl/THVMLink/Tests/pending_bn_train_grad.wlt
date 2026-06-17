(* pending_bn_train_grad.wlt -- TBatchNormTrain BACKWARD numerical
   correctness regression.

   Predecessor pendings (now fixed):
   - pending_bn_reshape_expand.wlt (forward all-zero collapse): FIXED
     on Metal by the kernel_lift trailing-collapsed-iter fast path
     (commit 3ed449d9).  Still broken on default CPU backend.

   This file tracks the REMAINING bug:
   `TGrad[loss(BN-train(x)), x]` now RETURNS a tensor (was
   `Missing[NotATensor, UOP]` before the uop_grad + bufferize
   patches in this commit's series) but the GRADIENT VALUE is
   wrong -- max diff vs finite-difference probe ~0.76 on the
   {2,2,2,2} test (sym=0.17, fd=-0.06, sign disagrees).

   With wrong gradient through BN-train, beautiful_mnist training
   updates weights in the wrong direction.  Effect: loss doesn't
   descend reliably even though step 1 → step 2 no longer NaN's.

   Suspect: the chain-rule transport through TBatchNormTrain's
   compound expression (centered = x - mean, var = mean(centered^2),
   normalized = centered/sqrt(var+eps)*gamma + beta) has multiple
   shared subexpressions (centered fans out to var AND to
   normalize).  The IC machinery's DUP commutation through this
   shared graph may be producing wrong adjoint values even when
   the result tensor materializes successfully.

   The bufferize per-absorbing-boundary REDUCE cap that unblocked
   the NotATensor path may also be slightly over- or under-
   merging downstream kernels; this needs an FD-parity probe per
   intermediate stage.

   Suggested next debug steps:
   1. Run TGrad against each intermediate stage of BN-train's body
      (mean, centered, var, normalised) and FD-probe each.
   2. Localize which stage's adjoint goes wrong.
   3. Check whether centered's reuse (`var` AND `normalised` both
      consume it) triggers a DUP fanout the chain rule mishandles
      (analogous to the gTen*gTen Adam bug in 40f129b4 -- maybe
      fix-by-rewrite at the WL level by binding `centered` to a
      TMaterialize, OR fix the actual chain-rule semantics for
      diamond-shaped DAGs).

   This file is run as INFORMATIONAL by the WL test runner. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];


(* === TBatchNormTrain backward parity: d/dx through the TRAIN form
   (uses x's own mean/var, no running stats).  This is the rank-4
   shape the beautiful-mnist BN layer hits; the rank-4 BN-train
   backward used to return Missing[NotATensor, UOP] before the
   bufferize per-absorbing-boundary REDUCE cap landed (multiple
   sibling broadcast REDUCEs would all un-mark and produce a
   single kernel with two REDUCE ops that materialize bails on). *)

bnTrainGradX[xData_, gData_, bData_, tData_] :=
    Module[{x, gT, bT, tT, diff, out, loss},
        TInit[];
        x  = TTensorCreate @ NumericArray[xData, "Real32"];
        gT = TTensorCreate @ NumericArray[gData, "Real32"];
        bT = TTensorCreate @ NumericArray[bData, "Real32"];
        tT = TTensorCreate @ NumericArray[tData, "Real32"];
        out = TBatchNormTrain[x, gT, bT];
        diff = out - tT;
        loss = TUOpReduce[TUOpReduce[TUOpReduce[TUOpReduce[
            diff * diff,
            3, "SUM"], 2, "SUM"], 1, "SUM"], 0, "SUM"];
        Normal @ TTensorData @ TRealize @ TGrad[loss, x]
    ];

bnTrainEvalLoss[xData_, gData_, bData_, tData_] :=
    Module[{x, gT, bT, tT, out, diff},
        TInit[];
        x  = TTensorCreate @ NumericArray[xData, "Real32"];
        gT = TTensorCreate @ NumericArray[gData, "Real32"];
        bT = TTensorCreate @ NumericArray[bData, "Real32"];
        tT = TTensorCreate @ NumericArray[tData, "Real32"];
        out = TBatchNormTrain[x, gT, bT];
        diff = out - tT;
        First @ Normal @ TTensorData @ TRealize @ TUOpReduce[
            TUOpReduce[TUOpReduce[TUOpReduce[
                diff * diff,
                3, "SUM"], 2, "SUM"], 1, "SUM"], 0, "SUM"]
    ];

VerificationTest[
    SeedRandom[19];
    nShape = {2, 2, 2, 2};
    nChan  = 2;
    xD = N @ RandomReal[{-1, 1}, nShape];
    gD = N @ RandomReal[{0.5, 1.5}, {nChan}];
    bD = N @ RandomReal[{-0.5, 0.5}, {nChan}];
    tD = N @ RandomReal[{-1, 1}, nShape];
    h  = 0.001;
    sym = bnTrainGradX[xD, gD, bD, tD];
    fd  = Table[
        ((bnTrainEvalLoss[ReplacePart[xD, {bi, ci, hi, wi} ->
                                       xD[[bi, ci, hi, wi]] + h], gD, bD, tD]
          - bnTrainEvalLoss[ReplacePart[xD, {bi, ci, hi, wi} ->
                                          xD[[bi, ci, hi, wi]] - h], gD, bD, tD])
         / (2 h)),
        {bi, nShape[[1]]}, {ci, nShape[[2]]},
        {hi, nShape[[3]]}, {wi, nShape[[4]]}];
    Max @ Abs @ Flatten[sym - fd] < 0.05,
    True,
    TestID -> "pending/bn-grad-train-dx-finite-diff"
]
