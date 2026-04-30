(* bitcast.wlt -- bit-level reinterpret BITCAST UOP.  Source and
   destination must share itemsize.  Phase E. *)

(* === 4-byte: f32 <-> i32 / u32 === *)

VerificationTest[
    (* 1.0 = 0x3F800000 = 1065353216 *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 4.0}, "Real32"];
    r = TRealize @ TUOpBitcast[a, "i32"];
    Normal @ TTensorData[r],
    {1065353216, 1073741824, 1082130432},   (* 0x3F800000 / 0x40000000 / 0x40800000 *)
    TestID -> "bitcast/4B/f32-to-i32-bit-pattern"
]

VerificationTest[
    (* Round-trip f32 -> i32 -> f32 is exact. *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.5, -2.25, 3.125}, "Real32"];
    r = TRealize @ TUOpBitcast[TUOpBitcast[a, "i32"], "f32"];
    Normal @ TTensorData[r],
    {1.5, -2.25, 3.125},
    TestID -> "bitcast/4B/f32-i32-roundtrip"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3}, "UnsignedInteger32"];
    r = TRealize @ TUOpBitcast[a, "i32"];
    Normal @ TTensorData[r],
    {1, 2, 3},
    TestID -> "bitcast/4B/u32-to-i32"
]

(* === 8-byte: f64 <-> i64 / u64 === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.5, 2.5}, "Real64"];
    r = TRealize @ TUOpBitcast[TUOpBitcast[a, "i64"], "f64"];
    Normal @ TTensorData[r],
    {1.5, 2.5},
    TestID -> "bitcast/8B/f64-i64-roundtrip"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{100, 200, -1}, "Integer64"];
    r = TRealize @ TUOpBitcast[a, "u64"];
    {TTensorDType[r], Normal @ TTensorData[r]},
    {"u64", {100, 200, 18446744073709551615}},   (* -1 reinterpreted as u64 *)
    TestID -> "bitcast/8B/i64-to-u64-negative"
]

(* === 2-byte: f16 / bf16 / i16 / u16 === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3}, "Integer16"];
    r = TRealize @ TUOpBitcast[a, "u16"];
    Normal @ TTensorData[r],
    {1, 2, 3},
    TestID -> "bitcast/2B/i16-to-u16"
]

VerificationTest[
    (* fp16 of 1.0 = 0x3C00 = 15360 unsigned. *)
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 4.0}, "f16"];
    r = TRealize @ TUOpBitcast[a, "u16"];
    Normal @ TTensorData[r],
    {15360, 16384, 17408},   (* 0x3C00 / 0x4000 / 0x4400 *)
    TestID -> "bitcast/2B/f16-to-u16-bit-pattern"
]

(* === 1-byte: i8 / u8 / fp8 / bool === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, -1, 100}, "Integer8"];
    r = TRealize @ TUOpBitcast[a, "u8"];
    Normal @ TTensorData[r],
    {1, 255, 100},   (* -1 reinterpreted as u8 = 255 *)
    TestID -> "bitcast/1B/i8-to-u8"
]

VerificationTest[
    (* fp8e4m3 of 1.0 = 0x38 = 56 unsigned. *)
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 4.0}, "fp8e4m3"];
    r = TRealize @ TUOpBitcast[a, "u8"];
    Normal @ TTensorData[r],
    {56, 64, 72},   (* 0x38 / 0x40 / 0x48 *)
    TestID -> "bitcast/1B/fp8e4m3-to-u8-bit-pattern"
]

(* === Folding === *)

VerificationTest[
    (* BITCAST(BITCAST(x, mid), dst) -> BITCAST(x, dst) *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0}, "Real32"];
    r = TRealize @ TUOpBitcast[TUOpBitcast[a, "u32"], "i32"];
    Normal @ TTensorData[r],
    {1065353216, 1073741824},
    TestID -> "bitcast/fold/double-bitcast"
]

VerificationTest[
    (* BITCAST(x, x.dtype) -> x *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3}, "Integer32"];
    bc = TUOpBitcast[a, "i32"];
    TTermVal[bc] === TTermVal[a] && TTermTag[bc] === TTermTag[a],
    True,
    TestID -> "bitcast/fold/identity"
]
