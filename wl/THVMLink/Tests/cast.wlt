(* cast.wlt -- value-preserving CAST UOP across the wired dtypes.
   Phase E.  Each case checks that TUOpCast routes through the
   to_fp32_lane / from_fp32_lane primitives correctly: int->int
   sign / zero extension, int<->float (fptosi / sitofp), float<->
   float precision drift. *)

(* === int -> int === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, -2, 3, -4}, "Integer8"];
    r = TRealize @ TUOpCast[a, "i32"];
    {TTensorDType[r], Normal @ TTensorData[r]},
    {"i32", {1, -2, 3, -4}},
    TestID -> "cast/int/i8-to-i32-sign-extend"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{200, 100, 50}, "UnsignedInteger8"];
    r = TRealize @ TUOpCast[a, "i32"];
    Normal @ TTensorData[r],
    {200, 100, 50},
    TestID -> "cast/int/u8-to-i32-zero-extend"
]

VerificationTest[
    (* int32 -> int8 truncates the high bytes. *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{300, 1024, -1}, "Integer32"];
    r = TRealize @ TUOpCast[a, "i8"];
    Normal @ TTensorData[r],
    {44, 0, -1},   (* 300 mod 256 = 44; 1024 mod 256 = 0; -1 = 0xFF as i8 = -1 *)
    TestID -> "cast/int/i32-to-i8-truncate"
]

(* === int -> float === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3, -4}, "Integer32"];
    r = TRealize @ TUOpCast[a, "f32"];
    {TTensorDType[r], Normal @ TTensorData[r]},
    {"f32", {1., 2., 3., -4.}},
    TestID -> "cast/int-to-float/i32-to-f32"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{255, 128}, "UnsignedInteger8"];
    r = TRealize @ TUOpCast[a, "f32"];
    Normal @ TTensorData[r],
    {255., 128.},
    TestID -> "cast/int-to-float/u8-to-f32"
]

(* === float -> int === *)

VerificationTest[
    (* Float -> int truncates toward zero. *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.5, -1.5, 2.7, -2.7}, "Real32"];
    r = TRealize @ TUOpCast[a, "i32"];
    Normal @ TTensorData[r],
    {1, -1, 2, -2},
    TestID -> "cast/float-to-int/f32-to-i32-trunc"
]

(* === float -> float (precision drift) === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    r = TRealize @ TUOpCast[a, "f64"];
    {TTensorDType[r], Normal @ TTensorData[r]},
    {"f64", {1., 2., 3., 4.}},
    TestID -> "cast/float/f32-to-f64-exact"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 4.0, 8.0}, "Real64"];
    r = TRealize @ TUOpCast[a, "f32"];
    Normal @ TTensorData[r],
    {1., 2., 4., 8.},
    TestID -> "cast/float/f64-to-f32-exact-powers-of-2"
]

VerificationTest[
    (* f32 -> f16 -> f32 round-trip exact for representable values. *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 0.5, 0.25}, "Real32"];
    r = TRealize @ TUOpCast[TUOpCast[a, "f16"], "f32"];
    Normal @ TTensorData[r],
    {1., 2., 0.5, 0.25},
    TestID -> "cast/float/f32-f16-f32-roundtrip"
]

VerificationTest[
    (* f32 -> bf16 -> f32 exact for integer values. *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.0, 4.0, 16.0, 64.0}, "Real32"];
    r = TRealize @ TUOpCast[TUOpCast[a, "bf16"], "f32"];
    Normal @ TTensorData[r],
    {1., 4., 16., 64.},
    TestID -> "cast/float/f32-bf16-f32-roundtrip"
]

(* === bool === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.5, 0.0, -2.0, 0.0}, "Real32"];
    r = TRealize @ TUOpCast[a, "bool"];
    {TTensorDType[r], Normal @ TTensorData[r]},
    {"bool", {1, 0, 1, 0}},
    TestID -> "cast/bool/from-f32"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 0, 1, 0}, "UnsignedInteger8"];
    r = TRealize @ TUOpCast[TUOpCast[a, "bool"], "f32"];
    Normal @ TTensorData[r],
    {1., 0., 1., 0.},
    TestID -> "cast/bool/to-f32"
]

(* === FP8 + CAST === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 4.0, 8.0}, "Real32"];
    r = TRealize @ TUOpCast[a, "fp8e4m3"];
    {TTensorDType[r], TFP8E4M3ToReal @ TTensorData[r]},
    {"fp8e4m3", {1., 2., 4., 8.}},
    TestID -> "cast/fp8/f32-to-fp8e4m3"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1.0, 2.0, 4.0, 8.0}, "fp8e5m2"];
    r = TRealize @ TUOpCast[a, "f32"];
    Normal @ TTensorData[r],
    {1., 2., 4., 8.},
    TestID -> "cast/fp8/fp8e5m2-to-f32"
]

(* === Identity fold === *)

VerificationTest[
    (* CAST(x, x.dtype) -> x.  Verify by checking the raw Term value
       is unchanged -- TUOpCast returns the source as-is when the
       target dtype already matches. *)
    TInit[]; TReset[];
    a = TTensorCreate @ NumericArray[{1, 2, 3}, "Integer32"];
    casted = TUOpCast[a, "i32"];
    TTermVal[casted] === TTermVal[a] && TTermTag[casted] === TTermTag[a],
    True,
    TestID -> "cast/fold/identity"
]
