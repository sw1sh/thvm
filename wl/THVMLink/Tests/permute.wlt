(* permute.wlt -- UOP_PERMUTE axis reorder.  axes is a list where
   axes[i] is the source axis index that becomes output axis i. *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}},
                                     "Real32"];
    r = TRealize @ TUOpPermute[a, {1, 0}];   (* transpose *)
    Normal @ TTensorData[r],
    {{1.0, 4.0}, {2.0, 5.0}, {3.0, 6.0}},
    TestID -> "permute/2d-transpose"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}},
                                     "Real32"];
    r = TRealize @ TUOpPermute[a, {0, 1}];   (* identity *)
    Normal @ TTensorData[r],
    {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}},
    TestID -> "permute/2d-identity"
]

VerificationTest[
    TInit[];
    (* 3D: shape {2, 3, 4} permuted to {3, 4, 2} via {1, 2, 0}. *)
    a = TTensorCreate @ NumericArray[
        Table[i + 10*j + 100*k, {i, 2}, {j, 3}, {k, 4}], "Real32"];
    r = TRealize @ TUOpPermute[a, {1, 2, 0}];
    TTensorShape[r],
    {3, 4, 2},
    TestID -> "permute/3d-rotate-axes"
]

VerificationTest[
    TInit[];
    (* CONV-grad-style: weights {C_out=2, C_in=3, kh=2, kw=2}
       permuted to {C_in=3, C_out=2, kh, kw} for transposed-conv. *)
    a = TTensorCreate @ NumericArray[
        Table[c0*100 + ci*10 + ky + 0.1*kx,
              {c0, 2}, {ci, 3}, {ky, 2}, {kx, 2}],
        "Real32"];
    r = TRealize @ TUOpPermute[a, {1, 0, 2, 3}];
    TTensorShape[r],
    {3, 2, 2, 2},
    TestID -> "permute/conv2d-grad-shape-swap-c-out-c-in"
]
