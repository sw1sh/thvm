(* term_eq.wlt -- VerificationTest specs for TTermEq / TTermSame.

   TTermEq:        cnf-reduce both sides, then structural compare.
   TTermSame:  no reduction; raw structural compare.

   Used by pattern compilers to verify repeat-binder patterns
   like `f[x_, x_]`.  See src/term/eq.c. *)

(* === atoms: NUM-NUM, ERA-ERA, etc. ================================= *)

VerificationTest[
    TTermSame[TNum[5], TNum[5]],
    True,
    TestID -> "TermEq/struct-num-eq"
]

VerificationTest[
    TTermSame[TNum[5], TNum[6]],
    False,
    TestID -> "TermEq/struct-num-neq"
]

VerificationTest[
    TTermSame[TEra[], TEra[]],
    True,
    TestID -> "TermEq/struct-era-era"
]

VerificationTest[
    TTermSame[TNum[0], TEra[]],
    False,
    TestID -> "TermEq/struct-num-vs-era"
]

(* === reducing equality: cnf-then-compare ========================== *)

VerificationTest[
    TTermEq[TOp2["+", TNum[2], TNum[3]], TNum[5]],
    True,
    TestID -> "TermEq/cnf-2+3=5"
]

VerificationTest[
    TTermEq[TOp2["+", TNum[2], TNum[3]], TNum[6]],
    False,
    TestID -> "TermEq/cnf-2+3-neq-6"
]

VerificationTest[
    TTermEq[TOp2["*", TNum[3], TNum[4]],
            TOp2["+", TNum[10], TNum[2]]],
    True,
    TestID -> "TermEq/cnf-3*4-eq-10+2"
]

(* === structural compare DOES NOT reduce ============================ *)

VerificationTest[
    TTermSame[TOp2["+", TNum[2], TNum[3]], TNum[5]],
    False,
    TestID -> "TermEq/struct-op2-vs-num-no-reduction"
]

(* === CTRs: same ctor + same arity + same children ================== *)

VerificationTest[
    TTermSame[TLazyEncode[{1, 2, 3}], TLazyEncode[{1, 2, 3}]],
    True,
    TestID -> "TermEq/struct-tuple-ctr-equal"
]

VerificationTest[
    TTermSame[TLazyEncode[{1, 2, 3}], TLazyEncode[{1, 2, 4}]],
    False,
    TestID -> "TermEq/struct-tuple-ctr-different-leaf"
]

VerificationTest[
    TTermSame[TLazyEncode[{1, 2, 3}], TLazyEncode[{1, 2}]],
    False,
    TestID -> "TermEq/struct-tuple-ctr-different-arity"
]

VerificationTest[
    TTermSame[TLazyEncode[a], TLazyEncode[a]],
    True,
    TestID -> "TermEq/struct-symbol-ctr-equal"
]

VerificationTest[
    TTermSame[TLazyEncode[a], TLazyEncode[b]],
    False,
    TestID -> "TermEq/struct-different-symbols"
]

VerificationTest[
    TTermSame[TLazyEncode[{a, {b, c}, 3}],
                  TLazyEncode[{a, {b, c}, 3}]],
    True,
    TestID -> "TermEq/struct-nested-equal"
]

(* === DSU/DDU shapes compare structurally ========================== *)

VerificationTest[
    TTermSame[TDsu[TNum[5], TNum[1], TNum[2]],
                  TDsu[TNum[5], TNum[1], TNum[2]]],
    True,
    TestID -> "TermEq/struct-dsu-equal"
]

VerificationTest[
    TTermSame[TDsu[TNum[5], TNum[1], TNum[2]],
                  TDsu[TNum[7], TNum[1], TNum[2]]],
    False,
    TestID -> "TermEq/struct-dsu-different-label"
]

(* === DSU reducing: TWnf'd DSU equals plain SUP ===================== *)

VerificationTest[
    TTermEq[TDsu[TNum[7], TNum[11], TNum[22]],
            TSup[7, TNum[11], TNum[22]]],
    True,
    TestID -> "TermEq/cnf-dsu-num-equals-sup"
]

(* === self-equality of arbitrary lazy-encoded value ================ *)

VerificationTest[
    Block[{t = TLazyEncode[{1, {a, b}, {{c}}}]},
        TTermEq[t, t]],
    True,
    TestID -> "TermEq/self-equal-nested"
]

(* === reflexivity / symmetry checks ================================ *)

VerificationTest[
    Block[{a = TLazyEncode[{1, 2}], b = TLazyEncode[{1, 2}]},
        TTermEq[a, b] === TTermEq[b, a]],
    True,
    TestID -> "TermEq/symmetry"
]

VerificationTest[
    Block[{a = TOp2["+", TNum[1], TNum[2]],
           b = TOp2["+", TNum[2], TNum[1]]},
        (* 1+2 = 2+1 = 3 by cnf reduction.  Structural without
           reduction would say different argument order. *)
        {TTermEq[a, b], TTermSame[a, b]}],
    {True, False},
    TestID -> "TermEq/cnf-vs-struct-disagreement"
]
