(* ::Package:: *)
(* Heap.wl -- portable, hand-authorable, roundtrippable snapshots of
   the live thvm heap.

   Heads:
     - Term[tag, ext, val] / Term[tag, ext, val, sub]: positional
       per-cell descriptor for a DYNAMIC-heap cell.  `val` is an
       index into the enclosing Heap's Cells list; for TAG_TEN cells
       `val` is a dense slot id keyed against Heap["Tensors"]; for
       TAG_NUM / TAG_REF / TAG_ERA `val` stays as raw bits / name id
       / 0.
     - BookTerm[tag, ext, val] / BookTerm[tag, ext, val, sub]: same
       schema as Term but for cells living in the BOOK heap (or for a
       book-domain term sitting at heap[ALO.val] inside the dyn heap).
       `val` is an index into Heap["BookCells"].
     - Heap[<|"Root", "Cells", "BookCells", "Tensors", "Defs",
              "AloStates", "Labels", "State"|>]: the full snapshot.

   Cross-restart roundtrip: bundling BookCells + Defs + AloStates lets
   a snapshot be loaded after TFree+TInit (a fresh kernel).  When any
   of those three keys is non-empty, HeapInitialize calls the C-side
   wipe (book + defs + alo states) before restoring everything from
   the bundle.  When they're all empty, HeapInitialize falls back to
   the legacy TReset + dyn-only restore.

   Snapshot strategy: dump the entire [0, THeapPos[]) dyn range and
   the entire [0, BookPos[]) book range.  No reachability walk -- it
   sidesteps variable-arity UOp / ALO arity logic and matches THeap[]
   semantics.  Cells at heap[ALO.val] are tagged BookTerm because
   their Term's val refers to BOOK_HEAP, not the dyn heap.

   Tensor slot ids are renumbered to dense 0..N-1 during snapshot so
   the result survives heap resets and kernel restarts.

   Loaded from THVMLink.wl inside Begin["`Private`"]; sees the
   private bridge symbols (ttermRaw, $heapPosFn, $heapReadFn,
   $heapAllocFn, $heapSetFn, $termNewFn, $term*Fn, $tensor*Fn,
   uopCellCount, $tagNames, $op2Names, $uopNames, dtypeCode,
   $labelCounter, $defNames, $defNext) without qualification. *)

BeginPackage["THVMLink`"];

Heap::usage         = "Heap[<|\"Root\", \"Cells\", \"BookCells\", \"Tensors\", \"Defs\", \"AloStates\", \"Labels\", \"State\"|>] is a portable snapshot of the live heap.  Construct via HeapSnapshot[]; restore via HeapInitialize.  When BookCells / Defs / AloStates are non-empty, the snapshot is self-contained and survives a fresh kernel (TFree + TInit).";
Term::usage         = "Term[tag_String, ext, val] (or Term[tag, ext, val, sub]) is a dynamic-heap cell descriptor inside a Heap.  `val` is heap-relative -- an index into the enclosing Heap's Cells list.  For TAG_TEN cells, `val` is a dense slot id keyed in Heap[\"Tensors\"]; for TAG_NUM, val is the raw bits; for TAG_REF, val is the def slot.  Hand-authored Term[<|\"tag\", \"ext\", \"val\", \"sub\"|>] is accepted and normalized to the positional form.";
BookTerm::usage     = "BookTerm[tag_String, ext, val] is the same as Term[...] but for a BOOK-domain cell (lives either in Heap[\"BookCells\"] or in the dyn cell at heap[ALO.val]).  `val` is an index into Heap[\"BookCells\"].";
HeapSnapshot::usage = "HeapSnapshot[] returns a Heap[<|...|>] capturing every cell in [0, THeapPos[]), every book cell in [1, BookPos[]), all referenced tensors with data, the DEFS table (with name strings interned in TDef), and the ALO substitution chain.  HeapSnapshot[root_TTerm] additionally records `root` as the snapshot's entry point.";
HeapInitialize::usage = "HeapInitialize[h_Heap] restores `h` into the live runtime and returns the root as a live TTerm (or Missing[\"NoRoot\"] if the snapshot has no root).  Cross-restart capable: when BookCells / Defs / AloStates are bundled, HeapInitialize wipes the C-side book / DEFS / ALO_STATES first, then restores them, then the dyn heap.  HeapInitialize[h, \"ZeroFill\" -> True] also accepts Uninitialized snapshots; tensors are allocated zero-filled.";
HeapStrip::usage    = "HeapStrip[h_Heap] returns h with all NumericArray tensor buffers replaced by <|\"shape\" -> _, \"dtype\" -> _|>.  Pure WL; does not touch the runtime.";
THeapToTermTree::usage = "THeapToTermTree[h_Heap] returns a nested string-headed expression mirroring TTermExpr but driven by the snapshot's Cells list (no live runtime needed).  Cycles render as \"Cycle\"[idx].  Read-only projection.";

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

