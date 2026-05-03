(* pattern.wlt -- VerificationTest specs for Pattern.wl v0
   (Match-object shape, tracking Wolfram`Patterns`).

   Match objects:
     TMatch[m]           wrapper
     TMatchSum[m1, ...]  alternatives  (lazy OR; empty = no match)
     TMatchProduct[...]  conjunction  (head + per-arg children)
     TMatchPart[part, HoldPattern[p], submatch]
     TMatchValues[v, ...] leaf TTerms

   Accessors:
     TMatchBindings[m]   -> List of binding-Associations
     TMatchApply[rhs, m] -> List of substituted TTerms
     TMatchParts[m]      -> List of path->TTerm Associations *)

(* === TMatchObjectQ ================================================ *)

VerificationTest[
    TMatchObjectQ @ TMatch[TMatchValues[TNum[7]]],
    True,
    TestID -> "Pattern/MatchObjectQ-TMatch"
]

VerificationTest[
    TMatchObjectQ @ TMatchSum[],
    True,
    TestID -> "Pattern/MatchObjectQ-empty-Sum"
]

VerificationTest[
    TMatchObjectQ @ {TNum[1], TNum[2]},
    False,
    TestID -> "Pattern/MatchObjectQ-list-rejects"
]

(* === Blank: matches anything; no name binding ===================== *)

VerificationTest[
    Head @ TPatternMatch[TNum[7], _],
    TMatch,
    TestID -> "Pattern/blank-matches-anything"
]

VerificationTest[
    TMatchBindings @ TPatternMatch[TNum[7], _],
    {<||>},
    TestID -> "Pattern/blank-yields-empty-bindings"
]

(* === Named blank: x_ binds the matched term ======================= *)

VerificationTest[
    With[{bs = TMatchBindings @ TPatternMatch[TNum[5], x_]},
        TTermVal[bs[[1, Key[x]]]]],
    5,
    TestID -> "Pattern/named-blank-binds"
]

(* === Numeric literal pattern ====================================== *)

VerificationTest[
    TMatchBindings @ TPatternMatch[TNum[3], 3],
    {<||>},
    TestID -> "Pattern/integer-literal-match"
]

VerificationTest[
    TPatternMatch[TNum[3], 4],
    TMatchSum[],
    TestID -> "Pattern/integer-literal-mismatch-empty-Sum"
]

(* === TTerm literal: TTermSame ==================================== *)

VerificationTest[
    Block[{a = TLazyEncode[{1, 2, 3}], b = TLazyEncode[{1, 2, 3}]},
        TMatchBindings @ TPatternMatch[a, b]],
    {<||>},
    TestID -> "Pattern/TTerm-literal-match"
]

VerificationTest[
    TPatternMatch[TLazyEncode[{1, 2}], TLazyEncode[{1, 3}]],
    TMatchSum[],
    TestID -> "Pattern/TTerm-literal-mismatch"
]

(* === Compound pattern: head[args__] decomposes a CTR ============== *)

VerificationTest[
    Block[{m, bs, dx, dy},
        m  = TPatternMatch[TLazyEncode[g[a, b]], g[x_, y_]];
        bs = TMatchBindings[m];
        dx = bs[[1, Key[x]]];
        dy = bs[[1, Key[y]]];
        {TTermSame[dx, TLazyEncode[a]],
         TTermSame[dy, TLazyEncode[b]]}],
    {True, True},
    TestID -> "Pattern/compound-CTR-bindings"
]

VerificationTest[
    TPatternMatch[TLazyEncode[g[a, b]], h[x_, y_]],
    TMatchSum[],
    TestID -> "Pattern/compound-CTR-wrong-head"
]

VerificationTest[
    TPatternMatch[TLazyEncode[g[a, b, c]], g[x_, y_]],
    TMatchSum[],
    TestID -> "Pattern/compound-CTR-wrong-arity"
]

(* === Repeat-binder pattern: f[x_, x_] requires both children
   to be TTermEq.  Mismatch drops the outcome from TMatchBindings. *)

VerificationTest[
    Length @ TMatchBindings @ TPatternMatch[
        TLazyEncode[g[a, a]],
        g[x_, x_]],
    1,
    TestID -> "Pattern/repeat-binder-equal-yields-binding"
]

VerificationTest[
    TMatchBindings @ TPatternMatch[
        TLazyEncode[g[a, b]],
        g[x_, x_]],
    {},
    TestID -> "Pattern/repeat-binder-different-no-bindings"
]

(* === Mixed: literal in one slot, binder in another =============== *)

VerificationTest[
    With[{bs = TMatchBindings @ TPatternMatch[TLazyEncode[g[a, b]],
                                              g[a, x_]]},
        TTermSame[bs[[1, Key[x]]], TLazyEncode[b]]],
    True,
    TestID -> "Pattern/literal-and-binder-mixed"
]

VerificationTest[
    TPatternMatch[TLazyEncode[g[a, b]], g[c, x_]],
    TMatchSum[],
    TestID -> "Pattern/literal-mismatch-rejects"
]

(* === Nested compound: g[h[x_], y_] =============================== *)

VerificationTest[
    With[{bs = TMatchBindings @ TPatternMatch[
                  TLazyEncode[g[h[a], b]],
                  g[h[x_], y_]]},
        {TTermSame[bs[[1, Key[x]]], TLazyEncode[a]],
         TTermSame[bs[[1, Key[y]]], TLazyEncode[b]]}],
    {True, True},
    TestID -> "Pattern/nested-compound-bindings"
]

(* === HeadBlank: x_g matches only g-shaped TTerms =================== *)

VerificationTest[
    TMatchBindings @ TPatternMatch[TLazyEncode[g[a]], _g],
    {<||>},
    TestID -> "Pattern/HeadBlank-matches-registered-ctor"
]

VerificationTest[
    TPatternMatch[TLazyEncode[g[a]], _h],
    TMatchSum[],
    TestID -> "Pattern/HeadBlank-wrong-ctor"
]

(* === TMatchApply: substitute RHS per outcome ====================== *)

VerificationTest[
    Block[{m = TPatternMatch[TLazyEncode[g[a, b]], g[x_, y_]],
           outs},
        outs = TMatchApply[h[y, x], m];
        {Length[outs], TLazyDecode @ outs[[1]]}],
    {1, h[b, a]},
    TestID -> "Pattern/MatchApply-swap-args"
]

VerificationTest[
    Block[{m = TPatternMatch[TLazyEncode[g[a, b]], f[x_]]},
        TMatchApply[q[x], m]],
    {},
    TestID -> "Pattern/MatchApply-no-match-yields-empty"
]

VerificationTest[
    Block[{m = TPatternMatch[TLazyEncode[g[a]], g[x_]]},
        TLazyDecode @ TMatchApply[g[x, x], m][[1]]],
    g[a, a],
    TestID -> "Pattern/MatchApply-substitute-into-compound"
]

(* === TMatchParts: enumerate path-keyed captures =================== *)

VerificationTest[
    Block[{m = TPatternMatch[TLazyEncode[g[a, b]], g[x_, y_]],
           parts},
        parts = TMatchParts[m];
        Sort @ Keys @ parts[[1]]],
    Sort @ {{1}, {2}},
    TestID -> "Pattern/MatchParts-yields-leaf-paths"
]

VerificationTest[
    Block[{m = TPatternMatch[TLazyEncode[g[a, b]], g[x_, y_]],
           parts},
        parts = TMatchParts[m];
        TLazyDecode /@ {parts[[1, Key[{1}]]], parts[[1, Key[{2}]]]}],
    {a, b},
    TestID -> "Pattern/MatchParts-leaf-values-decode"
]
