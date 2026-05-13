(* heap_snapshot.wlt -- TContextSnapshot / TInitialize / TContextStrip /
   TContextToTermTree from Heap.wl.

   Roundtrip strategy: build a term, snapshot, TReset, initialize,
   compare TTermExpr of the new root vs. the original.  TTermExpr
   abstracts away absolute heap locs so renumbering is invisible. *)

(* === structural shape === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{h = TContextSnapshot[TEra[]], a},
        a = First[h];
        {Head[h], KeyExistsQ[a, "Cells"], KeyExistsQ[a, "Tensors"],
         KeyExistsQ[a, "Root"], KeyExistsQ[a, "State"]}
    ],
    {TContext, True, True, True, True},
    TestID -> "heap-snapshot/structural-keys"
]

VerificationTest[
    (* Empty heap: TEra[] is atomic so nothing gets allocated. *)
    TInit[];
    TReset[];
    Module[{h = TContextSnapshot[TEra[]]},
        {Length[First[h]["Cells"]], First[h]["State"]}
    ],
    {0, "Initialized"},
    TestID -> "heap-snapshot/empty-heap-era-root"
]

VerificationTest[
    (* Root Term carries the tag + heap-relative val. *)
    TInit[];
    TReset[];
    Module[{lam = TLam[x, x], h, root},
        h = TContextSnapshot[lam];
        root = First[h]["Root"];
        {Head[root], root[[1]], root[[3]]}
    ],
    {Term, "LAM", 0},
    TestID -> "heap-snapshot/root-term-fields"
]

(* === Term[<|...|>] normalization === *)

VerificationTest[
    Term[<|"tag" -> "LAM", "ext" -> 0, "val" -> 5|>],
    Term["LAM", 0, 5],
    TestID -> "term/assoc-normalizes-to-positional"
]

VerificationTest[
    Term[<|"tag" -> "VAR", "ext" -> 0, "val" -> 3, "sub" -> 1|>],
    Term["VAR", 0, 3, 1],
    TestID -> "term/assoc-with-sub-keeps-4-arg"
]

VerificationTest[
    Term[<|"tag" -> "ERA", "ext" -> 0, "val" -> 0, "sub" -> 0|>],
    Term["ERA", 0, 0],
    TestID -> "term/assoc-sub-zero-elides-4th-arg"
]

(* === cycle roundtrip: TLam[x, x] === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{orig, before, h, restored, after},
        orig = TLam[x, x];
        before = TTermExpr[orig];
        h = TContextSnapshot[orig];
        TReset[];
        restored = TInitialize[h];
        after = TTermExpr[restored];
        before === after
    ],
    True,
    TestID -> "heap-snapshot/lam-cycle-roundtrip"
]

(* === multi-cell roundtrip: TLam[x, TApp[x, TEra[]]] === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{orig, before, h, restored, after},
        orig = TLam[x, TApp[x, TEra[]]];
        before = TTermExpr[orig];
        h = TContextSnapshot[orig];
        TReset[];
        restored = TInitialize[h];
        after = TTermExpr[restored];
        before === after
    ],
    True,
    TestID -> "heap-snapshot/lam-app-era-roundtrip"
]

(* === SUP / DUP roundtrip preserves label sharing === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{orig, before, h, restored, after},
        orig = TSup[42, TEra[], TEra[]];
        before = TTermExpr[orig];
        h = TContextSnapshot[orig];
        TReset[];
        restored = TInitialize[h];
        after = TTermExpr[restored];
        before === after
    ],
    True,
    TestID -> "heap-snapshot/sup-explicit-label-roundtrip"
]

(* === fresh-label counter is restored === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{h, fresh1},
        TFreshLabel[]; TFreshLabel[]; TFreshLabel[];   (* counter -> 4 *)
        h = TContextSnapshot[TEra[]];
        TReset[];
        TFreshLabel[];                                 (* counter would be 2 if not restored *)
        TInitialize[h];
        fresh1 = TFreshLabel[];
        {First[h]["Labels"], fresh1}
    ],
    {4, 4},
    TestID -> "heap-snapshot/labels-restored"
]

