(* dtype_f64.wlt -- float64 round-trip and arithmetic.  Verifies the
   native f64 ALU path landed in Phase C alongside f16 / bf16 (which
   ride on promote-to-f32). *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.5, 2.5, 3.5, 4.5}, "Real64"];
    {TTensorDType[a], Normal @ TTensorData[a]},
    {"f64", {1.5, 2.5, 3.5, 4.5}},
    TestID -> "dtype/f64-roundtrip"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.5, 2.5, 3.5, 4.5}, "Real64"];
    b = TTensorCreate @ NumericArray[{0.5, 0.5, 0.5, 0.5}, "Real64"];
    Normal @ TTensorData @ TRealize[a + b],
    {2., 3., 4., 5.},
    TestID -> "dtype/f64-add-1d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{2., 4., 6., 8.}, "Real64"];
    b = TTensorCreate @ NumericArray[{1., 2., 3., 4.}, "Real64"];
    Normal @ TTensorData @ TRealize[a * b],
    {2., 8., 18., 32.},
    TestID -> "dtype/f64-mul-1d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1., -2., 3., -4.}, "Real64"];
    Normal @ TTensorData @ TRealize[-a],
    {-1., 2., -3., 4.},
    TestID -> "dtype/f64-neg"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{4., 9., 16., 25.}, "Real64"];
    Normal @ TTensorData @ TRealize @ Sqrt[a],
    {2., 3., 4., 5.},
    TestID -> "dtype/f64-sqrt"
]

VerificationTest[
    (* Precision past the f32 epsilon: 1.0 + 1.0e-9 - 1.0 should
       survive as 1.0e-9 in f64 but not in f32. *)
    TInit[]; TReset[];
    eps = 1.0*^-9;
    one = TTensorCreate @ NumericArray[{1.0}, "Real64"];
    add = TTensorCreate @ NumericArray[{eps}, "Real64"];
    delta = TRealize[(one + add) - one];
    Abs[First @ Normal @ TTensorData[delta] - eps] < 1.0*^-12,
    True,
    TestID -> "dtype/f64-precision"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0, 5.0}, "Real64"];
    Normal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "SUM"],
    {15.},
    TestID -> "dtype/f64-reduce-sum"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{ {1., 2., 3.}, {4., 5., 6.} }, "Real64"];
    Normal @ TTensorData @ TRealize @ TUOpPermute[a, {1, 0}],
    { {1., 4.}, {2., 5.}, {3., 6.} },
    TestID -> "dtype/f64-permute-2d"
]

VerificationTest[
    (* 8-byte movement: flip f64 *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{10., 20., 30., 40.}, "Real64"];
    Normal @ TTensorData @ TRealize @ TUOpFlip[a, {0}],
    {40., 30., 20., 10.},
    TestID -> "dtype/f64-flip"
]
