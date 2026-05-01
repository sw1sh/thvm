(* aot.wlt -- VerificationTest specs for the AOT path.

   Run via:  bash wl/THVMLink/Tests/run.sh

   Coverage:
     1. TAOTPrograms[] returns the catalog of hand-coded programs.
     2. TAOT[name, defs] registers and returns a TAOTProgram object
        with the right shape (name, defs, slots, registered, calls0).
     3. TAOTProgram fields are accessible: prog["name"], prog["calls"]
        reads through to the live counter.
     4. fib_nat AOT and the interpreter agree on output and ITRS for
        a sweep of N.  The AOT does WNF -> AOT dispatches counted via
        prog["calls"]; the interpreter does 0.
     5. TAOTReset (= TInit here) clears registered programs, so
        subsequent reductions go through the lazy ALO interpreter
        again -- TAOTCalls[] zeroes too.
*)

(* Helpers shared across tests.  Mirror the bench script's defs. *)

setupFibNatDefs[] := (
    TInit[];
    TDef["add", TMatChain[
        <|0 -> TLam[b, b],
          1 -> TLam[a, TLam[b, TApp[TApp[TRef["add"], a], TCtr[1, b]]]]
         |>,
        TLam[ignored, TEra[]]
    ]];
    TDef["fib", TMatChain[
        <|0 -> TCtr[0],
          1 -> TMatChain[
                 <|0 -> TCtr[1, TCtr[0]],
                   1 -> TLam[p,
                             TDup[7, p, {p0, p1} |->
                                 TApp[TApp[TRef["add"],
                                          TApp[TRef["fib"], TCtr[1, p0]]],
                                      TApp[TRef["fib"], p1]]]]
                  |>,
                 TLam[ignored, TEra[]]
               ]
         |>,
        TLam[ignored, TEra[]]
    ]];
    TDef["u32", TMatChain[
        <|0 -> TLam[n, n],
          1 -> TLam[p, TLam[n, TApp[TApp[TRef["u32"], p], TOp2["+", TNum[1], n]]]]
         |>,
        TLam[ignored, TEra[]]
    ]];
);

natTerm[n_Integer] := If[ n <= 0, TCtr[0], TCtr[1, natTerm[n - 1]]];

fibTerm[n_Integer] :=
    TApp[TApp[TRef["u32"], TApp[TRef["fib"], natTerm[n]]], TNum[0]];

(* === 1. catalog =========================================== *)

VerificationTest[
    MemberQ[TAOTPrograms[], "fib_nat"],
    True,
    TestID -> "TAOTPrograms lists fib_nat"
]

(* === 2. registration returns a typed wrapper ============== *)

VerificationTest[
    setupFibNatDefs[];
    Head[TAOT["fib_nat", {"add", "fib", "u32"}]],
    TAOTProgram,
    TestID -> "TAOT returns a TAOTProgram object"
]

VerificationTest[
    setupFibNatDefs[];
    Module[{prog = TAOT["fib_nat", {"add", "fib", "u32"}]},
        {prog["name"], prog["defs"], prog["registered"], Length[prog["slots"]]}
    ],
    {"fib_nat", {"add", "fib", "u32"}, 3, 3},
    TestID -> "TAOTProgram fields populated"
]

(* === 3. live calls counter ================================ *)

VerificationTest[
    setupFibNatDefs[];
    Module[{prog = TAOT["fib_nat", {"add", "fib", "u32"}], before, after},
        before = prog["calls"];
        TWnf @ fibTerm[5];
        after = prog["calls"];
        after > before
    ],
    True,
    TestID -> "prog[calls] increments after a reduction"
]

(* === 4. AOT and interpreter agree ========================= *)

(* fib(N) value via two paths -- without AOT (lazy ALO) and with AOT
   (hand-coded specialisation).  Both must produce identical NUM
   values AND identical ITRS counts for every N. *)

verifyAgreement[n_Integer] := Module[{
    interpItrs, interpVal, aotItrs, aotVal, prog
},
    setupFibNatDefs[];
    Block[{itrs0 = TItrs[]},
        TWnf @ fibTerm[n];
        interpItrs = TItrs[] - itrs0
    ];
    interpVal = TWnf[fibTerm[n]]["val"];

    setupFibNatDefs[];
    prog = TAOT["fib_nat", {"add", "fib", "u32"}];
    Block[{itrs0 = TItrs[]},
        TWnf @ fibTerm[n];
        aotItrs = TItrs[] - itrs0
    ];
    aotVal = TWnf[fibTerm[n]]["val"];

    {interpVal === aotVal === Fibonacci[n], interpItrs === aotItrs}
];

VerificationTest[
    verifyAgreement[0],
    {True, True},
    TestID -> "fib(0): AOT == interpreter (value + ITRS)"
]

VerificationTest[
    verifyAgreement[5],
    {True, True},
    TestID -> "fib(5): AOT == interpreter (value + ITRS)"
]

VerificationTest[
    verifyAgreement[10],
    {True, True},
    TestID -> "fib(10): AOT == interpreter (value + ITRS)"
]

VerificationTest[
    verifyAgreement[15],
    {True, True},
    TestID -> "fib(15): AOT == interpreter (value + ITRS)"
]

(* === 5. reset clears registrations ========================= *)

VerificationTest[
    setupFibNatDefs[];
    TAOT["fib_nat", {"add", "fib", "u32"}];
    TWnf @ fibTerm[5];
    Module[{callsBefore = TAOTCalls[], callsAfter},
        TAOTReset[];
        callsAfter = TAOTCalls[];
        {callsBefore > 0, callsAfter}
    ],
    {True, 0},
    TestID -> "TAOTReset clears the counter"
]

(* === 6. catalog discoverability =========================== *)

VerificationTest[
    Module[{},
        TAOT["definitely_not_a_real_program",
             {"add", "fib", "u32"}] // Quiet
    ],
    $Failed,
    TestID -> "TAOT on unknown program returns $Failed"
]

VerificationTest[
    setupFibNatDefs[];
    TAOT["fib_nat", {"add", "fib"}] // Quiet,
    $Failed,
    TestID -> "TAOT with wrong arity returns $Failed"
]

(* === 7. gab_tak AOT correctness =========================== *)
(* Five-def hand-coded AOT.  ITRS counts diverge from HVM4 by a
   constant factor (we skip the auto-dup-induced DUP-NODs); only
   value correctness is checked here.  Performance is documented
   in src/aot/programs/gab_tak.c -- AOT is faster than the
   interpreter for small inputs and slower for deep recursion. *)

setupGabTakDefs[] := (
    TInit[];
    TDef["pred", TMatChain[
        <|0 -> TCtr[0],
          1 -> TLam[p, p]
         |>, TLam[ignored, TEra[]]]];
    TDef["lte", TMatChain[
        <|0 -> TLam[ignoredb, TCtr[2]],
          1 -> TLam[p, TMatChain[
                 <|0 -> TCtr[3],
                   1 -> TLam[q, TApp[TApp[TRef["lte"], p], q]]
                  |>, TLam[ignored, TEra[]]]]
         |>, TLam[ignored, TEra[]]]];
    TDef["tak_go", TMatChain[
        <|2 -> TLam[x, TLam[y, TLam[z, y]]],
          3 -> TLam[x, TLam[y, TLam[z,
                 TApp[TApp[TApp[TRef["tak"],
                          TApp[TApp[TApp[TRef["tak"], TApp[TRef["pred"], x]], y], z]],
                          TApp[TApp[TApp[TRef["tak"], TApp[TRef["pred"], y]], z], x]],
                          TApp[TApp[TApp[TRef["tak"], TApp[TRef["pred"], z]], x], y]
                 ]]]]
         |>, TLam[ignored, TEra[]]]];
    TDef["tak", TLam[x, TLam[y, TLam[z,
        TApp[TApp[TApp[TApp[TRef["tak_go"],
            TApp[TApp[TRef["lte"], x], y]], x], y], z]
    ]]]];
    TDef["u32_to", TMatChain[
        <|0 -> TNum[0],
          1 -> TLam[p, TOp2["+", TApp[TRef["u32_to"], p], TNum[1]]]
         |>, TLam[ignored, TEra[]]]];
);

natTermGT[n_Integer] := If[ n <= 0, TCtr[0], TCtr[1, natTermGT[n - 1]]];
takMain[x_, y_, z_] := TApp[TRef["u32_to"],
    TApp[TApp[TApp[TRef["tak"], natTermGT[x]], natTermGT[y]], natTermGT[z]]];

VerificationTest[
    MemberQ[TAOTPrograms[], "gab_tak"],
    True,
    TestID -> "TAOTPrograms lists gab_tak"
]

VerificationTest[
    setupGabTakDefs[];
    Head[TAOT["gab_tak", {"pred", "lte", "tak_go", "tak", "u32_to"}]],
    TAOTProgram,
    TestID -> "TAOT[gab_tak] returns a TAOTProgram"
]

(* tak(2,1,0) = 2 -- AOT and interp agree on value. *)
VerificationTest[
    setupGabTakDefs[];
    Module[{interpVal, aotVal},
        interpVal = TWnf[takMain[2, 1, 0]]["val"];
        setupGabTakDefs[];
        TAOT["gab_tak", {"pred", "lte", "tak_go", "tak", "u32_to"}];
        aotVal = TWnf[takMain[2, 1, 0]]["val"];
        {interpVal, aotVal}
    ],
    {2, 2},
    TestID -> "tak(2,1,0): AOT == interpreter == 2"
]

VerificationTest[
    setupGabTakDefs[];
    Module[{interpVal, aotVal},
        interpVal = TWnf[takMain[5, 4, 3]]["val"];
        setupGabTakDefs[];
        TAOT["gab_tak", {"pred", "lte", "tak_go", "tak", "u32_to"}];
        aotVal = TWnf[takMain[5, 4, 3]]["val"];
        {interpVal, aotVal}
    ],
    {5, 5},
    TestID -> "tak(5,4,3): AOT == interpreter == 5"
]

VerificationTest[
    setupGabTakDefs[];
    Module[{interpVal, aotVal},
        interpVal = TWnf[takMain[8, 6, 4]]["val"];
        setupGabTakDefs[];
        TAOT["gab_tak", {"pred", "lte", "tak_go", "tak", "u32_to"}];
        aotVal = TWnf[takMain[8, 6, 4]]["val"];
        {interpVal, aotVal}
    ],
    {8, 8},
    TestID -> "tak(8,6,4): AOT == interpreter == 8"
]

(* === 8. u32_fib AOT correctness =========================== *)
(* Single-def, single-arg, dispatch on a NUM head.  Fully ITRS-
   matching since the recursion works on FRESH NUMs.  Bigger
   speedup than gab_tak: ~11x at n=25-30. *)

setupU32FibDef[] := (
    TInit[];
    TDef["u32_fib", TMatChain[
        <|0 -> TNum[0],
          1 -> TNum[1]
         |>,
        TLam[k, TOp2["+",
            TApp[TRef["u32_fib"], TOp2["-", k, TNum[1]]],
            TApp[TRef["u32_fib"], TOp2["-", k, TNum[2]]]
        ]]
    ]];
);

u32FibTerm[n_Integer] := TApp[TRef["u32_fib"], TNum[n]];

VerificationTest[
    MemberQ[TAOTPrograms[], "u32_fib"],
    True,
    TestID -> "TAOTPrograms lists u32_fib"
]

VerificationTest[
    setupU32FibDef[];
    Head[TAOT["u32_fib", {"u32_fib"}]],
    TAOTProgram,
    TestID -> "TAOT[u32_fib] returns a TAOTProgram"
]

(* Single-def AOT, ITRS-exact match across N. *)

verifyU32FibAgreement[n_Integer] := Module[{
    interpItrs, interpVal, aotItrs, aotVal
},
    setupU32FibDef[];
    Block[{itrs0 = TItrs[]},
        TWnf @ u32FibTerm[n];
        interpItrs = TItrs[] - itrs0
    ];
    interpVal = TWnf[u32FibTerm[n]]["val"];

    setupU32FibDef[];
    TAOT["u32_fib", {"u32_fib"}];
    Block[{itrs0 = TItrs[]},
        TWnf @ u32FibTerm[n];
        aotItrs = TItrs[] - itrs0
    ];
    aotVal = TWnf[u32FibTerm[n]]["val"];

    {interpVal === aotVal === Fibonacci[n], interpItrs === aotItrs}
];

VerificationTest[
    verifyU32FibAgreement[5],
    {True, True},
    TestID -> "u32_fib(5): AOT == interpreter (value + ITRS)"
]

VerificationTest[
    verifyU32FibAgreement[10],
    {True, True},
    TestID -> "u32_fib(10): AOT == interpreter (value + ITRS)"
]

VerificationTest[
    verifyU32FibAgreement[15],
    {True, True},
    TestID -> "u32_fib(15): AOT == interpreter (value + ITRS)"
]

(* === 9. Phase-0 auto-emitter (TAOTEmit) =================== *)
(* The emitter walks a TDef'd book template and produces C source
   that mirrors the interpreter's behaviour for that def.  Phase
   0 supports only constant-returning defs and the identity
   lambda; anything else emits a fallback comment.  These tests
   verify the emitter recognises both patterns and produces
   well-formed text. *)

VerificationTest[
    TInit[];
    TDef["emit_zero_test", TNum[42]];
    With[{src = TAOTEmit["emit_zero_test"]},
        StringContainsQ[src, "term_new(0, TAG_NUM"] &&
        StringContainsQ[src, "42"] &&
        StringContainsQ[src, "aot_program_emit_"] &&
        StringContainsQ[src, "_register"]
    ],
    True,
    TestID -> "TAOTEmit on a TNum constant emits term_new(TAG_NUM, ...) + register fn"
]

VerificationTest[
    TInit[];
    TDef["emit_id_test", TLam[x, x]];
    With[{src = TAOTEmit["emit_id_test"]},
        StringContainsQ[src, "aot_pop_app_arg"] &&
        StringContainsQ[src, "APP-LAM"] &&
        StringContainsQ[src, "arg_0"]
    ],
    True,
    TestID -> "TAOTEmit on TLam[x,x] emits aot_pop_app_arg + APP-LAM"
]

VerificationTest[
    TInit[];
    TDef["emit_apply_test", TLam[x, TApp[x, x]]];
    With[{src = TAOTEmit["emit_apply_test"]},
        (* Phase 1 handles TApp via aot_new_app -- no longer
           "unsupported" as it was in Phase 0. *)
        StringContainsQ[src, "aot_new_app"] &&
        StringContainsQ[src, "aot_pop_app_arg"]
    ],
    True,
    TestID -> "TAOTEmit on TLam[x, x x] emits aot_new_app"
]

(* Phase 1: numeric MAT chain dispatch + recursive REF call + OP2.
   The whole u32_fib body should auto-emit cleanly. *)

VerificationTest[
    TInit[];
    TDef["emit_u32_fib_test", TMatChain[
        <|0 -> TNum[0],
          1 -> TNum[1]
         |>,
        TLam[k, TOp2["+",
            TApp[TRef["emit_u32_fib_test"], TOp2["-", k, TNum[1]]],
            TApp[TRef["emit_u32_fib_test"], TOp2["-", k, TNum[2]]]
        ]]
    ]];
    With[{src = TAOTEmit["emit_u32_fib_test"]},
        (* MAT chain dispatched via tag/value if-tree *)
        StringContainsQ[src, "TAG_NUM"] &&
        StringContainsQ[src, "term_val"] &&
        (* OP2 cells built lazily *)
        StringContainsQ[src, "TAG_OP2"] &&
        StringContainsQ[src, "OP_SUB"] &&
        StringContainsQ[src, "OP_ADD"] &&
        (* Recursive call via REF *)
        StringContainsQ[src, "TAG_REF"] &&
        StringContainsQ[src, "aot_new_app"]
    ],
    True,
    TestID -> "TAOTEmit on u32_fib body emits MAT/OP2/REF correctly"
]

VerificationTest[
    TAOTEmit["definitely_does_not_exist"],
    "",
    TestID -> "TAOTEmit on undefined def returns empty string"
]

(* === 10. Phase 2 emitter (TCtr + DUP + CTR-MAT) =========== *)

VerificationTest[
    TInit[];
    TDef["emit_mksuc_test", TLam[x, TCtr[1, x]]];
    With[{src = TAOTEmit["emit_mksuc_test"]},
        StringContainsQ[src, "term_new_ctr"] &&
        StringContainsQ[src, "aot_pop_app_arg"]
    ],
    True,
    TestID -> "TAOTEmit on TLam[x, SUC{x}] emits term_new_ctr"
]

VerificationTest[
    TInit[];
    TDef["emit_unsuc_test", TMatChain[
        <|0 -> TCtr[0],
          1 -> TLam[p, p]
         |>,
        TLam[ignored, TEra[]]
    ]];
    With[{src = TAOTEmit["emit_unsuc_test"]},
        StringContainsQ[src, "TAG_CTR"] &&
        StringContainsQ[src, "term_ctr_at"]
    ],
    True,
    TestID -> "TAOTEmit on CTR-MAT destructure emits term_ctr_at"
]

VerificationTest[
    TInit[];
    TDef["emit_dup_test", TLam[x,
        TDup[7, x, {x0, x1} |->
            TApp[TApp[TRef["emit_dup_test"], x0], x1]]]];
    With[{src = TAOTEmit["emit_dup_test"]},
        StringContainsQ[src, "aot_make_dup"] &&
        StringContainsQ[src, "dp0_"] &&
        StringContainsQ[src, "dp1_"]
    ],
    True,
    TestID -> "TAOTEmit on TDup body emits aot_make_dup"
]

VerificationTest[
    TInit[];
    TDef["emit_fib_test", TMatChain[
        <|0 -> TCtr[0],
          1 -> TMatChain[
                 <|0 -> TCtr[1, TCtr[0]],
                   1 -> TLam[p, TDup[7, p, {p0, p1} |->
                         TApp[TApp[TRef["emit_fib_test"],
                                  TApp[TRef["emit_fib_test"], TCtr[1, p0]]],
                              TApp[TRef["emit_fib_test"], p1]]]]
                  |>, TLam[ignored, TEra[]]]
         |>, TLam[ignored, TEra[]]]];
    With[{src = TAOTEmit["emit_fib_test"]},
        StringContainsQ[src, "term_new_ctr"] &&
        StringContainsQ[src, "term_ctr_at"] &&
        StringContainsQ[src, "aot_make_dup"]
    ],
    True,
    TestID -> "TAOTEmit on full fib_nat body emits CTR + dup + nested MAT"
]
