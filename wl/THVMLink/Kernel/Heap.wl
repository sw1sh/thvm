(* ::Package:: *)
(* Heap.wl - portable, hand-authorable, roundtrippable snapshots of
   the live thvm heap.

   Heads:
     - Term[tag, ext, val] / Term[tag, ext, val, sub]: positional
       per-cell descriptor for a DYNAMIC-heap cell.  `val` is an
       index into the enclosing Heap's Cells list; for TAG_TEN cells
       `val` is a dense slot id keyed against TContext["Tensors"]; for
       TAG_NUM / TAG_REF / TAG_ERA `val` stays as raw bits / name id
       / 0.
     - BookCell[tag, ext, val] / BookCell[tag, ext, val, sub]: same
       schema as Term but for cells living in the BOOK heap (or for a
       book-domain term sitting at heap[ALO.val] inside the dyn heap).
       `val` is an index into TContext["BookCells"].
     - TContext[<|"Root", "Cells", "BookCells", "Tensors", "Defs",
              "AloStates", "Labels", "State"|>]: the full snapshot.

   Cross-restart roundtrip: bundling BookCells + Defs + AloStates lets
   a snapshot be loaded after TFree+TInit (a fresh kernel).  When any
   of those three keys is non-empty, TInitialize calls the C-side
   wipe (book + defs + alo states) before restoring everything from
   the bundle.  When they're all empty, TInitialize falls back to
   the dyn-only TReset path (no book/defs/alo to restore).

   Snapshot strategy: dump the entire [0, THeapPos[]) dyn range and
   the entire [0, BookPos[]) book range.  No reachability walk: it
   sidesteps variable-arity UOp / ALO arity logic and matches THeap[]
   semantics.  Cells at heap[ALO.val] are tagged BookCell because
   their Term's val refers to BOOK_HEAP, not the dyn heap.

   Tensor slot ids are renumbered to dense 0..N-1 during snapshot so
   the result survives heap resets and kernel restarts.

   Loaded from THVMLink.wl inside Begin["`Private`"]; sees the
   private bridge symbols (ttermRaw, $heapPosFn, $heapReadFn,
   $heapAllocFn, $heapSetFn, $termNewFn, $term*Fn, $tensor*Fn,
   uopCellCount, $tagNames, $op2Names, $uopNames, dtypeCode,
   $labelCounter, $defNames, $defNext) without qualification. *)

BeginPackage["THVMLink`"];

