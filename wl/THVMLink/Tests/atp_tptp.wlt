(* TPTP import + TFindProof[File / inline-string] tests.
   Each VerificationTest is self-contained; the file is exercised by
   wl/THVMLink/Tests/run.wls. *)

VerificationTest[
    Module[{r = TPTPImport[
        "cnf(a, axiom, and(X, Y) = and(Y, X))."]},
        {Length[r["Axioms"]], MatchQ[r["Conjecture"], None]}
    ],
    {1, True},
    TestID -> "ATP/tptp/inline-axiom-only-parses"
]

VerificationTest[
    Module[{r = TPTPImport[
        "cnf(g, negated_conjecture, foo(sk) != sk)."]},
        Head[r["Conjecture"]]
    ],
    Equal,
    TestID -> "ATP/tptp/negated-conjecture-flips-to-equation"
]

VerificationTest[
    Module[{r = TPTPImport[
        "cnf(a1, axiom, and(X1, and(X2, X3)) = and(and(X1, X2), X3))."]},
        FreeQ[r["Axioms"], Missing]
    ],
    True,
    TestID -> "ATP/tptp/variables-resolve-not-Missing"
]

VerificationTest[
    Module[{r = TPTPImport[
        "cnf(g, negated_conjecture, sk_c1 != sk_c1)."]},
        FreeQ[ToString @ InputForm @ r["Conjecture"], "_c1"]
    ],
    True,
    TestID -> "ATP/tptp/underscore-name-mangled-to-camelCase"
]

VerificationTest[
    Module[{r = TPTPImport[File[
        FileNameJoin[{Directory[], "tools", "baselines", "vampire_tptp",
            "AbelianGroupAxioms__InverseOfInverse.p"}]
    ]]},
        {Length[r["Axioms"]], MatchQ[r["Conjecture"], _Equal]}
    ],
    {4, True},
    TestID -> "ATP/tptp/file-roundtrip-abeliangroup"
]