(* === tensor: data preserved through Initialized roundtrip === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{orig, h, restored},
        orig = TTensor[{4}, {1., 2., 3., 4.}];
        h = TContextSnapshot[orig];
        TReset[];
        restored = TInitialize[h];
        Normal @ TTensorData[restored]
    ],
    {1., 2., 3., 4.},
    TestID -> "heap-snapshot/tensor-data-roundtrip"
]

VerificationTest[
    (* TEN slot ids are renumbered to dense 0..N-1.  Use TUOpAdd to
       bring both tensors into heap cells (a TUOpAdd UOP cell stores
       both TAG_TEN operand terms). *)
    TInit[];
    TReset[];
    Module[{a = TTensor[{2}, {1., 2.}], b = TTensor[{2}, {3., 4.}], expr, h, slots},
        expr = TUOpAdd[a, b];
        h = TContextSnapshot[expr];
        slots = Sort @ Keys @ First[h]["Tensors"];
        slots
    ],
    {0, 1},
    TestID -> "heap-snapshot/tensor-slots-dense"
]

(* === TContextStrip and ZeroFill init === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{orig, h, stripped},
        orig = TTensor[{3}, {7., 8., 9.}];
        h = TContextSnapshot[orig];
        stripped = TContextStrip[h];
        {First[stripped]["State"],
         AssociationQ @ First[stripped]["Tensors"][0],
         First[stripped]["Tensors"][0]["shape"],
         First[stripped]["Tensors"][0]["dtype"]}
    ],
    {"Uninitialized", True, {3}, "f32"},
    TestID -> "heap-strip/replaces-numeric-array"
]

VerificationTest[
    TInit[];
    TReset[];
    Module[{orig, h, stripped, restored},
        orig = TTensor[{3}, {7., 8., 9.}];
        h = TContextSnapshot[orig];
        stripped = TContextStrip[h];
        TReset[];
        restored = TInitialize[stripped, "ZeroFill" -> True];
        {Normal @ TTensorData[restored], TTensorShape[restored]}
    ],
    {{0., 0., 0.}, {3}},
    TestID -> "heap-init/zero-fill-uninitialized"
]

VerificationTest[
    (* Initializing an Uninitialized snapshot WITHOUT ZeroFill throws. *)
    TInit[];
    TReset[];
    Catch[
        Module[{h, stripped},
            h = TContextSnapshot[TTensor[{2}, {1., 2.}]];
            stripped = TContextStrip[h];
            TReset[];
            TInitialize[stripped]
        ],
        "TInitialize::uninit"
    ],
    $Failed,
    {TInitialize::uninit},
    TestID -> "heap-init/uninit-without-zerofill-throws"
]

(* === TContextToTermTree projection === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{orig = TLam[x, TApp[x, TEra[]]], h, viaProjection, viaWalk},
        viaWalk = TTermExpr[orig];
        h = TContextSnapshot[orig];
        viaProjection = TContextToTermTree[h];
        viaProjection === viaWalk
    ],
    True,
    TestID -> "heap-projection/matches-ttermexpr"
]

(* === textual InputForm roundtrip === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{orig, h, text, parsed, restored},
        orig = TLam[x, TApp[x, TEra[]]];
        h = TContextSnapshot[orig];
        text = ToString[h, InputForm];
        parsed = ToExpression[text];
        TReset[];
        restored = TInitialize[parsed];
        TTermExpr[restored] === TTermExpr[orig]
    ],
    True,
    TestID -> "heap-snapshot/inputform-textual-roundtrip"
]

(* === REF roundtrip ===
   thvm_wl_reset clears only HEAP/HEAP_NEXT/WNF_S_POS/ITRS, so DEFS
   and BOOK_HEAP survive across TReset.  A snapshot containing REF
   cells therefore restores correctly when the same kernel is reused
   (no need to re-register the def). *)

VerificationTest[
    (* Atomic REF cell: snapshot, reset, restore, apply, reduce. *)
    TInit[];
    TReset[];
    TDef["id-snap", TLam[x, x]];
    Module[{ref, h, restored, out},
        ref = TRef["id-snap"];
        h = TContextSnapshot[ref];
        TReset[];   (* defs + book preserved *)
        restored = TInitialize[h];
        out = TWnf @ TApp[restored, TEra[]];
        {TTagName[TTermTag[restored]], TTagName[TTermTag[out]]}
    ],
    {"REF", "ERA"},
    TestID -> "heap-snapshot/ref-atomic-roundtrip"
]

