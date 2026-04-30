(* blas_dtypes.wlt -- BLAS dispatch on f64 tensors (Phase H).  The
   Accelerate cblas_d* family handles double-precision matmul /
   matvec / dot identical to the single-precision path; cpu_blas.c
   now gates on uniform-float dtype (f32 or f64) and dispatches to
   the right cblas variant. *)

VerificationTest[
    (* DOT on f64. *)
    TInit[]; TReset[];
    n = 64;
    a = TTensorCreate @ NumericArray[Range[1.0, 1.0 n], "Real64"];
    b = TTensorCreate @ NumericArray[ConstantArray[2.0, n], "Real64"];
    Normal @ TTensorData @ TRealize @ TDot[a, b],
    {2.0 (n (n + 1) / 2)},
    TestID -> "blas/f64-dot"
]

VerificationTest[
    (* GEMV on f64.  W:{4,3} @ x:{3} -> {4}. *)
    TInit[]; TReset[];
    W = TTensorCreate[
        ArrayReshape[N @ Range[12], {4, 3}], "f64"];
    x = TTensorCreate @ NumericArray[{1.0, 0.5, 0.25}, "Real64"];
    out = TRealize @ TMatVec[W, x];
    Normal @ TTensorData[out],
    Table[
        Sum[ArrayReshape[N @ Range[12], {4, 3}][[i, j]]
              * ({1.0, 0.5, 0.25}[[j]]),
            {j, 3}],
        {i, 4}],
    TestID -> "blas/f64-gemv"
]

VerificationTest[
    (* GEMM on f64.  A:{4,3} @ B:{3,2} -> {4,2}. *)
    TInit[]; TReset[];
    A = TTensorCreate[
        ArrayReshape[N @ Range[12], {4, 3}], "f64"];
    B = TTensorCreate[
        ArrayReshape[N @ Range[6],  {3, 2}], "f64"];
    out = TRealize @ TMatMul[A, B];
    ref = ArrayReshape[N @ Range[12], {4, 3}] . ArrayReshape[N @ Range[6], {3, 2}];
    Normal @ TTensorData[out] === ref,
    True,
    TestID -> "blas/f64-gemm"
]

VerificationTest[
    (* f32 GEMM still works (regression check). *)
    TInit[]; TReset[];
    A = TTensorCreate[
        ArrayReshape[N @ Range[12], {4, 3}], "f32"];
    B = TTensorCreate[
        ArrayReshape[N @ Range[6],  {3, 2}], "f32"];
    out = TRealize @ TMatMul[A, B];
    Normal @ TTensorData[out],
    ArrayReshape[N @ Range[12], {4, 3}] . ArrayReshape[N @ Range[6], {3, 2}],
    TestID -> "blas/f32-gemm-regression"
]

VerificationTest[
    (* Mixed dtype matmul falls back to interpret -- correctness only. *)
    TInit[]; TReset[];
    A = TTensorCreate @ NumericArray[ArrayReshape[Range[12], {4, 3}], "Integer32"];
    B = TTensorCreate @ NumericArray[ArrayReshape[Range[6],  {3, 2}], "Integer32"];
    out = TRealize @ TMatMul[A, B];
    Normal @ TTensorData[out],
    ArrayReshape[Range[12], {4, 3}] . ArrayReshape[Range[6], {3, 2}],
    TestID -> "blas/i32-matmul-via-interpret"
]