VerificationTest[
    Module[{p = TFindProof[
        File[FileNameJoin[{Directory[], "tools", "baselines", "vampire_tptp",
            "AbelianGroupAxioms__InverseOfInverse.p"}]],
        TimeConstraint -> 10]},
        Head @ Quiet @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/tptp/findproof-file-overload-proves"
]

VerificationTest[
    Module[{p = TFindProof[
        "cnf(a1, axiom, and(X, Y) = and(Y, X)).
         cnf(a2, axiom, and(X, and(Y, Z)) = and(and(X, Y), Z)).
         cnf(g, negated_conjecture, and(and(p, q), r) != and(r, and(q, p))).",
        TimeConstraint -> 10]},
        Head @ Quiet @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/tptp/findproof-inline-string-overload-proves"
]

VerificationTest[
    (* No conjecture clause -- saturation mode, returns the completed
       rule set as default "Lemmas" projection. *)
    MatchQ[
        TFindProof[
            "cnf(a1, axiom, mul(X, e) = X).
             cnf(a2, axiom, mul(e, X) = X).",
            TimeConstraint -> 5],
        {__}
    ],
    True,
    TestID -> "ATP/tptp/findproof-no-conjecture-completes"
]

VerificationTest[
    (* FOF without quantifier -- free variables become universals. *)
    Module[{r = TPTPImport[
        "fof(a, axiom, and(X, Y) = and(Y, X))."]},
        {Length[r["Axioms"]], MatchQ[r["Conjecture"], None]}
    ],
    {1, True},
    TestID -> "ATP/tptp/fof-free-vars-as-universals"
]

VerificationTest[
    (* FOF with explicit ! [X,Y] universal -- quantifier stripped, body kept. *)
    Module[{r = TPTPImport[
        "fof(comm, axiom, ! [X, Y] : (and(X, Y) = and(Y, X)))."]},
        {Length[r["Axioms"]], FreeQ[r["Axioms"], Missing]}
    ],
    {1, True},
    TestID -> "ATP/tptp/fof-universal-quantifier-stripped"
]

VerificationTest[
    Module[{p = TFindProof[
        "fof(assoc, axiom, ! [X, Y, Z] : (and(X, and(Y, Z)) = and(and(X, Y), Z))).
         fof(comm, axiom, ! [X, Y] : (and(X, Y) = and(Y, X))).
         fof(goal, negated_conjecture, and(and(p, q), r) != and(r, and(q, p))).",
        TimeConstraint -> 10]},
        Head @ Quiet @ p["ProofFunction"][p["Theorems"]]
    ],
    Success,
    TestID -> "ATP/tptp/findproof-fof-string-proves"
]

(* Multi-literal cnf: 3 literals -> Or of 3.  Predicate atoms and
   negated atoms supported alongside equational literals. *)
VerificationTest[
    Module[{r = TPTPImport[
        "cnf(a, axiom, p(X) | q(X) | ~r(X))."]},
        {Head @ r["Axioms"][[1]], Length @ r["Axioms"][[1]]}
    ],
    {Or, 3},
    TestID -> "ATP/tptp/cnf-multi-literal-disjunction"
]

(* fof Boolean connectives: =>, &, |, <=>, ~&. *)
VerificationTest[
    Head @ TPTPImport["fof(a, axiom, p(X) => q(X))."]["Axioms"][[1]],
    Implies,
    TestID -> "ATP/tptp/fof-implies"
]

VerificationTest[
    Module[{r = TPTPImport["fof(a, axiom, p & q & r)."]["Axioms"][[1]]},
        {Head[r], Length[r]}
    ],
    {And, 3},
    TestID -> "ATP/tptp/fof-and-left-associative"
]

VerificationTest[
    Head @ TPTPImport["fof(a, axiom, p <=> q)."]["Axioms"][[1]],
    Equivalent,
    TestID -> "ATP/tptp/fof-equivalent"
]

VerificationTest[
    MatchQ[
        TPTPImport["fof(a, axiom, p ~& q)."]["Axioms"][[1]],
        Not[_And]],
    True,
    TestID -> "ATP/tptp/fof-nand-shorthand"
]

(* Existential quantifier: bound vars are bare Symbols, body
   references them via the same Symbol -- Exists wrapper preserved. *)
VerificationTest[
    Module[{r = TPTPImport["fof(a, axiom, ? [X] : p(X))."]["Axioms"][[1]]},
        Head[r]
    ],
    Exists,
    TestID -> "ATP/tptp/fof-exists-preserves-wrapper"
]

(* tff: signature declarations are skipped; sort annotations stripped
   so the universally-quantified body parses just like untyped fof. *)
VerificationTest[
    Module[{r = TPTPImport[
        "tff(p_type, type, p: $i > $o).
         tff(a, axiom, ! [X:$i] : p(X))."]},
        {Length @ r["Axioms"], FreeQ[r["Axioms"], "$i"]}
    ],
    {1, True},
    TestID -> "ATP/tptp/tff-type-decl-skipped-sort-stripped"
]

(* tcf: typed cnf, sort annotations stripped, multi-literal -> Or. *)
VerificationTest[
    Head @ TPTPImport[
        "tcf(a, axiom, p(X:nat) | q(X:nat))."]["Axioms"][[1]],
    Or,
    TestID -> "ATP/tptp/tcf-multi-literal-with-sorts"
]

(* include: write a temp ax file + main file, parse main, confirm
   the axiom from the included file appears in the imported axioms. *)
VerificationTest[
    Module[{tmpdir = CreateDirectory[], r},
        Export[FileNameJoin[{tmpdir, "ax.ax"}],
            "cnf(a1, axiom, mul(X, e) = X).\n", "Text"];
        Export[FileNameJoin[{tmpdir, "main.p"}],
            "include('ax.ax').\n" <>
            "cnf(g, negated_conjecture, mul(c, e) != c).\n",
            "Text"];
        r = TPTPImport[File @ FileNameJoin[{tmpdir, "main.p"}]];
        {Length @ r["Axioms"], Head @ r["Conjecture"]}
    ],
    {1, Equal},
    TestID -> "ATP/tptp/include-relative-path"
]

(* tpi is silently skipped (process instruction, no semantic content). *)
VerificationTest[
    Module[{r = TPTPImport[
        "tpi(set, axiom, $set($timeout, 30)).\n" <>
        "cnf(a, axiom, foo(X) = X)."]},
        Length @ r["Axioms"]
    ],
    1,
    TestID -> "ATP/tptp/tpi-skipped-silently"
]
