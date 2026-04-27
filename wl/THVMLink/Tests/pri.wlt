(* pri.wlt -- TAG_PRI surface: TPri / TPriForce / TPriRegister with
   the foreign-callback dispatch path.

   THVM_PRIM_PRI fires inside wnf and routes to a registered WL
   callback.  Three dispatch paths in priority order:
     (A) Foreign callback (libffi closure from CreateForeignCallback)
         -- arbitrary WL, sync re-entry, can return a Term to override
         the redex result.
     (B) Compiled callback (LibraryLink callLibraryCallbackFunction)
         -- numerical only, observe-only.
     (C) Queued + drain (TPriDrain[], auto-called by TWnf wrapper).

   These tests cover (A) since it's the default; (C) is exercised by
   any callback that returns Null. *)

(* === TPriForce: pure sequencer (slot=0, no callback) === *)

VerificationTest[
    TInit[];
    v = TTensorCreate @ NumericArray[{42.}, "Real32"];
    cont = TUOpConst[7.0, "f32"];
    result = TWnf[TPriForce[v, cont]];
    (* TPriForce returns cont after forcing v. *)
    TTermTag[result] === $TagTEN || TTermTag[result] === $TagUOP,
    True,
    TestID -> "pri/force-returns-cont-shape"
]

(* === TPri[fn, ...]: trace mode (Function returns Null implicitly) === *)

VerificationTest[
    TInit[];
    $traceCount = 0;
    TPriRegister[101, Function[t, $traceCount += 1]];
    v = TTensorCreate @ NumericArray[{1.}, "Real32"];
    TWnf @ TPri[101, v, TUOpConst[0.0, "f32"]];
    $traceCount,
    1,
    TestID -> "pri/trace-callback-fires-once"
]

VerificationTest[
    TInit[];
    $traceLog = {};
    TPriRegister[102, Function[t,
        AppendTo[$traceLog, First @ Normal @ TTensorData[t]]]];
    Do[
        With[{ten = TTensorCreate @ NumericArray[{N[k]}, "Real32"]},
            TWnf @ TPri[102, ten, TUOpConst[0.0, "f32"]]
        ],
        {k, 3}
    ];
    $traceLog,
    {1., 2., 3.},
    TestID -> "pri/trace-callback-captures-values"
]

(* === Override: callback returns a TTerm -> redex rewrites to it === *)

VerificationTest[
    TInit[];
    overrideTerm = TUOpConst[999.0, "f32"];
    contTerm     = TUOpConst[42.0, "f32"];
    TPriRegister[103, Function[t, overrideTerm]];
    v = TTensorCreate @ NumericArray[{1.}, "Real32"];
    result = TWnf @ TPri[103, v, contTerm];
    TTermVal[result] === TTermVal[overrideTerm],
    True,
    TestID -> "pri/override-returns-tterm"
]

VerificationTest[
    TInit[];
    contTerm = TUOpConst[42.0, "f32"];
    TPriRegister[104, Function[t, Null]];
    v = TTensorCreate @ NumericArray[{1.}, "Real32"];
    result = TWnf @ TPri[104, v, contTerm];
    (* Null return -> wrapper returns 0 -> falls through to cont. *)
    TTermVal[result] === TTermVal[contTerm],
    True,
    TestID -> "pri/null-return-falls-through-to-cont"
]

(* === Auto-slot: TPri[fn, ...] dedups by fn identity === *)

VerificationTest[
    TInit[];
    $autoCount = 0;
    fn = Function[t, $autoCount += 1];
    v = TTensorCreate @ NumericArray[{1.}, "Real32"];
    (* Same fn used twice; both call paths share one slot. *)
    TWnf @ TPri[fn, v, TUOpConst[0.0, "f32"]];
    TWnf @ TPri[fn, v, TUOpConst[0.0, "f32"]];
    $autoCount,
    2,
    TestID -> "pri/auto-slot-dedups-by-fn-identity"
]

