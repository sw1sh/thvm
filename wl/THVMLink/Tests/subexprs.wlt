(* subexprs.wlt -- VerificationTest specs for TTermSubexprs +
   TSubexprAt + TDefGet / TDefExpr / TDefTree.

   See wl/THVMLink/Kernel/THVMLink.wl (TTermSubexprs / TSubexprAt
   alongside TTermExpr / TTermTree) and wl/THVMLink/Kernel/Ref.wl
   (TDefGet / TDefExpr / TDefTree).  TTermSubexprs returns a List
   of `path -> TTerm` rules; sibling output to TTermExpr/TTermTree. *)

(* === TTermSubexprs: pre-order DFS over heap structure ============== *)

VerificationTest[
    Block[{t = TNum[7], pairs},
        pairs = TTermSubexprs[t];
        Length[pairs]],
    1,
    TestID -> "Subexprs/atom-yields-one-pair"
]

VerificationTest[
    Block[{t = TNum[7], pairs},
        pairs = TTermSubexprs[t];
        First[pairs][[1]]],
    {},
    TestID -> "Subexprs/atom-root-path-empty"
]

VerificationTest[
    Block[{t = TOp2["+", TNum[1], TNum[2]], pairs},
        pairs = TTermSubexprs[t];
        Length[pairs]],
    3,
    TestID -> "Subexprs/op2-yields-three"
]

VerificationTest[
    Block[{t = TOp2["+", TNum[1], TNum[2]], pairs},
        pairs = TTermSubexprs[t];
        First /@ pairs],
    {{}, {0}, {1}},
    TestID -> "Subexprs/op2-paths-pre-order"
]

VerificationTest[
    Block[{a = ToTTerm[a], b = ToTTerm[b], c = ToTTerm[c],
           t, pairs},
        t     = TOp2["*", TOp2["+", a, b], c];
        pairs = TTermSubexprs[t];
        First /@ pairs],
    {{}, {0}, {0, 0}, {0, 1}, {1}},
    TestID -> "Subexprs/nested-op2-paths"
]

VerificationTest[
    Block[{t = ToTTerm[{a, b, c}], pairs},
        pairs = TTermSubexprs[t];
        Length[pairs]],
    4,                          (* Tuple-CTR + 3 leaves *)
    TestID -> "Subexprs/tuple-ctr-yields-1+arity"
]

VerificationTest[
    Block[{t = ToTTerm[{a, b, c}], pairs},
        pairs = TTermSubexprs[t];
        First /@ pairs],
    {{}, {1}, {2}, {3}},
    TestID -> "Subexprs/ctr-paths-skip-arity-slot-0"
]

(* === Rule shape: {path -> TTerm, ...} ============================= *)

VerificationTest[
    Block[{t = TOp2["+", TNum[1], TNum[2]], pairs},
        pairs = TTermSubexprs[t];
        And @@ (Head[#] === Rule & /@ pairs)],
    True,
    TestID -> "Subexprs/entries-are-Rules"
]

VerificationTest[
    Block[{t = TOp2["+", TNum[1], TNum[2]], pairs, lookup},
        pairs  = TTermSubexprs[t];
        lookup = Association[pairs];
        TTermVal /@ {lookup[{0}], lookup[{1}]}],
    {1, 2},
    TestID -> "Subexprs/Association-lookup-by-path"
]

(* === TSubexprAt: random-access path navigation ===================== *)

VerificationTest[
    Block[{t = TOp2["+", TNum[10], TNum[20]]},
        TTermVal @ TSubexprAt[t, {0}]],
    10,
    TestID -> "SubexprAt/op2-left-arg"
]

VerificationTest[
    Block[{t = TOp2["+", TNum[10], TNum[20]]},
        TTermVal @ TSubexprAt[t, {1}]],
    20,
    TestID -> "SubexprAt/op2-right-arg"
]

VerificationTest[
    Block[{t = TOp2["+", TNum[10], TNum[20]]},
        TSubexprAt[t, {7}]],
    Missing["OutOfBounds", {7}],
    TestID -> "SubexprAt/out-of-bounds-returns-missing"
]

VerificationTest[
    Block[{t = TOp2["+", TNum[10], TNum[20]]},
        Head @ TSubexprAt[t, {}]],
    TTerm,
    TestID -> "SubexprAt/empty-path-returns-root"
]

VerificationTest[
    Block[{a = ToTTerm[a], b = ToTTerm[b], c = ToTTerm[c],
           t, sub},
        t   = TOp2["*", TOp2["+", a, b], c];
        sub = TSubexprAt[t, {0, 0}];
        TTermExpr[sub]],
    "CTR"[10001],
    TestID -> "SubexprAt/nested-symbol-leaf"
]

VerificationTest[
    Block[{t = ToTTerm[{a, b, c}]},
        Sort @ Table[TTermExpr @ TSubexprAt[t, {i}], {i, 1, 3}]],
    Sort @ {"CTR"[10001], "CTR"[10002], "CTR"[10003]},
    TestID -> "SubexprAt/ctr-children-via-offset"
]

(* === TDefGet / TDefExpr / TDefTree =================================
   TLazyRange registers a recursive lazyRange def; we can extract its
   body to inspect the structure without forcing reduction. *)

VerificationTest[
    TLazyRange[5];
    Head @ TDefGet["$THVMLink__lazyRange"],
    TTerm,
    TestID -> "DefGet/registered-def-returns-TTerm"
]

VerificationTest[
    TLazyRange[5];
    TTermTag @ TDefGet["$THVMLink__lazyRange"],
    $TagLAM,
    TestID -> "DefGet/lazyRange-body-is-LAM"
]

VerificationTest[
    TDefGet["this-def-name-was-never-registered"],
    Missing["UnregisteredDef", "this-def-name-was-never-registered"],
    TestID -> "DefGet/unregistered-returns-missing"
]

VerificationTest[
    TLazyRange[5];
    (* Outermost decoded expr is "LAM"[...]; head is the string "LAM". *)
    Head @ TDefExpr["$THVMLink__lazyRange"],
    "LAM",
    TestID -> "DefExpr/returns-decoded-expression"
]

VerificationTest[
    TLazyRange[5];
    Head @ TDefTree["$THVMLink__lazyRange"],
    Tree,
    TestID -> "DefTree/returns-Tree-object"
]

VerificationTest[
    TDefTree["unregistered-name-here"],
    Missing["UnregisteredDef", "unregistered-name-here"],
    TestID -> "DefTree/unregistered-returns-missing"
]

(* === DefExpr decodes recursive references via REF leaves =========== *)
(* lazyRange's body should contain REF[<def-id>] back-references. *)

VerificationTest[
    TLazyRange[5];
    With[{e = TDefExpr["$THVMLink__lazyRange"]},
        FreeQ[e, "REF"[_]]],
    False,                      (* not free of REF -- has them *)
    TestID -> "DefExpr/lazyRange-contains-REF-backrefs"
]
