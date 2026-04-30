(* jit_dtypes.wlt -- verify the clang JIT path covers every native
   ALU dtype (Phase G).  The JIT triggers on kernels with uniform
   dtype (no mixed-type chains) and matches one of the
   `cg_dtype_supported` values: bool, i8/u8, i16/u16, i32/u32, i64/u64,
   f32, f64.  f16 / bf16 / fp8 / int4 / uint4 fall through to the
   interpreter (no clang-buildable native type).

   Each test runs an arithmetic kernel large enough to force a JIT
   dispatch (n >= 64) and checks the result. *)

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate @ NumericArray[Range[n], "Integer8"];
    b = TTensorCreate @ NumericArray[ConstantArray[1, n], "Integer8"];
    Take[Normal @ TTensorData @ TRealize[a + b], 4],
    {2, 3, 4, 5},
    TestID -> "jit/i8-add"
]

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate @ NumericArray[Range[n], "Integer16"];
    b = TTensorCreate @ NumericArray[ConstantArray[10, n], "Integer16"];
    Take[Normal @ TTensorData @ TRealize[a + b], 4],
    {11, 12, 13, 14},
    TestID -> "jit/i16-add"
]

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate @ NumericArray[Range[n], "Integer32"];
    b = TTensorCreate @ NumericArray[ConstantArray[100, n], "Integer32"];
    Take[Normal @ TTensorData @ TRealize[a + b], 4],
    {101, 102, 103, 104},
    TestID -> "jit/i32-add"
]

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate @ NumericArray[Range[n], "Integer64"];
    b = TTensorCreate @ NumericArray[ConstantArray[1, n], "Integer64"];
    Take[Normal @ TTensorData @ TRealize[a + b], 4],
    {2, 3, 4, 5},
    TestID -> "jit/i64-add"
]

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate @ NumericArray[Range[n], "UnsignedInteger32"];
    b = TTensorCreate @ NumericArray[ConstantArray[1, n], "UnsignedInteger32"];
    Take[Normal @ TTensorData @ TRealize[a * b], 4],
    {1, 2, 3, 4},
    TestID -> "jit/u32-mul"
]

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate @ NumericArray[N @ Range[n], "Real64"];
    b = TTensorCreate @ NumericArray[ConstantArray[0.5, n], "Real64"];
    Take[Normal @ TTensorData @ TRealize[a + b], 4],
    {1.5, 2.5, 3.5, 4.5},
    TestID -> "jit/f64-add"
]

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate @ NumericArray[N @ Range[n], "Real64"];
    Take[Normal @ TTensorData @ TRealize @ Sqrt[a], 4],
    Sqrt[N @ Range[4]],
    TestID -> "jit/f64-sqrt"
]

(* JIT reduce-tail kernel for non-F32 dtypes. *)

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate @ NumericArray[Range[n], "Integer32"];
    Normal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "SUM"],
    {n (n + 1) / 2},
    TestID -> "jit/i32-reduce-sum"
]

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate @ NumericArray[N @ Range[n], "Real64"];
    Normal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "SUM"],
    {N[n (n + 1) / 2]},
    TestID -> "jit/f64-reduce-sum"
]

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate @ NumericArray[Reverse @ Range[n], "Integer32"];
    Normal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "MAX"],
    {n},
    TestID -> "jit/i32-reduce-max"
]

(* Integer comparison output is typed 1/0 (interpreter convention). *)

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate @ NumericArray[Range[n], "Integer32"];
    b = TTensorCreate @ NumericArray[ConstantArray[10, n], "Integer32"];
    Take[Normal @ TTensorData @ TRealize @ TUOpCmplt[a, b], 12],
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0},
    TestID -> "jit/i32-cmplt"
]

(* f16 / bf16 / fp8 / int4 fall back to interpreter (cg_supports
   rejects those dtypes).  Verifying correctness here is enough --
   the JIT path simply isn't taken. *)

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate[N @ Range[n], "f16"];
    b = TTensorCreate[ConstantArray[0.5, n], "f16"];
    Take[TFP16ToReal @ TTensorData @ TRealize[a + b], 4],
    {1.5, 2.5, 3.5, 4.5},
    TestID -> "jit/f16-falls-back-to-interp"
]

VerificationTest[
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate[N @ Range[n] / 64, "fp8e4m3"];
    b = TTensorCreate[ConstantArray[0.0, n], "fp8e4m3"];
    Take[TFP8E4M3ToReal @ TTensorData @ TRealize[a + b], 4],
    Take[TFP8E4M3ToReal @ TRealToFP8E4M3[N @ Range[n] / 64], 4],
    TestID -> "jit/fp8-falls-back-to-interp"
]