(* dtypeCode handles both "f32"/"i32" and 0/1 already (Tensor.wl). *)

dtypeNameSafe[0]  := "f32"
dtypeNameSafe[1]  := "i32"
dtypeNameSafe[d_] := d

numericArrayDType[na_NumericArray] := Switch[ NumericArrayType[na],
    "Real32",    "f32",
    "Integer32", "i32",
    _,           "f32"
]

(* === Term / BookTerm: Association-form normalization to positional === *)

Term[a_Association] /; KeyExistsQ[a, "tag"] && KeyExistsQ[a, "ext"] && KeyExistsQ[a, "val"] :=
    With[{tag = a["tag"], ext = a["ext"], val = a["val"], sub = Lookup[a, "sub", 0]},
        If[ sub === 0,
            Term[tag, ext, val],
            Term[tag, ext, val, sub]
        ]
    ]

BookTerm[a_Association] /; KeyExistsQ[a, "tag"] && KeyExistsQ[a, "ext"] && KeyExistsQ[a, "val"] :=
    With[{tag = a["tag"], ext = a["ext"], val = a["val"], sub = Lookup[a, "sub", 0]},
        If[ sub === 0,
            BookTerm[tag, ext, val],
            BookTerm[tag, ext, val, sub]
        ]
    ]

(* === predicates === *)

heapPayloadQ[a_Association] :=
    KeyExistsQ[a, "Cells"] && KeyExistsQ[a, "Tensors"]
heapPayloadQ[___] := False

heapNewQ[Heap[a_Association]] := heapPayloadQ[a]
heapNewQ[___] := False

