(* lazy.wlt -- VerificationTest specs for Lazy.wl.

   IC-native laziness: TLazyRange uses TDef + TRef so forcing
   one element fires only the interactions needed for that element.

   See docs/plans/lazy_patterns.md (Phase 0). *)

(* === lifecycle === *)

VerificationTest[
    TInit[],
    True,
    TestID -> "Lazy/init"
]

VerificationTest[
    TReset[],
    1,
    TestID -> "Lazy/reset"
]

(* === encode / decode round-trip === *)

VerificationTest[
    TLazyDecode @ TLazyEncode[42],
    42,
    TestID -> "Lazy/encode/integer-roundtrip"
]

VerificationTest[
    TLazyDecode @ TLazyEncode[a],
    a,
    TestID -> "Lazy/encode/symbol-roundtrip"
]

VerificationTest[
    TLazyDecode @ TLazyEncode[{1, 2, 3}],
    {1, 2, 3},
    TestID -> "Lazy/encode/integer-list-roundtrip"
]

VerificationTest[
    TLazyDecode @ TLazyEncode[{a, b, c}],
    {a, b, c},
    TestID -> "Lazy/encode/symbol-list-roundtrip"
]

VerificationTest[
    TLazyDecode @ TLazyEncode[{1, {a, b}, 3}],
    {1, {a, b}, 3},
    TestID -> "Lazy/encode/nested-list-roundtrip"
]

(* === TLazyRange: TDef-driven, IC-native lazy === *)

VerificationTest[
    Head @ TLazyRange[5],
    TTerm,
    TestID -> "Lazy/range/returns-TTerm"
]

(* TLazyRange construction is O(1) regardless of count: it builds
   one APP-APP-APP-REF chain over three NUM args.  10^6 should be
   indistinguishable from 10. *)
VerificationTest[
    TReset[];
    Block[{base = THeapPos[]},
        TLazyRange[10^6];
        (* Bound is loose; what matters is that it does NOT scale
           with 10^6.  ~50 cells is plenty of headroom for the
           one-time TDef body snapshot + the four head cells. *)
        THeapPos[] - base < 200
    ],
    True,
    TestID -> "Lazy/range/big-construction-is-O(1)"
]

VerificationTest[
    TLazyToList @ TLazyRange[5],
    {1, 2, 3, 4, 5},
    TestID -> "Lazy/range/1-to-5"
]

VerificationTest[
    TLazyToList @ TLazyRange[3, 7],
    {3, 4, 5, 6, 7},
    TestID -> "Lazy/range/3-to-7"
]

VerificationTest[
    TLazyToList @ TLazyRange[1, 10, 2],
    {1, 3, 5, 7, 9},
    TestID -> "Lazy/range/step-2"
]

VerificationTest[
    TLazyToList @ TLazyRange[0],
    {},
    TestID -> "Lazy/range/empty"
]

VerificationTest[
    TLazyToList @ TLazyTake[TLazyRange[100], 4],
    {1, 2, 3, 4},
    TestID -> "Lazy/range/take-prefix"
]

VerificationTest[
    TLazyToList @ TLazyTake[TLazyRange[3], 100],
    {1, 2, 3},
    TestID -> "Lazy/range/take-overshoot"
]

VerificationTest[
    TLazyToList @ TLazyTake[TLazyRange[10^6], 5],
    {1, 2, 3, 4, 5},
    TestID -> "Lazy/range/take-from-big-range"
]

(* TLazyTake itself is lazy: returns a TTerm (an APP-APP-REF redex)
   without firing the take traversal. *)
VerificationTest[
    Head @ TLazyTake[TLazyRange[10^6], 5],
    TTerm,
    TestID -> "Lazy/take/returns-TTerm"
]

(* Construction of TLazyTake[bigStream, k] is O(1): one APP-APP-REF
   chain regardless of k or stream size. *)
VerificationTest[
    TReset[];
    Block[{base},
        TLazyRange[10^6];
        base = THeapPos[];
        TLazyTake[TLazyRange[10^6], 1000];
        THeapPos[] - base < 200
    ],
    True,
    TestID -> "Lazy/take/construction-is-O(1)"
]

(* Forcing the lazy TLazyTake fires interactions proportional to k,
   not to the source stream size. *)
VerificationTest[
    TReset[];
    Block[{a, b},
        a = TItrs[];
        TLazyToList @ TLazyTake[TLazyRange[10^6], 5];
        b = TItrs[];
        b - a < 2000
    ],
    True,
    TestID -> "Lazy/take/force-fires-bounded-interactions"
]

