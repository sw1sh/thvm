(* QF_UF / congruence-closure tests for TSatEUF and
   TFindProofSMT.  Each VerificationTest is
   self-contained; the file is exercised by wl/THVMLink/Tests/run.wls. *)

VerificationTest[
    TSatEUF[{}, {}]["Status"],
    "SAT",
    TestID -> "ATP/smt/empty-input-sat"
]

VerificationTest[
    (* a = b together with a != b is the canonical 1-step UNSAT. *)
    TSatEUF[{a == b}, {a != b}]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/equality-vs-disequality"
]

VerificationTest[
    (* Transitivity: a = b /\ b = c /\ a != c -> UNSAT. *)
    TSatEUF[{a == b, b == c}, {a != c}]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/transitivity-three-vars"
]

VerificationTest[
    (* Independent constants: no propagation, SAT. *)
    TSatEUF[{a == b}, {c != d}]["Status"],
    "SAT",
    TestID -> "ATP/smt/independent-constants-sat"
]

VerificationTest[
    (* Single-arg congruence: a = b -> f[a] = f[b]. *)
    TSatEUF[{a == b}, {f[a] != f[b]}]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/congruence-single-arg"
]

VerificationTest[
    (* Nested congruence: a = b -> f[g[a]] = f[g[b]]. *)
    TSatEUF[{a == b}, {f[g[a]] != f[g[b]]}]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/congruence-nested"
]

VerificationTest[
    (* Multi-arg congruence: a = c /\ b = d -> f[a, b] = f[c, d]. *)
    TSatEUF[
        {a == c, b == d}, {f[a, b] != f[c, d]}]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/congruence-multi-arg"
]

VerificationTest[
    (* No anti-congruence: f[a] = f[b] does NOT entail a = b.
       The procedure is sound -- SAT here. *)
    TSatEUF[{f[a] == f[b]}, {a != b}]["Status"],
    "SAT",
    TestID -> "ATP/smt/no-anti-congruence"
]

VerificationTest[
    Head @ TFindProofSMT[a == c, {a == b, b == c}],
    Association,
    TestID -> "ATP/smt/findproof-transitivity-proves"
]

VerificationTest[
    TFindProofSMT[a == c, {a == b}],
    $Failed,
    TestID -> "ATP/smt/findproof-sat-returns-failed"
]

VerificationTest[
    (* DST canonical: x = f^5(x) /\ x = f^3(x) -> x = f(x).
       gcd(5,3) = 1 so x must equal f(x).  Three propagation
       rounds inside congruence closure. *)
    TSatEUF[
        {x == f[f[f[f[f[x]]]]], x == f[f[f[x]]]},
        {x != f[x]}
    ]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/nested-fixpoint-gcd-one"
]

VerificationTest[
    (* Bidirectional propagation: same merge collapses both
       directions through compound parents. *)
    TSatEUF[
        {a == b, h[a, c] == d}, {h[b, c] != d}
    ]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/congruence-then-direct-eq"
]

