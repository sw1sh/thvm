(* ::Package:: *)
(* Pattern.wl - Match-object pattern matching against TTerm values.

   API mirrors Wolfram`Patterns` (which itself uses Wolfram`Lazy`):
   the matcher returns a structured Match object instead of an eager
   binding Association.  Match objects are HoldComplete-friendly and
   compose with sum / product algebra so multi-valued matching
   (Orderless, sequence patterns) and lazy enumeration are natural
   extensions on the same shape.

   Match object grammar (heads with Hold attributes):

       TMatch[innerMatch]
       TMatchSum    [m_1, m_2, ...]   - alternatives (lazy OR)
       TMatchProduct[m_1, m_2, ...]   - conjunction  (lazy AND)
       TMatchPart   [part, HoldPattern[p], submatch]
       TMatchValues [v_1, v_2, ...]   - leaves (held)

   `part` is a List of integer offsets into the heap layout (root =
   {}), matching the convention used by TTermSubexprs / TSubexprAt.
   `p` is the held WL pattern that matched at that part; `submatch`
   is the recursive Match for that subterm's children.

   Yields eager Match trees for single-valued patterns: blank, named
   blank `x_`, head-restricted `x_h`, integer literal, TTerm literal,
   atomic-symbol CTR, compound CTR `head[args__]`, Tuple-CTR
   `{args__}`.  Repeat binders (`f[x_, x_]`) are validated by
   TMatchBindings via TTermEq when the same name appears multiple
   times: mismatches drop that branch. *)

BeginPackage["THVMLink`"];

