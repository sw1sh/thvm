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

BeginPackage["THVMLink`"];

TLazyRange::usage        = "TLazyRange[n] / TLazyRange[a, b] / TLazyRange[a, b, step] returns a TTerm whose head is an APP-REF chain to a recursive `lazyRange` TDef.  Forcing one element via TWnf fires exactly the interactions needed to expose the next Cons; the rest stays unforced.  TLazyRange[10^6] allocates O(1) cells at construction.";
TLazyPermutations::usage = "TLazyPermutations[xs] returns a TTerm enumerating Permutations[xs] in lex order, lazily.  IC-native: backed by a recursive `permsLex` TDef that consumes one outer Cons per fired interaction.  TLazyTake[TLazyPermutations[Range[20]], 5] allocates O(1) at construction.";
TLazySplits::usage       = "TLazySplits[xs, n] returns a TTerm enumerating ordered n-way splits of xs (parts may be empty), lazily.  IC-native via TDef.  TLazySplits[xs] defaults to n=2.";
TLazyTuples::usage       = "TLazyTuples[{xs1, xs2, ...}] returns a TTerm enumerating Tuples[{xs1, ...}] (cross product), lazily.  IC-native via TDef.";
TLazySubsets::usage      = "TLazySubsets[xs] returns a TTerm enumerating Subsets[xs] (sorted by cardinality, then lex), lazily.  IC-native via TDef.";

TLazyFirst::usage   = "TLazyFirst[s] forces the head of a stream via TWnf and returns the decoded WL value.  Returns Missing[\"EmptyStream\"] when the head reduces to Nil / ERA.";
TLazyRest::usage    = "TLazyRest[s] forces the head and returns the (still unforced) tail TTerm.";
TLazyTake::usage    = "TLazyTake[s, n] returns a TTerm representing the first n elements of `s` as a lazy Cons chain.  The result is itself lazy: it stays an unforced APP-APP-REF redex until something walks it.  Built on a TDef-based `take` body so the n elements are produced one at a time by IC reduction.  Use TLazyToList[TLazyTake[s, n]] to force into a WL List.";
TLazyToList::usage  = "TLazyToList[s] forces every element and returns a WL List.  Will hang if the stream is infinite.";
TLazyMap::usage     = "TLazyMap[f, s] = f /@ TLazyToList[s].  Forces the full stream; not lazy on its output.";
TLazyFold::usage    = "TLazyFold[f, x, s] = Fold[f, x, TLazyToList[s]].  Forces the full stream.";
TLazySubexprs::usage = "TLazySubexprs[t] returns a WL List of {path, subterm} pairs covering every position in `t`, pre-order DFS.  `path` is a list of integer offsets from `t` to the subterm (root has empty path); `subterm` is a TTerm.  Pattern.wl uses this to walk a term and try a rule at every subexpression.  The list is built eagerly; for big inputs prefer the underlying `subexprChildren` walker plus your own iteration.";
TSubexprAt::usage    = "TSubexprAt[t, path] navigates `t` along `path` (a List of integer offsets) and returns the subterm as a TTerm.  Returns Missing[\"OutOfBounds\"] when the path doesn't fit the term.";

TLazyEncode::usage  = "TLazyEncode[v] encodes a WL value into a TTerm.";
TLazyDecode::usage  = "TLazyDecode[t] decodes a TTerm back to a WL value.";

(* Forward-declare symbols owned by sibling files (Ref.wl, Switch.wl)
   so bare references inside Begin[`Private`] resolve to THVMLink`X
   instead of phantom THVMLink`Private`X.  Lazy.wl is parsed before
   Ref.wl and Switch.wl in alphabetical order, so the public names
   don't yet exist when this file's body runs. *)
{TDef, TRef, TIfZero, TOp2, TNum, TMatCtr};

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

TLazyEncode[v_] := tlazyEncode[v]

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

TLazyDecode[t_] := tlazyDecode[t]

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

TLazyMap[f_, t_TTerm]      := Map[f, TLazyToList[t]]
TLazyFold[f_, x_, t_TTerm] := Fold[f, x, TLazyToList[t]]

(* === subexpression traversal ===

   subexprChildren[raw] returns the list of (offset, child-raw)
   pairs that describe the structural children of a heap term.
   Used both by TLazySubexprs (pre-order DFS yielding {path,
   subterm} pairs) and by TSubexprAt (random-access navigation
   along an explicit path).

   Per-tag arities:
     LAM, ALO, DP0, DP1, DUP        -- 1 child  (body / wrapped)
     APP, SUP, OP2, MAT, EQL, AND,  -- 2 children
       OR, WHEN, ANN, BRI
     DSU, DDU                        -- 3 children
     CTR                             -- variable; NUM(arity) at +0
                                        is metadata (skip), data
                                        children at +1..+n
     UOP                             -- arity from uopArity[opcode]
     VAR, REF, NUM, ERA, TEN, ANY,   -- 0 children (leaves)
       FVR, PRI

   The "offset" returned is the path-segment integer used by
   TSubexprAt to navigate; children of LAM are at offset 0, APP's
   f at 0 / x at 1, CTR's first data child at 1 (mirroring the
   heap layout), etc. *)

subexprChildren[raw_Integer] := Block[{
    tag = $termTagFn[raw], val = $termValFn[raw], ext = $termExtFn[raw],
    n
},
    Switch[ tag,
        $TagLAM | $TagDUP | $TagDP0 | $TagDP1 | $TagALO,
            {{0, $heapReadFn[val]}},
        $TagAPP | $TagSUP,
            {{0, $heapReadFn[val]}, {1, $heapReadFn[val + 1]}},
        $TagOP2 | $TagEQL | $TagAND | $TagOR | $TagWHEN | $TagANN | $TagMAT | $TagBRI,
            {{0, $heapReadFn[val]}, {1, $heapReadFn[val + 1]}},
        $TagDSU | $TagDDU,
            {{0, $heapReadFn[val + 0]},
             {1, $heapReadFn[val + 1]},
             {2, $heapReadFn[val + 2]}},
        $TagCTR,
            (n = $termValFn[$heapReadFn[val]];
             Table[{i, $heapReadFn[val + i]}, {i, 1, n}]),
        $TagUOP,
            With[{ar = uopArity[ext]},
                Table[{i, $heapReadFn[val + i]}, {i, 0, ar - 1}]],
        _, {}
    ]
]

walkSubexprsRaw[raw_Integer, path_List] := Block[{
    children = subexprChildren[raw]
},
    Prepend[
        Catenate[
            (walkSubexprsRaw[#[[2]], Append[path, #[[1]]]] & /@ children)
        ],
        {path, raw}
    ]
]

TLazySubexprs[t_TTerm] := (
    ensureInit[];
    {#[[1]], TTerm[#[[2]]]} & /@ walkSubexprsRaw[ttermRaw[t], {}]
)

TSubexprAt[t_TTerm, path_List] := Block[{
    raw = ttermRaw[t], k, ch
},
    Catch[
        Do[
            ch = subexprChildren[raw];
            k  = SelectFirst[ch, First[#] === offset &, None];
            If[ k === None, Throw[Missing["OutOfBounds", path]]];
            raw = k[[2]],
            {offset, path}
        ];
        TTerm[raw]
    ]
]

End[];
EndPackage[];
