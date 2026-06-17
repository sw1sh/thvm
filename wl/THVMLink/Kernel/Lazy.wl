(* ::Package:: *)
(* Lazy.wl - lazy streams whose laziness comes from the IC reducer.

   `TLazyRange[a, b, step]` registers a recursive `TDef` body:

       lazyRange = lambda a step k.
           TIfZero[k, Nil, Cons[a, lazyRange (a+step) step (k-1)]]

   and returns `TApp[TApp[TApp[TRef[lazyRange], a], step], k]`.  The
   call itself does no enumeration: it is one APP cell and three NUM
   args.  Forcing the head via `TWnf` fires APP-REF + APP-LAM + MAT
   + OP2 just enough to expose ONE Cons cell whose tail is another
   unfired APP-APP-APP-REF redex.  Walking the tail forces the next
   layer.  This is the canonical HVM lazy-stream encoding -- cf.
   `@X = &L{#Z, #S{@X}}` in TinyHVM/HVM4/test/collapse_9.hvm and
   the SGD recursion in Optim.wl.

   `TLazyPermutations`, `TLazySplits`, `TLazyTuples`, `TLazySubsets`
   currently build the SUP-stream eagerly, one Tuple-CTR per branch.
   This is the right shape (HVM `&L{p_0, &L{p_1, ...}}`) for
   APP-SUP optimal sharing under pattern matching, but construction
   cost is O(|stream|).  These generators are intended for the small
   inputs that pattern matching produces (BlankSequence over an
   n-arg flat CTR, OrderlessPatternSequence over n args).  A
   TDef-based lazy encoding of the next-permutation function is
   future work; until then prefer `TLazyRange` for big-N sequences.

   Element encoding: Integer -> TAG_NUM(i32); Symbol -> 0-ary CTR
   (label cached per session, allocated above the reserved band);
   List -> Tuple-CTR.  TTerm passes through.

   Consumer combinators (TLazyFirst / TLazyRest / TLazyTake /
   TLazyToList / TLazyMap / TLazyFold) walk the heap via `TWnf` to
   force one layer at a time.  TLazyMap / TLazyFold force the full
   stream; use carefully on TLazyRange[bigN]. *)

BeginPackage["WolframInstitute`THVMLink`"];