heapStateOf[a_Association] := With[{vals = Values[a]},
    Which[
        vals === {},                                "Initialized",
        AllTrue[vals, MatchQ[#, _NumericArray] &],   "Initialized",
        AllTrue[vals, AssociationQ],                 "Uninitialized",
        True,                                        "Mixed"
    ]
]

(* === HeapSnapshot ===
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
   BookTerm).  Only TAG_TEN val is rewritten (runtime tensor id ->
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
rawToBookTerm[raw_, remap_] := rawToHead[BookTerm, raw, remap]

(* For each TAG_ALO cell at dyn loc K, mark heap[ALO.val] as a
   book-domain holder: the term sitting in that cell has its `val`
   pointing into BOOK_HEAP, so it must be serialized as BookTerm. *)
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
        i -> rawToBookTerm[$bookReadFn[i], remap],
        {i, 1, nb - 1}
    ]
]

(* Snapshot DEFS: iterate slots [0, DEFS_CAP), keep non-zero entries.
   Reverse-lookup name string from $defNames where available. *)
snapshotDefs[remap_Association] := Module[{slotToName, entries = {}},
    slotToName = If[ AssociationQ[$defNames],
        AssociationMap[Reverse, $defNames],
        <||>
    ];
    Do[
        With[{rootRaw = $defGetFn[slot]},
            If[ rootRaw =!= 0,
                AppendTo[entries, slot -> <|
                    "name" -> Lookup[slotToName, slot, None],
                    "root" -> rawToBookTerm[rootRaw, remap]
                |>]
            ]
        ],
        {slot, 0, 255}
    ];
    Association @ entries
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

HeapSnapshot[]                    := HeapSnapshot[Missing["NoRoot"]]

HeapSnapshot[root_TTerm]          := snapshotImpl[ttermRaw[root]]
HeapSnapshot[Missing["NoRoot"]]   := snapshotImpl[Missing["NoRoot"]]
HeapSnapshot[None]                := snapshotImpl[Missing["NoRoot"]]

snapshotImpl[rootRawOrMissing_] := Module[{
    n, nb, cellRaws, bookRaws, defRootRaws, bookHolders,
    allRaws, remap,
    cells, bookCells, tensors, defs, aloStates, rootTerm,
    extraRaws = If[ IntegerQ[rootRawOrMissing], {rootRawOrMissing}, {}]
},
    ensureInit[];
    n           = $heapPosFn[];
    cellRaws    = Table[$heapReadFn[i],   {i, 0, n - 1}];
    nb          = $bookPosFn[];
    bookRaws    = Table[$bookReadFn[i],   {i, 1, nb - 1}];
    defRootRaws = Table[$defGetFn[slot],  {slot, 0, 255}];
    bookHolders = collectBookHolderIndices[cellRaws];

    allRaws = Join[cellRaws, bookRaws, defRootRaws, extraRaws];
    remap   = collectTensorRemap[allRaws];

    cells = Association @ MapIndexed[
        Function[{raw, idx},
            With[{i = First[idx] - 1},
                i -> If[ MemberQ[bookHolders, i],
                    rawToBookTerm[raw, remap],
                    rawToTerm[raw, remap]
                ]
            ]
        ],
        cellRaws
    ];
    bookCells = snapshotBookCells[remap];
    tensors   = Association @ KeyValueMap[
        Function[{rid, slot}, slot -> $tensorReadFn[rid]],
        remap
    ];
    defs      = snapshotDefs[remap];
    aloStates = snapshotAloStates[];
    rootTerm  = If[ IntegerQ[rootRawOrMissing],
        rawToTerm[rootRawOrMissing, remap],
        Missing["NoRoot"]
    ];
    Heap[<|
        "Root"      -> rootTerm,
        "Cells"     -> cells,
        "BookCells" -> bookCells,
        "Tensors"   -> tensors,
        "Defs"      -> defs,
        "AloStates" -> aloStates,
        "Labels"    -> $labelCounter,
        "State"     -> heapStateOf[tensors]
    |>]
]

(* === HeapStrip ===
   Replace each NumericArray with <|"shape", "dtype"|>.  Pure WL. *)

stripTensorEntry[na_NumericArray] := <|
    "shape" -> Dimensions[na],
    "dtype" -> numericArrayDType[na]
|>
stripTensorEntry[a_Association] := a

HeapStrip[Heap[a_Association]] := Module[{stripped},
    stripped = Map[stripTensorEntry, Lookup[a, "Tensors", <||>]];
    Heap[<|
        "Root"      -> Lookup[a, "Root",      Missing["NoRoot"]],
        "Cells"     -> Lookup[a, "Cells",     <||>],
        "BookCells" -> Lookup[a, "BookCells", <||>],
        "Tensors"   -> stripped,
        "Defs"      -> Lookup[a, "Defs",      <||>],
        "AloStates" -> Lookup[a, "AloStates", {}],
        "Labels"    -> Lookup[a, "Labels", 1],
        "State"     -> heapStateOf[stripped]
    |>]
]

(* === HeapInitialize ===
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
      slot -> runtime-id map.  Both Term and BookTerm cells pack via
      the same termToRaw -- val passes through directly because
      cell-domain interpretation is the receiver's responsibility. *)

Options[HeapInitialize] = {"ZeroFill" -> False}

HeapInitialize[Heap[a_Association], opts:OptionsPattern[]] := Module[{
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
        Function[{slot, entry},
            slot -> initTensorEntry[entry, zeroFill]
        ],
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
        Function[{slot, entry},
            $defSetFn[slot, termToRaw[entry["root"], slotToRuntime]];
            With[{name = Lookup[entry, "name", None]},
                If[ StringQ[name],
                    $defNames[name] = slot;
                    If[ slot >= $defNext, $defNext = slot + 1]
                ]
            ]
        ],
        defs
    ];

    (* Restore ALO_STATES.  List index i (1-based) -> state id i. *)
    If[ Length[aloStates] > 0,
        MapIndexed[
            Function[{entry, idx},
                $aloStateSetFn[
                    First[idx],
                    entry["parent"], entry["old_loc"], entry["new_loc"]
                ]
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
        MatchQ[root, _Term] || MatchQ[root, _BookTerm],
            termToRaw[root, slotToRuntime],
        True,
            Missing["NoRoot"]
    ];
    If[ MissingQ[rootRaw], Missing["NoRoot"], TTerm[rootRaw]]
]

(* Allocate + (optionally) write a single tensor entry; return the
   runtime tensor id. *)
initTensorEntry[na_NumericArray, _] := Module[{
    shape, dtype, rid
},
    shape = Dimensions[na];
    dtype = numericArrayDType[na];
    rid   = $tensorAllocFn[dtypeCode[dtype], shape];
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
        Message[HeapInitialize::uninit];
        Throw[$Failed, "HeapInitialize::uninit"]
    ];
    rid   = $tensorAllocFn[dtypeCode[dtype], shape];
    count = Times @@ shape;
    If[ dtype === "f32",
        $tensorWriteFn [rid, ConstantArray[0., count]],
        $tensorWriteIFn[rid, ConstantArray[0,  count]]
    ];
    rid
]

HeapInitialize::uninit = "HeapInitialize: snapshot is Uninitialized; pass \"ZeroFill\" -> True to allocate zero-filled tensor buffers.";

(* Convert a Term[...] back into a packed runtime u64 via $termNewFn.
   Symbolic ext fields for OP2 / UOP / TEN / NUM are decoded back to
   integer codes; TAG_TEN val is remapped via slotMap. *)

(* Term/BookTerm[a_Association] normalize via the global rewrite up
   top, so by the time we get here `t` is positional.  Both heads
   pack identically -- domain interpretation (dyn vs book loc) is the
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
termToRaw[BookTerm[tag_String, ext_, val_],            slotMap_] := packCell[tag, ext, val, 0,   slotMap]
termToRaw[BookTerm[tag_String, ext_, val_, sub_Integer], slotMap_] := packCell[tag, ext, val, sub, slotMap]

(* === THeapToTermTree ===
   Recursive nested-Term view, mirroring tTreeWalk in THVMLink.wl
   but reading from a Cells Association instead of $heapReadFn.
   Cycles render as "Cycle"[idx].  Read-only projection. *)

THeapToTermTree[Heap[a_Association]] := With[{
    root = Lookup[a, "Root", Missing["NoRoot"]]
},
    If[ MatchQ[root, _Term],
        cellTreeWalkTerm[a["Cells"], root, <||>],
        Missing["NoRoot"]
    ]
]

THeapToTermTree[Heap[a_Association], root_Term] :=
    cellTreeWalkTerm[a["Cells"], root, <||>]

cellTreeWalkLoc[cells_, idx_Integer, seen_] := With[{
    cell = Lookup[cells, idx, Missing["NoCell", idx]]
},
    Which[
        MatchQ[cell, _Term],     cellTreeWalkTerm[cells, cell, seen],
        MatchQ[cell, _BookTerm], "Book"[cell[[1]], cell[[2]], cell[[3]]],
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
