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
