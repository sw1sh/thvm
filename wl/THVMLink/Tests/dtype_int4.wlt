(* dtype_int4.wlt -- packed int4 / uint4 round-trip and arithmetic.
   Storage: 2 nibbles per byte, low nibble first.  numel is the
   logical nibble count; dtype_storage_bytes(numel) = (numel+1)/2.
   Arithmetic routes through eager-unpack-to-int8 (cpu_op_run_via_f32
   path); movement ops through cpu_op_run_via_i8.
   Dequantization at the user surface is CAST(int4_tensor, "f32"). *)

(* === pack / unpack helpers === *)

VerificationTest[
    TInit[];
    TUnpackInt4[TPackInt4[{1, -2, 3, -4}], 4],
    {1, -2, 3, -4},
    TestID -> "int4/pack-unpack/signed-roundtrip"
]

VerificationTest[
    TInit[];
    TUnpackUInt4[TPackUInt4[{0, 5, 10, 15}], 4],
    {0, 5, 10, 15},
    TestID -> "int4/pack-unpack/unsigned-roundtrip"
]

VerificationTest[
    (* Boundary values: -8 (0x8 sign-extended) and 7 (0x7). *)
    TInit[];
    TUnpackInt4[TPackInt4[{-8, -1, 0, 7}], 4],
    {-8, -1, 0, 7},
    TestID -> "int4/pack-unpack/signed-boundaries"
]

VerificationTest[
    (* Odd numel: trailing nibble lands in the low nibble of the last byte. *)
    TInit[];
    TUnpackInt4[TPackInt4[{1, 2, 3}], 3],
    {1, 2, 3},
    TestID -> "int4/pack-unpack/odd-numel"
]

VerificationTest[
    (* Storage byte count = ceil(numel/2). *)
    TInit[];
    Length[Normal @ TPackInt4[{1, 2, 3, 4, 5}]],
    3,   (* ceil(5/2) = 3 *)
    TestID -> "int4/pack-unpack/storage-bytes"
]

(* === tensor surface === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1, -2, 3, -4}, "i4"];
    {TTensorDType[a], TTensorShape[a],
     TUnpackInt4[TTensorData[a], 4]},
    {"i4", {4}, {1, -2, 3, -4}},
    TestID -> "int4/tensor/roundtrip"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{0, 5, 10, 15}, "u4"];
    {TTensorDType[a], TUnpackUInt4[TTensorData[a], 4]},
    {"u4", {0, 5, 10, 15}},
    TestID -> "uint4/tensor/roundtrip"
]

(* === arithmetic === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1, 2, 3, -4}, "i4"];
    b = TTensorCreate[{2, 1, 0, 1}, "i4"];
    sum = TRealize[a + b];
    TUnpackInt4[TTensorData[sum], 4],
    {3, 3, 3, -3},
    TestID -> "int4/arith/add"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1, 2, 3, 4}, "i4"];
    b = TTensorCreate[{2, 2, 2, 2}, "i4"];
    prod = TRealize[a * b];
    TUnpackInt4[TTensorData[prod], 4],
    {2, 4, 6, -8},   (* 4*2=8 wraps to -8 in int4 *)
    TestID -> "int4/arith/mul-with-wrap"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1, -2, 3, -4}, "i4"];
    neg = TRealize[-a];
    TUnpackInt4[TTensorData[neg], 4],
    {-1, 2, -3, 4},
    TestID -> "int4/arith/neg"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1, 2, 3, 4}, "u4"];
    b = TTensorCreate[{15, 14, 13, 12}, "u4"];
    sum = TRealize[a + b];
    TUnpackUInt4[TTensorData[sum], 4],
    {0, 0, 0, 0},   (* uint4 wraps: 1+15=16=0, etc. *)
    TestID -> "uint4/arith/add-wrap"
]

(* === reduce === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1, 2, 3, 1}, "i4"];
    s = TRealize @ TUOpReduce[a, 0, "SUM"];
    TUnpackInt4[TTensorData[s], 1],
    {7},
    TestID -> "int4/reduce-sum"
]

(* === movement (eager unpack-to-i8 path) === *)

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1, 2, 3, 4}, "i4"];
    r = TRealize @ TUOpFlip[a, {0}];
    TUnpackInt4[TTensorData[r], 4],
    {4, 3, 2, 1},
    TestID -> "int4/movement-flip"
]

VerificationTest[
    TInit[]; TReset[];
    a = TTensorCreate[{1, 2, 3, 4, 5, 6}, "i4"];
    r = TRealize @ TUOpReshape[a, {2, 3}];
    {TTensorShape[r], TUnpackInt4[TTensorData[r], 6]},
    {{2, 3}, {1, 2, 3, 4, 5, 6}},
    TestID -> "int4/movement-reshape"
]

(* === dequant via CAST === *)

VerificationTest[
    (* CAST int4 -> f32 is the dequantization: each nibble lifts into
       its signed integer value as a float.  User-side dequant scales
       this by a per-tensor or per-channel scale + zero_point. *)
    TInit[]; TReset[];
    a = TTensorCreate[{1, -2, 3, -4}, "i4"];
    f = TRealize @ TUOpCast[a, "f32"];
    {TTensorDType[f], Normal @ TTensorData[f]},
    {"f32", {1., -2., 3., -4.}},
    TestID -> "int4/cast-to-f32-dequant"
]

VerificationTest[
    (* Quantization round-trip: f32 -> int4 -> f32 truncates to
       integer values in the int4 range. *)
    TInit[]; TReset[];
    f = TTensorCreate @ NumericArray[{1.7, -2.3, 3.0, -4.0}, "Real32"];
    f2 = TRealize @ TUOpCast[TUOpCast[f, "i4"], "f32"];
    Normal @ TTensorData[f2],
    {1., -2., 3., -4.},
    TestID -> "int4/quantize-via-cast"
]

VerificationTest[
    (* Dequantization with scale + zero_point: scale=0.5, zp=-3.
       (raw - zp) * scale.  raw=1 -> (1 - -3) * 0.5 = 2.0. *)
    TInit[]; TReset[];
    a = TTensorCreate[{1, 2, 3, 4}, "i4"];
    af = TUOpCast[a, "f32"];
    zp = TTensorCreate @ NumericArray[{-3.0}, "Real32"];
    sc = TTensorCreate @ NumericArray[{0.5}, "Real32"];
    out = TRealize[(af - zp) * sc];
    Normal @ TTensorData[out],
    {2.0, 2.5, 3.0, 3.5},
    TestID -> "int4/dequant-scale-zeropoint"
]
