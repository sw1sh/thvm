(* switch.wlt -- TNum / TOp2 / TMatNum + a recursive countdown
   (REF + MAT + OP2) end to end. *)

VerificationTest[
    TInit[];
    out = TWnf @ TOp2["-", TNum[7], TNum[3]];
    {TTagName[TTermTag[out]], TTermVal[out]},
    {"NUM", 4},
    TestID -> "switch/op2-sub"
]

VerificationTest[
    TInit[];
    out = TWnf @ TOp2["+", TNum[5], TNum[6]];
    TTermVal[out],
    11,
    TestID -> "switch/op2-add"
]

VerificationTest[
    TInit[];
    {TWnf[TOp2["==", TNum[9], TNum[9]]] // (TTermVal[#] &),
     TWnf[TOp2["==", TNum[9], TNum[8]]] // (TTermVal[#] &)},
    {1, 0},
    TestID -> "switch/op2-eq"
]

VerificationTest[
    TInit[];
    (* MAT[0]{handler=TNum[99], fallback=lam _. TNum[11]} applied to 0 *)
    mat = TMatNum[0, TNum[99], TLam[n, TNum[11]]];
    TTermVal[TWnf[TApp[mat, TNum[0]]]],
    99,
    TestID -> "switch/mat-zero-match"
]

VerificationTest[
    TInit[];
    mat = TMatNum[0, TNum[99], TLam[n, TNum[11]]];
    TTermVal[TWnf[TApp[mat, TNum[7]]]],
    11,
    TestID -> "switch/mat-non-zero-fallback"
]

VerificationTest[
    TInit[];
    TTermVal[TWnf @ TIfZero[TNum[0], TNum[1], TNum[2]]],
    1,
    TestID -> "switch/ifzero-true"
]

VerificationTest[
    TInit[];
    TTermVal[TWnf @ TIfZero[TNum[5], TNum[1], TNum[2]]],
    2,
    TestID -> "switch/ifzero-false"
]

(* Recursive countdown:
       count := λacc. λn. if (n == 0) then acc
                          else count (acc + 1) (n - 1)
   count(0, 5) should reduce to NUM(5). *)

VerificationTest[
    TInit[];
    TDef["count",
        TLam[acc,
            TLam[n,
                TIfZero[n,
                    acc,
                    TApp[TApp[TRef["count"],
                              TOp2["+", acc, TNum[1]]],
                         TOp2["-", n,   TNum[1]]]
                ]
            ]
        ]
    ];
    out = TWnf @ TApp[TApp[TRef["count"], TNum[0]], TNum[5]];
    {TTagName[TTermTag[out]], TTermVal[out]},
    {"NUM", 5},
    TestID -> "switch/recursive-countdown"
]

(* Slightly more interesting: a single recursive accumulator that
   sums n down to 0 -- 1+2+3+4+5 = 15. *)

VerificationTest[
    TInit[];
    TDef["sumto",
        TLam[acc,
            TLam[n,
                TIfZero[n,
                    acc,
                    TApp[TApp[TRef["sumto"],
                              TOp2["+", acc, n]],
                         TOp2["-", n, TNum[1]]]
                ]
            ]
        ]
    ];
    TTermVal[TWnf @ TApp[TApp[TRef["sumto"], TNum[0]], TNum[5]]],
    15,
    TestID -> "switch/recursive-sumto"
]
