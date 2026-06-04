(* QF_UF / congruence-closure tests for TSatEUF, TSmtDecide, and the
   TFindProof Method -> "SMT" entailment surface.  Each VerificationTest is
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
    Head @ TFindProof[a == c, {a == b, b == c}, Method -> "SMT"],
    Association,
    TestID -> "ATP/smt/findproof-transitivity-proves"
]

VerificationTest[
    (* SAT: a == c is not entailed by a == b alone.  The "Counterexample"
       output kind returns a CounterexampleObject whose Model is the
       congruence-closure quotient as a finite algebra in FindFiniteModels
       structure: {a, b} merged to element 0, c on its own as element 1. *)
    Module[{r = TFindProof[a == c, {a == b}, "Counterexample"]},
        {Head[r], r["Status"], r["Model"], r["Domain"], r["Goal"]}],
    {CounterexampleObject, "Refuted", <|a -> 0, b -> 0, c -> 1|>, 2, a == c},
    TestID -> "ATP/smt/findproof-sat-returns-countermodel"
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
    (* Ground TPTP CNF dispatch under Method -> "SMT": transitivity. *)
    TFindProof[
        "cnf(a1, axiom, a = b).
         cnf(a2, axiom, b = c).
         cnf(g, negated_conjecture, a != c).", Method -> "SMT"]["Status"],
    "Proved",
    TestID -> "ATP/smt/tptp-cnf-ground-trans-proves"
]

VerificationTest[
    (* Ground TPTP FOF dispatch under Method -> "SMT": congruence. *)
    TFindProof[
        "fof(a1, axiom, a = b).
         fof(g, negated_conjecture, f(a) != f(b)).", Method -> "SMT"]["Status"],
    "Proved",
    TestID -> "ATP/smt/tptp-fof-ground-cong-proves"
]

VerificationTest[
    (* SAT TPTP input returns a refuting CounterexampleObject, not $Failed. *)
    Head @ TFindProof[
        "cnf(a1, axiom, a = b).
         cnf(g, negated_conjecture, a != c).", Method -> "SMT"],
    CounterexampleObject,
    TestID -> "ATP/smt/tptp-sat-returns-countermodel"
]

VerificationTest[
    (* Non-ground TPTP input under Method -> "SMT" is rejected with a message. *)
    TFindProof[
        "cnf(a1, axiom, and(X, Y) = and(Y, X)).
         cnf(g, negated_conjecture, and(a, b) != and(b, a)).", Method -> "SMT"],
    $Failed,
    {TFindProof::nonground},
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
    (* A Boolean-combination goal auto-routes to DPLL(T) -- no Method -> "SMT"
       needed.  This implication is a tautology, so it is proved. *)
    TFindProof[
        Implies[a == b && b == c, a == c], {}]["Status"],
    "Proved",
    TestID -> "ATP/smt/dpllt-findproof-implication"
]

VerificationTest[
    (* Boolean non-entailment: a == c does not follow from a == b, so the
       DPLL(T) path returns a refuting CounterexampleObject whose Model is the
       certified satisfying truth assignment. *)
    Module[{r = TFindProof[Implies[a == b, a == c], {}, "Counterexample"]},
        {Head[r], r["Status"], AssociationQ[r["Model"]]}],
    {CounterexampleObject, "Refuted", True},
    TestID -> "ATP/smt/dpllt-findproof-refuted"
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

(* Method -> "SMT" with a pre-evaluated True hypothesis: a vacuous `a == a`
   in the hypothesis list is skipped, matching the TSatEUF preprocess shape. *)
VerificationTest[
    TFindProof[a == c, {a == a, a == b, b == c}, Method -> "SMT"]["Status"],
    "Proved",
    TestID -> "SMT/method-smt/skips-True-hypothesis"
]

(* Inactive[Equal] / Inactive[Unequal] are FindEquationalProof's
   inert ProofObject lemma form -- TSatEUF should accept them
   alongside the bare Equal / Unequal heads (iter 59). *)
VerificationTest[
    TSatEUF[{Inactive[Equal][a, b], Inactive[Equal][b, c]},
        {Inactive[Unequal][a, c]}]["Status"],
    "UNSAT",
    TestID -> "SMT/inactive/transitive-contradiction"
]

VerificationTest[
    TSatEUF[{Inactive[Equal][a, b]}, {}]["Status"],
    "SAT",
    TestID -> "SMT/inactive/single-equality-SAT"
]

(* TFindProof[..., Method -> "SMT"] should also auto-flatten nested
   axiom lists, mirroring the iter-62 TFindProof entry shape.  Pre-
   iter-67, a nested {{a == b}, {b == c}} hit collectLiterals'
   catch-all and returned $Failed. *)
VerificationTest[
    TFindProof[a == c, {{a == b}, {b == c}}, Method -> "SMT"]["Status"],
    "Proved",
    TestID -> "SMT/method-smt-nested-axioms-flattened"
]

(* TFindProof[goal, single_hypothesis, Method -> "SMT"] auto-wraps the lone
   hypothesis, parity with the TFindProof / TRelevantAxioms / TATP wraps. *)
VerificationTest[
    TFindProof[a == c, a == c, Method -> "SMT"]["Status"],
    "Proved",
    TestID -> "SMT/method-smt/single-hyp-wraps"
]
