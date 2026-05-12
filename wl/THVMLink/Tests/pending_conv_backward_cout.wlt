(* pending_conv_backward_cout.wlt -- conv weight-gradient replicates
   cOut=1 from cOut=0 (and all higher cOut from cOut=0).

   The forward of `TConv2D[x, w, b]` (rank-4, BS>=1, cIn>=1, cOut>=2)
   is correct -- each output channel gets distinct values.  But the
   gradient `TGrad[loss, w]` returns w-gradient where rows 1..cOut-1
   are IDENTICAL to row 0:

       gW[cOut=0, :, :, :]  -- correct (matches finite-difference)
       gW[cOut=1, :, :, :]  == gW[cOut=0, :, :, :]  (WRONG)
       gW[cOut=2, :, :, :]  == gW[cOut=0, :, :, :]  (WRONG)
       ...

   The bias-gradient (`TGrad[loss, b]`) IS correct, so the bug is
   specific to the weight gradient path through `wExp = EXPAND(reshape
   (wFlat, {cOut, kFlat, 1, 1, 1}), {cOut, kFlat, B, hOut, wOut})`.

   Forward debug (cIn=2 case) was fixed in src/schedule/rangeify.c by
   disambiguating the reduce-axis search with `dims[k] == r_size`;
   the backward goes through a different code path and still
   collapses the cOut axis.

   Trace probe at /tmp/conv-grad-probe.wls demonstrates the failure:
   for {B=2, cIn=1, H=W=5} * {cOut=2, cIn=1, kH=kW=3}, gW[1, :, :, :]
   matches FD within 1e-3 but gW[2, :, :, :] is literally a copy of
   gW[1, :, :, :] (max abs diff ~14).

   Real-impact: blocks beautiful_mnist training convergence -- conv1,
   conv2, conv3, conv4 all have cOut>1 so all their weight gradients
   are missing cross-channel signal, and Adam can't drive the loss
   down.  Linear-only classifier converges fine (no conv).

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
    x = TTensorCreate @ NumericArray[xH, "Real32"];
    w = TTensorCreate @ NumericArray[wH, "Real32"];
    b = TTensorCreate @ NumericArray[bH, "Real32"];
    t = TTensorCreate @ NumericArray[tH, "Real32"];
    diff = TConv2D[x, w, b] - t;
    loss = TUOpReduce[TUOpReduce[TUOpReduce[TUOpReduce[diff * diff,
        3, "SUM"], 2, "SUM"], 1, "SUM"], 0, "SUM"];
    gW = Normal @ TTensorData @ TRealize @ TGrad[loss, w];
    (* When the bug is fixed, the two cOut rows should be different. *)
    Max @ Abs @ Flatten[gW[[1]] - gW[[2]]],
    _ ? (# > 0.01 &),
    SameTest -> MatchQ,
    TestID -> "pending/conv2d-grad-w-cout-rows-must-differ"
]
