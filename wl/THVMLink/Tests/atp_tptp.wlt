(* TFindProof[File / inline-string] tests.  TPTP-parsing coverage now
   lives in the Wolfram/WolframParser paclet (Tests/TPTP.wlt); the
   tests here exercise the thvm-side dispatch that pipes TPTPImport
   output into the C ATP engine.  Each VerificationTest is
   self-contained; the file is exercised by wl/THVMLink/Tests/run.wls. *)

Needs["Wolfram`Parser`"]


(* The file overload reads a problem this test writes itself: inv(inv(a)) = a in an abelian group. *)
VerificationTest[
    Module[{f = FileNameJoin[{$TemporaryDirectory, "thvm-" <> CreateUUID[] <> ".p"}], p},
        WriteString[f,
            "cnf(left_identity, axiom, mult(e, X) = X).\n" <>
            "cnf(left_inverse, axiom, mult(inv(X), X) = e).\n" <>
            "cnf(associativity, axiom, mult(mult(X, Y), Z) = mult(X, mult(Y, Z))).\n" <>
            "cnf(commutativity, axiom, mult(X, Y) = mult(Y, X)).\n" <>
            "cnf(goal, negated_conjecture, inv(inv(a)) != a).\n"];
        Close[f];
        p = TFindProof[File[f], TimeConstraint -> 10];
        DeleteFile[f];
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
    (* No conjecture clause -- saturation mode, returns a ProofObject
       with Theorems -> None per b25ea718.  The "Lemmas" projection
       still works (TFindProof[axioms, "Lemmas"]) but the default is
       now "ProofObject" for consistency with goal-directed forms. *)
    Module[{p = TFindProof[
            "cnf(a1, axiom, mul(X, e) = X).
             cnf(a2, axiom, mul(e, X) = X).",
            TimeConstraint -> 5]},
        Head[p] === ProofObject &&
            Length @ Lookup[p[[4]], "Proof", {}] >= 2
    ],
    True,
    TestID -> "ATP/tptp/findproof-no-conjecture-completes"
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
