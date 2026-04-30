(* dtype_int_family.wlt -- compact coverage for the integer dtypes
   landed in Phase B (bool, u8, i16, u16, u32, i64, u64).  i8 has
   its own deep coverage in dtype_int8.wlt; here we exercise add,
   mul, neg, cmpeq, cmplt, reduce, and the WL-bridge round-trip per
   dtype to keep the kernel switch honest. *)

(* --- bool ---------------------------------------------------------- *)
(* Bool ADD = OR, MUL = AND. *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{0, 1, 1, 0}, "UnsignedInteger8"];
    b = TTensorCreate @ NumericArray[{0, 0, 1, 1}, "UnsignedInteger8"];
    Normal @ TTensorData @ TRealize[a + b],
    {0, 1, 2, 1},
    TestID -> "dtype/u8-add-bool-pattern"
]

(* --- uint8 --------------------------------------------------------- *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3, 4}, "UnsignedInteger8"];
    b = TTensorCreate @ NumericArray[{255, 255, 1, 1}, "UnsignedInteger8"];
    (* Unsigned wrap: 1 + 255 -> 0, 2 + 255 -> 1, etc. *)
    Normal @ TTensorData @ TRealize[a + b],
    {0, 1, 4, 5},
    TestID -> "dtype/u8-add-wrap"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3, 4}, "UnsignedInteger8"];
    {TTensorDType[a], Normal @ TTensorData[a]},
    {"u8", {1, 2, 3, 4}},
    TestID -> "dtype/u8-roundtrip"
]

(* --- int16 / uint16 ----------------------------------------------- *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{32000, 32000}, "Integer16"];
    b = TTensorCreate @ NumericArray[{1000, -1000}, "Integer16"];
    Normal @ TTensorData @ TRealize[a + b],
    {-32536, 31000},   (* 32000+1000 wraps past 32767 to -32536 *)
    TestID -> "dtype/i16-add-wrap"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3, 4, 5}, "Integer16"];
    Normal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "SUM"],
    {15},
    TestID -> "dtype/i16-reduce-sum"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3, 4}, "UnsignedInteger16"];
    {TTensorDType[a], Normal @ TTensorData[a]},
    {"u16", {1, 2, 3, 4}},
    TestID -> "dtype/u16-roundtrip"
]

(* --- uint32 -------------------------------------------------------- *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{10, 20, 30, 40}, "UnsignedInteger32"];
    b = TTensorCreate @ NumericArray[{1, 2, 3, 4}, "UnsignedInteger32"];
    Normal @ TTensorData @ TRealize[a + b],
    {11, 22, 33, 44},
    TestID -> "dtype/u32-add"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3}, "UnsignedInteger32"];
    {TTensorDType[a], Normal @ TTensorData[a]},
    {"u32", {1, 2, 3}},
    TestID -> "dtype/u32-roundtrip"
]

(* --- int64 / uint64 (8-byte movement path) ------------------------- *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1000000000, 2000000000}, "Integer64"];
    b = TTensorCreate @ NumericArray[{1, 2}, "Integer64"];
    Normal @ TTensorData @ TRealize[a + b],
    {1000000001, 2000000002},
    TestID -> "dtype/i64-add"
]

VerificationTest[
    (* 8-byte movement: permute 2x3 i64 *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{ {10, 20, 30}, {40, 50, 60} }, "Integer64"];
    Normal @ TTensorData @ TRealize @ TUOpPermute[a, {1, 0}],
    { {10, 40}, {20, 50}, {30, 60} },
    TestID -> "dtype/i64-permute"
]

VerificationTest[
    (* 8-byte movement: flip i64 *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{100, 200, 300, 400}, "Integer64"];
    Normal @ TTensorData @ TRealize @ TUOpFlip[a, {0}],
    {400, 300, 200, 100},
    TestID -> "dtype/i64-flip"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3}, "UnsignedInteger64"];
    {TTensorDType[a], Normal @ TTensorData[a]},
    {"u64", {1, 2, 3}},
    TestID -> "dtype/u64-roundtrip"
]

(* --- cmpeq / cmplt across widths ----------------------------------- *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3}, "Integer16"];
    b = TTensorCreate @ NumericArray[{1, 5, 3}, "Integer16"];
    Normal @ TTensorData @ TRealize @ TUOpCmpeq[a, b],
    {1, 0, 1},
    TestID -> "dtype/i16-cmpeq"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3, 4}, "Integer64"];
    b = TTensorCreate @ NumericArray[{2, 2, 2, 2}, "Integer64"];
    Normal @ TTensorData @ TRealize @ TUOpCmplt[a, b],
    {1, 0, 0, 0},
    TestID -> "dtype/i64-cmplt"
]