GeneralUtilities`SetUsage[TMatch, "TMatch[m$] is the canonical outer container wrapping a Match expression, as returned by TPatternMatch."];
GeneralUtilities`SetUsage[TMatchSum, "TMatchSum[m$1, m$2, $$] represents alternative matches (lazy OR).
Empty TMatchSum[] means no match; SequenceHold and Flat so nested unions auto-flatten."];
GeneralUtilities`SetUsage[TMatchProduct, "TMatchProduct[m$1, m$2, $$] represents the conjunction of sub-matches (head plus each argument).
Empty TMatchProduct[] is a vacuously-true match producing the empty bindings."];
GeneralUtilities`SetUsage[TMatchPart, "TMatchPart[part$, HoldPattern[p$], submatch$] tags submatch$ as having matched the held WL pattern p$ at heap position part$ (a List of integer offsets)."];
GeneralUtilities`SetUsage[TMatchValues, "TMatchValues[v$1, v$2, $$] holds a sequence of leaf TTerms or held values; the leaf-level Match.
SequenceHold and Flat."];
GeneralUtilities`SetUsage[TMatchObjectQ, "TMatchObjectQ[expr$] returns True if expr$ is one of TMatch, TMatchSum, TMatchProduct, TMatchPart, TMatchValues."];
GeneralUtilities`SetUsage[TPatternMatch, "TPatternMatch[expr$, pattern$] matches the TTerm expr$ against the held WL pattern pattern$, returning a TMatch object on success or TMatchSum[] on no match.
See TMatchBindings, TMatchApply, TMatchParts for accessors."];
GeneralUtilities`SetUsage[TMatchBindings, "TMatchBindings[match$] returns a List of binding-Associations (one per outcome), each mapping binder names to TTerm values; repeat-binder consistency is enforced via TTermEq."];
GeneralUtilities`SetUsage[TMatchApply, "TMatchApply[rhs$, match$] returns a List of TTerms produced by substituting each outcome's bindings into the held WL rhs$ template."];
GeneralUtilities`SetUsage[TMatchParts, "TMatchParts[match$] returns a List of Associations mapping each path (List of integer offsets) to the TTerm captured at that position, for path-keyed inspection of where each subterm came from."];

(* Forward refs to private symbols owned by sibling files. *)
{ToTTerm, TSubexprAt, TTermSubexprs, TTermSame, TTermEq};

Begin["`Private`"];

(* === Match-object scaffolding =====================================
   SequenceHold + Flat keep the algebra clean: nested
   TMatchSum[TMatchSum[...]] auto-flattens, TMatchValues sequences
   concatenate, etc.  HoldAllComplete on the leaf containers means
   user-side patterns and bound TTerms don't get re-evaluated when
   the match tree is reshuffled. *)

SetAttributes[TMatchSum,     {Flat, OneIdentity}]
SetAttributes[TMatchProduct, {Flat, OneIdentity}]
SetAttributes[TMatchValues,  {Flat, OneIdentity}]

(* TMatchObjectQ - shape predicate. *)
TMatchObjectQ[expr_] :=
    MatchQ[expr, _TMatch | _TMatchSum | _TMatchProduct | _TMatchPart | _TMatchValues]

(* === pattern dispatch ============================================
   buildMatch[t_TTerm, pat] returns a Match object.  The matcher
   wraps every successful sub-match in a TMatchPart so callers can
   later replay paths / patterns / submatches without re-walking. *)

(* Numeric literal pattern - expr must be NUM with that value. *)
buildMatch[t_TTerm, n_Integer] :=
    If[ TTermTag[t] === $TagNUM && TTermVal[t] === n,
        TMatchPart[{}, HoldPattern[n], TMatchValues[t]],
        TMatchSum[]
    ]

(* TTerm literal pattern - exact structural equality. *)
buildMatch[t_TTerm, lit_TTerm] :=
    If[ TTermSame[t, lit],
        TMatchPart[{}, HoldPattern[lit], TMatchValues[t]],
        TMatchSum[]
    ]

(* Blank: matches anything; no name.  We still wrap in TMatchPart
   so consumers see the held pattern. *)
buildMatch[t_TTerm, p : Verbatim[Blank][]] :=
    TMatchPart[{}, HoldPattern[p], TMatchValues[t]]

(* Head-restricted Blank.  Only accept if t's head matches headSym
   (registered ctor or tag-name string match). *)
buildMatch[t_TTerm, p : Verbatim[Blank][headSym_Symbol]] :=
    If[ matchHead[t, headSym],
        TMatchPart[{}, HoldPattern[p], TMatchValues[t]],
        TMatchSum[]
    ]

(* Named blank: x_ - TMatchPart with HoldPattern[name_]. *)
buildMatch[t_TTerm, p : Verbatim[Pattern][name_Symbol, Verbatim[Blank][]]] :=
    TMatchPart[{}, HoldPattern[p], TMatchValues[t]]

(* Named head-restricted: x_h.  Two-step: head-check, then label. *)
buildMatch[t_TTerm, p : Verbatim[Pattern][name_Symbol, Verbatim[Blank][headSym_Symbol]]] :=
    If[ matchHead[t, headSym],
        TMatchPart[{}, HoldPattern[p], TMatchValues[t]],
        TMatchSum[]
    ]

(* List literal as pattern - treat as Tuple-CTR (Lazy.wl shape). *)
buildMatch[t_TTerm, l_List] :=
    matchTupleCtr[t, l]

(* Atomic Symbol pattern (e.g. `a` -> 0-ary CTR with ctor "a"). *)
buildMatch[t_TTerm, sym_Symbol] :=
    matchCompound[t, sym, {}]

(* Compound symbolic pattern: head[arg1, arg2, ...]. *)
buildMatch[t_TTerm, expr_] :=
    With[{h = Head[Unevaluated[expr]]},
        If[ MatchQ[h, _Symbol],
            matchCompound[t, h, List @@ Unevaluated[expr]],
            TMatchSum[]
        ]
    ]

(* === head / ctor checks ========================================== *)

matchHead[t_TTerm, headSym_Symbol] := Block[{
    tag    = TTermTag[t],
    label  = Lookup[$lazySymLabel, ToString[Unevaluated[headSym]], None],
    tagInv
},
    Which[
        tag === $TagCTR && label =!= None && TTermExt[t] === label,
            True,
        (* Fallback: tag-name string match for non-CTR tags
           (e.g. `_NUM`, `_LAM`, ...). *)
        True,
            tagInv = Association[Reverse /@ Normal[$tagNames]];
            tag === Lookup[tagInv, ToString[Unevaluated[headSym]], -1]
    ]
]

(* Compound CTR pattern matcher.  Builds a TMatchPart wrapping a
   TMatchProduct whose factors are the per-child sub-matches.  Each
   child match is shifted by Prepend[part, i+1] so consumers see
   absolute paths.  On any failure, returns TMatchSum[]. *)
matchCompound[t_TTerm, head_Symbol, args_List] := Block[{
    label = Lookup[$lazySymLabel, ToString[Unevaluated[head]], None],
    raw, n, childMatches, anyFail
},
    If[ label === None || TTermTag[t] =!= $TagCTR || TTermExt[t] =!= label,
        Return @ TMatchSum[]
    ];
    raw = ttermRaw[t];
    n   = $termCtrNFn[raw];
    If[ n =!= Length[args], Return @ TMatchSum[] ];
    anyFail      = False;
    childMatches = Table[
        Module[{cm = buildMatch[TTerm[$termCtrAtFn[raw, i]], args[[i + 1]]]},
            If[ cm === TMatchSum[], anyFail = True];
            shiftPath[cm, {i + 1}]
        ],
        {i, 0, n - 1}];
    If[ anyFail, Return @ TMatchSum[] ];
    TMatchPart[{},
        HoldPattern[head @@ args],
        TMatchProduct @@ childMatches]
]

(* List-pattern variant - expr must be a Tuple-CTR ($LazyTuple)
   with same arity, then per-arg matching. *)
matchTupleCtr[t_TTerm, args_List] := Block[{
    raw, n, childMatches, anyFail
},
    If[ TTermTag[t] =!= $TagCTR || TTermExt[t] =!= $LazyTuple,
        Return @ TMatchSum[]
    ];
    raw = ttermRaw[t];
    n   = $termCtrNFn[raw];
    If[ n =!= Length[args], Return @ TMatchSum[] ];
    anyFail      = False;
    childMatches = Table[
        Module[{cm = buildMatch[TTerm[$termCtrAtFn[raw, i]], args[[i + 1]]]},
            If[ cm === TMatchSum[], anyFail = True];
            shiftPath[cm, {i + 1}]
        ],
        {i, 0, n - 1}];
    If[ anyFail, Return @ TMatchSum[] ];
    TMatchPart[{},
        HoldPattern[args],
        TMatchProduct @@ childMatches]
]

(* shiftPath: prepend prefix to the part of every TMatchPart at the
   top level of a Match tree.  Empty TMatchSum[] passes through. *)
shiftPath[TMatchPart[part_, hp_, sub_], prefix_List] :=
    TMatchPart[Join[prefix, part], hp, sub]
shiftPath[m_TMatchSum, _]     := m
shiftPath[m_TMatchProduct, _] := m
shiftPath[m_TMatchValues, _]  := m
shiftPath[m_, _]              := m

(* === public entry ================================================ *)

SetAttributes[TPatternMatch, HoldRest]

TPatternMatch[t_TTerm, pat_] := With[{m = buildMatch[t, pat]},
    If[ m === TMatchSum[], TMatchSum[], TMatch[m]]
]

(* === Match-object accessors ====================================== *)

(* TMatchBindings - enumerate every outcome's name -> TTerm
   bindings.  Single-valued outcomes return a one-element List;
   TMatchSum branches expand into separate entries.  Repeat-binder
   consistency: when the same Pattern[name, ...] appears more than
   once, the recorded values must TTermEq each other; otherwise the
   outcome is dropped. *)

TMatchBindings[TMatch[m_]] := TMatchBindings[m]

TMatchBindings[TMatchSum[ms___]] := Catenate[TMatchBindings /@ {ms}]

TMatchBindings[TMatchProduct[ms___]] := Module[{combos},
    combos = Tuples[TMatchBindings /@ {ms}];
    Select[
        mergeBindings /@ combos,
        # =!= $bindingsConflict &
    ]
]

(* TMatchPart bindings: when the held pattern is `name_` or `name_h`,
   record `name -> v` from the submatch's leaf TMatchValues; otherwise
   recurse into the submatch.  We drop the `_HoldPattern` type check
   in the LHS because it interacts oddly with HoldPattern's
   matcher-special status. *)
TMatchBindings[TMatchPart[_, hp_, sub_]] :=
    Module[{name = bindingNameOf[hp]},
        If[ name === None,
            TMatchBindings[sub],
            With[{leaf = leafValueOf[sub]},
                If[ leaf === None,
                    TMatchBindings[sub],
                    {<|name -> leaf|>}
                ]
            ]
        ]
    ]

TMatchBindings[TMatchValues[___]] := {<||>}

TMatchBindings[_] := {}

(* Extract the binder name from a held pattern.  Returns None when
   the pattern isn't of the form `name_` / `name_h`.  HoldPattern
   is a pattern-matcher head with special semantics in LHS, so we
   wrap it with Verbatim to match it literally; same for the inner
   Pattern / Blank. *)
bindingNameOf[Verbatim[HoldPattern][
        Verbatim[Pattern][n_Symbol, Verbatim[Blank][___]]]] := n
bindingNameOf[_] := None

(* First leaf TTerm of a submatch tree (used by TMatchBindings to
   pair a binder name with its captured value).  Returns None when
   the submatch is empty / non-leaf-bearing. *)
leafValueOf[TMatchValues[v_, ___]]              := v
leafValueOf[TMatchPart[_, _, sub_]]              := leafValueOf[sub]
leafValueOf[TMatchProduct[m_, ___]]              := leafValueOf[m]
leafValueOf[TMatchSum[m_, ___]]                  := leafValueOf[m]
leafValueOf[_]                                   := None

(* Internal: merge a List of binding Associations into one,
   enforcing TTermEq for repeated names.  Returns
   $bindingsConflict on conflict. *)
mergeBindings[bs_List] := Module[{out = <||>, ok = True},
    Scan[
        a |-> KeyValueMap[
            {k, v} |-> If[ KeyExistsQ[out, k],
                If[ !TTermEq[out[k], v], ok = False];,
                AssociateTo[out, k -> v]
            ],
            a
        ],
        bs
    ];
    If[ok, out, $bindingsConflict]
]

(* TMatchApply - substitute bindings into a held RHS template per
   outcome, encode result via tlazyEncode.  Returns a List of TTerms.
   When no outcome has consistent bindings, returns {}. *)

SetAttributes[TMatchApply, HoldFirst]

TMatchApply[rhs_, match_] := tlazyEncode[applyEnv[#, rhs]] & /@
    TMatchBindings[match]

(* Recursive substitution helper.  Pattern binders get replaced
   with their bound TTerms; everything else passes through. *)
SetAttributes[applyEnv, HoldRest]
applyEnv[env_Association, sym_Symbol] := Lookup[env, sym, sym]
applyEnv[env_Association, e_] :=
    With[{h = Head[Unevaluated[e]]},
        Which[
            IntegerQ[e] || NumericQ[e] || MatchQ[e, _TTerm], e,
            ListQ[e],          applyEnv[env, #] & /@ e,
            MatchQ[h, _Symbol],
                Apply[h, applyEnv[env, #] & /@ List @@ Unevaluated[e]],
            True, e
        ]
    ]

(* TMatchParts - enumerate (path -> TTerm) Associations per
   outcome.  Useful for path-keyed inspection. *)

TMatchParts[TMatch[m_]] := TMatchParts[m]

TMatchParts[TMatchSum[ms___]] := Catenate[TMatchParts /@ {ms}]

TMatchParts[TMatchProduct[ms___]] := Module[{combos},
    combos = Tuples[TMatchParts /@ {ms}];
    mergePartsAll /@ combos
]

TMatchParts[TMatchPart[part_, _, sub_]] :=
    Map[partsPrepend[part, #] &, TMatchParts[sub]]

TMatchParts[TMatchValues[v_, ___]] := {<|{} -> v|>}

TMatchParts[_] := {<||>}

partsPrepend[prefix_List, a_Association] :=
    KeyMap[Join[prefix, #] &, a]

mergePartsAll[as_List] := Module[{out = <||>},
    Scan[(out = Join[out, #]) &, as];
    out
]

End[];

EndPackage[];