GeneralUtilities`SetUsage[TContext, "TContext[<|\"Root\", \"Cells\", \"BookCells\", \"Tensors\", \"Defs\", \"AloStates\", \"Labels\", \"State\"|>] is a portable snapshot of the live thvm runtime context.
Construct via TContextSnapshot[]; restore via TInitialize. When BookCells, Defs, or AloStates are non-empty the snapshot is self-contained and survives a fresh kernel (TFree then TInit)."];
GeneralUtilities`SetUsage[Term, "Term[t$] walks the live heap from TTerm t$ and returns a fully unrolled nested Term[head$, args$$] expression, the structural canonical form (LAM and DUP cells carry a binder id so VAR and DP cells reference back). Used as vertex identity in TMultiwayGraph and as a readable, serialisable representation of a TTerm, e.g. Term[\"OP2\", \"+\", Term[\"NUM\", 1], Term[\"NUM\", 2]].
Term[tag$, ext$, val$] and Term[tag$, ext$, val$, sub$] is a single heap-cell descriptor used inside TContext[\"Cells\"] (snapshot form); val$ is a heap loc and ext$ the secondary field (op code, dtype, label).
Term[<|\"tag\", \"ext\", \"val\", \"sub\"|>] in association form is accepted and normalized to the positional form."];
GeneralUtilities`SetUsage[BookCell, "BookCell[tag$, ext$, val$] is the same as Term[tag$, ext$, val$] but for a book-domain cell (living either in TContext[\"BookCells\"] or in the dyn cell at heap[ALO.val]); val$ is an index into TContext[\"BookCells\"]."];
GeneralUtilities`SetUsage[TContextSnapshot, "TContextSnapshot[] returns a TContext[<|$$|>] capturing every cell in [0, THeapPos[]), every book cell in [1, BookPos[]), all referenced tensors with data, the DEFS table (with name strings interned in TDef), and the ALO substitution chain.
TContextSnapshot[root$] additionally records the TTerm root$ as the snapshot's entry point."];
GeneralUtilities`SetUsage[TInitialize, "TInitialize[h$] restores the TContext snapshot h$ into the live runtime and returns the root as a live TTerm (or Missing[\"NoRoot\"] when the snapshot has no root). When BookCells, Defs, or AloStates are bundled it wipes the C-side book, DEFS, and ALO_STATES first, then restores them, then the dyn heap.
TInitialize[h$, \"ZeroFill\"] also accepts Uninitialized snapshots, allocating tensors zero-filled.
TInitialize[term$] rebuilds a nested Term expression into a freshly-allocated TTerm in the current dyn heap (the inverse of Term[t$])."];
GeneralUtilities`SetUsage[TContextStrip, "TContextStrip[h$] returns the TContext snapshot h$ with all NumericArray tensor buffers replaced by <|\"shape\", \"dtype\"|>. Pure WL; does not touch the runtime."];
GeneralUtilities`SetUsage[TContextToTermTree, "TContextToTermTree[h$] returns a nested string-headed expression mirroring TTermExpr but driven by the Cells list of the TContext snapshot h$ (no live runtime needed). Cycles render as \"Cycle\"[idx$]. Read-only projection.
TContextToTermTree[h$, root$] walks from the given Term root$ instead of the snapshot's recorded root."];

(* Forward references: the high-level constructors live in sibling
   files (Switch.wl, etc.) that load AFTER Heap.wl in alphabetical
   FileNames order.  Touching the symbols here interns them in the
   PUBLIC `THVMLink`` context so references inside our Private body
   resolve correctly when used. *)
TOp2; TLam; TApp; TSup; TDup; TNum; TEra; TAny;

Begin["`Private`"];

(* === library function loaders for cross-restart bundling ===
   `load` is defined in THVMLink.wl Private context and visible to
   every sibling.  These wrap the new thvm_wl_book_* / thvm_wl_def_*
   / thvm_wl_alo_* exports added in CSource/thvmlink.c. *)

$bookPosFn       := $bookPosFn       = load["thvm_wl_book_pos",        {},                            Integer]
$bookReadFn      := $bookReadFn      = load["thvm_wl_book_read",       {Integer},                     Integer]
$bookAllocFn     := $bookAllocFn     = load["thvm_wl_book_alloc",      {Integer},                     Integer]
$bookSetFn       := $bookSetFn       = load["thvm_wl_book_set",        {Integer, Integer},            Integer]
$bookSetNextFn   := $bookSetNextFn   = load["thvm_wl_book_set_next",   {Integer},                     Integer]
$bookResetFn     := $bookResetFn     = load["thvm_wl_book_reset",      {},                            Integer]
$defGetFn        := $defGetFn        = load["thvm_wl_def_get",         {Integer},                     Integer]
$defSetFn        := $defSetFn        = load["thvm_wl_def_set",         {Integer, Integer},            Integer]
$aloStatesNextFn := $aloStatesNextFn = load["thvm_wl_alo_states_next", {},                            Integer]
$aloStateParentFn := $aloStateParentFn = load["thvm_wl_alo_state_parent",  {Integer},                 Integer]
$aloStateOldLocFn := $aloStateOldLocFn = load["thvm_wl_alo_state_old_loc", {Integer},                 Integer]
$aloStateNewLocFn := $aloStateNewLocFn = load["thvm_wl_alo_state_new_loc", {Integer},                 Integer]
$aloStateSetFn   := $aloStateSetFn   = load["thvm_wl_alo_state_set",       {Integer, Integer, Integer, Integer}, Integer]
$aloStatesSetNextFn := $aloStatesSetNextFn = load["thvm_wl_alo_states_set_next", {Integer},          Integer]

(* === reverse name maps === *)

$tagCode = AssociationMap[Reverse, $tagNames];
$op2Code = AssociationMap[Reverse, $op2Names];
$uopCode = AssociationMap[Reverse, $uopNames];

tagToCode[s_String] := Lookup[$tagCode, s, $Failed]
tagToCode[i_Integer] := i

op2ToCode[s_String] := Lookup[$op2Code, s, $Failed]
op2ToCode[i_Integer] := i

uopToCode[s_String] := Lookup[$uopCode, s, $Failed]
uopToCode[i_Integer] := i

(* dtypeCode/dtypeName cover the full enum (THVMLink.wl).
   dtypeNameSafe lets a string fall through unchanged. *)

dtypeNameSafe[d_Integer] := dtypeName[d]
dtypeNameSafe[d_]        := d

numericArrayDType[na_NumericArray] := Switch[ NumericArrayType[na],
    "Real32",    "f32",
    "Integer32", "i32",
    _,           "f32"
]

(* === Term / BookCell: Association-form normalization to positional === *)

Term[a_Association] /; KeyExistsQ[a, "tag"] && KeyExistsQ[a, "ext"] && KeyExistsQ[a, "val"] :=
    With[{tag = a["tag"], ext = a["ext"], val = a["val"], sub = Lookup[a, "sub", 0]},
        If[ sub === 0,
            Term[tag, ext, val],
            Term[tag, ext, val, sub]
        ]
    ]

BookCell[a_Association] /; KeyExistsQ[a, "tag"] && KeyExistsQ[a, "ext"] && KeyExistsQ[a, "val"] :=
    With[{tag = a["tag"], ext = a["ext"], val = a["val"], sub = Lookup[a, "sub", 0]},
        If[ sub === 0,
            BookCell[tag, ext, val],
            BookCell[tag, ext, val, sub]
        ]
    ]

(* === Term[t_TTerm]: TTerm -> nested Term[head, args...] form ===

   Walks the live heap from `t`, chasing SUB-flagged DP / VAR
   projections, and returns a fully unrolled `Term[...]` tree.  Used
   as canonical vertex identity in TMultiwayGraph (two terms with
   the same structural content compare equal via SameQ regardless of
   heap layout) and as a readable / serialisable representation of a
   TTerm.

   Depth-bounded at 16 to break cycles (e.g. self-referential ALO);
   over-deep subtrees collapse to Term["..."].  LAMs / DUPs carry a
   binder id (the lam_loc / dup_loc) so VAR / DP references can
   match the binder symbolically: this id IS the heap loc today
   but is treated as an opaque binder identifier in the
   structural-equality sense, matching how TLam[x, body]'s `x` is
   alpha-renamable. *)
$termDepthDefault = 16;

Term[t_TTerm] := termFromTTermDepth[t, $termDepthDefault]

termFromTTermDepth[t_TTerm, 0] := Term["..."]
termFromTTermDepth[t_TTerm, d_Integer] := Block[
    {tag, val, ext, cell},
    tag = TTermTag[t];
    val = TTermVal[t];
    ext = TTermExt[t];
    Switch[tag,
        $TagNUM, Term["NUM", val],
        $TagERA, Term["ERA"],
        $TagTEN, Term["TEN", val],
        $TagREF, Term["REF", ext],
        $TagANY, Term["ANY"],
        $TagDP0 | $TagDP1,
            cell = THeapRead[val];
            If[ TTermSub[cell] === 1,
                (* DUP-X fired: projection resolved, chase through
                   to the substituted branch's canonical. *)
                termFromTTermDepth[
                    packTerm[0, TTermTag[cell], TTermExt[cell],
                             TTermVal[cell]],
                    d - 1],
                (* Pre-resolution: wrap the body's term with the
                   projection index (DP0 / DP1) and the dup label so
                   DP0 and DP1 of the same DUP are distinct. *)
                Term[
                    If[ tag === $TagDP0, "DP0", "DP1"], ext,
                    termFromTTermDepth[cell, d - 1]]],
        $TagVAR,
            cell = THeapRead[val];
            If[ TTermSub[cell] === 1,
                termFromTTermDepth[
                    packTerm[0, TTermTag[cell], TTermExt[cell],
                             TTermVal[cell]],
                    d - 1],
                Term["VAR", val]],
        $TagLAM,
            Term["LAM", val, termFromTTermDepth[THeapRead[val], d - 1]],
        $TagAPP,
            Term["APP",
                termFromTTermDepth[THeapRead[val + 0], d - 1],
                termFromTTermDepth[THeapRead[val + 1], d - 1]],
        $TagSUP,
            Term["SUP", ext,
                termFromTTermDepth[THeapRead[val + 0], d - 1],
                termFromTTermDepth[THeapRead[val + 1], d - 1]],
        $TagDUP,
            Term["DUP", val,
                termFromTTermDepth[THeapRead[val], d - 1]],
        $TagOP2,
            Term["OP2", Lookup[$op2Names, ext, ext],
                termFromTTermDepth[THeapRead[val + 0], d - 1],
                termFromTTermDepth[THeapRead[val + 1], d - 1]],
        $TagMAT,
            Term["MAT", ext,
                termFromTTermDepth[THeapRead[val + 0], d - 1],
                termFromTTermDepth[THeapRead[val + 1], d - 1]],
        $TagCTR,
            Block[{n = TTermVal[THeapRead[val]]},
                Term @@ Join[
                    {"CTR", ext},
                    Table[
                        termFromTTermDepth[THeapRead[val + 1 + i], d - 1],
                        {i, 0, n - 1}]]],
        $TagALO,
            Term["ALO", TTermVal[THeapRead[val + 1]],
                termFromTTermDepth[THeapRead[val + 0], d - 1]],
        $TagUOP,
            Block[{opName = Lookup[$uopNames, ext, ext],
                   arity  = uopCellCount[ext]},
                Term @@ Join[
                    {"UOP", opName},
                    Table[
                        termFromTTermDepth[THeapRead[val + i], d - 1],
                        {i, 0, arity - 1}]]],
        _, Term[TTagName[tag], val, ext]]
]

(* === predicates === *)

contextPayloadQ[a_Association] :=
    KeyExistsQ[a, "Cells"] && KeyExistsQ[a, "Tensors"]
contextPayloadQ[___] := False

contextNewQ[TContext[a_Association]] := contextPayloadQ[a]
contextNewQ[___] := False

contextStateOf[a_Association] := With[{vals = Values[a]},
    Which[
        vals === {},                                "Initialized",
        AllTrue[vals, MatchQ[#, _NumericArray] &],   "Initialized",
        AllTrue[vals, AssociationQ],                 "Uninitialized",
        True,                                        "Mixed"
    ]
]

(* === TContextSnapshot ===
   Walk every cell in [0, THeapPos[]) (dyn) and [1, BookPos[]) (book).
   Collect every TAG_TEN val (the runtime tensor id) across BOTH
   heaps + def roots, renumber to dense 0..N-1.  Read each tensor's
   data as a NumericArray.  Bundle the DEFS table (with name strings
   from $defNames where available) and the ALO_STATES chain so the
   snapshot can be restored after a fresh kernel. *)

collectTensorRemap[allRaws_List] := Module[{ids},
    ids = DeleteDuplicates @ Cases[
        allRaws,
        r_Integer /; r =!= 0 && $termTagFn[r] === $TagTEN :> $termValFn[r]
    ];
    AssociationThread[ids -> Range[0, Length[ids] - 1]]
]

(* Convert a packed runtime u64 into a portable head[...] (Term or
   BookCell).  Only TAG_TEN val is rewritten (runtime tensor id ->
   dense slot).  All other val fields are heap locs (matching Cells
   or BookCells indices since both are 1:1 with locs) or atomic data
   (NUM bits, REF slot, ERA 0).  ext is rendered symbolically for
   TAG_OP2 / TAG_UOP / TAG_TEN / TAG_NUM. *)

rawToHead[head_, raw_Integer, remap_Association] := Module[{
    tag, ext, val, sub, tagName, extOut, valOut
},
    tag = $termTagFn[raw];
    ext = $termExtFn[raw];
    val = $termValFn[raw];
    sub = $termSubFn[raw];
    tagName = Lookup[$tagNames, tag, "TAG?" <> ToString[tag]];
    extOut = Switch[tag,
        $TagOP2,            Lookup[$op2Names, ext, ext],
        $TagUOP,            Lookup[$uopNames, ext, ext],
        $TagTEN | $TagNUM,  dtypeNameSafe[ext],
        _,                  ext
    ];
    valOut = If[ tag === $TagTEN,
        Lookup[remap, val, val],
        val
    ];
    If[ sub === 0,
        head[tagName, extOut, valOut],
        head[tagName, extOut, valOut, sub]
    ]
]

rawToTerm[raw_, remap_]     := rawToHead[Term,     raw, remap]
rawToBookCell[raw_, remap_] := rawToHead[BookCell, raw, remap]

(* For each TAG_ALO cell at dyn loc K, mark heap[ALO.val] as a
   book-domain holder: the term sitting in that cell has its `val`
   pointing into BOOK_HEAP, so it must be serialized as BookCell. *)
collectBookHolderIndices[cellRaws_List] :=
    DeleteDuplicates @ Cases[
        cellRaws,
        r_Integer /; $termTagFn[r] === $TagALO :> $termValFn[r]
    ]

(* Snapshot the BOOK heap at locs [1, BookPos[]).  Keys of BookCells
   are book locs directly (1-indexed; loc 0 is reserved). *)
snapshotBookCells[remap_Association] := Module[{nb},
    nb = $bookPosFn[];
    Association @ Table[
        i -> rawToBookCell[$bookReadFn[i], remap],
        {i, 1, nb - 1}
    ]
]

(* Snapshot DEFS: iterate slots [0, DEFS_CAP), keep non-zero entries.
   Reverse-lookup name string from $defNames where available. *)
snapshotDefs[remap_Association] := Module[{slotToName},
    slotToName = If[ AssociationQ[$defNames],
        AssociationMap[Reverse, $defNames],
        <||>
    ];
    Association @ Table[
        With[{rootRaw = $defGetFn[slot]},
            If[ rootRaw =!= 0,
                slot -> <|
                    "name" -> Lookup[slotToName, slot, None],
                    "root" -> rawToBookCell[rootRaw, remap]
                |>,
                Nothing
            ]
        ],
        {slot, 0, 255}
    ]
]

(* Snapshot ALO_STATES at ids [1, ALO_STATES_NEXT).  Returned as an
   ordered list whose index i (1-based) corresponds to state id i. *)
snapshotAloStates[] := Module[{n},
    n = $aloStatesNextFn[];
    Table[
        <|
            "parent"  -> $aloStateParentFn[i],
            "old_loc" -> $aloStateOldLocFn[i],
            "new_loc" -> $aloStateNewLocFn[i]
        |>,
        {i, 1, n - 1}
    ]
]

TContextSnapshot[]                    := TContextSnapshot[Missing["NoRoot"]]

TContextSnapshot[root_TTerm]          := snapshotImpl[ttermRaw[root]]
TContextSnapshot[Missing["NoRoot"]]   := snapshotImpl[Missing["NoRoot"]]
TContextSnapshot[None]                := snapshotImpl[Missing["NoRoot"]]

snapshotImpl[rootRawOrMissing_] := Module[{
    lo, n, nb, cellRaws, cellLocs, bookRaws, defRootRaws, bookHolders,
    allRaws, remap,
    cells, bookCells, tensors, defs, aloStates, rootTerm,
    extraRaws = If[ IntegerQ[rootRawOrMissing], {rootRawOrMissing}, {}]
},
    ensureInit[];
    lo          = $heapBaseFn[];
    n           = $heapPosFn[];
    cellLocs    = Range[lo, n - 1];
    cellRaws    = $heapReadFn /@ cellLocs;
    nb          = $bookPosFn[];
    bookRaws    = Table[$bookReadFn[i],   {i, 1, nb - 1}];
    defRootRaws = Table[$defGetFn[slot],  {slot, 0, 255}];
    bookHolders = collectBookHolderIndices[cellRaws];

    allRaws = Join[cellRaws, bookRaws, defRootRaws, extraRaws];
    remap   = collectTensorRemap[allRaws];

    cells = Association @ MapIndexed[
        {raw, idx} |-> With[{i = First[idx] - 1, locOff = lo},
            (i + locOff) -> If[ MemberQ[bookHolders, i],
                rawToBookCell[raw, remap],
                rawToTerm[raw, remap]
            ]
        ],
        cellRaws
    ];
    bookCells = snapshotBookCells[remap];
    tensors   = Association @ KeyValueMap[
        {rid, slot} |-> slot -> $tensorReadFn[rid],
        remap
    ];
    defs      = snapshotDefs[remap];
    aloStates = snapshotAloStates[];
    rootTerm  = If[ IntegerQ[rootRawOrMissing],
        rawToTerm[rootRawOrMissing, remap],
        Missing["NoRoot"]
    ];
    TContext[<|
        "Root"      -> rootTerm,
        "Cells"     -> cells,
        "BookCells" -> bookCells,
        "Tensors"   -> tensors,
        "Defs"      -> defs,
        "AloStates" -> aloStates,
        "Labels"    -> $labelCounter,
        "State"     -> contextStateOf[tensors]
    |>]
]

(* === TContextStrip ===
   Replace each NumericArray with <|"shape", "dtype"|>.  Pure WL. *)

stripTensorEntry[na_NumericArray] := <|
    "shape" -> Dimensions[na],
    "dtype" -> numericArrayDType[na]
|>
stripTensorEntry[a_Association] := a

TContextStrip[TContext[a_Association]] := Module[{stripped},
    stripped = Map[stripTensorEntry, Lookup[a, "Tensors", <||>]];
    TContext[<|
        "Root"      -> Lookup[a, "Root",      Missing["NoRoot"]],
        "Cells"     -> Lookup[a, "Cells",     <||>],
        "BookCells" -> Lookup[a, "BookCells", <||>],
        "Tensors"   -> stripped,
        "Defs"      -> Lookup[a, "Defs",      <||>],
        "AloStates" -> Lookup[a, "AloStates", {}],
        "Labels"    -> Lookup[a, "Labels", 1],
        "State"     -> contextStateOf[stripped]
    |>]
]

(* === TInitialize ===
   1. If BookCells / Defs / AloStates are non-empty, wipe BOOK_HEAP +
      DEFS + ALO_STATES via thvm_wl_book_reset, then restore them.
      Otherwise leave them alone (legacy in-session restore).
   2. TReset clears the dyn heap.
   3. Allocate fresh tensors for every Tensors slot (Initialized:
      writes data; Uninitialized: alloc only, zero-fill on demand).
   4. Restore book cells (if bundled).
   5. Restore DEFS slots and re-intern names in $defNames.
   6. Restore ALO_STATES entries and ALO_STATES_NEXT.
   7. THeapAlloc[Length[Cells]] in one go (heap is fresh, so base = 0
      and Cells indices line up 1:1 with heap locs).
   8. Iterate writing each cell, remapping TAG_TEN val through the
      slot -> runtime-id map.  Both Term and BookCell cells pack via
      the same termToRaw; val passes through directly because
      cell-domain interpretation is the receiver's responsibility. *)

Options[TInitialize] = {"ZeroFill" -> False}

(* === TInitialize[Term[...]]: construct a nested Term in the live heap ===

   Inverse of `Term[t_TTerm]`.  Walks the nested expression
   bottom-up, calling the standard high-level constructors
   (TNum / TEra / TSup / TApp / TLam / TDup / TOp2 / ...) so the
   result is a freshly-allocated TTerm in the current dyn heap.

   Binder ids in LAM / DUP / DP0 / DP1 are alpha-renamed during the
   walk; the id in the Term is just a placeholder telling the
   walker which VAR / DP cells inside the body refer back to which
   binder.  Fresh symbols are minted via Unique[] and substituted
   for the placeholders so the high-level TLam / TDup machinery
   handles binding correctly. *)

TInitialize[t_TTerm] := t                                     (* identity *)
TInitialize[Term["NUM", v_Integer]]      := TNum[v]
TInitialize[Term["NUM", _, v_Integer]]   := TNum[v]
TInitialize[Term["ERA"]]                 := TEra[]
TInitialize[Term["ANY"]]                 := TAny[]
TInitialize[Term["REF", slot_Integer]]   := packTerm[0, $TagREF, slot, 0]
TInitialize[Term["TEN", id_Integer]]     := packTerm[0, $TagTEN, 5, id]
TInitialize[Term["VAR", binderId_]]      :=
    (Message[TInitialize::orphanvar, binderId]; $Failed)
TInitialize::orphanvar = "Term[\"VAR\", ``] outside any enclosing Term[\"LAM\", ``, ...] - VARs only make sense in a LAM body.";

(* Identity rule for already-built TTerms: lets nested recursion mix
   freshly-built TTerm leaves into a Term-shaped expression. *)
TInitialize[s_TTerm] := s

TInitialize[Term["LAM", binderId_, body_]] :=
    Block[{xSym},
        xSym = Unique["x$"];
        (* Substitute Term["VAR", binderId] with a Term["@VAR", xSym]
           placeholder before handing the body to TLam.  TLam's
           HoldAll + Function[xSym, body] then alpha-substitutes xSym
           -> TVarFor[loc] inside the held body, so each placeholder
           becomes Term["@VAR", TVarFor[loc]], which TInitialize's
           leaf rule below unwraps to TVarFor[loc] during the walk. *)
        TLam[xSym,
            TInitialize[body /. Term["VAR", binderId] :> Term["@VAR", xSym]]]
    ]

TInitialize[Term["@VAR", t_]] := t

TInitialize[Term["APP", f_, x_]] := TApp[TInitialize[f], TInitialize[x]]

TInitialize[Term["SUP", label_Integer, a_, b_]] :=
    TSup[label, TInitialize[a], TInitialize[b]]

TInitialize[Term["OP2", op_, a_, b_]] :=
    TOp2[op, TInitialize[a], TInitialize[b]]

(* DUP / DP0 / DP1 share a body cell; the binder id in the Term tells
   us which DP-projection sites in nested bodies refer back to it.
   First TDup the body, then substitute the projection slots in the
   outer body, then pick the appropriate projection. *)
TInitialize[Term["DUP", binderId_, body_]] :=
    Block[{dp0, dp1, b = TInitialize[body]},
        {dp0, dp1} = TDup[b];
        TDup[{dp0, dp1}]
    ]
TInitialize[Term["DP0", label_Integer, body_]] :=
    First @ TDup[label, TInitialize[body]]
TInitialize[Term["DP1", label_Integer, body_]] :=
    Last  @ TDup[label, TInitialize[body]]

TInitialize[TContext[a_Association], opts:OptionsPattern[]] := Module[{
    cellsAssoc, bookCellsAssoc, tensorsAssoc, defs, aloStates,
    root, labels, zeroFill, hasBundle,
    cellList, n, bookKeys, base, slotToRuntime, rootRaw
},
    ensureInit[];
    zeroFill        = TrueQ @ OptionValue["ZeroFill"];
    cellsAssoc      = Lookup[a, "Cells",     <||>];
    bookCellsAssoc  = Lookup[a, "BookCells", <||>];
    tensorsAssoc    = Lookup[a, "Tensors",   <||>];
    defs            = Lookup[a, "Defs",      <||>];
    aloStates       = Lookup[a, "AloStates", {}];
    root            = Lookup[a, "Root",      Missing["NoRoot"]];
    labels          = Lookup[a, "Labels", 1];

    hasBundle = Length[bookCellsAssoc] > 0
              || Length[defs]           > 0
              || Length[aloStates]      > 0;

    If[ hasBundle, $bookResetFn[]];
    TReset[];

    slotToRuntime = Association @ KeyValueMap[
        {slot, entry} |-> slot -> initTensorEntry[entry, zeroFill],
        tensorsAssoc
    ];

    (* Restore book cells.  BookCells keys are book locs (1-indexed). *)
    If[ Length[bookCellsAssoc] > 0,
        bookKeys = Sort @ Keys[bookCellsAssoc];
        $bookAllocFn[Last[bookKeys]];
        Do[
            $bookSetFn[k, termToRaw[bookCellsAssoc[k], slotToRuntime]],
            {k, bookKeys}
        ]
    ];

    (* Restore DEFS slots and re-intern names in Ref.wl's $defNames. *)
    KeyValueMap[
        {slot, entry} |-> (
            $defSetFn[slot, termToRaw[entry["root"], slotToRuntime]];
            With[{name = Lookup[entry, "name", None]},
                If[ StringQ[name],
                    $defNames[name] = slot;
                    If[ slot >= $defNext, $defNext = slot + 1]
                ]
            ]
        ),
        defs
    ];

    (* Restore ALO_STATES.  List index i (1-based) -> state id i. *)
    If[ Length[aloStates] > 0,
        MapIndexed[
            {entry, idx} |-> $aloStateSetFn[
                First[idx],
                entry["parent"], entry["old_loc"], entry["new_loc"]
            ],
            aloStates
        ];
        $aloStatesSetNextFn[Length[aloStates] + 1]
    ];

    (* Restore dyn heap. *)
    cellList = Values @ KeySort[cellsAssoc];
    n = Length[cellList];
    base = If[ n > 0, $heapAllocFn[n], 0];
    Do[
        $heapSetFn[base + i - 1, termToRaw[cellList[[i]], slotToRuntime]],
        {i, n}
    ];

    $labelCounter = labels;

    rootRaw = Which[
        MatchQ[root, _Term] || MatchQ[root, _BookCell],
            termToRaw[root, slotToRuntime],
        True,
            Missing["NoRoot"]
    ];
    If[ MissingQ[rootRaw], Missing["NoRoot"], TTerm[rootRaw]]
]

(* Allocate + (optionally) write a single tensor entry; return the
   runtime tensor id (TenDesc slot, not the packed TAG_TEN term). *)
initTensorEntry[na_NumericArray, _] := Module[{
    shape, dtype, rid
},
    shape = Dimensions[na];
    dtype = numericArrayDType[na];
    rid   = TTermVal @ $tensorAllocFn[dtypeCode[dtype], shape];
    If[ dtype === "f32",
        $tensorWriteFn [rid, N    @ Flatten @ Normal @ na],
        $tensorWriteIFn[rid, Round @ Flatten @ Normal @ na]
    ];
    rid
]

initTensorEntry[a_Association, zeroFill_] := Module[{
    shape = a["shape"], dtype = a["dtype"], rid, count
},
    If[ ! zeroFill,
        Message[TInitialize::uninit];
        Throw[$Failed, "TInitialize::uninit"]
    ];
    rid   = TTermVal @ $tensorAllocFn[dtypeCode[dtype], shape];
    count = Times @@ shape;
    If[ dtype === "f32",
        $tensorWriteFn [rid, ConstantArray[0., count]],
        $tensorWriteIFn[rid, ConstantArray[0,  count]]
    ];
    rid
]

TInitialize::uninit = "TInitialize: snapshot is Uninitialized; pass \"ZeroFill\" -> True to allocate zero-filled tensor buffers.";

(* Convert a Term[...] back into a packed runtime u64 via $termNewFn.
   Symbolic ext fields for OP2 / UOP / TEN / NUM are decoded back to
   integer codes; TAG_TEN val is remapped via slotMap. *)

(* Term/BookCell[a_Association] normalize via the global rewrite up
   top, so by the time we get here `t` is positional.  Both heads
   pack identically; domain interpretation (dyn vs book loc) is the
   C-side runtime's job and only matters for tag-context references
   (TAG_ALO).  We just preserve val. *)

packCell[tag_String, ext_, val_, sub_, slotMap_] := Module[{
    tagCode, extCode, valCode
},
    tagCode = tagToCode[tag];
    extCode = Switch[tagCode,
        $TagOP2,            op2ToCode[ext],
        $TagUOP,            uopToCode[ext],
        $TagTEN | $TagNUM,  dtypeCode[ext],
        _,                  ext
    ];
    valCode = If[ tagCode === $TagTEN,
        Lookup[slotMap, val, val],
        val
    ];
    $termNewFn[sub, tagCode, extCode, valCode]
]

termToRaw[Term[tag_String, ext_, val_],                slotMap_] := packCell[tag, ext, val, 0,   slotMap]
termToRaw[Term[tag_String, ext_, val_, sub_Integer],   slotMap_] := packCell[tag, ext, val, sub, slotMap]
termToRaw[BookCell[tag_String, ext_, val_],            slotMap_] := packCell[tag, ext, val, 0,   slotMap]
termToRaw[BookCell[tag_String, ext_, val_, sub_Integer], slotMap_] := packCell[tag, ext, val, sub, slotMap]

(* === TContextToTermTree ===
   Recursive nested-Term view, mirroring tTreeWalk in THVMLink.wl
   but reading from a Cells Association instead of $heapReadFn.
   Cycles render as "Cycle"[idx].  Read-only projection. *)

TContextToTermTree[TContext[a_Association]] := With[{
    root = Lookup[a, "Root", Missing["NoRoot"]]
},
    If[ MatchQ[root, _Term],
        cellTreeWalkTerm[a["Cells"], root, <||>],
        Missing["NoRoot"]
    ]
]

TContextToTermTree[TContext[a_Association], root_Term] :=
    cellTreeWalkTerm[a["Cells"], root, <||>]

cellTreeWalkLoc[cells_, idx_Integer, seen_] := With[{
    cell = Lookup[cells, idx, Missing["NoCell", idx]]
},
    Which[
        MatchQ[cell, _Term],     cellTreeWalkTerm[cells, cell, seen],
        MatchQ[cell, _BookCell], "Book"[cell[[1]], cell[[2]], cell[[3]]],
        True,                    cell
    ]
]

cellTreeWalkTerm[cells_, Term[tag_String, ext_, val_, ___], seen_] := Module[{
    tagCode = tagToCode[tag], n, seen2, stateCell
},
    Switch[tagCode,
        $TagERA, "ERA",
        $TagVAR, "VAR"[val],
        (* Mirror TTermExpr in THVMLink.wl: drop dtype for the common
           i32 case, surface "f32" only for f32 NUM cells. *)
        $TagNUM, If[ext === 0, "NUM"["f32", val], "NUM"[val]],
        $TagTEN, "TEN"[val],
        $TagREF, "REF"[ext, val],
        $TagDP0, "DP0"[ext, cellTreeWalkLoc[cells, val, seen]],
        $TagDP1, "DP1"[ext, cellTreeWalkLoc[cells, val, seen]],
        $TagLAM,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "LAM"[cellTreeWalkLoc[cells, val, seen2]]
            ],
        $TagAPP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "APP"[cellTreeWalkLoc[cells, val,     seen2],
                      cellTreeWalkLoc[cells, val + 1, seen2]]
            ],
        $TagSUP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "SUP"[ext,
                      cellTreeWalkLoc[cells, val,     seen2],
                      cellTreeWalkLoc[cells, val + 1, seen2]]
            ],
        $TagDUP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "DUP"[ext, cellTreeWalkLoc[cells, val, seen2]]
            ],
        $TagALO,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                stateCell = Lookup[cells, val + 1, Missing["NoCell"]];
                "ALO"[
                    If[ MatchQ[stateCell, Term[_, _, _, ___]],
                        stateCell[[3]],
                        Missing["BadAlo"]
                    ],
                    cellTreeWalkLoc[cells, val, seen2]
                ]
            ],
        $TagOP2,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "OP2"[ext,
                      cellTreeWalkLoc[cells, val,     seen2],
                      cellTreeWalkLoc[cells, val + 1, seen2]]
            ],
        $TagMAT,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                "MAT"[ext,
                      cellTreeWalkLoc[cells, val,     seen2],
                      cellTreeWalkLoc[cells, val + 1, seen2]]
            ],
        $TagUOP,
            If[ KeyExistsQ[seen, val], "Cycle"[val],
                seen2 = Append[seen, val -> True];
                n = uopCellCount[uopToCode[ext]];
                "UOP" @@ Prepend[
                    Table[cellTreeWalkLoc[cells, val + i, seen2], {i, 0, n - 1}],
                    ext
                ]
            ],
        _, "Unknown"[tag]
    ]
]

End[];
EndPackage[];
