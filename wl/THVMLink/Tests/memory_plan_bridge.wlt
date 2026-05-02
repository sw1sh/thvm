(* memory_plan_bridge.wlt -- TKernelTable / TKernelInputs /
   TTensTable / TCpuBufTable / TMetalBufTable shape + smoke
   checks (mp1 of the TMemoryPlan visualization arc).  Verifies
   the bridge functions return the right schema after a small
   ADD materialize.  Higher-level joining + topo-depth derivation
   lives in MemoryPlan.wl (mp2). *)

VerificationTest[
    TInit[];
    TReset[];
    a    = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b    = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    res  = TRealize @ TUOpAdd[a, b];
    kt   = TKernelTable[];
    (* Cols: {n_inputs, output_tid, reserved0, spliced,
       consumer_count, output_numel, output_dtype}.  The `fired`
       flag at column 3 was removed (kernels re-fire on every redex,
       OP2-style); slot reads 0 as a placeholder. *)
    {Length[kt] >= 1, Length[First[kt]] === 7,
     Max[#[[3]] & /@ kt] === 0},
    {True, True, True},
    TestID -> "memory-plan-bridge/kernel-table-shape"
]

VerificationTest[
    TInit[];
    TReset[];
    a    = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b    = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    res  = TRealize @ TUOpAdd[a, b];
    (* The ADD kernel is the last one allocated.  Its inputs
       should be the two TenDescs we uploaded. *)
    kid  = TKernelCount[] - 1;
    inps = TKernelInputs[kid];
    {Length[inps], AllTrue[inps, # > 0 &]},
    {2, True},
    TestID -> "memory-plan-bridge/kernel-inputs-add-has-two"
]

VerificationTest[
    TInit[];
    TReset[];
    a   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b   = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    res = TRealize @ TUOpAdd[a, b];
    tt  = TTensTable[];
    (* At least 3 TenDescs (a, b, ADD output), schema width 7,
       backend_id column (index 7) should be 1 for CPU. *)
    {Length[tt] >= 3, Length[First[tt]] === 7,
     Max[#[[7]] & /@ tt] === 1},
    {True, True, True},
    TestID -> "memory-plan-bridge/tens-table-shape-and-backend"
]

VerificationTest[
    TInit[];
    TReset[];
    a   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b   = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    res = TRealize @ TUOpAdd[a, b];
    cb  = TCpuBufTable[];
    (* >= 3 buffers, schema width 5, refcount column non-zero
       (live bufs after materialize). *)
    {Length[cb] >= 3, Length[First[cb]] === 5,
     Max[#[[2]] & /@ cb] >= 1},
    {True, True, True},
    TestID -> "memory-plan-bridge/cpu-buf-table-shape-and-refcount"
]

VerificationTest[
    TInit[];
    TReset[];
    (* Even on a CPU run, TMetalBufTable should return an empty
       (or tiny) list with the right schema.  The dylib is built
       with -DTHVM_HAS_METAL on Darwin so the function is always
       linked, just inactive when CURRENT_BACKEND is CPU. *)
    mb = TMetalBufTable[];
    AllTrue[mb, Length[#] === 2 &],
    True,
    TestID -> "memory-plan-bridge/metal-buf-table-schema-on-cpu"
]

VerificationTest[
    TInit[];
    TReset[];
    ms = TMetalBufSummary[];
    {Sort @ Keys[ms],
     And @@ IntegerQ /@ Values[ms],
     ms["LiveBytes"], ms["DeferredBytes"], ms["DeferredCount"],
     ms["PeakLiveBytes"], ms["PeakRetainedBytes"],
     ms["PeakDeferredBytes"]},
    {Sort @ {"LiveBytes", "RetainedBytes", "DeferredBytes",
             "DeferredCount", "FreelistCount", "PeakLiveBytes",
             "PeakRetainedBytes", "PeakDeferredBytes"},
     True, 0, 0, 0, 0, 0, 0},
    TestID -> "memory-plan-bridge/metal-buf-summary-schema-on-cpu"
]

VerificationTest[
    TInit[];
    TReset[];
    mp = TMetalMemoryProfile[];
    {SubsetQ[Keys[mp],
        {"LiveBytes", "RetainedBytes", "DeferredBytes",
         "PeakLiveBytes", "PeakRetainedBytes", "PeakDeferredBytes",
         "BufferCount", "LiveBuffers", "RetainedBuffers",
         "FreelistBytes", "LargestLiveBytes",
         "LargestRetainedBytes"}],
     And @@ IntegerQ /@ Values[mp],
     mp["LiveBytes"], mp["FreelistBytes"]},
    {True, True, 0, 0},
    TestID -> "memory-plan-bridge/metal-memory-profile-schema-on-cpu"
]

VerificationTest[
    TInit[];
    TReset[];
    Module[{ctx = TContextNew["metal"], ok},
        If[ctx === 0, Return[True]];
        ok = TInContext[ctx,
            a = TTensorCreate @ NumericArray[{1., 2., 3., 4.}, "Real32"];
            b = TTensorCreate @ NumericArray[{0.5, 0.5, 0.5, 0.5}, "Real32"];
            TRealize[a + b];
            mp = TMetalMemoryProfile[];
            mp["LiveBytes"] > 0 &&
            mp["RetainedBytes"] >= mp["LiveBytes"] &&
            mp["PeakLiveBytes"] >= mp["LiveBytes"] &&
            mp["PeakRetainedBytes"] >= mp["RetainedBytes"] &&
            mp["DeferredBytes"] === 0 &&
            mp["DeferredCount"] === 0
        ];
        TContextDestroy[ctx];
        ok
    ],
    True,
    TestID -> "memory-plan-bridge/metal-memory-profile-after-small-dispatch"
]
