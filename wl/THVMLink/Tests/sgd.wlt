(* sgd.wlt -- recursive SGD optimizer expressed as a TDef'd lambda
   term that adds GRAD nodes and recurses via TRef.  Exercises the
   full Phase-1/2/3 stack: REF/ALO unfolding (TDef/TRef), MAT base
   case (TIfZero), OP2 counter decrement, lazy GRAD chain rule
   inside a closure, and materialize caching that lets shared
   subgraphs survive multi-use without breaking grad's leaf check. *)

(* === one-step SGD as a lambda (no recursion) === *)

VerificationTest[
    TInit[];
    target = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    lr     = TUOpConst[0.1, "f32"];
    sgd1   = TLam[Function[w,
        TUOpMaterialize[
            TUOpAdd[w,
                TUOpNeg[TUOpMul[lr,
                    TGrad[
                        TL2Loss[TUOpAdd[w, TUOpNeg[target]]],
                        w
                    ]
                ]]
            ]
        ]
    ]];
    w0 = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
    Round[Normal @ TTensorData @ TWnf @ TApp[sgd1, w0], 0.001],
    {0.2, 0.4, 0.6},
    TestID -> "sgd/one-step-as-lambda"
]

(* === recursive SGD via TDef + TRef + TIfZero ===
   sgd_loop(w, n) = if n == 0 then w
                    else sgd_loop(materialize(step(w)), n-1)
   step(w) = w - lr * grad(L2(w - target), w)
*)

defineSgd[targetTen_, lrConst_] := TDef["sgd_loop",
    TLam[Function[w,
        TLam[Function[n,
            TIfZero[n,
                w,
                TApp[
                    TApp[TRef["sgd_loop"],
                        TUOpMaterialize[
                            TUOpAdd[w,
                                TUOpNeg[TUOpMul[lrConst,
                                    TGrad[
                                        TL2Loss[TUOpAdd[w, TUOpNeg[targetTen]]],
                                        w
                                    ]
                                ]]
                            ]
                        ]
                    ],
                    TOp2["-", n, TNum[1]]
                ]
            ]
        ]]
    ]]
]

VerificationTest[
    TInit[];
    target = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    lr     = TUOpConst[0.1, "f32"];
    defineSgd[target, lr];
    w0 = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
    Normal @ TTensorData @ TWnf @ TApp[TApp[TRef["sgd_loop"], w0], TNum[0]],
    {0.0, 0.0, 0.0},
    TestID -> "sgd/recursive-zero-iters-returns-w0"
]

VerificationTest[
    TInit[];
    target = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    lr     = TUOpConst[0.1, "f32"];
    defineSgd[target, lr];
    w0 = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
    Round[Normal @ TTensorData @ TWnf @ TApp[TApp[TRef["sgd_loop"], w0], TNum[1]], 0.001],
    {0.2, 0.4, 0.6},
    TestID -> "sgd/recursive-one-iter"
]

VerificationTest[
    TInit[];
    target = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    lr     = TUOpConst[0.1, "f32"];
    defineSgd[target, lr];
    w0 = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
    (* w_{i+1} = 0.8 w_i + 0.2 target.  Two iters from zero:
         w1 = 0.2 * target = {0.2, 0.4, 0.6}
         w2 = 0.8 * w1 + 0.2 * target = {0.36, 0.72, 1.08} *)
    Round[Normal @ TTensorData @ TWnf @ TApp[TApp[TRef["sgd_loop"], w0], TNum[2]], 0.001],
    {0.36, 0.72, 1.08},
    TestID -> "sgd/recursive-two-iters-strictly-improves"
]
