(* dtype_int8.wlt -- arithmetic + movement on signed-int8 tensors.
   Verifies the Phase B integer-family kernel path (CPU interpret +
   per-dtype switch) and the WL bridge for the Integer8 NumericArray
   carrier.  Wrap-around at the int8 boundary (-128..127) is the key
   correctness property; we test it explicitly. *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3, 4}, "Integer8"];
    b = TTensorCreate @ NumericArray[{10, 20, 30, 40}, "Integer8"];
    Normal @ TTensorData @ TRealize[a + b],
    {11, 22, 33, 44},
    TestID -> "dtype/i8-add-1d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{2, 4, 6, 8}, "Integer8"];
    b = TTensorCreate @ NumericArray[{3, 5, 7, 9}, "Integer8"];
    Normal @ TTensorData @ TRealize[a * b],
    {6, 20, 42, 72},
    TestID -> "dtype/i8-mul-1d"
]

VerificationTest[
    (* Signed wrap: 127 + 1 -> -128 *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{127, 127, -128, -128}, "Integer8"];
    b = TTensorCreate @ NumericArray[{1, 2, -1, -2}, "Integer8"];
    Normal @ TTensorData @ TRealize[a + b],
    {-128, -127, 127, 126},
    TestID -> "dtype/i8-add-wrap"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, -2, 3, -4}, "Integer8"];
    Normal @ TTensorData @ TRealize[-a],
    {-1, 2, -3, 4},
    TestID -> "dtype/i8-neg"
]

VerificationTest[
    (* dtype is preserved through reduce-sum.  Signed accumulator. *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3, 4, 5}, "Integer8"];
    Normal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "SUM"],
    {15},
    TestID -> "dtype/i8-reduce-sum-1d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{ {1, 2, 3}, {10, 20, 30} }, "Integer8"];
    Normal @ TTensorData @ TRealize @ TUOpReduce[a, 1, "MAX"],
    {3, 30},
    TestID -> "dtype/i8-reduce-max-2d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3, 4}, "Integer8"];
    b = TTensorCreate @ NumericArray[{1, 5, 3, 9}, "Integer8"];
    Normal @ TTensorData @ TRealize @ TUOpCmpeq[a, b],
    {1, 0, 1, 0},
    TestID -> "dtype/i8-cmpeq"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 5, 3, 9}, "Integer8"];
    b = TTensorCreate @ NumericArray[{2, 4, 3, 10}, "Integer8"];
    Normal @ TTensorData @ TRealize @ TUOpCmplt[a, b],
    {1, 0, 0, 1},
    TestID -> "dtype/i8-cmplt"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{ {1, 2, 3}, {4, 5, 6} }, "Integer8"];
    Normal @ TTensorData @ TRealize @ TUOpReshape[a, {6}],
    {1, 2, 3, 4, 5, 6},
    TestID -> "dtype/i8-reshape"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{ {1, 2, 3}, {4, 5, 6} }, "Integer8"];
    Normal @ TTensorData @ TRealize @ TUOpPermute[a, {1, 0}],
    { {1, 4}, {2, 5}, {3, 6} },
    TestID -> "dtype/i8-permute-2d"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3}, "Integer8"];
    Normal @ TTensorData @ TRealize @ TUOpFlip[a, {0}],
    {3, 2, 1},
    TestID -> "dtype/i8-flip"
]

VerificationTest[
    (* dtype roundtrip: write Integer8 NA -> tensor -> read back *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{-5, 0, 5, 127, -128}, "Integer8"];
    {TTensorDType[a], Normal @ TTensorData[a]},
    {"i8", {-5, 0, 5, 127, -128}},
    TestID -> "dtype/i8-roundtrip"
]
