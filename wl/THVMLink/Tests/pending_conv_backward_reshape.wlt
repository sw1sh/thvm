(* pending_conv_backward_reshape.wlt -- conv weight gradient
   replicates cOut=1 from cOut=0 due to a reshape-backward bug.

   Forward `TConv2D[x, w, b]` (rank-4) is correct.  Bias gradient is
   correct.  Weight gradient via TConv2DIm2ColBatchedPool's
   `TUOpReshape[weights, {cOut, cIn*kh*kw}]` step silently drops
   cOut differentiation in the backward chain: gW[cOut=1, :, :, :]
   comes back identical to gW[cOut=0, :, :, :].

   Bisected to the reshape itself.  With weights pre-built as
   `{cOut, kFlat}` (skip the reshape from {cOut, cIn, kh, kw}), the
   gradient is correct.  Even a 2-step reshape
   `{cOut, cIn, kh, kw} -> {cOut, cIn, kh*kw} -> {cOut, kFlat}`
   still loses cOut.  The bug is in the TGrad chain through any
   rank-changing reshape that fuses >=2 trailing axes into one --
   the cOut info that LIVES in axis 0 (which is NOT collapsed)
   ends up replicated/lost somewhere in the chain-rule transport.

   Tried a workaround (commit 9bdd8721) by skipping the reshape
   entirely and broadcasting weights directly from
   `{cOut, cIn, kh, kw}` to the 7-D matmul operand.  Gradient was
   correct but peak memory jumped from ~200 MB to ~5 GB at BS=128
   because each of the 3 reduce intermediates was the full 7-D
   tensor instead of a flat 5-D one -- training hits the
   THVM_MAX_LIVE_BYTES ceiling.  Reverted (commit 661c7662).

   Real-impact: conv1/2/3/4 in beautiful_mnist all have cOut > 1,
   so all their weight gradients are missing cross-channel signal.
   Adam now CAN update weights (commit 40f129b4 fixed the
   gTen * gTen IC-OP2 issue) -- the v-state, m-state, w-state all
   update correctly per the rank-1 test -- but the cross-channel
   gradient information is wrong, so multi-channel layers won't
   learn the right thing.

   Proper fix: TGrad over a rank-changing reshape that fuses K
   leading-or-middle axes into one needs to walk the inverse
   shape transform correctly during the chain rule.  Likely lives
   in src/uop/grad.c or wherever the reshape adjoint is generated.

   This file is run as INFORMATIONAL by the WL test runner. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

VerificationTest[
    TInit[];
    SeedRandom[42];
    xH = N @ RandomReal[{-1, 1}, {2, 1, 5, 5}];
    wH = N @ RandomVariate[NormalDistribution[0., 0.5], {2, 1, 3, 3}];
    bH = N @ RandomReal[{-0.2, 0.2}, {2}];
    tH = N @ RandomReal[{-1, 1}, {2, 2, 3, 3}];
    fwdLoss[wD_] := Module[{x, w, b, t, diff},
        TInit[];
        x = TTensorCreate @ NumericArray[xH, "Real32"];
        w = TTensorCreate @ NumericArray[wD, "Real32"];
        b = TTensorCreate @ NumericArray[bH, "Real32"];
        t = TTensorCreate @ NumericArray[tH, "Real32"];
        diff = TConv2D[x, w, b] - t;
        First @ Normal @ TTensorData @ TRealize @ TUOpReduce[TUOpReduce[
            TUOpReduce[TUOpReduce[diff * diff, 3, "SUM"], 2, "SUM"],
            1, "SUM"], 0, "SUM"]];
    TInit[];
    xT = TTensorCreate @ NumericArray[xH, "Real32"];
    wT = TTensorCreate @ NumericArray[wH, "Real32"];
    bT = TTensorCreate @ NumericArray[bH, "Real32"];
    tT = TTensorCreate @ NumericArray[tH, "Real32"];
    diff = TConv2D[xT, wT, bT] - tT;
    loss = TUOpReduce[TUOpReduce[TUOpReduce[TUOpReduce[diff * diff,
        3, "SUM"], 2, "SUM"], 1, "SUM"], 0, "SUM"];
    gW = Normal @ TTensorData @ TRealize @ TGrad[loss, wT];
    fdGW = Table[
        (fwdLoss[ReplacePart[wH, {co, ci, ki, kj} -> wH[[co, ci, ki, kj]] + 0.001]]
       - fwdLoss[ReplacePart[wH, {co, ci, ki, kj} -> wH[[co, ci, ki, kj]] - 0.001]]) / 0.002,
        {co, 2}, {ci, 1}, {ki, 3}, {kj, 3}];
    Max @ Abs @ Flatten[gW - fdGW],
    _ ? (# < 0.01 &),
    SameTest -> MatchQ,
    TestID -> "pending/conv2d-grad-w-matches-finite-difference"
]
