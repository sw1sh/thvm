(* pattern.wlt -- VerificationTest specs for Pattern.wl v0.

   First slice: WL-pattern-shaped matching against TTerm values.
   Single-valued bindings, leftmost-only replacement.  Multi-valued
   matching (Orderless / sequence patterns) and IC-native compilation
   to SUP-of-bindings streams are follow-ups. *)

(* === Blank: matches anything, no bindings ========================= *)

VerificationTest[
    TPMatch[TNum[7], _],
    <||>,
    TestID -> "Pattern/blank-matches-anything"
]

VerificationTest[
    TPMatch[TLazyEncode[a], _],
    <||>,
    TestID -> "Pattern/blank-matches-symbol-ctr"
]

VerificationTest[
    TPMatchQ[TNum[42], _],
    True,
    TestID -> "Pattern/MatchQ-blank-true"
]

(* === Named blank: x_ binds the matched term ======================= *)

VerificationTest[
    With[{m = TPMatch[TNum[5], x_]},
        TTermVal[m[x]]],
    5,
    TestID -> "Pattern/named-blank-binds"
]

(* === Numeric literal pattern ====================================== *)

VerificationTest[
    TPMatchQ[TNum[3], 3],
    True,
    TestID -> "Pattern/integer-literal-match"
]

VerificationTest[
    TPMatchQ[TNum[3], 4],
    False,
    TestID -> "Pattern/integer-literal-mismatch"
]

(* === TTerm literal: TTermSame ==================================== *)

VerificationTest[
    Block[{a = TLazyEncode[{1, 2, 3}], b = TLazyEncode[{1, 2, 3}]},
        TPMatchQ[a, b]],
    True,
    TestID -> "Pattern/TTerm-literal-match"
]

VerificationTest[
    TPMatchQ[TLazyEncode[{1, 2}], TLazyEncode[{1, 3}]],
    False,
    TestID -> "Pattern/TTerm-literal-mismatch"
]

(* === Compound pattern: head[args__] decomposes a CTR ============== *)
(* TLazyEncode[g[x, y, z]] yields a CTR with the symbol-`g` ctor tag. *)

VerificationTest[
    With[{t = TLazyEncode[g[a, b]],
          m = TPMatch[TLazyEncode[g[a, b]], g[x_, y_]]},
        {TTermSame[m[x], TLazyEncode[a]],
         TTermSame[m[y], TLazyEncode[b]]}],
    {True, True},
    TestID -> "Pattern/compound-CTR-bindings"
]

VerificationTest[
    TPMatchQ[TLazyEncode[g[a, b]], h[x_, y_]],
    False,
    TestID -> "Pattern/compound-CTR-wrong-head"
]

VerificationTest[
    TPMatchQ[TLazyEncode[g[a, b, c]], g[x_, y_]],
    False,
    TestID -> "Pattern/compound-CTR-wrong-arity"
]

(* === Repeat-binder pattern: f[x_, x_] requires both children
   to be TTermEq.  Cnf-reducing equality kicks in: if the children
   are 2+3 and 5, they should still match. *)

VerificationTest[
    TPMatchQ[
        TLazyEncode[g[a, a]],
        g[x_, x_]],
    True,
    TestID -> "Pattern/repeat-binder-equal"
]

VerificationTest[
    TPMatchQ[
        TLazyEncode[g[a, b]],
        g[x_, x_]],
    False,
    TestID -> "Pattern/repeat-binder-different"
]

(* === Mixed: literal in one slot, binder in another =============== *)

VerificationTest[
    With[{m = TPMatch[TLazyEncode[g[a, b]], g[a, x_]]},
        TTermSame[m[x], TLazyEncode[b]]],
    True,
    TestID -> "Pattern/literal-and-binder-mixed"
]

VerificationTest[
    TPMatchQ[TLazyEncode[g[a, b]], g[c, x_]],
    False,
    TestID -> "Pattern/literal-mismatch-rejects"
]

(* === Nested compound: g[h[x_], y_] =============================== *)

VerificationTest[
    With[{t = TLazyEncode[g[h[a], b]],
          m = TPMatch[TLazyEncode[g[h[a], b]], g[h[x_], y_]]},
        {TTermSame[m[x], TLazyEncode[a]],
         TTermSame[m[y], TLazyEncode[b]]}],
    {True, True},
    TestID -> "Pattern/nested-compound-bindings"
]

(* === Substitution: TPSubstitute fills RHS from bindings ========== *)

VerificationTest[
    TLazyDecode @ TPSubstitute[<|x -> TLazyEncode[42]|>, x],
    42,
    TestID -> "Pattern/substitute-bound-symbol"
]

VerificationTest[
    TLazyDecode @ TPSubstitute[<|x -> TLazyEncode[a]|>, g[x, x]],
    g[a, a],
    TestID -> "Pattern/substitute-into-compound"
]

(* === TPReplace: rewrite at root if pattern matches =============== *)

VerificationTest[
    TLazyDecode @ TPReplace[
        TLazyEncode[g[a, b]],
        g[x_, y_] -> h[y, x]],
    h[b, a],
    TestID -> "Pattern/replace-swap-args"
]

VerificationTest[
    TLazyDecode @ TPReplace[
        TLazyEncode[g[a, b]],
        f[x_] -> q[x]],
    g[a, b],                       (* no match: returns original *)
    TestID -> "Pattern/replace-no-match-passthrough"
]

(* === TPReplaceList: enumerate matches at every position ========== *)

VerificationTest[
    Length @ TPReplaceList[
        TLazyEncode[g[h[a], h[b]]],
        h[x_] -> q[x]],
    2,
    TestID -> "Pattern/replace-list-finds-two"
]

VerificationTest[
    Sort[TLazyDecode /@ (Last /@ TPReplaceList[
        TLazyEncode[g[h[a], h[b]]],
        h[x_] -> q[x]])],
    Sort @ {q[a], q[b]},
    TestID -> "Pattern/replace-list-substituted-payloads"
]

(* === HeadBlank: x_Cons matches only Cons-shaped TTerms =========== *)
(* Lazy.wl exposes Cons via $LazyCons; we need a registered symbol
   "Cons" via symLabelFor to make this clickable from WL.  Use g
   directly here -- TLazyEncode[g[...]] registers symbol "g" via
   symLabelFor on first use. *)

VerificationTest[
    TPMatchQ[TLazyEncode[g[a]], _g],
    True,
    TestID -> "Pattern/HeadBlank-matches-registered-ctor"
]

VerificationTest[
    TPMatchQ[TLazyEncode[g[a]], _h],
    False,
    TestID -> "Pattern/HeadBlank-wrong-ctor"
]
