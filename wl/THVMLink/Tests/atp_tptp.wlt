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

(* Single-quoted atoms: contents become the String head of a 0-arity
   compound, so atom names with spaces / special chars round-trip. *)
VerificationTest[
    TPTPImport["cnf(a, axiom, eq(a, 'hello world'))."]["Axioms"][[1, 2]],
    "hello world"[],
    TestID -> "ATP/tptp/single-quoted-atom-with-space"
]

(* Numeric literals: unsigned, signed, real, rational, scientific. *)
VerificationTest[
    {TPTPImport["cnf(a, axiom, foo(42) = bar)."]["Axioms"][[1, 1, 1]],
     TPTPImport["cnf(a, axiom, foo(-42) = bar)."]["Axioms"][[1, 1, 1]],
     TPTPImport["cnf(a, axiom, foo(3.14) = bar)."]["Axioms"][[1, 1, 1]],
     TPTPImport["cnf(a, axiom, foo(3/4) = bar)."]["Axioms"][[1, 1, 1]],
     TPTPImport["cnf(a, axiom, foo(1.5e-3) = bar)."]["Axioms"][[1, 1, 1]]
    },
    {"42"[], "-42"[], "3.14"[], "3/4"[], "1.5e-3"[]},
    TestID -> "ATP/tptp/numeric-literals"
]

(* Distinct objects: literal `"` characters in the String head
   distinguish from plain quoted atoms. *)
VerificationTest[
    Module[{r = TPTPImport[
        "cnf(a, axiom, eq(\"distinct1\", \"distinct2\"))."]},
        Head @ r["Axioms"][[1, 1]]
    ],
    "\"distinct1\"",
    TestID -> "ATP/tptp/distinct-object-quote-preserved"
]

(* include selector: only named clauses admitted. *)
VerificationTest[
    Module[{tmpdir = CreateDirectory[], r},
        Export[FileNameJoin[{tmpdir, "ax.ax"}],
            "cnf(a1, axiom, mul(X, e) = X).\n" <>
            "cnf(a2, axiom, mul(e, X) = X).\n" <>
            "cnf(a3, axiom, mul(inv(X), X) = e).\n", "Text"];
        Export[FileNameJoin[{tmpdir, "main.p"}],
            "include('ax.ax', [a1, a3]).\n", "Text"];
        r = TPTPImport[File @ FileNameJoin[{tmpdir, "main.p"}]];
        Length @ r["Axioms"]
    ],
    2,
    TestID -> "ATP/tptp/include-clause-selector"
]

(* $-defined predicates / arithmetic functions / $true / $false. *)
VerificationTest[
    {Head @ TPTPImport[
        "fof(a, axiom, $sum(2, 3) = 5)."]["Axioms"][[1, 1]],
     Head @ TPTPImport[
        "fof(a, axiom, $distinct(a, b, c))."]["Axioms"][[1]],
     TPTPImport["fof(a, axiom, $true)."]["Axioms"][[1]],
     TPTPImport["fof(a, axiom, $false)."]["Axioms"][[1]]
    },
    {"$sum", "$distinct", True, False},
    TestID -> "ATP/tptp/dollar-defined-forms"
]

(* thf: type decl skipped; lambda + @ application + Boolean combinators. *)
VerificationTest[
    {Length @ TPTPImport["thf(p, type, p: $i > $o)."]["Axioms"],
     MatchQ[TPTPImport["thf(a, axiom, ! [X:$i] : (p @ X))."]["Axioms"][[1]],
        "p"[_Pattern]],
     TPTPImport["thf(a, axiom, f @ x @ y)."]["Axioms"][[1]],
     Head @ TPTPImport[
        "thf(a, axiom, ^ [X:$i] : (f @ X))."]["Axioms"][[1]]
    },
    {0, True, "f"["x"[]]["y"[]], Function},
    TestID -> "ATP/tptp/thf-lambda-application-types"
]

(* ncf modal operators: `$box`, `$dia` parse as generic compounds. *)
VerificationTest[
    Module[{r = TPTPImport["ncf(a, axiom, $box(p) => $dia(p))."]},
        {Head @ r["Axioms"][[1]],
         Head @ r["Axioms"][[1, 1]],
         Head @ r["Axioms"][[1, 2]]}
    ],
    {Implies, "$box", "$dia"},
    TestID -> "ATP/tptp/ncf-modal-via-dollar-defined"
]

(* Sequent: `A1, A2 --> B1, B2` rewrites to `Implies[And[..], Or[..]]`. *)
VerificationTest[
    Module[{r = TPTPImport["fof(a, axiom, p & q --> r | s)."]},
        Head @ r["Axioms"][[1]]
    ],
    Implies,
    TestID -> "ATP/tptp/sequent-form-to-implies"
]

(* Anti-infinite-loop guard: malformed inputs that put a Boolean
   inside a term-arg position now bail rather than wedge readArgs. *)
VerificationTest[
    MatchQ[
        TPTPImport["ncf(a, axiom, $dia(p & q))."],
        _Association],
    True,
    TestID -> "ATP/tptp/malformed-arg-list-doesnt-hang"
]
