(* ::Package:: *)
(* Pattern.wl - WL-style pattern matching against TTerm expressions.

   First slice (v0): single-valued matching plus first-position
   ReplaceAll-style rewriting.  Patterns are written as ordinary
   WL expressions on top of TTerm values:

     _              -- match any term, no binding
     x_             -- match any term, bind to x
     x_h            -- match if head matches `h` (Integer / Symbol)
     <lit_TTerm>    -- match by TTermSame (raw structural eq)
     <lit_Integer>  -- match if expr is NUM with that value
     head[args__]   -- match if expr is a CTR whose ctor label
                       resolves to `head` (via $lazyLabelSym /
                       symLabelFor) AND each arg recursively matches
                       the corresponding child slot

   Repeated binders (e.g. `f[x_, x_]`) are checked via TTermEq
   (cnf-reducing equality) so two bindings to the same value match
   even if they came from structurally-different IC representations.

   This file is the foundational piece: WL-side compilation of the
   pattern AST + recursive walk against the TTerm.  Multi-valued
   matching (Orderless, sequence patterns) and IC-native compilation
   to SUP-of-bindings streams are follow-ups -- the API surface
   (TPMatch, TPMatchQ, TPReplace, TPReplaceList) is shaped so we
   can plug those in without breaking callers. *)

BeginPackage["THVMLink`"];

TPMatch::usage      = "TPMatch[expr, pattern] tries to match the TTerm `expr` against the WL pattern `pattern`.  Returns an Association of binder-name -> TTerm on success, or Missing[\"NoMatch\", ...] otherwise.  Pattern syntax: `_` (any), `x_` (bind any), `x_h` (head-restricted), literal TTerm (TTermSame), literal Integer (NUM), `head[args__]` (CTR decomposition).";

TPMatchQ::usage     = "TPMatchQ[expr, pattern] returns True if TPMatch succeeds, False otherwise.";

TPReplace::usage    = "TPReplace[expr, pattern -> rhs] applies the rule once at the leftmost / topmost matching position in `expr` and returns the rewritten TTerm.  When no position matches, returns `expr` unchanged.  RHS is built by tlazyEncode after substituting bindings into the held WL expression.";

TPReplaceList::usage = "TPReplaceList[expr, pattern -> rhs] returns the List of all single-position rewrites of `expr` under the rule (one entry per matching position).  Each entry is a TTerm.  Useful for ATP-style enumeration where every redex matters.";

(* RHS substitution helper -- exposed so callers can inspect a
   match's bindings substituted into a held WL template without
   re-running the matcher.  Returns a TTerm. *)
TPSubstitute::usage = "TPSubstitute[bindings, rhs] builds a TTerm by replacing every binder reference in the held WL `rhs` with the corresponding bound TTerm from `bindings` (an Association), then encoding the result via tlazyEncode.";

(* Forward refs to private symbols owned by sibling files. *)
{TLazyEncode, TSubexprAt, TTermSubexprs, TTermSame, TTermEq};

Begin["`Private`"];

(* === pattern-AST classifiers ===

   We dispatch on Head[pat] to decide the matcher rule.  HoldFirst
   on TPMatch (and HoldRest on TPReplace) keep `_`, `x_`, etc.
   from being interpreted by WL itself before we get to look at
   them. *)

(* Head-restricted blanks: Blank[h] -- pattern `_h`, head h is a
   *Symbol* in WL semantics but here we let it be the corresponding
   CTR label (via symLabelFor) so `_Cons` matches a CTR with
   ctor-tag = $lazyLabelSym["Cons"]. *)
matchHead[t_TTerm, headSym_Symbol] := Block[{
    raw    = ttermRaw[t],
    tag    = TTermTag[t],
    wanted = Lookup[$lazySymLabel, ToString[Unevaluated[headSym]], None]
},
    Which[
        wanted === None,
            (* No registered ctor with that name -- we'd need the
               full WL Head test, which doesn't have a meaning at
               the IC level for non-CTR tags.  Conservative: only
               accept if the headSym matches the tag-name string. *)
            tag === Lookup[Association[Reverse /@ Normal[$tagNames]],
                           ToString[Unevaluated[headSym]],
                           -1],
        tag === $TagCTR && TTermExt[t] === wanted,
            True,
        True,
            False
    ]
]

