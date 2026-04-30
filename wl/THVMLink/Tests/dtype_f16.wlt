(* dtype_f16.wlt -- IEEE-754 half precision (5-exp / 10-mantissa).
   Storage in u16 raw bytes; arithmetic via promote-to-f32 ALU.
   Round-trip exact for f16-representable values; arithmetic
   correctness within f16 ULP. *)

(* === conversion roundtrip === *)

VerificationTest[
    (* Powers of two are exactly representable in f16 up to 2^15. *)
    TInit[];
    TFP16ToReal @ TRealToFP16[{1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0}],
    {1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0},
    TestID -> "fp16/convert/powers-of-2"
]

VerificationTest[
    TInit[];
    TFP16ToReal @ TRealToFP16[{0.0, 0.5, 0.25, 0.125, -0.5, -0.25}],
    {0.0, 0.5, 0.25, 0.125, -0.5, -0.25},
    TestID -> "fp16/convert/sub-unit"
]

VerificationTest[
    (* Max finite f16 = 65504. *)
    TInit[];
    First @ TFP16ToReal @ TRealToFP16[{65504.0}],
    65504.0,
    TestID -> "fp16/convert/max-finite"
]

VerificationTest[
    (* Just-under-max value still finite; just-over starts to lose
       precision but stays finite up to 65504. *)
    TInit[];
    First @ TFP16ToReal @ TRealToFP16[{60000.0}],
    60000.0,
    TestID -> "fp16/convert/large-finite"
]

(* === tensor surface === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 3.0, 4.0}, "f16"];
    {TTensorDType[a], TFP16ToReal @ TTensorData[a]},
    {"f16", {1.0, 2.0, 3.0, 4.0}},
    TestID -> "fp16/tensor-roundtrip"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1.5, 2.5, 3.5, 4.5}, "f16"];
    b = TTensorCreate[{0.5, 0.5, 0.5, 0.5}, "f16"];
    TFP16ToReal @ TTensorData @ TRealize[a + b],
    {2.0, 3.0, 4.0, 5.0},
    TestID -> "fp16/add-1d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{2.0, 4.0, 6.0}, "f16"];
    b = TTensorCreate[{3.0, 0.5, 2.0}, "f16"];
    TFP16ToReal @ TTensorData @ TRealize[a * b],
    {6.0, 2.0, 12.0},
    TestID -> "fp16/mul-1d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{4.0, 9.0, 16.0, 25.0}, "f16"];
    TFP16ToReal @ TTensorData @ TRealize @ Sqrt[a],
    {2.0, 3.0, 4.0, 5.0},
    TestID -> "fp16/sqrt"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1., 2., 3., 4.}, "f16"];
    TFP16ToReal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "SUM"],
    {10.},
    TestID -> "fp16/reduce-sum"
]

VerificationTest[
    (* Movement: 2-byte permute.  TTensorData reads back the raw u16
       packed bytes; TFP16ToReal needs a NumericArray, but Flatten/
       Normal returns a list of u16 ints, so we re-pack as a flat
       UnsignedInteger16 NumericArray for the unpack helper. *)
    TInit[]; TReset[];
    a = TTensorCreate[{ {1., 2., 3.}, {4., 5., 6.} }, "f16"];
    Module[{na = TTensorData @ TRealize @ TUOpPermute[a, {1, 0}]},
        TFP16ToReal @ NumericArray[Flatten @ Normal @ na, "UnsignedInteger16"]
    ],
    {1., 4., 2., 5., 3., 6.},
    TestID -> "fp16/permute-2d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1., 2., 3.}, "f16"];
    TFP16ToReal @ TTensorData @ TRealize @ TUOpFlip[a, {0}],
    {3., 2., 1.},
    TestID -> "fp16/flip"
]
