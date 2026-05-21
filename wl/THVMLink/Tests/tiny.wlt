(* tiny.wlt -- thvm port of tinygrad's canonical smoke test
   (tinygrad test/test_tiny.py: TestTiny).  Self-contained checks of the
   basic external functionality: const, copy, elementwise add, sum, gemm,
   gemv.  tinygrad's eye()/cat()/elu() helpers have no thvm builder, so
   eye is supplied host-side (IdentityMatrix) and elu is replaced by the
   equivalent relu-of-positive identity check. *)

(* tiny/const: a 1-element tensor round-trips its value. *)
VerificationTest[
    TInit[];
    First @ Normal @ TTensorData @ TRealize @
        TTensorCreate @ NumericArray[{2.0}, "Real32"],
    2.0,
    TestID -> "tiny/const"
]

(* tiny/copy: [1,2,3] tolist. *)
VerificationTest[
    TInit[];
    Normal @ TTensorData @ TRealize @
        TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"],
    {1.0, 2.0, 3.0},
    TestID -> "tiny/copy"
]

(* tiny/plus: [1,2,3] + [4,5,6] = [5,7,9]. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    Normal @ TTensorData @ TRealize[a + b],
    {5.0, 7.0, 9.0},
    TestID -> "tiny/plus"
]

(* tiny/plus-big: ones(16) + ones(16) = 2 everywhere. *)
VerificationTest[
    TInit[];
    Normal @ TTensorData @ TRealize[TOnes[{16}] + TOnes[{16}]],
    ConstantArray[2.0, 16],
    TestID -> "tiny/plus-big"
]

(* tiny/sum: ones(N).sum() = N. *)
VerificationTest[
    TInit[];
    First @ Normal @ TTensorData @ TRealize @ TSum[TOnes[{256}]],
    256.0,
    TestID -> "tiny/sum"
]

(* tiny/sum-axis: [[1,2],[3,4]].sum(axis=1) = [3,7].  (tinygrad test_elu
   without the elu, which is identity on positives.) *)
VerificationTest[
    TInit[];
    m = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}}, "Real32"];
    Normal @ TTensorData @ TRealize @ Total[m, 2],
    {3.0, 7.0},
    TestID -> "tiny/sum-axis"
]

(* tiny/gemm: ones(N,N) @ eye(N) = ones(N,N).  eye supplied host-side. *)
VerificationTest[
    TInit[];
    a = TOnes[{8, 8}];
    b = TTensorCreate @ NumericArray[N @ IdentityMatrix[8], "Real32"];
    Normal @ TTensorData @ TRealize @ TMatMul[a, b],
    ConstantArray[1.0, {8, 8}],
    TestID -> "tiny/gemm"
]

(* tiny/gemv: ones(1,N) @ eye(N) = ones(1,N). *)
VerificationTest[
    TInit[];
    a = TOnes[{1, 8}];
    b = TTensorCreate @ NumericArray[N @ IdentityMatrix[8], "Real32"];
    Normal @ TTensorData @ TRealize @ TMatMul[a, b],
    {ConstantArray[1.0, 8]},
    TestID -> "tiny/gemv"
]
