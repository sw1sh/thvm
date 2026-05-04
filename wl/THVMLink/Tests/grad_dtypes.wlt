(* grad_dtypes.wlt -- TGrad parity across non-f32 float dtypes.
   Mirrors a subset of grad.wlt's f32 cases at f64 / f16 / bf16.

   Regression for the dtype-aware cotangent fix.  Pre-fix
   gradOnesSeed built TUOpConst[1.0] at the default "f32" dtype
   regardless of y's dtype, and interact_grad emitted scalar zeros
   and ln2 / half / inv_ln2 bookkeeping consts via
   uop_const(DT_FP32, ...).  Both sources bit-misaligned the
   kernel buffer under f64 / f16 / bf16: cpu_op_add on a numel-1
   broadcast f32 zero against an f64 src reads 8 bytes from the
   4-byte const, which surfaced as ~e-314 garbage out of d(a*b)/da
   at f64 and as wrong-dtype zero leaks at the independent-leaf,
   ReLU-mask, and higher-order paths.

   The fix: WL gradOnesSeed + inheritDType walk the UOP / DP1 grad
   graph for the right dtype, term_dtype_in handles DP0/DP1 +
   DUP_GRAD_FLAG, and interact_grad threads grad_target_dtype()
   through every uop_const call. *)

(* === leaf identity (d(a)/d(a) = 1) === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real64"];
    g = TRealize @ TGrad[a, a];
    {TTensorDType[g], Normal @ TTensorData[g]},
    {"f64", {1.0, 1.0, 1.0}},
    TestID -> "grad/f64-identity-1d"
]

VerificationTest[
    TInit[];
    a = TTensorCreate[{1.0, 2.0, 3.0}, "f16"];
    g = TRealize @ TGrad[a, a];
    {TTensorDType[g], TFP16ToReal @ TTensorData[g]},
    {"f16", {1.0, 1.0, 1.0}},
    TestID -> "grad/f16-identity-1d"
]

VerificationTest[
    TInit[];
    a = TTensorCreate[{1.0, 2.0, 3.0}, "bf16"];
    g = TRealize @ TGrad[a, a];
    {TTensorDType[g], TBf16ToReal @ TTensorData[g]},
    {"bf16", {1.0, 1.0, 1.0}},
    TestID -> "grad/bf16-identity-1d"
]

(* === independent leaf zero (d(b)/d(a) = 0) === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real64"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real64"];
    g = TRealize @ TGrad[b, a];
    {TTensorDType[g], Normal @ TTensorData[g]},
    {"f64", {0.0, 0.0, 0.0}},
    TestID -> "grad/f64-independent-leaf-zero"
]

VerificationTest[
    TInit[];
    a = TTensorCreate[{1.0, 2.0, 3.0}, "f16"];
    b = TTensorCreate[{4.0, 5.0, 6.0}, "f16"];
    g = TRealize @ TGrad[b, a];
    {TTensorDType[g], TFP16ToReal @ TTensorData[g]},
    {"f16", {0.0, 0.0, 0.0}},
    TestID -> "grad/f16-independent-leaf-zero"
]

VerificationTest[
    TInit[];
    a = TTensorCreate[{1.0, 2.0, 3.0}, "bf16"];
    b = TTensorCreate[{4.0, 5.0, 6.0}, "bf16"];
    g = TRealize @ TGrad[b, a];
    {TTensorDType[g], TBf16ToReal @ TTensorData[g]},
    {"bf16", {0.0, 0.0, 0.0}},
    TestID -> "grad/bf16-independent-leaf-zero"
]

(* === ADD (d(a+b)/d(a) = 1) === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real64"];
    b = TTensorCreate @ NumericArray[{10.0, 20.0, 30.0}, "Real64"];
    g = TRealize @ TGrad[TUOpAdd[a, b], a];
    {TTensorDType[g], Normal @ TTensorData[g]},
    {"f64", {1.0, 1.0, 1.0}},
    TestID -> "grad/f64-add-w-r-t-a"
]

VerificationTest[
    TInit[];
    a = TTensorCreate[{1.0, 2.0, 3.0}, "f16"];
    b = TTensorCreate[{10.0, 20.0, 30.0}, "f16"];
    g = TRealize @ TGrad[TUOpAdd[a, b], a];
    {TTensorDType[g], TFP16ToReal @ TTensorData[g]},
    {"f16", {1.0, 1.0, 1.0}},
    TestID -> "grad/f16-add-w-r-t-a"
]

VerificationTest[
    TInit[];
    a = TTensorCreate[{1.0, 2.0, 3.0}, "bf16"];
    b = TTensorCreate[{10.0, 20.0, 30.0}, "bf16"];
    g = TRealize @ TGrad[TUOpAdd[a, b], a];
    {TTensorDType[g], TBf16ToReal @ TTensorData[g]},
    {"bf16", {1.0, 1.0, 1.0}},
    TestID -> "grad/bf16-add-w-r-t-a"
]