VerificationTest[
    TLazyFirst @ TLazyRange[10, 20],
    10,
    TestID -> "Lazy/range/first"
]

VerificationTest[
    TLazyToList @ TLazyRest @ TLazyRange[5],
    {2, 3, 4, 5},
    TestID -> "Lazy/range/rest"
]

VerificationTest[
    TLazyFirst @ TLazyRange[0],
    Missing["EmptyStream"],
    TestID -> "Lazy/range/empty-first"
]

(* === Eager SUP-stream generators === *)

VerificationTest[
    Sort @ TLazyToList @ TLazyPermutations[{a, b, c}],
    Sort @ Permutations[{a, b, c}],
    TestID -> "Lazy/permutations/abc-set-equal"
]

VerificationTest[
    Length @ TLazyToList @ TLazyPermutations[{1, 2, 3, 4}],
    24,
    TestID -> "Lazy/permutations/4!=24"
]

VerificationTest[
    TLazyToList @ TLazyPermutations[{1}],
    {{1}},
    TestID -> "Lazy/permutations/singleton"
]

(* Permutations is now IC-native via TDef + TRef (APP-REF chain),
   not an eager SUP stream.  Construction returns an APP-rooted
   redex; forcing peels Cons cells one at a time. *)
VerificationTest[
    TTermTag @ TLazyPermutations[{a, b, c}],
    $TagAPP,
    TestID -> "Lazy/permutations/root-is-APP"
]

VerificationTest[
    TLazyToList @ TLazySplits[{a, b, c}],
    {{{}, {a, b, c}}, {{a}, {b, c}}, {{a, b}, {c}}, {{a, b, c}, {}}},
    TestID -> "Lazy/splits/2-way-of-3"
]

VerificationTest[
    Length @ TLazyToList @ TLazySplits[{a, b, c, d, e}, 2],
    6,
    TestID -> "Lazy/splits/2-way-of-5-count"
]

VerificationTest[
    Length @ TLazyToList @ TLazySplits[{a, b, c}, 3],
    10,
    TestID -> "Lazy/splits/3-way-of-3-count"
]

VerificationTest[
    Sort @ TLazyToList @ TLazyTuples[{{1, 2}, {a, b}}],
    Sort @ Tuples[{{1, 2}, {a, b}}],
    TestID -> "Lazy/tuples/2x2"
]

VerificationTest[
    Length @ TLazyToList @ TLazyTuples[{{1, 2, 3}, {a, b}, {x, y, z}}],
    18,
    TestID -> "Lazy/tuples/3x2x3-count"
]

VerificationTest[
    Sort @ TLazyToList @ TLazySubsets[{a, b, c}],
    Sort @ Subsets[{a, b, c}],
    TestID -> "Lazy/subsets/abc"
]

VerificationTest[
    Length @ TLazyToList @ TLazySubsets[{1, 2, 3, 4}],
    16,
    TestID -> "Lazy/subsets/4-elements-2^4"
]

(* === IC-native big-input laziness ===
   No eager construction; even huge inputs are O(1) at construction
   time and TLazyTake fires per-element interactions only. *)

VerificationTest[
    Head @ TLazyPermutations[Range[20]],
    TTerm,
    TestID -> "Lazy/permutations/big-input-no-failure"
]

VerificationTest[
    TLazyToList @ TLazyTake[TLazyPermutations[Range[20]], 1],
    {Range[20]},
    TestID -> "Lazy/permutations/take-1-of-20"
]

VerificationTest[
    Length @ TLazyToList @ TLazyTake[TLazyPermutations[Range[20]], 5],
    5,
    TestID -> "Lazy/permutations/take-5-of-20-length"
]

VerificationTest[
    First @ TLazyToList @ TLazyTake[TLazyPermutations[Range[20]], 5],
    Range[20],
    TestID -> "Lazy/permutations/take-5-of-20-first-is-identity"
]

VerificationTest[
    Length @ TLazyToList @ TLazyTake[TLazySubsets[Range[40]], 7],
    7,
    TestID -> "Lazy/subsets/take-7-of-40"
]

VerificationTest[
    Length @ TLazyToList @ TLazyTake[TLazyTuples[ConstantArray[Range[10], 5]], 3],
    3,
    TestID -> "Lazy/tuples/take-3-of-10^5"
]

(* === Map / Fold === *)

VerificationTest[
    TLazyMap[f, TLazyRange[3]],
    {f[1], f[2], f[3]},
    TestID -> "Lazy/map/over-range"
]

VerificationTest[
    TLazyFold[Plus, 0, TLazyRange[10]],
    Total[Range[10]],
    TestID -> "Lazy/fold/sum-over-range"
]