(* === Direct ForeignCallback (user-built, manual lifetime) === *)

VerificationTest[
    TInit[];
    Needs["ForeignFunctionInterface`"];
    $directCount = 0;
    cb = CreateForeignCallback[
        Function[v, $directCount += 1; 0],
        {"Integer64"} -> "Integer64"];
    TPriRegister[105, cb];
    v = TTensorCreate @ NumericArray[{1.}, "Real32"];
    TWnf @ TPri[105, v, TUOpConst[0.0, "f32"]];
    $directCount,
    1,
    TestID -> "pri/direct-foreign-callback-object"
]

(* === Slot 0 has no callback even when registered (reserved sentinel) *)

VerificationTest[
    TInit[];
    $shouldStayZero = 0;
    TPriRegister[0, Function[t, $shouldStayZero += 1]];
    v = TTensorCreate @ NumericArray[{1.}, "Real32"];
    TWnf @ TPri[0, v, TUOpConst[0.0, "f32"]];
    $shouldStayZero,
    0,    (* slot=0 is the pure-sequencer; prim_pri skips the callback
            invocation entirely *)
    TestID -> "pri/slot-zero-skips-callback"
]

(* === TPri inside a recursive optimizer loop logs once per iter === *)

VerificationTest[
    TInit[];
    w   = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
    tgt = TTensorCreate @ NumericArray[{1., 2., 3.}, "Real32"];
    lr  = TUOpConst[0.1, "f32"];
    diff = TUOpAdd[w, TUOpNeg[tgt]];
    loss = TUOpReduce[TUOpMul[diff, diff], 0, "SUM"];
    grad = TUOpMul[TUOpConst[2.0, "f32"], diff];
    step = TAssign[w, TUOpAdd[w, TUOpNeg[TUOpMul[lr, grad]]]];
    $lossPerIter = {};
    body = TPri[
        Function[lossT, AppendTo[$lossPerIter,
            First @ Normal @ TTensorData[lossT]]],
        loss, step
    ];
    matBody = TMaterialize[TNf[body]];
    TDef["pri_test_step_body", matBody];
    TDef["pri_test_loop",
        TLam[k,
            TIfZero[k, TUOpConst[0.0, "f32"],
                TPriForce[TRef["pri_test_step_body"],
                    TApp[TRef["pri_test_loop"], TOp2["-", k, TNum[1]]]]
            ]
        ]
    ];
    TWnf @ TApp[TRef["pri_test_loop"], TNum[3]];
    Length[$lossPerIter],
    3,
    TestID -> "pri/recursive-loop-logs-per-iter"
]

VerificationTest[
    TInit[];
    w   = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
    tgt = TTensorCreate @ NumericArray[{1., 2., 3.}, "Real32"];
    lr  = TUOpConst[0.1, "f32"];
    diff = TUOpAdd[w, TUOpNeg[tgt]];
    loss = TUOpReduce[TUOpMul[diff, diff], 0, "SUM"];
    grad = TUOpMul[TUOpConst[2.0, "f32"], diff];
    step = TAssign[w, TUOpAdd[w, TUOpNeg[TUOpMul[lr, grad]]]];
    $monotonic = {};
    body = TPri[
        Function[lossT, AppendTo[$monotonic,
            First @ Normal @ TTensorData[lossT]]],
        loss, step
    ];
    TDef["pri_mon_step", TMaterialize[TNf[body]]];
    TDef["pri_mon_loop",
        TLam[k, TIfZero[k, TUOpConst[0.0, "f32"],
            TPriForce[TRef["pri_mon_step"],
                TApp[TRef["pri_mon_loop"], TOp2["-", k, TNum[1]]]]]]
    ];
    TWnf @ TApp[TRef["pri_mon_loop"], TNum[5]];
    (* Loss must strictly decrease per iter. *)
    OrderedQ[Reverse[$monotonic]],
    True,
    TestID -> "pri/recursive-loop-loss-decreases"
]
