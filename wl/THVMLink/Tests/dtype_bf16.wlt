(* dtype_bf16.wlt -- bfloat16 (8-exp / 7-mantissa, same exp range as
   f32).  Truncation of the low 16 mantissa bits is the only
   precision loss; range matches f32, so overflow only occurs at the
   f32 boundary. *)

VerificationTest[
    (* bf16 = high 16 bits of f32; integer values up to 2^7 fit
       exactly. *)
    TInit[];
    TBf16ToReal @ TRealToBf16[{1.0, 2.0, 4.0, 8.0, 16.0, 64.0, 128.0}],
    {1.0, 2.0, 4.0, 8.0, 16.0, 64.0, 128.0},
    TestID -> "bf16/convert/integers"
]

VerificationTest[
    TInit[];
    TBf16ToReal @ TRealToBf16[{0.0, 0.5, 0.25, -0.5}],
    {0.0, 0.5, 0.25, -0.5},
    TestID -> "bf16/convert/sub-unit"
]

VerificationTest[
    (* bf16 has the same exponent range as f32 -- huge values stay
       finite. *)
    TInit[];
    Module[{r = First @ TBf16ToReal @ TRealToBf16[{1.0*^30}]},
        r > 0 && r < Infinity
    ],
    True,
    TestID -> "bf16/convert/large-finite"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 3.0, 4.0}, "bf16"];
    {TTensorDType[a], TBf16ToReal @ TTensorData[a]},
    {"bf16", {1.0, 2.0, 3.0, 4.0}},
    TestID -> "bf16/tensor-roundtrip"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 3.0, 4.0}, "bf16"];
    b = TTensorCreate[{0.5, 0.5, 0.5, 0.5}, "bf16"];
    TBf16ToReal @ TTensorData @ TRealize[a + b],
    {1.5, 2.5, 3.5, 4.5},
    TestID -> "bf16/add-1d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{2.0, 4.0, 8.0}, "bf16"];
    b = TTensorCreate[{0.5, 0.5, 0.5}, "bf16"];
    TBf16ToReal @ TTensorData @ TRealize[a * b],
    {1.0, 2.0, 4.0},
    TestID -> "bf16/mul-1d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1., 2., 3., 4.}, "bf16"];
    TBf16ToReal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "SUM"],
    {10.},
    TestID -> "bf16/reduce-sum"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1., -2., 3., -4.}, "bf16"];
    TBf16ToReal @ TTensorData @ TRealize[-a],
    {-1., 2., -3., 4.},
    TestID -> "bf16/neg"
]