(* Numeric literal on the pattern side -- match if expr is a NUM
   carrying the same value. *)
matchNumLit[t_TTerm, n_Integer] :=
    TTermTag[t] === $TagNUM && TTermVal[t] === n

(* Decompose a held WL pattern of the form Head[args__] into a CTR
   match: t must be a CTR whose ctor label is symLabelFor[Head]
   and whose data-children match args one-by-one (mod $LazyTuple
   for List literals).  Returns the merged binding Association
   on success or Missing["NoMatch"] otherwise. *)
matchCompound[t_TTerm, head_Symbol, args_List, env_Association] := Block[{
    label = Lookup[$lazySymLabel,
                   ToString[Unevaluated[head]],
                   None],
    n
},
    If[ label === None,                           Return @ Missing["NoMatch", "unknown-ctor", head]];
    If[ TTermTag[t]  =!= $TagCTR,                 Return @ Missing["NoMatch", "not-CTR", t]];
    If[ TTermExt[t]  =!= label,                   Return @ Missing["NoMatch", "ctor-tag", head]];
    n = $termCtrNFn[ttermRaw[t]];
    If[ n =!= Length[args],                       Return @ Missing["NoMatch", "arity", head]];
    matchAll[
        Table[TTerm[$termCtrAtFn[ttermRaw[t], i]], {i, 0, n - 1}],
        args, env]
]

(* List pattern -- the LHS shape is a literal List.  In Lazy.wl we
   encode lists as Tuple-CTRs ($LazyTuple); accept both Tuple and
   Cons-list shapes by walking heads here.  For Tuple-CTR the
   semantics is identical to a Head-keyed compound, just with
   $LazyTuple as the label. *)
matchListPattern[t_TTerm, args_List, env_Association] := Block[{
    raw = ttermRaw[t], tag, ext, n
},
    tag = TTermTag[t]; ext = TTermExt[t];
    If[ tag =!= $TagCTR || ext =!= $LazyTuple,
        Return @ Missing["NoMatch", "expected-Tuple-CTR"]];
    n = $termCtrNFn[raw];
    If[ n =!= Length[args],
        Return @ Missing["NoMatch", "tuple-arity"]];
    matchAll[
        Table[TTerm[$termCtrAtFn[raw, i]], {i, 0, n - 1}],
        args, env]
]

(* Recursive matcher.  `t` is a TTerm; `pat` is a WL pattern (held
   from the public-API entry, then carried as a normal expression
   through internal recursion); `env` is an Association of
   accumulated bindings.  Returns a new Association on success or
   Missing[...] on failure.  No HoldRest -- the recursive callers
   already extract patterns from a held List via Part, and Part
   returns the held element verbatim. *)

matchOne[t_TTerm, Verbatim[Blank][], env_Association] :=
    env

matchOne[t_TTerm, Verbatim[Blank][headSym_], env_Association] :=
    If[matchHead[t, headSym], env, Missing["NoMatch", "head", headSym]]

matchOne[t_TTerm, Verbatim[Pattern][name_Symbol, Verbatim[Blank][]], env_Association] :=
    Module[{prior = Lookup[env, name, $unbound]},
        Which[
            prior === $unbound,            Append[env, name -> t],
            TTermEq[prior, t],             env,
            True,                          Missing["NoMatch", "rebind-mismatch", name]
        ]
    ]

matchOne[t_TTerm, Verbatim[Pattern][nm_Symbol,
                                    Verbatim[Blank][headSym_Symbol]], env_Association] :=
    If[ matchHead[t, headSym],
        (* Inner recurse via With so `nm` is materialised before
           Pattern[...] rebuilds the inner WL pattern. *)
        With[{n = nm}, matchOne[t, n_, env]],
        Missing["NoMatch", "head", headSym]]

matchOne[t_TTerm, lit_TTerm, env_Association] :=
    If[TTermSame[t, lit], env, Missing["NoMatch", "literal-TTerm"]]

matchOne[t_TTerm, n_Integer, env_Association] :=
    If[matchNumLit[t, n], env, Missing["NoMatch", "literal-int", n]]

matchOne[t_TTerm, l_List, env_Association] :=
    matchListPattern[t, l, env]

(* Atomic symbol pattern (e.g. `a` standing for the 0-ary CTR
   `a`).  Treat as compound with empty args -- matchCompound
   verifies expr is a CTR with ctor-tag = symLabelFor[a]. *)
