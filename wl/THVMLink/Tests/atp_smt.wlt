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

VerificationTest[
    (* Ground TPTP CNF dispatch: transitivity. *)
    THVMLink`SMT`TFindProofSMT[
        "cnf(a1, axiom, a = b).
         cnf(a2, axiom, b = c).
         cnf(g, negated_conjecture, a != c)."]["Status"],
    "Proved",
    TestID -> "ATP/smt/tptp-cnf-ground-trans-proves"
]

VerificationTest[
    (* Ground TPTP FOF dispatch: congruence. *)
    THVMLink`SMT`TFindProofSMT[
        "fof(a1, axiom, a = b).
         fof(g, negated_conjecture, f(a) != f(b))."]["Status"],
    "Proved",
    TestID -> "ATP/smt/tptp-fof-ground-cong-proves"
]

VerificationTest[
    (* SAT TPTP input returns $Failed. *)
    THVMLink`SMT`TFindProofSMT[
        "cnf(a1, axiom, a = b).
         cnf(g, negated_conjecture, a != c)."],
    $Failed,
    TestID -> "ATP/smt/tptp-sat-returns-failed"
]

VerificationTest[
    (* Non-ground input is rejected with a message. *)
    THVMLink`SMT`TFindProofSMT[
        "cnf(a1, axiom, and(X, Y) = and(Y, X)).
         cnf(g, negated_conjecture, and(a, b) != and(b, a))."],
    $Failed,
    {THVMLink`SMT`TFindProofSMT::nonground},
    TestID -> "ATP/smt/tptp-nonground-rejected"
]

(* ----- DPLL(T) shell over Boolean combinations of equality atoms ----- *)

VerificationTest[
    THVMLink`SMT`TSmtDecide[a == b]["Status"],
    "SAT",
    TestID -> "ATP/smt/dpllt-atom-sat"
]

VerificationTest[
    (* Direct propositional contradiction caught by the SAT kernel. *)
    THVMLink`SMT`TSmtDecide[a == b && a != b]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/dpllt-direct-contradiction"
]

VerificationTest[
    (* Theory contradiction caught after SAT yields a model. *)
    THVMLink`SMT`TSmtDecide[a == b && b == c && a != c]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/dpllt-theory-transitivity"
]

VerificationTest[
    (* Disjunction with one feasible branch -> SAT. *)
    THVMLink`SMT`TSmtDecide[(a == b || c == d) && c == e]["Status"],
    "SAT",
    TestID -> "ATP/smt/dpllt-disjunction-feasible"
]

VerificationTest[
    (* Both disjuncts blocked by theory -> UNSAT only after blocking. *)
    THVMLink`SMT`TSmtDecide[
        (a == b || b == c) && a != b && b != c]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/dpllt-or-both-blocked"
]

VerificationTest[
    (* Congruence inside a disjunction: T-solver must propagate
       through the (a==b && c==d) branch and discover f[a,c]=f[b,d]. *)
    THVMLink`SMT`TSmtDecide[
        ((a == b && c == d) || x == y) && f[a, c] != f[b, d] && x != y
    ]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/dpllt-congruence-under-disjunction"
]

VerificationTest[
    THVMLink`SMT`TFindProofSMT[
        Implies[a == b && b == c, a == c]]["Status"],
    "Proved",
    TestID -> "ATP/smt/dpllt-findproof-implication"
]

VerificationTest[
    (* Equivalent[a==b, b==a] is a tautology: SAT. *)
    THVMLink`SMT`TSmtDecide[Equivalent[a == b, b == a]]["Status"],
    "SAT",
    TestID -> "ATP/smt/dpllt-equivalent-symmetry"
]
