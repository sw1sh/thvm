(* reshape.wlt -- runtime support for UOP_RESHAPE.  RESHAPE is
   row-major view-preserving (contiguous tensors), so the underlying
   bytes are unchanged; only the View shape header differs.  Output
   numel must equal input numel; the materialize step verifies this
   implicitly by reading dim NUM cells until the running product
   matches the input numel. *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, "Real32"];
    r = TRealize @ TUOpReshape[x, {2, 3}];
    {TTensorShape[r], Normal @ TTensorData[r]},
    {{2, 3}, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}},
    TestID -> "reshape/1d-to-2d-row-major"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}}, "Real32"];
    r = TRealize @ TUOpReshape[x, {4}];
    {TTensorShape[r], Normal @ TTensorData[r]},
    {{4}, {1.0, 2.0, 3.0, 4.0}},
    TestID -> "reshape/2d-to-1d-flatten"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[Range[12] * 1.0, "Real32"];
    r = TRealize @ TUOpReshape[x, {3, 2, 2}];
    TTensorShape[r],
    {3, 2, 2},
    TestID -> "reshape/1d-to-3d-shape"
]

(* Regression for the leading-1s shape-recovery bug.  Previously the
   materializer recovered ndim by walking dim cells until the
   running product hit input numel; for a numel-4 source reshaped
   to {1, 4}, prod=1 already after the first cell so the loop
   broke early and produced shape {1} instead of {1, 4}.  Now ndim
   is stored explicitly. *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    r = TRealize @ TUOpReshape[x, {1, 4}];
    TTensorShape[r],
    {1, 4},
    TestID -> "reshape/leading-one-rank-preserved"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, "Real32"];
    r = TRealize @ TUOpReshape[x, {1, 1, 6}];
    TTensorShape[r],
    {1, 1, 6},
    TestID -> "reshape/multiple-leading-ones-rank-preserved"
]

(* === sub-item f3b: RESHAPE on a contiguous source is view-only ===
   The materializer aliases the source's TenDesc with a fresh
   TenDesc carrying the new shape and DOES NOT emit a kernel.
   Verify by asserting the kernel count doesn't change across a
   pure-reshape materialize. *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, "Real32"];
    Module[{kBefore, kAfter, r},
        kBefore = TKernelCount[];
        r = TMaterialize @ TUOpReshape[x, {2, 3}];
        kAfter = TKernelCount[];
        {kAfter - kBefore, TTensorShape[r], Normal @ TTensorData @ TRealize[r]}
    ],
    {0, {2, 3}, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}},
    TestID -> "reshape/view-only-no-kernel-emitted"
]
