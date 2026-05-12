(* pending_bn_reshape_expand.wlt -- regression test for the BN-train all-
   zeros bug that blocks beautiful_mnist BS>=8 training.

   The canonical pattern:
       reduce  : {B, C, H, W} -> {C}   (sum over B, H, W)
       reshape : {C}          -> {1, C, 1, 1}
       expand  : {1, C, 1, 1} -> {B, C, H, W}

   This is the per-channel mean broadcast inside TBatchNormTrain.  The
   fused reduce->reshape->expand chain produces all-zero output -- only
   element [0] of the post-reshape buffer survives, and the consumer
   reads zeros at every other index.  Diagnostic: setting
   `THVM_DUMP_LIFT_REJECT=1` prints

       lift reject: index/ndim-mismatch buf_ndim=4 src_count=4
         ndim-mismatch: outer_rank=3 dims=[B,C,H,W] range_extents=[...]

   for every such kernel.  The lift bails and the legacy program[] path
   also computes wrong values.

   Effect on beautiful_mnist:  TBatchNormTrain outputs all zeros for any
   shape where B*C > 4.  Forward produces all-zero post-BN activations,
   loss is still finite (logits stay reasonable), gradient explodes at
   the conv weights upstream of BN (dL/dW ~ 1e5), Adam takes a huge
   first step, step 2's forward NaN's out.

   The fix is in C: the rangeify reduce->reshape->expand fusion or the
   kernel_lift index walk needs to handle this pattern.  See
   docs/plans/multicomputation_trace.md and the probe scripts under
   `scripts/probe_*.wls`. *)

PacletDirectoryLoad["wl/THVMLink"];
Needs["MUnit`"];
Needs["THVMLink`"];

VerificationTest[
    TInit[];
    SeedRandom[42];
    shape = {3, 2, 2, 2};
    xH = N @ {
        {{{0.1, 0.2}, {0.3, 0.4}}, {{0.5, 0.6}, {0.7, 0.8}}},
        {{{0.9, 1.0}, {1.1, 1.2}}, {{1.3, 1.4}, {1.5, 1.6}}},
        {{{1.7, 1.8}, {1.9, 2.0}}, {{2.1, 2.2}, {2.3, 2.4}}}
    };
    x = TTensorCreate @ NumericArray[xH, "Real32"];
    red = TUOpReduce[TUOpReduce[TUOpReduce[x, 3, "SUM"], 2, "SUM"], 0, "SUM"];
    reshaped = TUOpReshape[red, {1, 2, 1, 1}];
    expanded = TUOpExpand[reshaped, shape];
    data = Normal @ TTensorData @ TRealize @ expanded;
    (* Per-channel sum: c=0 -> 0.1+...+2.0 = 12.6; c=1 -> 0.5+...+2.4 = 17.4.
       Expanded should be {{12.6 across HxW for c=0, 17.4 across HxW for c=1}} per batch. *)
    {data[[1, 1, 1, 1]], data[[1, 2, 1, 1]], data[[2, 1, 1, 1]], data[[3, 2, 1, 1]]},
    {12.6, 17.4, 12.6, 17.4},
    SameTest -> (Max @ Abs[#1 - #2] < 0.01 &),
    TestID -> "pending/reduce-reshape-expand-multi-channel-broadcast"
]

VerificationTest[
    TInit[];
    SeedRandom[42];
    shape = {8, 4, 4, 4};
    x = TTensorCreate @ NumericArray[
        N @ RandomReal[{0, 1}, shape], "Real32"];
    gamma = TTensorCreate @ NumericArray[ConstantArray[1.0, 4], "Real32"];
    beta = TTensorCreate @ NumericArray[ConstantArray[0.0, 4], "Real32"];
    n = TRealize @ TBatchNormTrain[x, gamma, beta];
    flat = Flatten @ Normal @ TTensorData @ n;
    (* Per-channel mean=0, var=1 means absMean ~ 0.8.  All-zero output
       (the bug) gives absMean = 0.  Use a loose threshold of 0.4 to
       detect collapse. *)
    Mean[Abs[flat]],
    _ ? (# > 0.4 &),
    SameTest -> MatchQ,
    TestID -> "pending/batchnorm-train-rank4-no-channel-collapse-bs8"
]
