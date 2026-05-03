(* reduce.wlt -- UOP_REDUCE with non-innermost axes.
   Regression for the cpu_op_reduce bug where the kernel ignored
   its axis arg and always reduced over consecutive memory groups
   (effectively axis = ndim-1).  Materialize now repacks the op
   as (kind << 24 | inner) where inner = product of dims after
   the reduced axis, and the kernel strides correctly. *)

(* axis=0 of a rank-2 tensor -- reduce ROWS (sum along axis 0).
   Input:                        Sum axis 0:
   {{1, 2, 3},   {{1+4, 2+5, 3+6},
    {4, 5, 6}}    = {{5, 7, 9}}
   Hand-check: rows summed elementwise. *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}, "Real32"];
    r = TRealize @ TUOpReduce[x, 0, "SUM"];
    {TTensorShape[r], Normal @ TTensorData[r]},
    {{3}, {5.0, 7.0, 9.0}},
    TestID -> "reduce/axis0-rank2-sum"
]

(* axis=1 of a rank-2 tensor -- reduce COLUMNS (sum along axis 1).
   This was always working pre-fix (axis=1 is innermost). *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}, "Real32"];
    r = TRealize @ TUOpReduce[x, 1, "SUM"];
    {TTensorShape[r], Normal @ TTensorData[r]},
    {{2}, {6.0, 15.0}},
    TestID -> "reduce/axis1-rank2-sum-innermost"
]

(* axis=1 of a rank-3 {C=2, H=3, W=4} -- reduce H (the middle axis). *)
VerificationTest[
    TInit[];
    data = ArrayReshape[Range[24] * 1.0, {2, 3, 4}];
    x = TTensorCreate @ NumericArray[data, "Real32"];
    r = TRealize @ TUOpReduce[x, 1, "SUM"];
    {TTensorShape[r], Normal @ TTensorData[r]},
    (* Per-channel column sums:
       ch 0 (rows 1..3, 5..7, 9..11): cols 1+5+9, 2+6+10, 3+7+11, 4+8+12
       ch 1 (13..16, 17..20, 21..24): 13+17+21, 14+18+22, 15+19+23, 16+20+24 *)
    {{2, 4}, {{15.0, 18.0, 21.0, 24.0}, {51.0, 54.0, 57.0, 60.0}}},
    TestID -> "reduce/axis1-rank3-sum-middle"
]

(* MAX reduction with non-innermost axis. *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{{1.0, 5.0, 2.0}, {4.0, 3.0, 6.0}}, "Real32"];
    r = TRealize @ TUOpReduce[x, 0, "MAX"];
    {TTensorShape[r], Normal @ TTensorData[r]},
    {{3}, {4.0, 5.0, 6.0}},
    TestID -> "reduce/axis0-rank2-max"
]

(* Contiguous trailing reduction chains lower as one wider REDUCE.
   This keeps KProgOp/renderers on the existing one-reduce contract
   while avoiding an intermediate boundary for patterns like
   batchnorm's H/W reductions. *)
VerificationTest[
    TInit[];
    data = ArrayReshape[Range[24] * 1.0, {2, 3, 4}];
    x = TTensorCreate @ NumericArray[data, "Real32"];
    before = TKernelCount[];
    r = TRealize @ TUOpReduce[TUOpReduce[x, 2, "SUM"], 1, "SUM"];
    after = TKernelCount[];
    scalar = TKernelScalarUops[before];
    red = Select[scalar, #["op"] === "S_REDUCE_SUM" &];
    {
        TTensorShape[r],
        Normal @ TTensorData[r],
        after - before,
        Length @ First[red]["src"]
    },
    {
        {2},
        Table[Total[Flatten[data[[c]]]], {c, 2}],
        1,
        3
    },
    TestID -> "reduce/chain-trailing-sum-one-kernel"
]

VerificationTest[
    TInit[];
    data = ArrayReshape[Range[120] * 1.0, {2, 3, 4, 5}];
    x = TTensorCreate @ NumericArray[data, "Real32"];
    before = TKernelCount[];
    r = TRealize @ TUOpReduce[TUOpReduce[x, 3, "MAX"], 2, "MAX"];
    after = TKernelCount[];
    {
        TTensorShape[r],
        Normal @ TTensorData[r],
        after - before
    },
    {
        {2, 3},
        Table[Max[Flatten[data[[b, c]]]], {b, 2}, {c, 3}],
        1
    },
    TestID -> "reduce/chain-trailing-max-one-kernel"
]

VerificationTest[
    TInit[];
    data = ArrayReshape[Range[120] * 1.0, {2, 3, 4, 5}];
    x = TTensorCreate @ NumericArray[data, "Real32"];
    xp = TUOpReshape[TUOpPermute[x, {1, 0, 2, 3}], {3, 40}];
    before = TKernelCount[];
    r = TRealize @ TUOpReduce[xp, 1, "SUM"];
    after = TKernelCount[];
    {
        TTensorShape[r],
        Normal @ TTensorData[r],
        after - before
    },
    {
        {3},
        Table[Total[Flatten[data[[All, c, All, All]]]], {c, 3}],
        1
    },
    TestID -> "reduce/permuted-channel-flatten-sum-one-kernel"
]
