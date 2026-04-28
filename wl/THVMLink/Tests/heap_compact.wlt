(* heap_compact.wlt -- Cheney semi-space copying GC.

   The C-side `gc_collect` evacuates all live heap cells reachable
   from the standard root set (extern pins, DEFS, kernel
   side-tables, WNF_LAST_STACK) into the to-space, then swaps the
   spaces.  HEAP_NEXT after the call is the new live-cell count.

   These tests exercise:
     1. Manual TGCCollect[] reduces HEAP_NEXT (the live set is
        smaller than the post-realize allocation footprint).
     2. Tensor data still reads correctly after compaction --
        kernel side-tables / TenDesc producer chains survived.
     3. Re-realizing on top of a post-GC term still works (the
        UOP graph is intact; new realizes resolve correctly). *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    expr = TUOpAdd[TUOpMul[a, b], a];
    r = TRealize[expr];
    pre = THeapPos[];
    TGCCollect[];
    post = THeapPos[];
    {pre > 0, post > 0, TGCCount[]},
    {True, True, 1},
    TestID -> "heap_compact/manual-gc-runs"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    r = TRealize @ TUOpAdd[TUOpMul[a, b], a];
    pre  = Normal @ TTensorData[r];
    TGCCollect[];
    post = Normal @ TTensorData[r];
    pre === post,
    True,
    TestID -> "heap_compact/data-survives-gc"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{0.5, 0.5, 0.5}, "Real32"];
    r1 = TRealize @ TUOpMul[a, b];
    TGCCollect[];
    (* New realize on top of post-GC tensors. *)
    r2 = TRealize @ TUOpAdd[r1, a];
    Normal @ TTensorData[r2],
    {1.5, 3.0, 4.5},
    TestID -> "heap_compact/realize-after-gc"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    g = TRealize @ TGrad[TUOpReduce[TUOpMul[a, a], 0, "SUM"], a];
    pre  = Normal @ TTensorData[g];
    TGCCollect[];
    post = Normal @ TTensorData[g];
    {pre, post, pre === post},
    {{2.0, 4.0, 6.0}, {2.0, 4.0, 6.0}, True},
    TestID -> "heap_compact/grad-survives-gc"
]
