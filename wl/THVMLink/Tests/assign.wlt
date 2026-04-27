(* assign.wlt -- UOP_ASSIGN: in-place buffer write primitive.

   ASSIGN(dst, src) copies src.buf -> dst.buf and rewrites the redex
   to dst.  Wnf-fired once both children resolve to TAG_TEN; bails
   (returns the input ASSIGN unchanged) when either child isn't a TEN.

   Used as the in-place mutation primitive for optimizer loops --
   weights stay tid-stable across loop iterations while their buffer
   contents update each step. *)

(* === basic TEN -> TEN copy === *)

VerificationTest[
    TInit[];
    dst = TTensorCreate @ NumericArray[{0., 0., 0., 0.}, "Real32"];
    src = TTensorCreate @ NumericArray[{1., 2., 3., 4.}, "Real32"];
    TWnf[TAssign[dst, src]];
    Normal @ TTensorData[dst],
    {1., 2., 3., 4.},
    TestID -> "assign/basic-copy"
]

VerificationTest[
    TInit[];
    dst = TTensorCreate @ NumericArray[{0.}, "Real32"];
    src = TTensorCreate @ NumericArray[{42.}, "Real32"];
    result = TWnf[TAssign[dst, src]];
    (* Result Term is dst's tid (the ASSIGN rewrites to dst). *)
    TTermTag[result] === $TagTEN && TTermVal[result] === TTermVal[dst],
    True,
    TestID -> "assign/result-is-dst"
]

(* === ASSIGN with computed src (kernel chain forces, then copy) === *)

VerificationTest[
    TInit[];
    a   = TTensorCreate @ NumericArray[{1., 2., 3., 4.}, "Real32"];
    b   = TTensorCreate @ NumericArray[{10., 20., 30., 40.}, "Real32"];
    dst = TTensorCreate @ NumericArray[{0., 0., 0., 0.}, "Real32"];
    TRealize[TAssign[dst, TUOpAdd[a, b]]];
    Normal @ TTensorData[dst],
    {11., 22., 33., 44.},
    TestID -> "assign/computed-src-add"
]

(* === sequenced ASSIGNs (mutating chain) === *)

VerificationTest[
    TInit[];
    w  = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
    v1 = TTensorCreate @ NumericArray[{1., 1., 1.}, "Real32"];
    v2 = TTensorCreate @ NumericArray[{5., 5., 5.}, "Real32"];
    TRealize[TAssign[w, v1]];
    TRealize[TAssign[w, TUOpAdd[w, v2]]];
    Normal @ TTensorData[w],
    {6., 6., 6.},
    TestID -> "assign/two-step-mutation"
]

(* === nested ASSIGN: inner fires before outer (data-dependency
       sequencing) === *)

VerificationTest[
    TInit[];
    m   = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
    v   = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
    src = TTensorCreate @ NumericArray[{1., 2., 3.}, "Real32"];
    (* ASSIGN(v, ASSIGN(m, src) * 2): the inner ASSIGN forces first
       (writes m), then the outer reads m's NEW value through the
       MUL, then copies into v. *)
    mDone = TAssign[m, src];
    inner = TUOpMul[mDone, TUOpConst[2.0, "f32"]];
    TRealize[TAssign[v, inner]];
    {Normal @ TTensorData[m], Normal @ TTensorData[v]},
    {{1., 2., 3.}, {2., 4., 6.}},
    TestID -> "assign/nested-data-dependency-sequencing"
]

(* === TWnf re-fires kernels across loop iterations: each
       alo_realize REF unfold produces a fresh ASSIGN+kernel cell pair
       that wnf re-runs.  Tested via SGD-style loop on a scalar. === *)

VerificationTest[
    TInit[];
    w   = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
    tgt = TTensorCreate @ NumericArray[{1., 2., 3.}, "Real32"];
    lr  = TUOpConst[0.1, "f32"];
    diff = TUOpAdd[w, TUOpNeg[tgt]];
    grad = TUOpMul[TUOpConst[2.0, "f32"], diff];
    step = TAssign[w, TUOpAdd[w, TUOpNeg[TUOpMul[lr, grad]]]];
    matStep = TMaterialize[TNf[step]];
    TDef["sgd_assign_loop_test",
        TLam[k,
            TIfZero[k, TUOpConst[0.0, "f32"],
                TPriForce[
                    TRef["mat_step_test"],
                    TApp[TRef["sgd_assign_loop_test"],
                         TOp2["-", k, TNum[1]]]
                ]
            ]
        ]
    ];
    TDef["mat_step_test", matStep];
    TWnf @ TApp[TRef["sgd_assign_loop_test"], TNum[5]];
    Round[Normal @ TTensorData[w], 0.0001],
    Round[(1 - 0.8^5) * {1., 2., 3.}, 0.0001],
    TestID -> "assign/recursive-loop-converges"
]
