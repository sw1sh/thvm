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
