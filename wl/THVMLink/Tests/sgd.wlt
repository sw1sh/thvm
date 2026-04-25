(* sgd.wlt -- recursive SGD optimizer expressed as a TDef'd lambda
   term that adds GRAD nodes and recurses via TRef.  Exercises the
   full Phase-1/2/3 stack: REF/ALO unfolding (TDef/TRef), MAT base
   case (TIfZero), OP2 counter decrement, and a lazy GRAD chain
   rule inside a closure.

   The body intentionally has NO TUOpMaterialize wrapper -- the
   recursive call passes the symbolic step(w) UOp graph as the new
   w, building a deeply-nested expression that gets materialised
   once at the end via TRealize.  This keeps the same UOp Term
   value at every w reference inside the body (substitution is
   pure pointer-passing through APP-LAM beta), so grad's
   y == target leaf check works without any caching trick. *)

(* === one-step SGD as a lambda (no recursion) === *)

VerificationTest[
    TInit[];
    target = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    lr     = TUOpConst[0.1, "f32"];
    sgd1   = TLam[w,
        TUOpAdd[w,
            TUOpNeg[TUOpMul[lr,
                TGrad[
                    TL2Loss[TUOpAdd[w, TUOpNeg[target]]],
                    w
                ]
            ]]
        ]
    ];
    w0 = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
    Round[Normal @ TTensorData @ TRealize @ TApp[sgd1, w0], 0.001],
    {0.2, 0.4, 0.6},
    TestID -> "sgd/one-step-as-lambda"
]

(* === recursive SGD via TDef + TRef + TIfZero ===
   sgd_loop(w, n) = if n == 0 then w
                    else sgd_loop(step(w), n-1)
   step(w) = w - lr * grad(L2(w - target), w)
   The body returns a symbolic UOp graph; TRealize materialises. *)

defineSgd[targetTen_, lrConst_] := TDef["sgd_loop",
    TLam[w,
        TLam[n,
            TIfZero[n,
                w,
                TApp[
                    TApp[TRef["sgd_loop"],
                        TUOpAdd[w,
                            TUOpNeg[TUOpMul[lrConst,
                                TGrad[
                                    TL2Loss[TUOpAdd[w, TUOpNeg[targetTen]]],
                                    w
                                ]
                            ]]
                        ]
                    ],
                    TOp2["-", n, TNum[1]]
                ]
            ]
        ]
    ]
]

VerificationTest[
    TInit[];
    target = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    lr     = TUOpConst[0.1, "f32"];
    defineSgd[target, lr];
    w0 = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
    Round[Normal @ TTensorData @ TRealize @
        TApp[TApp[TRef["sgd_loop"], w0], TNum[0]], 0.001],
    {0.0, 0.0, 0.0},
    TestID -> "sgd/recursive-zero-iters-returns-w0"
]

VerificationTest[
    TInit[];
    target = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    lr     = TUOpConst[0.1, "f32"];
    defineSgd[target, lr];
    w0 = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
    Round[Normal @ TTensorData @ TRealize @
        TApp[TApp[TRef["sgd_loop"], w0], TNum[1]], 0.001],
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
    Round[Normal @ TTensorData @ TRealize @
        TApp[TApp[TRef["sgd_loop"], w0], TNum[2]], 0.001],
    {0.36, 0.72, 1.08},
    TestID -> "sgd/recursive-two-iters-strictly-improves"
]

VerificationTest[
    TInit[];
    target = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    lr     = TUOpConst[0.1, "f32"];
    defineSgd[target, lr];
    w0 = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
    (* w_3 = 0.8 w_2 + 0.2 target = {0.488, 0.976, 1.464} *)
    Round[Normal @ TTensorData @ TRealize @
        TApp[TApp[TRef["sgd_loop"], w0], TNum[3]], 0.001],
    {0.488, 0.976, 1.464},
    TestID -> "sgd/recursive-three-iters"
]
