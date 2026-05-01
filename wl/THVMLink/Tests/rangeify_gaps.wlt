(* rangeify_gaps.wlt -- focused no-bail coverage for movement-heavy
   rangeify gaps that used to fall back to the legacy KProgOp path. *)

SetAttributes[rangeifyGapWithRangeify, HoldRest];
rangeifyGapWithRangeify[body_] := Module[{prev, r},
    prev = Environment["THVM_RANGEIFY"];
    If[ prev === $Failed, prev = None];
    SetEnvironment["THVM_RANGEIFY" -> "1"];
    r = body;
    SetEnvironment["THVM_RANGEIFY" -> prev];
    r
]

rangeifyGapAllLowered[] := AllTrue[
    Range[TKernelCount[] - 1],
    TKernelInfo[#]["n_ops"] === 0 || TKernelScalarUops[#] =!= Missing["NotLowered"] &
]

VerificationTest[
    rangeifyGapWithRangeify[
        TInit[];
        a = TTensorCreate @ NumericArray[{1.0, 3.0, 2.0, 4.0}, "Real32"];
        pooled = TUOpReduce[TUOpReshape[a, {2, 2}], 1, "MAX"];
        summed = TUOpReduce[pooled, 0, "SUM"];
        g = TRealize @ TGrad[summed, a];
        {Normal @ TTensorData[g], rangeifyGapAllLowered[]}
    ],
    {{0.0, 1.0, 0.0, 1.0}, True},
    TestID -> "rangeify-gaps/grad-reshape-rank-mismatch-no-bail"
]

VerificationTest[
    rangeifyGapWithRangeify[
        TInit[];
        net = NetInitialize @ Quiet[NetModel["LeNet"], Import::nnincmpb];
        x = TTensorCreate @ NumericArray[ConstantArray[0.5, {1, 28, 28}], "Real32"];
        res = TRealize @ TFromNet[net, x];
        {TTensorShape[res], Round[Total @ Normal @ TTensorData[res], 0.0001],
         rangeifyGapAllLowered[]}
    ],
    {{10}, 1.0, True},
    TestID -> "rangeify-gaps/lenet-leading-1-pad-fanout-no-bail"
]

VerificationTest[
    rangeifyGapWithRangeify[
        TInit[];
        q = TTensorCreate @ NumericArray[N @ {{1, 0, 0, 0}, {0, 1, 0, 0}}, "Real32"];
        k = TTensorCreate @ NumericArray[N @ {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}, "Real32"];
        v = TTensorCreate @ NumericArray[N @ {{0.1, 0.2}, {0.3, 0.4}, {0.5, 0.6}}, "Real32"];
        r = TRealize @ TAttention[q, k, v];
        {TTensorShape[r], Normal @ TTensorData[r], rangeifyGapAllLowered[]}
    ],
    {{2, 2}, {{0.264441, 0.364441}, {0.300000, 0.400000}}, True},
    SameTest -> (#1[[1]] === #2[[1]]
              && #1[[3]] === #2[[3]]
              && Max[Abs[Flatten[#1[[2]] - #2[[2]]]]] < 1.0*^-4 &),
    TestID -> "rangeify-gaps/attention-pre-index-reshape-no-bail"
]
