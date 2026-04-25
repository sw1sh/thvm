(* flip.wlt -- UOP_FLIP per-axis mirroring.  TUOpFlip takes a list
   of axis indices to flip (the WL helper packs them into a
   bitmask). *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    r = TRealize @ TUOpFlip[a, {0}];
    Normal @ TTensorData[r],
    {4.0, 3.0, 2.0, 1.0},
    TestID -> "flip/1d-reverse"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}},
                                     "Real32"];
    r = TRealize @ TUOpFlip[a, {1}];
    Normal @ TTensorData[r],
    {{3.0, 2.0, 1.0}, {6.0, 5.0, 4.0}},
    TestID -> "flip/2d-axis-1-only"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}},
                                     "Real32"];
    r = TRealize @ TUOpFlip[a, {0}];
    Normal @ TTensorData[r],
    {{4.0, 5.0, 6.0}, {1.0, 2.0, 3.0}},
    TestID -> "flip/2d-axis-0-only"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}},
                                     "Real32"];
    r = TRealize @ TUOpFlip[a, {0, 1}];
    Normal @ TTensorData[r],
    {{6.0, 5.0, 4.0}, {3.0, 2.0, 1.0}},
    TestID -> "flip/2d-both-axes"
]

VerificationTest[
    TInit[];
    (* No-op flip: empty axes list -> bitmask 0 -> identity. *)
    a = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}}, "Real32"];
    r = TRealize @ TUOpFlip[a, {}];
    Normal @ TTensorData[r],
    {{1.0, 2.0}, {3.0, 4.0}},
    TestID -> "flip/no-op-empty-axes"
]
