(* knuth_bendix_completion.wlt -- TKnuthBendixCompletion: unfailing
   Knuth-Bendix completion to a rule system, and normal forms under it. *)

kbcGroup = AxiomaticTheory["GroupAxioms"] /. OverTilde[1] -> 1;
kbcAC = {ForAll[{x, y}, x \[CirclePlus] y == y \[CirclePlus] x],
    ForAll[{x, y, z}, (x \[CirclePlus] y) \[CirclePlus] z == x \[CirclePlus] (y \[CirclePlus] z)]};

(* === completion === *)

VerificationTest[
    Head @ TKnuthBendixCompletion[kbcGroup, {OverBar, CircleTimes, 1}],
    ProofObject,
    TestID -> "KBC/group-lpo-proofobject"
]

VerificationTest[
    Length @ TKnuthBendixCompletion[kbcGroup, {OverBar, CircleTimes, 1}]["Theorems"],
    10,
    TestID -> "KBC/group-lpo-ten-rules"
]

VerificationTest[
    MemberQ[TKnuthBendixCompletion[kbcGroup, {OverBar, CircleTimes, 1}]["Theorems"],
        OverBar[\[FormalA]_ \[CircleTimes] \[FormalB]_] -> OverBar[\[FormalB]] \[CircleTimes] OverBar[\[FormalA]]],
    True,
    TestID -> "KBC/group-lpo-inverse-of-product"
]

(* Weight 0 for the greatest unary symbol is the Knuth-Bendix group ordering. *)
VerificationTest[
    Length @ TKnuthBendixCompletion[kbcGroup, {{OverBar, 0}, {CircleTimes, 1}, {1, 1}}]["Theorems"],
    10,
    TestID -> "KBC/group-kbo-weight-zero-ten-rules"
]

VerificationTest[
    TKnuthBendixCompletion[kbcGroup, {OverBar, CircleTimes, 1}]["ProofFunction"][],
    Success["ProofFunction", <|"MessageTemplate" -> "Success!"|>],
    TestID -> "KBC/group-lpo-proof-checks"
]

(* An unorientable equation comes back as a TwoWayRule. *)
VerificationTest[
    Head /@ TKnuthBendixCompletion[kbcAC]["Theorems"],
    {TwoWayRule, Rule, TwoWayRule, TwoWayRule},
    TestID -> "KBC/ac-ground-complete"
]

(* A step budget makes a non-terminating completion machine-independent. *)
VerificationTest[
    Equal @@ Table[Length @ Quiet @ TKnuthBendixCompletion[kbcGroup,
        MaxSteps -> 100, TimeConstraint -> Infinity]["Theorems"], 2],
    True,
    TestID -> "KBC/step-budget-deterministic"
]

(* === normal forms === *)

VerificationTest[
    TKnuthBendixCompletion[kbcGroup,
        a \[CircleTimes] OverBar[b \[CircleTimes] (((c \[CircleTimes] OverBar[c]) \[CircleTimes] OverBar[d \[CircleTimes] b]) \[CircleTimes] a)],
        {OverBar, CircleTimes, 1}],
    d,
    TestID -> "KBC/group-normal-form"
]

VerificationTest[
    TKnuthBendixCompletion["GroupAxioms", OverBar[OverBar[a]]],
    a,
    TestID -> "KBC/theory-normal-form"
]

(* Equal terms modulo AC share one normal form, however they are written. *)
VerificationTest[
    TKnuthBendixCompletion[kbcAC, #] & /@ {c \[CirclePlus] (b \[CirclePlus] a),
        (a \[CirclePlus] c) \[CirclePlus] b, b \[CirclePlus] (a \[CirclePlus] c)},
    ConstantArray[a \[CirclePlus] (b \[CirclePlus] c), 3],
    TestID -> "KBC/ac-normal-form-canonical"
]

(* The ordering does not depend on the term: both sides of f1[x] == g1[x]
   reach the same normal form. *)
VerificationTest[
    TKnuthBendixCompletion[{ForAll[x, f1[x] == g1[x]]}, #] & /@ {f1[a], g1[a]},
    {g1[a], g1[a]},
    TestID -> "KBC/normal-form-goal-independent"
]

VerificationTest[
    TKnuthBendixCompletion[kbcGroup, OverBar[a \[CircleTimes] b] == OverBar[b] \[CircleTimes] OverBar[a],
        {OverBar, CircleTimes, 1}],
    True,
    TestID -> "KBC/equation-decided"
]

(* A saturated, ground-convergent system refutes an equation whose sides
   reach two normal forms. *)
VerificationTest[
    TKnuthBendixCompletion[ForAll[x, k[k[x]] == x], k[a] == a],
    False,
    TestID -> "KBC/equation-refuted"
]

(* The fresh goal constant never appears in a normal form, even when the
   axioms make every term of its kind equal. *)
VerificationTest[
    TKnuthBendixCompletion[{ForAll[x, q[c1] == q[x]]}, q[c2]],
    q[c2],
    TestID -> "KBC/goal-constant-hidden"
]

(* Sheffer's single axiom for Boolean algebra has no finite completed
   system; the time-bounded completion still reduces an instance of it. *)
VerificationTest[
    TKnuthBendixCompletion[{ForAll[{x, y}, nn[nn[x, y], nn[x, nn[y, y]]] == x]}, nn[nn[a, b], nn[a, nn[b, b]]]],
    a,
    TestID -> "KBC/sheffer-instance"
]

VerificationTest[
    TKnuthBendixCompletion[ForAll[x, g[g[x]] == x], g[g[g[a]]]],
    g[a],
    TestID -> "KBC/single-axiom"
]

(* === errors === *)

VerificationTest[
    TKnuthBendixCompletion[kbcGroup, {{OverBar, -1}}],
    $Failed,
    {TKnuthBendixCompletion::badord},
    TestID -> "KBC/bad-order"
]