(* === MUL product rule (d(a*b)/d(a) = b) === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real64"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real64"];
    g = TRealize @ TGrad[TUOpMul[a, b], a];
    {TTensorDType[g], Normal @ TTensorData[g]},
    {"f64", {4.0, 5.0, 6.0}},
    TestID -> "grad/f64-mul-product-rule"
]

VerificationTest[
    TInit[];
    a = TTensorCreate[{1.0, 2.0, 3.0}, "f16"];
    b = TTensorCreate[{4.0, 5.0, 6.0}, "f16"];
    g = TRealize @ TGrad[TUOpMul[a, b], a];
    {TTensorDType[g], TFP16ToReal @ TTensorData[g]},
    {"f16", {4.0, 5.0, 6.0}},
    TestID -> "grad/f16-mul-product-rule"
]

VerificationTest[
    TInit[];
    a = TTensorCreate[{1.0, 2.0, 3.0}, "bf16"];
    b = TTensorCreate[{4.0, 5.0, 6.0}, "bf16"];
    g = TRealize @ TGrad[TUOpMul[a, b], a];
    {TTensorDType[g], TBf16ToReal @ TTensorData[g]},
    {"bf16", {4.0, 5.0, 6.0}},
    TestID -> "grad/bf16-mul-product-rule"
]

(* === NEG (d(-a)/d(a) = -1) === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real64"];
    g = TRealize @ TGrad[TUOpNeg[a], a];
    {TTensorDType[g], Normal @ TTensorData[g]},
    {"f64", {-1.0, -1.0, -1.0}},
    TestID -> "grad/f64-neg"
]

VerificationTest[
    TInit[];
    a = TTensorCreate[{1.0, 2.0, 3.0}, "f16"];
    g = TRealize @ TGrad[TUOpNeg[a], a];
    {TTensorDType[g], TFP16ToReal @ TTensorData[g]},
    {"f16", {-1.0, -1.0, -1.0}},
    TestID -> "grad/f16-neg"
]

(* === REDUCE_SUM (d(sum a)/d(a) = ones) === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real64"];
    g = TRealize @ TGrad[TUOpReduce[a, 0, "SUM"], a];
    {TTensorDType[g], Normal @ TTensorData[g]},
    {"f64", {1.0, 1.0, 1.0, 1.0}},
    TestID -> "grad/f64-reduce-sum-broadcasts-back"
]

VerificationTest[
    TInit[];
    a = TTensorCreate[{1.0, 2.0, 3.0, 4.0}, "f16"];
    g = TRealize @ TGrad[TUOpReduce[a, 0, "SUM"], a];
    {TTensorDType[g], TFP16ToReal @ TTensorData[g]},
    {"f16", {1.0, 1.0, 1.0, 1.0}},
    TestID -> "grad/f16-reduce-sum-broadcasts-back"
]

(* === composite x*x (d(a*a)/d(a) = 2a) === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{2.0, 3.0, 5.0}, "Real64"];
    g = TRealize @ TGrad[TUOpMul[a, a], a];
    {TTensorDType[g], Normal @ TTensorData[g]},
    {"f64", {4.0, 6.0, 10.0}},
    TestID -> "grad/f64-x-times-x-equals-2x"
]

VerificationTest[
    TInit[];
    a = TTensorCreate[{2.0, 3.0, 5.0}, "f16"];
    g = TRealize @ TGrad[TUOpMul[a, a], a];
    {TTensorDType[g], TFP16ToReal @ TTensorData[g]},
    {"f16", {4.0, 6.0, 10.0}},
    TestID -> "grad/f16-x-times-x-equals-2x"
]

(* === RECIP (d(1/a)/d(a) = -1/a^2) === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{2.0, 4.0, 5.0}, "Real64"];
    g = TRealize @ TGrad[TUOpRecip[a], a];
    {TTensorDType[g], Normal @ TTensorData[g]},
    {"f64", {-0.25, -0.0625, -0.04}},
    SameTest -> (#1[[1]] === #2[[1]] && Max[Abs[#1[[2]] - #2[[2]]]] < 1.0*^-12 &),
    TestID -> "grad/f64-recip-derivative"
]

(* === ReLU mask (d(ReLU(a))/d(a) = mask) === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{-1.0, 2.0, -3.0, 4.0}, "Real64"];
    g = TRealize @ TGrad[TReLU[a], a];
    {TTensorDType[g], Normal @ TTensorData[g]},
    {"f64", {0.0, 1.0, 0.0, 1.0}},
    TestID -> "grad/f64-relu-mask"
]

(* === higher-order: d^2(a*a)/da^2 = 2 === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{3.0}, "Real64"];
    g = TRealize @ TGrad[TGrad[TUOpMul[a, a], a], a];
    {TTensorDType[g], Normal @ TTensorData[g]},
    {"f64", {2.0}},
    TestID -> "grad/f64-higher-order-square"
]
