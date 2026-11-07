(* atp_system.wlt -- TAtpComplete / TAtpNormalForm / TAtpEqualQ: a completed
   equational system kept in the engine and queried without completing again. *)

asGroup = {ForAll[{x, y, z}, m[m[x, y], z] == m[x, m[y, z]]], ForAll[x, m[ee, x] == x], ForAll[x, m[i[x], x] == ee]};
asAC = {ForAll[{x, y}, h[x, y] == h[y, x]], ForAll[{x, y, z}, h[h[x, y], z] == h[x, h[y, z]]]};

VerificationTest[
    Head @ TAtpComplete[asAC],
    TAtpSystem,
    TestID -> "AtpSystem/complete-gives-system"
]

VerificationTest[
    With[{sys = TAtpComplete[asAC]}, {sys["Status"], sys["Complete"], Length[sys["Rules"]]}],
    {"Saturated", True, 4},
    TestID -> "AtpSystem/ac-saturates"
]

(* A theory without a finite completed system stops at its step budget. *)
VerificationTest[
    With[{sys = TAtpComplete[asGroup]}, {sys["Status"], sys["Complete"]}],
    {"BudgetExhausted", False},
    TestID -> "AtpSystem/group-budget"
]

VerificationTest[
    TAtpNormalForm[TAtpComplete[asGroup], {m[a, i[a]], m[i[i[a]], b], m[m[a, i[b]], b]}],
    {ee, m[a, b], a},
    TestID -> "AtpSystem/group-normal-forms"
]

(* Symbols and numbers outside the axioms are constants below every axiom
   symbol, in canonical order. *)
VerificationTest[
    TAtpNormalForm[TAtpComplete[asAC], {h[c, h[b, a]], h[h[a, c], b], h[2, h[x, 1]]}],
    {h[a, h[b, c]], h[a, h[b, c]], h[1, h[2, x]]},
    TestID -> "AtpSystem/ac-canonical"
]

VerificationTest[
    TAtpNormalForm[TAtpComplete[asGroup], m[c, m[i[c], Power[y, 2]]]],
    y^2,
    TestID -> "AtpSystem/foreign-head"
]

VerificationTest[
    TAtpEqualQ[TAtpComplete[asAC], h[c, h[b, a]], h[h[a, c], b]],
    True,
    TestID -> "AtpSystem/equal-entailed"
]

(* Distinct normal forms under a complete system: not entailed, never False. *)
VerificationTest[
    Head /@ {TAtpEqualQ[TAtpComplete[asAC], h[a, b], h[a, c]],
        TAtpEqualQ[TAtpComplete[{ForAll[x, k[k[x]] == x]}], k[a], a]},
    {Failure, Failure},
    TestID -> "AtpSystem/not-entailed"
]

VerificationTest[
    TAtpEqualQ[TAtpComplete[asGroup], m[a, b], m[b, a]],
    Indeterminate,
    TestID -> "AtpSystem/indeterminate-on-budget"
]

(* A kept system survives another completion collecting the shared heap. *)
VerificationTest[
    Module[{sys = TAtpComplete[asAC]},
        TKnuthBendixCompletion["BooleanAxioms", a];
        TAtpNormalForm[sys, h[c, h[b, a]]]],
    h[a, h[b, c]],
    TestID -> "AtpSystem/survives-other-runs"
]

(* The allocation budget ends Sheffer's explosive completion cleanly. *)
VerificationTest[
    With[{sys = TAtpComplete[{ForAll[{x, y}, nn[nn[x, y], nn[x, nn[y, y]]] == x]}]},
        {sys["Status"], TAtpNormalForm[sys, nn[nn[a, b], nn[a, nn[b, b]]]]}],
    {"BudgetExhausted", a},
    TestID -> "AtpSystem/allocation-budget"
]

(* Dropping the last reference frees the engine state. *)
VerificationTest[
    Module[{sys = TAtpComplete[asAC], id},
        id = ManagedLibraryExpressionID[sys["Handle"]];
        sys =.;
        WolframInstitute`THVMLink`ATP`Private`$atpSystemQFn[id]],
    0,
    TestID -> "AtpSystem/freed-on-release"
]
