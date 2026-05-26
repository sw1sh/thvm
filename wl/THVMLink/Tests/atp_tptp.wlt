(* TPTP import + TFindProof[File / inline-string] tests.
   Each VerificationTest is self-contained; the file is exercised by
   wl/THVMLink/Tests/run.wls. *)

VerificationTest[
    Module[{r = THVMLink`TPTPImport`tptpImport[
        "cnf(a, axiom, and(X, Y) = and(Y, X))."]},
        {Length[r["Axioms"]], MatchQ[r["Conjecture"], None]}
    ],
    {1, True},
    TestID -> "ATP/tptp/inline-axiom-only-parses"
]

VerificationTest[
    Module[{r = THVMLink`TPTPImport`tptpImport[
        "cnf(g, negated_conjecture, foo(sk) != sk)."]},
        Head[r["Conjecture"]]
    ],
    Equal,
    TestID -> "ATP/tptp/negated-conjecture-flips-to-equation"
]

VerificationTest[
    Module[{r = THVMLink`TPTPImport`tptpImport[
        "cnf(a1, axiom, and(X1, and(X2, X3)) = and(and(X1, X2), X3))."]},
        FreeQ[r["Axioms"], Missing]
    ],
    True,
    TestID -> "ATP/tptp/variables-resolve-not-Missing"
]

VerificationTest[
    Module[{r = THVMLink`TPTPImport`tptpImport[
        "cnf(g, negated_conjecture, sk_c1 != sk_c1)."]},
        FreeQ[ToString @ InputForm @ r["Conjecture"], "_c1"]
    ],
    True,
    TestID -> "ATP/tptp/underscore-name-mangled-to-camelCase"
]

VerificationTest[
    Module[{r = THVMLink`TPTPImport`tptpImport[File[
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
    Module[{r = THVMLink`TPTPImport`tptpImport[
        "fof(a, axiom, and(X, Y) = and(Y, X))."]},
        {Length[r["Axioms"]], MatchQ[r["Conjecture"], None]}
    ],
    {1, True},
    TestID -> "ATP/tptp/fof-free-vars-as-universals"
]

VerificationTest[
    (* FOF with explicit ! [X,Y] universal -- quantifier stripped, body kept. *)
    Module[{r = THVMLink`TPTPImport`tptpImport[
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
