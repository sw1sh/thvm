(* dtype_fp8.wlt -- FP8 (e4m3 + e5m2) round-trip and arithmetic.
   Storage in u8 raw bytes; arithmetic via promote-to-f32 ALU.
   FP8 has very limited precision; tests stay on values exactly
   representable in each format. *)

(* === fp8e4m3: 4-bit exponent / 3-bit mantissa, EXP_BIAS=7 === *)
(* Representable powers of 2 from 2^-9 to 2^8 (256 = MAXNORM). *)

VerificationTest[
    TInit[];
    TFP8E4M3ToReal @ TRealToFP8E4M3[{0.0, 1.0, 2.0, 4.0, 8.0, 16.0}],
    {0.0, 1.0, 2.0, 4.0, 8.0, 16.0},
    TestID -> "fp8e4m3/convert/integers"
]

VerificationTest[
    TInit[];
    TFP8E4M3ToReal @ TRealToFP8E4M3[{1.0, -1.0, 2.0, -2.0}],
    {1.0, -1.0, 2.0, -2.0},
    TestID -> "fp8e4m3/convert/signs"
]

VerificationTest[
    TInit[];
    TFP8E4M3ToReal @ TRealToFP8E4M3[{0.5, -0.5, 0.25, -0.25}],
    {0.5, -0.5, 0.25, -0.25},
    TestID -> "fp8e4m3/convert/sub-unit"
]

VerificationTest[
    (* 448 = max representable in e4m3 (1.75 * 2^8 with mantissa 110). *)
    TInit[];
    First @ TFP8E4M3ToReal @ TRealToFP8E4M3[{448.0}],
    448.0,
    TestID -> "fp8e4m3/convert/max-finite"
]

VerificationTest[
    (* Tensor surface. *)
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 4.0, 8.0}, "fp8e4m3"];
    {TTensorDType[a], TFP8E4M3ToReal @ TTensorData[a]},
    {"fp8e4m3", {1.0, 2.0, 4.0, 8.0}},
    TestID -> "fp8e4m3/tensor-roundtrip"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 0.5, -1.5}, "fp8e4m3"];
    b = TTensorCreate[{0.5, 0.5, 0.5, 0.5}, "fp8e4m3"];
    TFP8E4M3ToReal @ TTensorData @ TRealize[a + b],
    {1.5, 2.5, 1.0, -1.0},
    TestID -> "fp8e4m3/add-1d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 4.0}, "fp8e4m3"];
    b = TTensorCreate[{2.0, 1.0, 0.5}, "fp8e4m3"];
    TFP8E4M3ToReal @ TTensorData @ TRealize[a * b],
    {2.0, 2.0, 2.0},
    TestID -> "fp8e4m3/mul-1d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, -2.0, 4.0, -0.5}, "fp8e4m3"];
    TFP8E4M3ToReal @ TTensorData @ TRealize[-a],
    {-1.0, 2.0, -4.0, 0.5},
    TestID -> "fp8e4m3/neg"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{4.0, 16.0, 64.0}, "fp8e4m3"];
    TFP8E4M3ToReal @ TTensorData @ TRealize @ Sqrt[a],
    {2.0, 4.0, 8.0},
    TestID -> "fp8e4m3/sqrt"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 4.0, 8.0}, "fp8e4m3"];
    TFP8E4M3ToReal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "SUM"],
    {15.0},
    TestID -> "fp8e4m3/reduce-sum"
]

(* === fp8e5m2: 5-bit exponent / 2-bit mantissa, EXP_BIAS=15 === *)

VerificationTest[
    TInit[];
    TFP8E5M2ToReal @ TRealToFP8E5M2[{0.0, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0}],
    {0.0, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0},
    TestID -> "fp8e5m2/convert/integers"
]

VerificationTest[
    TInit[];
    TFP8E5M2ToReal @ TRealToFP8E5M2[{1.0, -1.0, 256.0, -256.0}],
    {1.0, -1.0, 256.0, -256.0},
    TestID -> "fp8e5m2/convert/signs-and-large"
]

VerificationTest[
    (* fp8e5m2 has wider exponent range -> max ~57344. *)
    TInit[];
    First @ TFP8E5M2ToReal @ TRealToFP8E5M2[{49152.0}],
    49152.0,
    TestID -> "fp8e5m2/convert/large-finite"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 4.0, 8.0}, "fp8e5m2"];
    {TTensorDType[a], TFP8E5M2ToReal @ TTensorData[a]},
    {"fp8e5m2", {1.0, 2.0, 4.0, 8.0}},
    TestID -> "fp8e5m2/tensor-roundtrip"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 4.0, 8.0}, "fp8e5m2"];
    b = TTensorCreate[{1.0, 2.0, 4.0, 8.0}, "fp8e5m2"];
    TFP8E5M2ToReal @ TTensorData @ TRealize[a + b],
    {2.0, 4.0, 8.0, 16.0},
    TestID -> "fp8e5m2/add-1d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{2.0, 4.0, 8.0}, "fp8e5m2"];
    b = TTensorCreate[{2.0, 0.5, 0.5}, "fp8e5m2"];
    TFP8E5M2ToReal @ TTensorData @ TRealize[a * b],
    {4.0, 2.0, 4.0},
    TestID -> "fp8e5m2/mul-1d"
]

VerificationTest[
    (* {2,4,8,16} sums to 30 in real arithmetic; e5m2 has only 2
       mantissa bits, so 30 rounds to the nearest representable
       value 32 (28 is the predecessor). *)
    TInit[]; TReset[];
    a = TTensorCreate[{2.0, 4.0, 8.0, 16.0}, "fp8e5m2"];
    TFP8E5M2ToReal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "SUM"],
    {32.0},
    TestID -> "fp8e5m2/reduce-sum"
]

(* === movement (1-byte path; reuses int8 width) === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 4.0, 8.0}, "fp8e4m3"];
    TFP8E4M3ToReal @ TTensorData @ TRealize @ TUOpFlip[a, {0}],
    {8.0, 4.0, 2.0, 1.0},
    TestID -> "fp8/movement-flip"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{ {1.0, 2.0, 4.0}, {8.0, 16.0, 32.0} }, "fp8e5m2"];
    Module[{na = TTensorData @ TRealize @ TUOpPermute[a, {1, 0}]},
        TFP8E5M2ToReal @ NumericArray[Flatten @ Normal @ na, "UnsignedInteger8"]
    ],
    {1.0, 8.0, 2.0, 16.0, 4.0, 32.0},
    TestID -> "fp8/movement-permute-2d"
]
