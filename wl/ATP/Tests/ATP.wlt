(* ATP.wlt -- the WolframInstitute`ATP` package: TAtpComplete, TAtpNormalForm
   and TAtpEqualQ over libthvm_atp.  Load the package first:
     PacletDirectoryLoad["wl/ATP"]; Get["WolframInstitute`ATP`"] *)

pkGroup = {ForAll[{x, y, z}, m[m[x, y], z] == m[x, m[y, z]]], ForAll[x, m[ee, x] == x], ForAll[x, m[i[x], x] == ee]};
pkAC = {ForAll[{x, y}, h[x, y] == h[y, x]], ForAll[{x, y, z}, h[h[x, y], z] == h[x, h[y, z]]]};

VerificationTest[
    With[{sys = WolframInstitute`ATP`TAtpComplete[pkAC]}, {Head[sys], sys["Status"], sys["Complete"], Length[sys["Rules"]]}],
    {WolframInstitute`ATP`TAtpSystem, "Saturated", True, 4},
    TestID -> "ATP/ac-saturates"
]

VerificationTest[
    Head /@ WolframInstitute`ATP`TAtpComplete[pkAC]["Rules"],
    {TwoWayRule, Rule, TwoWayRule, TwoWayRule},
    TestID -> "ATP/ac-rule-kinds"
]

VerificationTest[
    WolframInstitute`ATP`TAtpNormalForm[WolframInstitute`ATP`TAtpComplete[pkAC], {h[c, h[b, a]], h[h[a, c], b], h[2, h[x, 1]]}],
    {h[a, h[b, c]], h[a, h[b, c]], h[1, h[2, x]]},
    TestID -> "ATP/ac-canonical"
]

VerificationTest[
    With[{sys = WolframInstitute`ATP`TAtpComplete[pkGroup]}, {sys["Status"], Length[sys["Rules"]],
        WolframInstitute`ATP`TAtpNormalForm[sys, {m[a, i[a]], m[i[i[a]], b], m[c, m[i[c], Power[y, 2]]]}]}],
    {"BudgetExhausted", 92, {ee, m[a, b], y^2}},
    TestID -> "ATP/group-budget"
]

VerificationTest[
    {#["Status"], Length[#["Rules"]]} & /@ {WolframInstitute`ATP`TAtpComplete[pkGroup, {i, m, ee}],
        WolframInstitute`ATP`TAtpComplete[pkGroup, {{i, 0}, {m, 1}, {ee, 1}}]},
    {{"Saturated", 10}, {"Saturated", 10}},
    TestID -> "ATP/group-orders-complete"
]

VerificationTest[
    With[{sys = WolframInstitute`ATP`TAtpComplete[{ForAll[x, k[k[x]] == x]}]},
        {WolframInstitute`ATP`TAtpEqualQ[sys, k[k[a]], a], Head @ WolframInstitute`ATP`TAtpEqualQ[sys, k[a], a]}],
    {True, Failure},
    TestID -> "ATP/equal-entailed-or-not"
]

VerificationTest[
    WolframInstitute`ATP`TAtpEqualQ[WolframInstitute`ATP`TAtpComplete[pkGroup], m[a, b], m[b, a]],
    Indeterminate,
    TestID -> "ATP/indeterminate-on-budget"
]

VerificationTest[
    With[{sys = WolframInstitute`ATP`TAtpComplete[{ForAll[{x, y}, nn[nn[x, y], nn[x, nn[y, y]]] == x]}]},
        {sys["Status"], WolframInstitute`ATP`TAtpNormalForm[sys, nn[nn[a, b], nn[a, nn[b, b]]]]}],
    {"BudgetExhausted", a},
    TestID -> "ATP/allocation-budget"
]

VerificationTest[
    Module[{sys = WolframInstitute`ATP`TAtpComplete[pkAC], id},
        id = ManagedLibraryExpressionID[sys["Handle"]];
        sys =.;
        WolframInstitute`ATP`Private`$systemQFn[id]],
    0,
    TestID -> "ATP/freed-on-release"
]

(* With THVMLink loaded too, both give the same systems and answers. *)
VerificationTest[
    If[ ! MemberQ[$Packages, "WolframInstitute`THVMLink`ATP`"], True,
        Module[{a = WolframInstitute`ATP`TAtpComplete[pkGroup], b = WolframInstitute`THVMLink`ATP`TAtpComplete[pkGroup]},
            {a["Status"], a["Rules"], WolframInstitute`ATP`TAtpNormalForm[a, m[c, m[i[c], i[i[d]]]]]} ===
            {b["Status"], b["Rules"], WolframInstitute`THVMLink`ATP`TAtpNormalForm[b, m[c, m[i[c], i[i[d]]]]]}]],
    True,
    TestID -> "ATP/agrees-with-THVMLink"
]
