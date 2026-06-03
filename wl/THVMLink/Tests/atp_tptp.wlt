(* TFindProof[File / inline-string] tests.  TPTP-parsing coverage now
   lives in the Wolfram/WolframParser paclet (Tests/TPTP.wlt); the
   tests here exercise the thvm-side dispatch that pipes TPTPImport
   output into the C ATP engine.  Each VerificationTest is
   self-contained; the file is exercised by wl/THVMLink/Tests/run.wls. *)

Needs["Wolfram`Parser`"]


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