VerificationTest[
    (* Ground TPTP CNF dispatch: transitivity. *)
    TFindProofSMT[
        "cnf(a1, axiom, a = b).
         cnf(a2, axiom, b = c).
         cnf(g, negated_conjecture, a != c)."]["Status"],
    "Proved",
    TestID -> "ATP/smt/tptp-cnf-ground-trans-proves"
]

VerificationTest[
    (* Ground TPTP FOF dispatch: congruence. *)
    TFindProofSMT[
        "fof(a1, axiom, a = b).
         fof(g, negated_conjecture, f(a) != f(b))."]["Status"],
    "Proved",
    TestID -> "ATP/smt/tptp-fof-ground-cong-proves"
]

VerificationTest[
    (* SAT TPTP input returns $Failed. *)
    TFindProofSMT[
        "cnf(a1, axiom, a = b).
         cnf(g, negated_conjecture, a != c)."],
    $Failed,
    TestID -> "ATP/smt/tptp-sat-returns-failed"
]

VerificationTest[
    (* Non-ground input is rejected with a message. *)
    TFindProofSMT[
        "cnf(a1, axiom, and(X, Y) = and(Y, X)).
         cnf(g, negated_conjecture, and(a, b) != and(b, a))."],
    $Failed,
    {TFindProofSMT::nonground},
    TestID -> "ATP/smt/tptp-nonground-rejected"
]

(* ----- DPLL(T) shell over Boolean combinations of equality atoms ----- *)

VerificationTest[
    TSmtDecide[a == b]["Status"],
    "SAT",
    TestID -> "ATP/smt/dpllt-atom-sat"
]

VerificationTest[
    (* Direct propositional contradiction caught by the SAT kernel. *)
    TSmtDecide[a == b && a != b]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/dpllt-direct-contradiction"
]

VerificationTest[
    (* Theory contradiction caught after SAT yields a model. *)
    TSmtDecide[a == b && b == c && a != c]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/dpllt-theory-transitivity"
]

VerificationTest[
    (* Disjunction with one feasible branch -> SAT. *)
    TSmtDecide[(a == b || c == d) && c == e]["Status"],
    "SAT",
    TestID -> "ATP/smt/dpllt-disjunction-feasible"
]

VerificationTest[
    (* Both disjuncts blocked by theory -> UNSAT only after blocking. *)
    TSmtDecide[
        (a == b || b == c) && a != b && b != c]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/dpllt-or-both-blocked"
]

VerificationTest[
    (* Congruence inside a disjunction: T-solver must propagate
       through the (a==b && c==d) branch and discover f[a,c]=f[b,d]. *)
    TSmtDecide[
        ((a == b && c == d) || x == y) && f[a, c] != f[b, d] && x != y
    ]["Status"],
    "UNSAT",
    TestID -> "ATP/smt/dpllt-congruence-under-disjunction"
]

VerificationTest[
    TFindProofSMT[
        Implies[a == b && b == c, a == c]]["Status"],
    "Proved",
    TestID -> "ATP/smt/dpllt-findproof-implication"
]

VerificationTest[
    (* Equivalent[a==b, b==a] is a tautology: SAT. *)
    TSmtDecide[Equivalent[a == b, b == a]]["Status"],
    "SAT",
    TestID -> "ATP/smt/dpllt-equivalent-symmetry"
]

(* ----- TFindProof[..., Method -> "SMT"] dispatch ----- *)

VerificationTest[
    (* Method -> "SMT" routes ground equational input to congruence
       closure instead of the saturator -- returns the SMT
       Association directly, not a ProofObject. *)
    TFindProof[a == c, {a == b, b == c}, Method -> "SMT"]["Status"],
    "Proved",
    TestID -> "ATP/smt/method-smt-ground-trans"
]

VerificationTest[
    TFindProof[Implies[a == b && b == c, a == c], {},
        Method -> "SMT"]["Status"],
    "Proved",
    TestID -> "ATP/smt/method-smt-boolean-implication"
]

VerificationTest[
    (* Without Method->"SMT" the saturator path is used; ground
       inputs still produce a regular ProofObject. *)
    Head @ TFindProof[a == c, {a == b, b == c}, TimeConstraint -> 5],
    ProofObject,
    TestID -> "ATP/smt/default-method-still-saturator"
]

(* === Reflexive-equality preprocessing (iter 57) =================== *)

(* WL evaluates `a == a` to True before TSatEUF sees it.  Pre-iter-57
   that hit the badin guard and returned $Failed; now reflexive
   equalities are dropped as vacuously SAT. *)
VerificationTest[
    TSatEUF[{a == a}, {}],
    <|"Status" -> "SAT", "Classes" -> {}|>,
    TestID -> "SMT/preprocess/reflexive-eq-is-vacuously-SAT"
]

(* A False inequality (e.g. Unequal[1, 2] which WL evaluates to True)
   is similarly dropped. *)
VerificationTest[
    TSatEUF[{}, {Unequal[1, 2]}],
    <|"Status" -> "SAT", "Classes" -> {}|>,
    TestID -> "SMT/preprocess/false-diseq-is-vacuously-SAT"
]

(* Mix: a == a (True, dropped) + a != b (real constraint, no
   contradiction without a == b in eqs) -- still SAT. *)
VerificationTest[
    TSatEUF[{a == a}, {a != b}]["Status"],
    "SAT",
    TestID -> "SMT/preprocess/reflexive-with-real-diseq"
]