VerificationTest[
    (* REF embedded in a heap term: snapshot the unreduced expression,
       reset, restore, then reduce on the restored heap. *)
    TInit[];
    TReset[];
    TDef["id-snap2", TLam[x, x]];
    Module[{expr, h, restored, out},
        expr = TApp[TRef["id-snap2"], TEra[]];
        h = TContextSnapshot[expr];
        TReset[];
        restored = TInitialize[h];
        out = TWnf[restored];
        TTagName[TTermTag[out]]
    ],
    "ERA",
    TestID -> "heap-snapshot/ref-embedded-roundtrip"
]

(* === ALO roundtrip ===
   ALO cells are created by alo_realize when REF unfolds.  ALO_STATES
   and ALO_STATES_NEXT also survive thvm_wl_reset; combined with our
   1:1 cells-index = heap-loc layout, the new_loc references inside
   ALO_STATES stay valid after TInitialize, so an ALO cell from
   the snapshot continues to force correctly post-restore. *)

VerificationTest[
    (* Self-referential def yields a result whose body still contains
       ALO/REF cells (lazy unfolding); roundtrip the entire heap. *)
    TInit[];
    TReset[];
    TDef["loop-snap", TLam[x, TRef["loop-snap"]]];
    Module[{out1, h, restored, out2},
        out1 = TWnf @ TApp[TRef["loop-snap"], TEra[]];
        h = TContextSnapshot[out1];
        TReset[];
        restored = TInitialize[h];
        out2 = TWnf @ TApp[restored, TEra[]];
        {TTagName[TTermTag[out1]], TTagName[TTermTag[out2]]}
    ],
    {"LAM", "LAM"},
    TestID -> "heap-snapshot/alo-self-referential-roundtrip"
]

VerificationTest[
    (* TTermExpr equality for a REF-bearing reduced term.  The
       projection walks REF/ALO leaves so the structure is comparable
       across the snapshot/restore cycle. *)
    TInit[];
    TReset[];
    TDef["pair-snap", TLam[x, TLam[y, x]]];
    Module[{out, h, restored, before, after},
        out = TWnf @ TApp[TApp[TRef["pair-snap"], TEra[]], TEra[]];
        before = TTermExpr[out];
        h = TContextSnapshot[out];
        TReset[];
        restored = TInitialize[h];
        after = TTermExpr[restored];
        before === after
    ],
    True,
    TestID -> "heap-snapshot/alo-bearing-ttermexpr-equal"
]

(* === cross-restart bundling ===
   BookCells / Defs / AloStates are bundled in the snapshot so a
   restore after TFree + TInit (a fresh kernel) reconstructs the same
   runtime state.  TInitialize auto-detects the bundle and wipes
   book / DEFS / ALO_STATES via thvm_wl_book_reset before restoring. *)

VerificationTest[
    TInit[];
    TReset[];
    TDef["x-snap", TLam[x, x]];
    Module[{h},
        h = TContextSnapshot[TRef["x-snap"]];
        Length[First[h]["Defs"]] >= 1 &&
            Length[First[h]["BookCells"]] >= 1
    ],
    True,
    TestID -> "heap-snapshot/bundles-defs-and-bookcells"
]

VerificationTest[
    (* Full kernel restart: TFree + TInit wipes book/defs/alo state.
       TInitialize must rebuild everything from the bundle. *)
    TInit[];
    TReset[];
    TDef["restart-id", TLam[x, x]];
    Module[{h, restored, out},
        h = TContextSnapshot[TRef["restart-id"]];
        TFree[];
        TInit[];   (* fresh kernel: no defs, no book, no alo state *)
        restored = TInitialize[h];
        out = TWnf @ TApp[restored, TEra[]];
        {TTagName[TTermTag[restored]], TTagName[TTermTag[out]]}
    ],
    {"REF", "ERA"},
    TestID -> "heap-snapshot/cross-restart-ref"
]

