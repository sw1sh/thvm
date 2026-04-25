(* shrink.wlt -- UOP_SHRINK extracts a sub-region [b_i, e_i) on
   each axis.  Inverse of UOP_PAD's shape operation; output dim
   along axis i is e_i - b_i. *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        Range[10] * 1.0, "Real32"];   (* shape {10} *)
    r = TRealize @ TUOpShrink[a, {{2, 7}}];
    Normal @ TTensorData[r],
    {3.0, 4.0, 5.0, 6.0, 7.0},
    TestID -> "shrink/1d-keep-middle"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        Table[10*r + c, {r, 1, 4}, {c, 1, 4}], "Real32"];   (* {4, 4} *)
    r = TRealize @ TUOpShrink[a, {{1, 3}, {1, 3}}];
    Normal @ TTensorData[r],
    {{22.0, 23.0}, {32.0, 33.0}},
    TestID -> "shrink/2d-center-2x2"
]

VerificationTest[
    TInit[];
    (* Asymmetric: keep first 2 rows, last 2 cols. *)
    a = TTensorCreate @ NumericArray[
        Table[10*r + c, {r, 1, 4}, {c, 1, 4}], "Real32"];
    r = TRealize @ TUOpShrink[a, {{0, 2}, {2, 4}}];
    Normal @ TTensorData[r],
    {{13.0, 14.0}, {23.0, 24.0}},
    TestID -> "shrink/2d-asymmetric"
]

VerificationTest[
    TInit[];
    (* No-op: keep the full extent. *)
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    r = TRealize @ TUOpShrink[a, {{0, 4}}];
    Normal @ TTensorData[r],
    {1.0, 2.0, 3.0, 4.0},
    TestID -> "shrink/no-op-full-extent"
]
