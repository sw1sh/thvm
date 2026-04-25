(* pad.wlt -- UOP_PAD zero-pad with per-axis (begin, end) widths.
   ranges = {{b0, e0}, {b1, e1}, ...}. *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    r = TRealize @ TUOpPad[a, {{1, 2}}];   (* {0, a..., 0, 0} *)
    Normal @ TTensorData[r],
    {0.0, 1.0, 2.0, 3.0, 0.0, 0.0},
    TestID -> "pad/1d-asymmetric"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}}, "Real32"];
    r = TRealize @ TUOpPad[a, {{1, 1}, {1, 1}}];   (* zero ring *)
    Normal @ TTensorData[r],
    {{0., 0., 0., 0.},
     {0., 1., 2., 0.},
     {0., 3., 4., 0.},
     {0., 0., 0., 0.}},
    TestID -> "pad/2d-symmetric-ring"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}}, "Real32"];
    r = TRealize @ TUOpPad[a, {{2, 0}, {0, 1}}];
    (* axis 0: pad 2 on begin, 0 on end -> 4 rows
       axis 1: pad 0 on begin, 1 on end -> 3 cols *)
    Normal @ TTensorData[r],
    {{0., 0., 0.},
     {0., 0., 0.},
     {1., 2., 0.},
     {3., 4., 0.}},
    TestID -> "pad/2d-asymmetric"
]

VerificationTest[
    TInit[];
    (* No-op: zero pads everywhere. *)
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    r = TRealize @ TUOpPad[a, {{0, 0}}];
    Normal @ TTensorData[r],
    {1.0, 2.0, 3.0, 4.0},
    TestID -> "pad/no-op-zero-widths"
]

VerificationTest[
    TInit[];
    (* CONV2D-grad-style: pad spatial axes by kh-1, kw-1 = 2, 2
       (for 3x3 weights).  Inner axis is kept; spatial axes padded. *)
    a = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}}, "Real32"];
    r = TRealize @ TUOpPad[a, {{2, 2}, {2, 2}}];
    TTensorShape[r],
    {6, 6},
    TestID -> "pad/conv2d-grad-style-shape"
]