VerificationTest[
    (* Cross-restart with an ALO-bearing reduced term.  After full
       wipe, the bundled book / defs / ALO_STATES are restored, then
       further reduction continues to work. *)
    TInit[];
    TReset[];
    TDef["restart-pair", TLam[x, TLam[y, x]]];
    Module[{out1, h, restored, expected, after},
        out1 = TWnf @ TApp[TApp[TRef["restart-pair"], TEra[]], TEra[]];
        expected = TTermExpr[out1];
        h = TContextSnapshot[out1];
        TFree[];
        TInit[];
        restored = TInitialize[h];
        after = TTermExpr[restored];
        after === expected
    ],
    True,
    TestID -> "heap-snapshot/cross-restart-alo-pair"
]

VerificationTest[
    (* Cross-restart self-referential def: restore must rebuild book
       cycle and DEFS slot, then reduce as before. *)
    TInit[];
    TReset[];
    TDef["restart-loop", TLam[x, TRef["restart-loop"]]];
    Module[{h, restored, out},
        h = TContextSnapshot[TRef["restart-loop"]];
        TFree[];
        TInit[];
        restored = TInitialize[h];
        out = TWnf @ TApp[restored, TEra[]];
        TTagName[TTermTag[out]]
    ],
    "LAM",
    TestID -> "heap-snapshot/cross-restart-self-ref"
]

VerificationTest[
    (* Cross-restart textual roundtrip: serialize a snapshot to text
       (InputForm), restart kernel, parse text back, restore. *)
    TInit[];
    TReset[];
    TDef["restart-txt", TLam[x, x]];
    Module[{orig, h, text, parsed, restored},
        orig = TRef["restart-txt"];
        h = TContextSnapshot[orig];
        text = ToString[h, InputForm];
        TFree[];
        TInit[];
        parsed = ToExpression[text];
        restored = TInitialize[parsed];
        TTagName[TTermTag[TWnf @ TApp[restored, TEra[]]]]
    ],
    "ERA",
    TestID -> "heap-snapshot/cross-restart-textual-roundtrip"
]

(* === Term[t_TTerm]: nested constructor form ===

   Distinct from the per-cell positional Term[tag, ext, val]: the
   walker produces a fully-unrolled tree whose every node is a
   `Term[head_String, args...]` expression with no heap locs (LAMs
   carry a binder id so VAR back-references match). *)

VerificationTest[
    TInit[];
    TReset[];
    Term[TSup[1, 2] + 3],
    Term["OP2", "+",
        Term["SUP", _, Term["NUM", 1], Term["NUM", 2]],
        Term["NUM", 3]],
    SameTest -> MatchQ,
    TestID -> "term/walker/sup-plus-num"
]

VerificationTest[
    TInit[];
    TReset[];
    Term[TLam[x, x + 3]],
    Term["LAM", _Integer,
        Term["OP2", "+", Term["VAR", _Integer], Term["NUM", 3]]],
    SameTest -> MatchQ,
    TestID -> "term/walker/lam-binder-and-body"
]

(* === TInitialize[Term[...]] round-trip ===

   Build a TTerm, snapshot to Term, reset the heap, then rebuild
   from Term and compare TTermExpr (which is heap-loc agnostic). *)

VerificationTest[
    Module[{orig, t, restored, before, after},
        TInit[];
        TReset[];
        orig = TLam[x, x + 3][TSup[1, 2]];
        t = Term[orig];
        before = TTermExpr[orig];
        TReset[];
        restored = TInitialize[t];
        after = TTermExpr[restored];
        before === after
    ],
    True,
    TestID -> "term/initialize/lam-app-sup-roundtrip"
]

VerificationTest[
    Module[{orig, t, restored, before, after},
        TInit[];
        TReset[];
        orig = TSup[1, 2] + 3;
        t = Term[orig];
        before = TTermExpr[orig];
        TReset[];
        restored = TInitialize[t];
        after = TTermExpr[restored];
        before === after
    ],
    True,
    TestID -> "term/initialize/op2-sup-roundtrip"
]

(* === Term[t] is what `CanonicalSlice` in TMultiTrace records,
   so distinct projections of a DUP map to distinct vertex IDs. *)
VerificationTest[
    TInit[];
    TReset[];
    Module[{dp0, dp1},
        {dp0, dp1} = TDup[3, 42];
        {
            MatchQ[Term[dp0], Term["DP0", 3, Term["NUM", 42]]],
            MatchQ[Term[dp1], Term["DP1", 3, Term["NUM", 42]]]
        }
    ],
    {True, True},
    TestID -> "term/walker/dp-projection-wrapping"
]