matchOne[t_TTerm, sym_Symbol, env_Association] :=
    matchCompound[t, sym, {}, env]

matchOne[t_TTerm, expr_, env_Association] :=
    With[{h = Head[Unevaluated[expr]]},
        If[ Head[h] === Symbol,
            matchCompound[t, h, List @@ Unevaluated[expr], env],
            Missing["NoMatch", "unsupported-pattern-head", h]
        ]
    ]

(* Match a list of subterms against a list of patterns position-by-
   position, threading the binding env through.  First failure
   short-circuits.  Lengths assumed equal. *)
matchAll[ts_List, ps_List, env0_Association] := Module[{env = env0, r},
    Catch[
        Do[
            r = matchOne[ts[[i]], ps[[i]], env];
            If[ MissingQ[r], Throw[r]];
            env = r,
            {i, Length[ts]}
        ];
        env
    ]
]

(* === public API ================================================== *)

SetAttributes[TPMatch, HoldRest]

TPMatch[t_TTerm, pat_] := matchOne[t, pat, <||>]

SetAttributes[TPMatchQ, HoldRest]

TPMatchQ[t_TTerm, pat_] := !MissingQ[TPMatch[t, pat]]

(* === RHS substitution + replacement ============================== *)

(* Recursively walk a held WL expression, replacing any pattern
   binder reference with its bound value (looked up in `env`).
   Returns a WL expression suitable for tlazyEncode.  If a binder
   is missing from the env, leaves the symbol verbatim -- callers
   can detect this by checking the return for un-substituted
   names before encoding. *)
SetAttributes[applyEnv, HoldRest]
applyEnv[env_Association, sym_Symbol] := Lookup[env, sym, sym]
applyEnv[env_Association, e_]         :=
    With[{h = Head[Unevaluated[e]]},
        Which[
            (* Literal types pass through. *)
            IntegerQ[e] || NumericQ[e] || Head[e] === TTerm, e,
            ListQ[e],
                applyEnv[env, #] & /@ e,
            Head[h] === Symbol,
                Apply[h, applyEnv[env, #] & /@ List @@ Unevaluated[e]],
            True, e
        ]
    ]

SetAttributes[TPSubstitute, HoldRest]
TPSubstitute[env_Association, rhs_] := tlazyEncode[applyEnv[env, rhs]]

(* TPReplace[expr, pat -> rhs]: leftmost top-down match-and-replace.
   Walks every position via TTermSubexprs; on the first matching
   position, substitutes via TPSubstitute and returns the rebuilt
   TTerm.  When no position matches, returns `expr` unchanged.

   "Rebuild" semantics: we don't yet have a generic
   TermReplaceAtPath helper (rebuilding a CTR with one child
   swapped for another).  For the v0 slice the API replaces only
   at the *root* path -- i.e., TPReplace acts like a top-level
   rewrite, not a deep one.  Deep replacement waits on a
   tterm-rebuild primitive. *)
SetAttributes[TPReplace, HoldRest]
TPReplace[t_TTerm, Verbatim[Rule][pat_, rhs_]] := Block[{m},
    m = TPMatch[t, pat];
    If[ MissingQ[m], t,
        TPSubstitute[m, rhs]
    ]
]
TPReplace[t_TTerm, Verbatim[RuleDelayed][pat_, rhs_]] := TPReplace[t, Rule[pat, rhs]]

(* TPReplaceList: enumerate every (path -> subterm) pair in the
   expression and try the rule against each subterm.  Returns the
   List of substituted bindings (as TTerms) -- one per matching
   position.  No reconstruction; callers compose via TSubexprAt /
   path-aware rebuild later. *)
SetAttributes[TPReplaceList, HoldRest]
TPReplaceList[t_TTerm, Verbatim[Rule][pat_, rhs_]] := Module[{
    pairs = TTermSubexprs[t], hits = {}, m
},
    Do[
        m = TPMatch[Last[pair], pat];
        If[ !MissingQ[m],
            AppendTo[hits, First[pair] -> TPSubstitute[m, rhs]]
        ],
        {pair, pairs}
    ];
    hits
]
TPReplaceList[t_TTerm, Verbatim[RuleDelayed][pat_, rhs_]] :=
    TPReplaceList[t, Rule[pat, rhs]]

End[];

EndPackage[];
