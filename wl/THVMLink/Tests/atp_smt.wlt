(* QF_UF / congruence-closure tests for THVMLink`SMT`TSatEUF and
   THVMLink`SMT`TFindProofSMT.  Each VerificationTest is
   self-contained; the file is exercised by wl/THVMLink/Tests/run.wls. *)

VerificationTest[
    THVMLink`SMT`TSatEUF[{}, {}]["Status"],
    "SAT",
    TestID -> "ATP/smt/empty-input-sat"
]

VerificationTest[
    (* a = b together with a != b is the canonical 1-step UNSAT. *)
    THVMLink`SMT`TSatEUF[{a == b}, {a != b}]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/equality-vs-disequality"
]

VerificationTest[
    (* Transitivity: a = b /\ b = c /\ a != c -> UNSAT. *)
    THVMLink`SMT`TSatEUF[{a == b, b == c}, {a != c}]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/transitivity-three-vars"
]

VerificationTest[
    (* Independent constants: no propagation, SAT. *)
    THVMLink`SMT`TSatEUF[{a == b}, {c != d}]["Status"],
    "SAT",
    TestID -> "ATP/smt/independent-constants-sat"
]

VerificationTest[
    (* Single-arg congruence: a = b -> f[a] = f[b]. *)
    THVMLink`SMT`TSatEUF[{a == b}, {f[a] != f[b]}]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/congruence-single-arg"
]

VerificationTest[
    (* Nested congruence: a = b -> f[g[a]] = f[g[b]]. *)
    THVMLink`SMT`TSatEUF[{a == b}, {f[g[a]] != f[g[b]]}]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/congruence-nested"
]

VerificationTest[
    (* Multi-arg congruence: a = c /\ b = d -> f[a, b] = f[c, d]. *)
    THVMLink`SMT`TSatEUF[
        {a == c, b == d}, {f[a, b] != f[c, d]}]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/congruence-multi-arg"
]

VerificationTest[
    (* No anti-congruence: f[a] = f[b] does NOT entail a = b.
       The procedure is sound -- SAT here. *)
    THVMLink`SMT`TSatEUF[{f[a] == f[b]}, {a != b}]["Status"],
    "SAT",
    TestID -> "ATP/smt/no-anti-congruence"
]

VerificationTest[
    Head @ THVMLink`SMT`TFindProofSMT[a == c, {a == b, b == c}],
    Association,
    TestID -> "ATP/smt/findproof-transitivity-proves"
]

VerificationTest[
    THVMLink`SMT`TFindProofSMT[a == c, {a == b}],
    $Failed,
    TestID -> "ATP/smt/findproof-sat-returns-failed"
]

VerificationTest[
    (* DST canonical: x = f^5(x) /\ x = f^3(x) -> x = f(x).
       gcd(5,3) = 1 so x must equal f(x).  Three propagation
       rounds inside congruence closure. *)
    THVMLink`SMT`TSatEUF[
        {x == f[f[f[f[f[x]]]]], x == f[f[f[x]]]},
        {x != f[x]}
    ]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/nested-fixpoint-gcd-one"
]

VerificationTest[
    (* Bidirectional propagation: same merge collapses both
       directions through compound parents. *)
    THVMLink`SMT`TSatEUF[
        {a == b, h[a, c] == d}, {h[b, c] != d}
    ]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/congruence-then-direct-eq"
]
