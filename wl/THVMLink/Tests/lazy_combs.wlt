(* lazy_combs.wlt -- VerificationTest specs for the lazy stream
   combinators added on top of Lazy.wl's TLazyRange / Cons-list:

       TLazyMap[f, s]        IC-native lazy map (TDef-backed)
       TLazySelect[s, p]     filter by predicate p (eager walker)
       TLazySelectFirst[s, p] first head where p(h) is non-zero
       TLazyCases[s, pat]    Pattern.wl-driven filter+capture
       TLazyCatenate[ss]     flatten Cons-stream of Cons-streams
       TLazyChoice[xs]       SUP-stream over a List literal

   The user-side function / predicate is a TLam.  Predicates return
   NUM(0) (drop) / NUM(non-zero) (keep). *)

(* === TLazyMap ===================================================== *)

VerificationTest[
    Block[{inc = With[{x = Module[{xs}, xs]},
                       TLam[x, TOp2["+", x, TNum[1]]]]},
        FromTTerm @ TLazyTake[TLazyMap[inc, TLazyRange[5]], 5]],
    {2, 3, 4, 5, 6},
    TestID -> "LazyCombs/Map/+1-over-Range5"
]

VerificationTest[
    Block[{dbl = With[{x = Module[{xs}, xs]},
                       TLam[x, TOp2["*", x, TNum[2]]]],
           xs = THVMLink`Private`encodeAsConsList[{1, 2, 3}]},
        FromTTerm @ TLazyMap[dbl, xs]],
    {2, 4, 6},
    TestID -> "LazyCombs/Map/double-on-3-element-Cons"
]

(* === TLazySelect ================================================== *)

VerificationTest[
    Block[{gt3 = With[{x = Module[{xs}, xs]},
                       TLam[x, TOp2["<", TNum[3], x]]],
           xs  = THVMLink`Private`encodeAsConsList[{1, 2, 3, 4, 5, 6, 7, 8}]},
        FromTTerm @ TLazySelect[xs, gt3]],
    {4, 5, 6, 7, 8},
    TestID -> "LazyCombs/Select/gt3-of-1-to-8"
]

VerificationTest[
    Block[{alwaysFalse = With[{x = Module[{xs}, xs]}, TLam[x, TNum[0]]],
           xs = THVMLink`Private`encodeAsConsList[{1, 2, 3}]},
        FromTTerm @ TLazySelect[xs, alwaysFalse]],
    {},
    TestID -> "LazyCombs/Select/alwaysFalse-yields-empty"
]

VerificationTest[
    Block[{alwaysTrue = With[{x = Module[{xs}, xs]}, TLam[x, TNum[1]]],
           xs = THVMLink`Private`encodeAsConsList[{1, 2, 3}]},
        FromTTerm @ TLazySelect[xs, alwaysTrue]],
    {1, 2, 3},
    TestID -> "LazyCombs/Select/alwaysTrue-keeps-all"
]

(* === TLazySelectFirst ============================================ *)

VerificationTest[
    Block[{gt5 = With[{x = Module[{xs}, xs]},
                       TLam[x, TOp2["<", TNum[5], x]]],
           xs  = THVMLink`Private`encodeAsConsList[{1, 2, 3, 4, 5, 6, 7, 8}]},
        TLazySelectFirst[xs, gt5]],
    6,
    TestID -> "LazyCombs/SelectFirst/gt5-finds-6"
]

VerificationTest[
    Block[{gt100 = With[{x = Module[{xs}, xs]},
                         TLam[x, TOp2["<", TNum[100], x]]],
           xs    = THVMLink`Private`encodeAsConsList[{1, 2, 3}]},
        TLazySelectFirst[xs, gt100]],
    Missing["NotFound"],
    TestID -> "LazyCombs/SelectFirst/no-match"
]

VerificationTest[
    Block[{gt5 = With[{x = Module[{xs}, xs]},
                       TLam[x, TOp2["<", TNum[5], x]]]},
        TLazySelectFirst[TLazyRange[20], gt5]],
    6,
    TestID -> "LazyCombs/SelectFirst/lazy-range-finds-6"
]

(* === TLazyCatenate =============================================== *)

VerificationTest[
    Block[{ss = THVMLink`Private`encodeAsConsList[{
                THVMLink`Private`encodeAsConsList[{1, 2}],
                THVMLink`Private`encodeAsConsList[{3, 4}]}]},
        FromTTerm @ TLazyCatenate[ss]],
    {1, 2, 3, 4},
    TestID -> "LazyCombs/Catenate/two-2-streams"
]

VerificationTest[
    Block[{ss = THVMLink`Private`encodeAsConsList[{
                THVMLink`Private`encodeAsConsList[{}],
                THVMLink`Private`encodeAsConsList[{1}],
                THVMLink`Private`encodeAsConsList[{}],
                THVMLink`Private`encodeAsConsList[{2, 3}]}]},
        FromTTerm @ TLazyCatenate[ss]],
    {1, 2, 3},
    TestID -> "LazyCombs/Catenate/with-empty-streams"
]

(* === TLazyCases (Pattern.wl-driven) ============================== *)

VerificationTest[
    Block[{xs = THVMLink`Private`encodeAsConsList[{
                ToTTerm[g[1]],
                ToTTerm[h[2]],
                ToTTerm[g[3]],
                ToTTerm[h[4]]}]},
        Length @ TLazyCases[xs, g[_]]],
    2,
    TestID -> "LazyCombs/Cases/g[_]-finds-two"
]

VerificationTest[
    Block[{xs = THVMLink`Private`encodeAsConsList[{
                ToTTerm[g[1]],
                ToTTerm[h[2]],
                ToTTerm[g[3]],
                ToTTerm[h[4]]}],
           hits},
        hits = TLazyCases[xs, g[x_]];
        FromTTerm /@ (Last /@ hits /. (b_Association :> b[x]))],
    {1, 3},
    TestID -> "LazyCombs/Cases/binding-extraction"
]

(* === TLazyChoice ================================================= *)

VerificationTest[
    TTermTag @ TLazyChoice[ToTTerm /@ {1, 2, 3, 4}],
    $TagSUP,
    TestID -> "LazyCombs/Choice/multi-element-is-SUP"
]

VerificationTest[
    TTermTag @ TLazyChoice[{ToTTerm[42]}],
    $TagNUM,
    TestID -> "LazyCombs/Choice/singleton-is-bare-element"
]

VerificationTest[
    TTermTag @ TLazyChoice[{}],
    $TagERA,
    TestID -> "LazyCombs/Choice/empty-is-ERA"
]