GeneralUtilities`SetUsage[TLazyRange, "TLazyRange[n$] streams 1 to n$ as a lazy TTerm.
TLazyRange[a$, b$] and TLazyRange[a$, b$, step$] stream a$ to b$, optionally by step$.
The result is an APP-REF chain to a recursive lazyRange TDef; forcing one element via TWnf exposes the next Cons while the rest stays unforced. TLazyRange[10^6] allocates O(1) cells at construction."];
GeneralUtilities`SetUsage[TLazyPermutations, "TLazyPermutations[xs$] returns a TTerm lazily enumerating Permutations[xs$] in lex order.
It is backed by a recursive permsLex TDef that consumes one outer Cons per fired interaction. TLazyTake[TLazyPermutations[Range[20]], 5] allocates O(1) at construction."];
GeneralUtilities`SetUsage[TLazySplits, "TLazySplits[xs$, n$] returns a TTerm lazily enumerating the ordered n$-way splits of xs$ (parts may be empty).
TLazySplits[xs$] defaults to n$ = 2."];
GeneralUtilities`SetUsage[TLazyTuples, "TLazyTuples[{xs$1, xs$2, $$}] returns a TTerm lazily enumerating the cross product Tuples[{xs$1, xs$2, $$}]."];
GeneralUtilities`SetUsage[TLazySubsets, "TLazySubsets[xs$] returns a TTerm lazily enumerating Subsets[xs$], sorted by cardinality then lex order."];

GeneralUtilities`SetUsage[TLazyFirst, "TLazyFirst[s$] forces the head of stream s$ and returns the decoded WL value.
Returns Missing[\"EmptyStream\"] when the head reduces to Nil or ERA."];
GeneralUtilities`SetUsage[TLazyRest, "TLazyRest[s$] forces the head of stream s$ and returns the still-unforced tail TTerm."];
GeneralUtilities`SetUsage[TLazyTake, "TLazyTake[s$, n$] returns a TTerm holding the first n$ elements of stream s$ as a lazy Cons chain.
The result stays an unforced APP-APP-REF redex until something walks it; the elements are produced one at a time by IC reduction. Use TLazyToList[TLazyTake[s$, n$]] to force into a WL List."];
GeneralUtilities`SetUsage[TLazyToList, "TLazyToList[s$] forces every element of stream s$ and returns a WL List. Hangs if the stream is infinite."];
GeneralUtilities`SetUsage[TLazyMap, "TLazyMap[f$, s$] returns a lazy Cons-stream whose i-th element is f$ applied to the i-th element of stream s$.
f$ is a TTerm (typically a TLam); forcing one element of the result fires exactly the interactions for that step. A plain WL function f$ forces s$ and maps host-side instead."];
GeneralUtilities`SetUsage[TLazySelect, "TLazySelect[s$, p$] returns a Cons-stream of those elements h$ of stream s$ for which TApp[p$, h$] reduces to a non-zero NUM. p$ is a TTerm predicate."];
GeneralUtilities`SetUsage[TLazySelectFirst, "TLazySelectFirst[s$, p$] forces stream s$ one Cons at a time and returns the first head whose TApp[p$, h$] reduces to a non-zero NUM, decoded via FromTTerm.
Returns Missing[\"NotFound\"] on stream end."];
GeneralUtilities`SetUsage[TLazyCatenate, "TLazyCatenate[ss$] flattens a Cons-stream of Cons-streams ss$ into a single stream."];
GeneralUtilities`SetUsage[TLazyCases, "TLazyCases[s$, pattern$] forces stream s$ and returns a WL List of {element-TTerm, bindings} for each Cons head matching the held WL pattern$.
bindings is an Association of binder name to TTerm. Input stays lazy; output is eager."];
GeneralUtilities`SetUsage[TLazyChoice, "TLazyChoice[xs$] returns a fresh SUP-stream over the encoded elements of list xs$: ERA on the empty list, the bare element on a singleton, nested SUPs otherwise.
All branches share a label so downstream DUP-SUP annihilations are clean."];
GeneralUtilities`SetUsage[TLazyFold, "TLazyFold[f$, x$, s$] is Fold[f$, x$, TLazyToList[s$]], forcing the full stream s$."];

(* WL <-> TTerm coercion lives in Expr.wl as ToTTerm / FromTTerm.
   The private workhorses tlazyEncode / tlazyDecode stay here so
   the lazy combinators below can keep using them directly. *)

(* Forward-declare symbols owned by sibling files (Ref.wl, Switch.wl)
   so bare references inside Begin[`Private`] resolve to WolframInstitute`THVMLink`X
   instead of phantom WolframInstitute`THVMLink`Private`X.  Lazy.wl is parsed before
   Ref.wl and Switch.wl in alphabetical order, so the public names
   don't yet exist when this file's body runs. *)
{TDef, TRef, TIfZero, TOp2, TNum, TMatCtr, TPatternMatch, TMatchBindings,
 TMatch, TMatchSum, TMatchProduct, TMatchPart, TMatchValues,
 ToTTerm, FromTTerm, TFreshLabel, TSup, TEra};

Begin["`Private`"];

(* === reserved CTR labels ===
   Above 1000 to keep clear of ATP / hand-rolled CTRs that start
   their numbering from 0/1.  Symbol labels start at 10001. *)
$LazyCons  = 1001
$LazyNil   = 1002
$LazyTuple = 1003

(* === per-session symbol label table === *)
$lazyNextSymLabel = 10001
$lazySymLabel     = <||>
$lazyLabelSym     = <||>

freshSymLabel[name_String] := Block[{lab = $lazyNextSymLabel},
    $lazySymLabel[name] = lab;
    $lazyLabelSym[lab]  = name;
    $lazyNextSymLabel = lab + 1;
    lab
]

symLabelFor[name_String] :=
    Lookup[$lazySymLabel, name, freshSymLabel[name]]

$termNewCtrFn := $termNewCtrFn = load["thvm_wl_term_new_ctr",
    {Integer, {Integer, 1}}, Integer]

(* === encode === *)

tlazyEncode[t_TTerm]   := t
tlazyEncode[n_Integer] := (ensureInit[];
    TTerm[$termNewFn[0, $TagNUM, $DTInt32, n]]
)
tlazyEncode[s_Symbol]  := (ensureInit[];
    With[{lab = symLabelFor[ToString[Unevaluated[s]]]},
        TTerm[$termNewCtrFn[lab, {}]]
    ]
)
tlazyEncode[l_List]    := (ensureInit[];
    With[{children = ttermRaw /@ (tlazyEncode /@ l)},
        TTerm[$termNewCtrFn[$LazyTuple, children]]
    ]
)
(* Compound symbolic head: f[a_1, ..., a_n] -> CTR labelled by `f`
   with the encoded args as children.  Symbol-ctor labels live
   above the reserved band ($lazyNextSymLabel >= 10001) so they
   don't clash with the special $LazyCons / $LazyNil / $LazyTuple
   slots.  A 0-ary symbol falls through to the `tlazyEncode[s_Symbol]`
   rule above; this clause covers the n>=1 case. *)
tlazyEncode[expr_] /; ! MatchQ[expr, _TTerm] && Head[Head[Unevaluated[expr]]] === Symbol :=
    (ensureInit[];
     With[{lab = symLabelFor[ToString[Head[Unevaluated[expr]]]],
           children = ttermRaw /@ (tlazyEncode /@ List @@ Unevaluated[expr])},
        TTerm[$termNewCtrFn[lab, children]]
     ])

(* tlazyEncode itself stays a private workhorse; the public
   coercion ToTTerm[v] in Expr.wl forwards to it. *)

(* === decode === *)

tlazyDecode[t_TTerm]   := tlazyDecodeRaw[ttermRaw[t]]
tlazyDecode[t_Integer] := tlazyDecodeRaw[t]

tlazyDecodeRaw[raw_Integer] := With[{tag = $termTagFn[raw]},
    Switch[ tag,
        $TagNUM, $termValFn[raw],
        $TagERA, Missing["EmptyStream"],
        $TagCTR, tlazyDecodeCtr[raw],
        _,
            (* TCnf (Levy-optimal cnf readback) so DP-rooted Cons cells
               from auto-dup'd recursive bodies fire their DUP-XXX at
               readback and resolve to a CTR head. *)
            With[{forced = ttermRaw[TCnf[TTerm[raw]]]},
                If[ forced === raw,
                    TTerm[raw],
                    tlazyDecodeRaw[forced]
                ]
            ]
    ]
]

tlazyDecodeCtr[raw_Integer] := Block[{
    label    = $termExtFn[raw],
    n        = $termCtrNFn[raw],
    children
},
    children = Table[$termCtrAtFn[raw, i], {i, 0, n - 1}];
    Which[
        label === $LazyNil,
            {},
        label === $LazyCons,
            tlazyConsToList[raw],
        label === $LazyTuple,
            tlazyDecodeRaw /@ children,
        KeyExistsQ[$lazyLabelSym, label],
            With[{name = $lazyLabelSym[label]},
                If[ n === 0,
                    Symbol[name],
                    Symbol[name] @@ (tlazyDecodeRaw /@ children)
                ]
            ],
        True,
            CTR @@ Prepend[tlazyDecodeRaw /@ children, label]
    ]
]

(* Cons-list spine walk: at each step, force the cell via TWnf if
   it's not already a CTR/ERA in WHNF (TDef-driven streams expose
   unforced ALO/APP redexes between Cons cells).  Each element is
   decoded via tlazyDecodeRaw, which auto-forces. *)
tlazyConsToList[raw_Integer] := Block[{
    out = {}, cur = raw, tag, label, forced
},
    While[ True,
        tag = $termTagFn[cur];
        If[ tag =!= $TagCTR && tag =!= $TagERA,
            forced = ttermRaw[TCnf[TTerm[cur]]];
            If[ forced === cur, Break[]];
            cur = forced;
            tag = $termTagFn[cur]
        ];
        If[ tag =!= $TagCTR, Break[]];
        label = $termExtFn[cur];
        Which[
            label === $LazyNil,
                Break[],
            label === $LazyCons,
                AppendTo[out, tlazyDecodeRaw[$termCtrAtFn[cur, 0]]];
                cur = $termCtrAtFn[cur, 1],
            True,
                Break[]
        ]
    ];
    out
]

(* tlazyDecode stays a private workhorse; FromTTerm forwards. *)

(* === heap cell builders === *)

ctrCell[label_Integer, children_List] := (ensureInit[];
    TTerm[$termNewCtrFn[label, ttermRaw /@ children]]
)

nilTerm[]            := ctrCell[$LazyNil,  {}]
consTerm[h_, t_]     := ctrCell[$LazyCons, {h, t}]
tupleTerm[xs_List]   := ctrCell[$LazyTuple, tlazyEncode /@ xs]

(* SUP-stream right-fold: &L{x_0, &L{x_1, ..., &L{x_{n-1}, ERA}}}.
   Same label across all branches so a downstream DUP same-label
   collapses pairwise.  Used by the eager combinatorial generators. *)
supStream[branches_List] /; Length[branches] === 0 := TEra[]
supStream[branches_List] := Block[{label = TFreshLabel[]},
    Fold[
        TSup[label, #2, #1] &,
        TEra[],
        Reverse[branches]
    ]
]

(* === IC-native lazy range ===

   Body, registered under a stable name.  TDef snapshots the body
   into the immutable book heap; subsequent dynamic-heap mutations
   (and TReset) don't disturb it.  TDef is idempotent on the same
   name -- re-registering on every TLazyRange call is cheap and
   keeps the def alive across hypothetical TFree/TInit cycles. *)

$LazyRangeDefName = "$THVMLink__lazyRange"

registerLazyRangeDef[] := Module[{aVar, stepVar, kVar},
    TDef[$LazyRangeDefName,
        TLam[aVar, TLam[stepVar, TLam[kVar,
            TIfZero[ kVar,
                nilTerm[],
                consTerm[
                    aVar,
                    TApp[
                        TApp[
                            TApp[TRef[$LazyRangeDefName],
                                 TOp2["+", aVar, stepVar]],
                            stepVar],
                        TOp2["-", kVar, TNum[1]]]
                ]
            ]
        ]]]
    ]
]

(* Compute Length[Range[a, b, step]] without materializing it. *)
rangeCount[a_Integer, b_Integer, 0]    := 0
rangeCount[a_Integer, b_Integer, step_Integer] := If[ step > 0,
    Max[0, Quotient[b - a, step] + 1],
    Max[0, Quotient[a - b, -step] + 1]
]

TLazyRange[n_Integer]                    := TLazyRange[1, n, 1]
TLazyRange[a_Integer, b_Integer]         := TLazyRange[a, b, 1]
TLazyRange[a_Integer, b_Integer, step_Integer] := (
    ensureInit[];
    registerLazyRangeDef[];
    TApp[
        TApp[
            TApp[TRef[$LazyRangeDefName], TNum[a]],
            TNum[step]],
        TNum[rangeCount[a, b, step]]]
)

(* === IC-native lazy `take` ===

   take = lambda n s.
       TIfZero[n,
           Nil,
           APP[ MAT[$LazyCons,
                    lambda h. lambda t. Cons[h, take (n-1) t],
                    lambda _. Nil ],
                s ]]

   On force: TIfZero peels the n=0 base; otherwise the inner APP-MAT
   forces s's head to either Cons (matched -> bind h, t, return new
   Cons cell with the recursive take in the tail) or anything else
   (fallback returns Nil regardless of the arg).  The returned Cons
   is in WHNF; its tail stays as an unforced APP-APP-REF redex. *)

$LazyTakeDefName = "$THVMLink__lazyTake"

registerLazyTakeDef[] := Module[{nVar, sVar, hVar, tVar, ignVar},
    TDef[$LazyTakeDefName,
        TLam[nVar, TLam[sVar,
            TIfZero[ nVar,
                nilTerm[],
                TApp[
                    TMatCtr[$LazyCons,
                        TLam[hVar, TLam[tVar,
                            consTerm[
                                hVar,
                                TApp[
                                    TApp[TRef[$LazyTakeDefName],
                                         TOp2["-", nVar, TNum[1]]],
                                    tVar
                                ]
                            ]
                        ]],
                        TLam[ignVar, nilTerm[]]
                    ],
                    sVar
                ]
            ]
        ]]
    ]
]

TLazyTake[t_TTerm, n_Integer ? NonNegative] := (
    ensureInit[];
    registerLazyTakeDef[];
    TApp[TApp[TRef[$LazyTakeDefName], TNum[n]], t]
)

(* === IC-native combinatorial generators ===

   thvm doesn't have auto-DUP for non-linear LAM uses, and a
   manually TDup'd LAM leaves a stuck `DP0[OP2[SUP[...]]]` shape
   that TWnf does not finish reducing.  We avoid the issue by
   never using a closure LAM as a "function arg" passed to a
   higher-order combinator.  Instead, every place that would call
   `map(f, xs)` or `concatMap(f, xs)` is realized as its own
   *specialized* recursive TDef whose body inlines the per-element
   transformation directly.  Cons-list args are TAG_CTR (passive),
   so multiple references read safely without DUP.

   Helpers (registered once, shared across generators):

       lazyConcat       : ConsList -> ConsList -> ConsList
       lazyPrependEach  : a -> ConsList (ConsList a) ->
                              ConsList (ConsList a)
                          -- prepend `a` to every element of the
                          -- outer list. *)

$LazyHelpersRegistered = False

defConcat[]    := Module[{xs, ys, h, t, ig},
    TDef["$THVMLink__lazyConcat",
        TLam[xs, TLam[ys,
            TApp[
                TMatCtr[$LazyCons,
                    TLam[h, TLam[t,
                        consTerm[h,
                            TApp[TApp[TRef["$THVMLink__lazyConcat"], t], ys]]
                    ]],
                    TLam[ig, ys]
                ],
                xs
            ]
        ]]
    ]
]

defPrependEach[] := Module[{x, xs, h, t, ig},
    TDef["$THVMLink__lazyPrependEach",
        TLam[x, TLam[xs,
            TApp[
                TMatCtr[$LazyCons,
                    TLam[h, TLam[t,
                        consTerm[
                            consTerm[x, h],
                            TApp[TApp[TRef["$THVMLink__lazyPrependEach"], x], t]]
                    ]],
                    TLam[ig, nilTerm[]]
                ],
                xs
            ]
        ]]
    ]
]

(* Permutations in WL lex order via "choose each as first":

       permsLex(xs) = case xs of
           Nil -> [[]]
           _   -> chooseEach(xs, [])

       chooseEach(remaining, before) = case remaining of
           Nil           -> []
           Cons(x, rest) ->
               concat(
                   prependEach(x, permsLex(concat(before, rest))),
                   chooseEach(rest, concat(before, [x]))) *)

defPermsLex[] := Module[{xs, ig},
    TDef["$THVMLink__lazyPermsLex",
        TLam[xs,
            TApp[
                TMatCtr[$LazyCons,
                    Module[{h, t},
                        TLam[h, TLam[t,
                            TApp[TApp[TRef["$THVMLink__lazyChooseEach"], xs],
                                 nilTerm[]]
                        ]]
                    ],
                    TLam[ig, consTerm[nilTerm[], nilTerm[]]]
                ],
                xs
            ]
        ]
    ]
]

defChooseEach[] := Module[{remaining, before, x, rest, ig},
    TDef["$THVMLink__lazyChooseEach",
        TLam[remaining, TLam[before,
            TApp[
                TMatCtr[$LazyCons,
                    TLam[x, TLam[rest,
                        TApp[
                            TApp[TRef["$THVMLink__lazyConcat"],
                                TApp[
                                    TApp[TRef["$THVMLink__lazyPrependEach"], x],
                                    TApp[TRef["$THVMLink__lazyPermsLex"],
                                        TApp[
                                            TApp[TRef["$THVMLink__lazyConcat"],
                                                before],
                                            rest]]]],
                            TApp[
                                TApp[TRef["$THVMLink__lazyChooseEach"],
                                    rest],
                                TApp[
                                    TApp[TRef["$THVMLink__lazyConcat"],
                                        before],
                                    consTerm[x, nilTerm[]]]]]
                    ]],
                    TLam[ig, nilTerm[]]
                ],
                remaining
            ]
        ]]
    ]
]

(* Subsets: "include or exclude head" recurrence.

       subsets([])          = [[]]
       subsets(Cons(x, ys)) = concat(subsets(ys), prependEach(x, subsets(ys)))

   `subsets(ys)` is invoked twice in the recursive case.  Both
   reads land on the same Cons-list (passive CTR cells); no DUP
   needed.  Total work for full enumeration is O(n * 2^n); a
   sharing-friendly variant is future work. *)

defSubsets[]   := Module[{xs, h, t, ig},
    TDef["$THVMLink__lazySubsets",
        TLam[xs,
            TApp[
                TMatCtr[$LazyCons,
                    TLam[h, TLam[t,
                        TApp[TApp[TRef["$THVMLink__lazyConcat"],
                                  TApp[TRef["$THVMLink__lazySubsets"], t]],
                            TApp[TApp[TRef["$THVMLink__lazyPrependEach"], h],
                                TApp[TRef["$THVMLink__lazySubsets"], t]]]
                    ]],
                    TLam[ig, consTerm[nilTerm[], nilTerm[]]]
                ],
                xs
            ]
        ]
    ]
]

(* Tuples: cross product, last-list iterates fastest.

       tuples([])           = [[]]
       tuples(Cons(xs, ys)) = tuplesHelper(xs, tuples(ys))

       tuplesHelper(xs, sub) = case xs of
           Nil           -> []
           Cons(x, rest) -> concat(prependEach(x, sub),
                                   tuplesHelper(rest, sub)) *)

defTuples[]       := Module[{lists, h, t, ig},
    TDef["$THVMLink__lazyTuples",
        TLam[lists,
            TApp[
                TMatCtr[$LazyCons,
                    TLam[h, TLam[t,
                        TApp[TApp[TRef["$THVMLink__lazyTuplesHelper"], h],
                            TApp[TRef["$THVMLink__lazyTuples"], t]]
                    ]],
                    TLam[ig, consTerm[nilTerm[], nilTerm[]]]
                ],
                lists
            ]
        ]
    ]
]

defTuplesHelper[] := Module[{xs, sub, x, rest, ig},
    TDef["$THVMLink__lazyTuplesHelper",
        TLam[xs, TLam[sub,
            TApp[
                TMatCtr[$LazyCons,
                    TLam[x, TLam[rest,
                        TApp[TApp[TRef["$THVMLink__lazyConcat"],
                                  TApp[TApp[TRef["$THVMLink__lazyPrependEach"], x],
                                       sub]],
                            TApp[TApp[TRef["$THVMLink__lazyTuplesHelper"], rest],
                                sub]]
                    ]],
                    TLam[ig, nilTerm[]]
                ],
                xs
            ]
        ]]
    ]
]

(* Splits: ordered n-way partitions (parts may be empty).

       splits(xs, 1)            = [[xs]]
       splits(Nil, n>1)         = prependNilToEach(splits(Nil, n-1))
       splits(Cons(h, t), n>1)  = concat(
           prependNilToEach(splits(Cons(h, t), n-1)),
           prependHToFirstEach(h, splits(t, n)))

       prependNilToEach(ss) = prependEach(Nil, ss)

       prependHToFirstEach(h, ss) = case ss of
           Nil          -> []
           Cons(s, rs)  -> case s of
               Cons(fp, rp) -> cons(cons(cons(h, fp), rp),
                                    prependHToFirstEach(h, rs))
               Nil          -> prependHToFirstEach(h, rs)

   "Empty-first" branch fires before "h-in-first", matching WL's
   k=0..L cut-position order at n=2. *)

defSplits[]              := Module[{xs, n, h, t, ig},
    TDef["$THVMLink__lazySplits",
        TLam[xs, TLam[n,
            TIfZero[TOp2["-", n, TNum[1]],
                consTerm[consTerm[xs, nilTerm[]], nilTerm[]],
                TApp[
                    TMatCtr[$LazyCons,
                        TLam[h, TLam[t,
                            TApp[TApp[TRef["$THVMLink__lazyConcat"],
                                    TApp[TApp[TRef["$THVMLink__lazyPrependEach"],
                                              nilTerm[]],
                                        TApp[TApp[TRef["$THVMLink__lazySplits"], xs],
                                            TOp2["-", n, TNum[1]]]]],
                                TApp[TApp[TRef["$THVMLink__lazyPrependHToFirstEach"], h],
                                    TApp[TApp[TRef["$THVMLink__lazySplits"], t], n]]]
                        ]],
                        TLam[ig,
                            TApp[TApp[TRef["$THVMLink__lazyPrependEach"], nilTerm[]],
                                TApp[TApp[TRef["$THVMLink__lazySplits"], xs],
                                    TOp2["-", n, TNum[1]]]]
                        ]
                    ],
                    xs
                ]
            ]
        ]]
    ]
]

(* `prependHToFirstEach` walks ss, prepending h to the first part of
   each split.  Splits are always Cons(fp, rp) by construction (n >= 1),
   so the inner Nil case is unreachable; we use a separate helper for
   the "transform one split" step to keep h's use count down. *)
defPrependHToFirst[] := Module[{h, s, fp, rp, ig},
    TDef["$THVMLink__lazyPrependHToFirst",
        TLam[h, TLam[s,
            TApp[
                TMatCtr[$LazyCons,
                    TLam[fp, TLam[rp,
                        consTerm[consTerm[h, fp], rp]
                    ]],
                    TLam[ig, nilTerm[]]
                ],
                s]
        ]]
    ]
]

defPrependHToFirstEach[] := Module[{h, ss, s, rs, ig},
    TDef["$THVMLink__lazyPrependHToFirstEach",
        TLam[h, TLam[ss,
            TApp[
                TMatCtr[$LazyCons,
                    TLam[s, TLam[rs,
                        consTerm[
                            TApp[TApp[TRef["$THVMLink__lazyPrependHToFirst"], h], s],
                            TApp[TApp[TRef["$THVMLink__lazyPrependHToFirstEach"], h], rs]]
                    ]],
                    TLam[ig, nilTerm[]]
                ],
                ss
            ]
        ]]
    ]
]

(* lazyMap = lambda f s.  match s with
                          Cons(h, t) -> Cons(f h, lazyMap f t)
                          Nil        -> Nil                     *)
defLazyMap[] := Module[{f, s, h, t, ig},
    TDef["$THVMLink__lazyMap",
        TLam[f, TLam[s,
            TApp[
                TMatCtr[$LazyCons,
                    TLam[h, TLam[t,
                        consTerm[
                            TApp[f, h],
                            TApp[TApp[TRef["$THVMLink__lazyMap"], f], t]]
                    ]],
                    TLam[ig, nilTerm[]]
                ],
                s
            ]
        ]]
    ]
]

registerLazyHelpers[] := If[ ! TrueQ[$LazyHelpersRegistered],
    defConcat[];
    defPrependEach[];
    defPermsLex[];
    defChooseEach[];
    defTuples[];
    defTuplesHelper[];
    defSubsets[];
    defSplits[];
    defPrependHToFirst[];
    defPrependHToFirstEach[];
    defLazyMap[];
    $LazyHelpersRegistered = True
]

(* Encode a WL List as a Cons-list of encoded element TTerms. *)
encodeAsConsList[xs_List] := (ensureInit[];
    Fold[consTerm[tlazyEncode[#2], #1] &, nilTerm[], Reverse[xs]]
)

(* Encode a WL List of Lists as a Cons-list of Cons-lists. *)
encodeAsConsListOfConsLists[lists_List] := (ensureInit[];
    Fold[consTerm[encodeAsConsList[#2], #1] &, nilTerm[], Reverse[lists]]
)

TLazyPermutations[xs_List] := (
    ensureInit[];
    registerLazyHelpers[];
    TApp[TRef["$THVMLink__lazyPermsLex"], encodeAsConsList[xs]]
)

TLazySplits[xs_List]                       := TLazySplits[xs, 2]
TLazySplits[xs_List, n_Integer ? Positive] := (
    ensureInit[];
    registerLazyHelpers[];
    TApp[
        TApp[TRef["$THVMLink__lazySplits"], encodeAsConsList[xs]],
        TNum[n]
    ]
)

TLazyTuples[lists : {___List}] := (
    ensureInit[];
    registerLazyHelpers[];
    TApp[TRef["$THVMLink__lazyTuples"], encodeAsConsListOfConsLists[lists]]
)

TLazySubsets[xs_List] := (
    ensureInit[];
    registerLazyHelpers[];
    TApp[TRef["$THVMLink__lazySubsets"], encodeAsConsList[xs]]
)

(* === consumer combinators (force via TWnf, walk heap shape) === *)

(* lazyStep[t]: force one layer of t with TCnf (Levy-optimal cnf
   readback so DP-rooted cells from auto-dup'd recursive bodies
   resolve to a CTR/SUP head) and return either
       {decodedHead, restTerm}  on a Cons cell or a SUP head
       Missing["EmptyStream"]   on Nil / ERA
       Missing["NotAStream"]    on a non-stream root *)
lazyStep[t_TTerm] := lazyStepForced @ TCnf[t]

lazyStepForced[forced_TTerm] := With[{
    raw = ttermRaw[forced], tag = TTermTag[forced]
},
    Switch[ tag,
        $TagSUP,
            With[{loc = $termValFn[raw]},
                {decodeForced[$heapReadFn[loc]],
                 TTerm[$heapReadFn[loc + 1]]}
            ],
        $TagERA,
            Missing["EmptyStream"],
        $TagCTR,
            With[{label = $termExtFn[raw]},
                Which[
                    label === $LazyNil,
                        Missing["EmptyStream"],
                    label === $LazyCons,
                        {decodeForced[$termCtrAtFn[raw, 0]],
                         TTerm[$termCtrAtFn[raw, 1]]},
                    True,
                        Missing["NotAStream"]
                ]
            ],
        _,
            Missing["NotAStream"]
    ]
]

(* The head of a Cons / first child of a SUP is typically a TAG_ALO
   (lazy book term wrapper) until something forces it.  Decode by
   first running TCnf to realize-and-resolve through any DP heads,
   then walking the post-WHNF shape. *)
decodeForced[raw_Integer] := tlazyDecodeRaw[ttermRaw @ TCnf[TTerm[raw]]]

TLazyFirst[t_TTerm] := Replace[lazyStep[t], {
    {h_, _}        :> h,
    m_Missing      :> m
}]

TLazyRest[t_TTerm] := Replace[lazyStep[t], {
    {_, r_}        :> r,
    _Missing       :> t
}]

TLazyToList[t_TTerm] := Block[{out = {}, cur = t, step},
    While[ True,
        step = lazyStep[cur];
        If[ MissingQ[step], Break[]];
        AppendTo[out, step[[1]]];
        cur = step[[2]]
    ];
    out
]

(* === lazy stream combinators (IC-native via TDef) ================
   TLazyMap / TLazySelect / TLazyCatenate construct APP chains over
   their registered TDefs.  The result stays unforced until something
   walks it (TLazyToList, TLazyTake, TLazySelectFirst, ...).  The
   function arg `f` for TLazyMap / pred `p` for TLazySelect is itself
   a TTerm (typically a TLam).
*)

TLazyMap[f_TTerm, s_TTerm] := (
    registerLazyHelpers[];
    (* Wrap `f` in a fresh TDef so each Cons step's APP-LAM gets a
       fresh dyn copy of the LAM body via alo_realize.  Without this,
       reusing the same TLam across calls (or across the recursive
       per-element apps) would consume its LAM cell on first APP-LAM
       and leave subsequent applications to misbehave. *)
    With[{predRef = registerLazyPred[f]},
        TApp[TApp[TRef["$THVMLink__lazyMap"], predRef], s]
    ]
)

(* Non-TTerm predicate (a plain WL function/symbol): force the stream
   then Map on the WL side, so callers like `TLazyMap[Function, ...]` or
   `TLazyMap[symbol, ...]` work.  The IC-native path above takes
   precedence when the predicate is a TTerm (the more general case). *)
TLazyMap[f_ /; ! MatchQ[f, _TTerm], s_TTerm] := Map[f, TLazyToList[s]]

(* TLazySelect / TLazyCatenate are WL-side eager walkers.  An IC-native
   TDef version runs away when cnf has to drive the predicate at every
   Cons step: the predicate's auto-dup'd binder leaves an APP-headed-by-DP
   arg under MAT, and a cnf-drive in APP-MAT broad enough to handle that
   triggers exponential allocations on recursive calls.  WL-side walking
   is correct and fast on small streams.  The result is rebuilt as a fresh
   Cons-list TTerm so downstream consumers (TLazyTake, FromTTerm,
   TLazyToList) see a normal lazy stream shape.

   IMPORTANT: applying the same TLam predicate multiple times from
   WL would consume the LAM after the first APP-LAM (linear use in
   IC).  Register the predicate as a TDef once, then issue
   APP[TRef[predName], h_i] -- alo_realize unfolds a fresh dyn copy
   per call, so each application sees an intact LAM body. *)

(* Register a TLam as a uniquely-named TDef and return TRef[name] so
   we can apply it many times.  Each call gets a fresh name from a
   monotonic counter; the unique prefix avoids collisions with
   user-side def names. *)
$lazyPredCounter = 0
registerLazyPred[p_TTerm] := Module[{name},
    name = "$THVMLink__lazyPred_" <> ToString[$lazyPredCounter];
    $lazyPredCounter += 1;
    TDef[name, p];
    TRef[name]
]

TLazySelect[s_TTerm, p_TTerm] := Module[{predRef, raws},
    predRef = registerLazyPred[p];
    raws    = consListAsRawTerms[s];
    encodeAsConsListOfTerms @ Select[raws,
        With[{r = TWnf @ TApp[predRef, TTerm[#]]},
             TTermTag[r] === $TagNUM && TTermVal[r] =!= 0] &
    ]
]

TLazyCatenate[ss_TTerm] := encodeAsConsListOfTerms @ Catenate[
    consListAsRawTerms /@ (TTerm /@ consListAsRawTerms[ss])
]

(* Helper: walk a Cons-list TTerm forcing each Cons cell, return a List
   of raw-Integer Term values for the elements.  Stops at Nil / ERA. *)
consListAsRawTerms[s_TTerm] := Block[{cur = ttermRaw[s], out = {}, tag, label, forced, h},
    While[ True,
        forced = ttermRaw @ TCnf[TTerm[cur]];
        tag    = $termTagFn[forced];
        If[ tag === $TagERA, Break[]];
        If[ tag =!= $TagCTR, Break[]];
        label = $termExtFn[forced];
        Which[
            label === $LazyNil,  Break[],
            label === $LazyCons,
                AppendTo[out, $termCtrAtFn[forced, 0]];
                cur = $termCtrAtFn[forced, 1],
            True, Break[]
        ]
    ];
    out
]

(* Helper: build a fresh Cons-list TTerm from a List of raw-Integer
   element Terms (as returned by consListAsRawTerms). *)
encodeAsConsListOfTerms[raws_List] := Fold[
    consTerm[TTerm[#2], #1] &,
    nilTerm[],
    Reverse[raws]
]

(* TLazySelectFirst[s, p]: walk the stream one Cons at a time,
   apply `p` to each head via TApp+TWnf, return the FIRST head whose
   result reduces to a non-zero NUM, decoded via FromTTerm.
   Returns Missing["NotFound"] when the stream is exhausted.  Lazy
   on input (only forces as far as the first hit), eager on output. *)
TLazySelectFirst[s_TTerm, p_TTerm] := Block[{
    predRef = registerLazyPred[p],
    cur = ttermRaw[s], forced, tag, label, headRaw, predResult
},
    While[ True,
        forced = ttermRaw @ TCnf[TTerm[cur]];
        tag    = $termTagFn[forced];
        If[ tag === $TagERA, Return[Missing["NotFound"]]];
        If[ tag =!= $TagCTR, Return[Missing["NotFound"]]];
        label = $termExtFn[forced];
        If[ label === $LazyNil, Return[Missing["NotFound"]]];
        If[ label =!= $LazyCons, Return[Missing["NotFound"]]];
        headRaw    = $termCtrAtFn[forced, 0];
        predResult = TWnf @ TApp[predRef, TTerm[headRaw]];
        If[ TTermTag[predResult] === $TagNUM &&
            TTermVal[predResult] =!= 0,
            Return[ FromTTerm @ TTerm[headRaw] ]
        ];
        cur = $termCtrAtFn[forced, 1]
    ]
]

(* TLazyCases[s, pat]: walk the stream eagerly, run TPatternMatch on
   each Cons head against the held WL `pattern`, return a WL List of
   {head-TTerm, bindings} for every match.  Multi-match patterns
   (when supported in Pattern.wl) yield multiple entries per head. *)
SetAttributes[TLazyCases, HoldRest]
TLazyCases[s_TTerm, pat_] := Block[{
    raws = consListAsRawTerms[s], h, m, out = {}
},
    Do[
        h = TTerm[r];
        (* With[{p = pat}, ...] syntactically substitutes the held
           pat into the body, so TPatternMatch's HoldRest sees the
           pattern literal (not the symbol `pat`). *)
        With[{p = pat},
            m = TPatternMatch[h, p];
            If[ MatchQ[m, _TMatch],
                With[{bs = TMatchBindings[m]},
                    Do[ AppendTo[out, {h, b}], {b, bs} ]
                ]
            ]
        ],
        {r, raws}
    ];
    out
]

(* TLazyChoice[xs_List]: build a SUP-stream over xs, encoded as
   nested SUPs at a fresh label.  Empty list -> ERA; one element ->
   that element verbatim; multiple -> &L{x1, &L{x2, ..., &L{xN-1,xN}}}.
   The shared label means downstream DUP-SUP commutes annihilate
   pairwise -- ideal for "pick one of these" choice points. *)
TLazyChoice[xs_List] := (
    ensureInit[];
    Module[{lab = TFreshLabel[], encoded},
        encoded = ToTTerm /@ xs;
        Switch[Length[encoded],
            0, TEra[],
            1, encoded[[1]],
            _, Fold[TSup[lab, #2, #1] &,
                    Last[encoded],
                    Reverse[Most[encoded]]]
        ]
    ]
)

TLazyFold[f_, x_, t_TTerm] := Fold[f, x, TLazyToList[t]]

End[];
EndPackage[];
