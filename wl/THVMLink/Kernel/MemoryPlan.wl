(* ::Package:: *)
(* MemoryPlan.wl -- buffer-lifecycle snapshot of the live thvm
   schedule, mapped over the kernel-DAG dependency order.

   Public surface:
     TMemoryPlan[]        -- snapshot the live runtime state
                             (KERNELS, TENS, CPU/Metal bufs) and
                             return a TMemoryPlan[<|...|>] object
                             with derived per-kernel topological
                             depths and per-buf (alloc, last_use)
                             intervals.
     TMemoryPlanReport[p] -- top-N largest bufs / longest-lived /
                             status counts / total live bytes.
     TMemoryPlanGantt[p]  -- (mp3, follow-up) Gantt-style Graphics.

   The snapshot is pure-data: no side effects on the runtime, just
   reads via the mp1 bridge tables (TKernelTable, TKernelInputs,
   TTensTable, TCpuBufTable, TMetalBufTable).

   Topological depth is the static-analysis x-axis (NOT actual
   firing order).  Two valid topological linearizations of the
   same DAG would produce identical depths; pinning bufs to a
   linearization would have implied a memory-management
   consequence that does not exist for kernels at the same
   depth (they can fire in any order). *)

BeginPackage["THVMLink`"];

TMemoryPlan::usage = "TMemoryPlan[] returns a TMemoryPlan[<|...|>] snapshot of the live thvm schedule, with per-kernel topological depths and per-buf (alloc_depth, last_use_depth, alive_span, status) intervals derived from the producer_kid / input_tids edges.  Aliasing-aware: TenDescs sharing a buf_id collapse into one Bufs entry whose alias_tids lists every contributing tid.";

TMemoryPlanReport::usage = "TMemoryPlanReport[plan] returns a Column with top-5 largest bufs by nbytes, top-5 longest-lived by alive_span, count by status, and total live bytes for the active backend.  Pass a TMemoryPlan[<|...|>] object (typically TMemoryPlan[]).";

(* Forward-declare bridge symbols owned by THVMLink.wl (loaded
   first via the autoload Sort + Get pattern in THVMLink.wl). *)
{TKernelTable, TKernelInputs, TTensTable, TCpuBufTable,
 TMetalBufTable, TKernelCount, TTensCount};

Begin["`Private`"];

(* === Topological depth ===
   depth[kid] = 1 + max(depth of producer kernel of any input tid).
   External tids (producer_kid == 0) contribute 0.  Memoized via a
   local Association so the cost stays linear in the DAG size even
   when multiple consumers share a producer. *)
computeKernelDepths[kernels_, tens_] := Module[
    {nKernels = Length[kernels], depthCache, depth, kernelInputs, producerOf},
    depthCache = <||>;
    (* tens is keyed by tid (1..N).  Look up producer_kid lazily;
       columns are {producer_kid, buf_id, dtype, ...}. *)
    producerOf[tid_Integer] := If[
        tid <= 0 || tid > Length[tens], 0, tens[[tid, 1]]
    ];
    kernelInputs[kid_Integer] := If[
        kid <= 0 || kid > nKernels, {},
        TKernelInputs[kid]
    ];
    depth[kid_Integer] /; KeyExistsQ[depthCache, kid] := depthCache[kid];
    depth[kid_Integer] := depthCache[kid] = If[
        kid <= 0 || kid > nKernels, 0,
        Module[{producerKids},
            producerKids = DeleteCases[producerOf /@ kernelInputs[kid], 0];
            If[ producerKids === {}, 0,
                1 + Max[depth /@ DeleteDuplicates[producerKids]]]
        ]
    ];
    Association @ Table[k -> depth[k], {k, nKernels}]
]

(* === Buf collation ===
   Group TenDescs by (backend_id, buf_id).  For each group:
     producer_kernel = the producer_kid recorded on any of the
                       grouped tids (they should agree because
                       tensor_view_of inherits producer_kid; we
                       defensively take Max[..., 0] if they don't).
     consumers = kernels whose input_tids include any tid in the
                 group.
     alloc_depth = kernelDepth[producer_kernel] or 0 (external).
     last_use_depth = Max[kernelDepth[c]] over consumers, or
                      alloc_depth if no consumers (preserved /
                      result buffers).
     status drawn from preserved/freeable + producer presence.
   Backend dispatch: backend_id == 1 reads the CPU buf table for
   nbytes/refcount/preserved/freeable; backend_id == 2 reads the
   Metal table (no preserved/freeable, defaulted to 0). *)
collateBufs[kernels_, tens_, kernelDepths_, cpuBufs_, metalBufs_] := Module[
    {byBuf, allKids, consumersOf},
    (* Pre-compute consumer kernels per tid: for each kernel kid,
       look up its input_tids and accumulate (tid -> {kids...}). *)
    consumersOf = <||>;
    Do[
        Module[{inps = TKernelInputs[kid]},
            Do[
                consumersOf[tid] = Append[Lookup[consumersOf, tid, {}], kid],
                {tid, DeleteCases[inps, 0]}
            ]
        ],
        {kid, Length[kernels]}
    ];
    (* Group tids by (backend_id, buf_id).  Skip tids with buf_id 0
       (external/unbound) -- they have no buffer to plot. *)
    byBuf = GroupBy[
        Range[Length[tens]],
        Function[tid, {tens[[tid, 7]], tens[[tid, 2]]}]
    ];
    KeyDropFrom[byBuf, Cases[Keys[byBuf], {_, 0}]];
    Map[
        Function[tids, Module[
            {firstTid = First[tids], backendId, bufId,
             bufRow, nbytes, refcount, preserved, freeable,
             producerKid, allConsumerKids, allocDepth, lastUseDepth, status},
            backendId = tens[[firstTid, 7]];
            bufId     = tens[[firstTid, 2]];
            (* Look up backend-specific buf row.  Defensive bounds
               check: a stale tid pointing past the buf table is
               treated as 0-byte. *)
            bufRow = Which[
                backendId === 1 && 1 <= bufId <= Length[cpuBufs],
                    cpuBufs[[bufId]],
                backendId === 2 && 1 <= bufId <= Length[metalBufs],
                    PadRight[metalBufs[[bufId]], 5, 0],   (* metal: pad preserved/freeable/owns_data to 0 *)
                True,
                    {0, 0, 0, 0, 0}
            ];
            nbytes    = bufRow[[1]];
            refcount  = bufRow[[2]];
            preserved = bufRow[[3]];
            freeable  = bufRow[[4]];
            (* All grouped tids should share the same producer_kid
               (tensor_view_of inherits it).  Take Max so a 0 from
               an external alias doesn't shadow a real producer. *)
            producerKid = Max @ Append[tens[[#, 1]] & /@ tids, 0];
            allConsumerKids = DeleteDuplicates @ Flatten[
                Lookup[consumersOf, #, {}] & /@ tids
            ];
            allocDepth = If[ producerKid === 0, 0,
                             Lookup[kernelDepths, producerKid, 0]];
            lastUseDepth = If[ allConsumerKids === {}, allocDepth,
                Max[Lookup[kernelDepths, #, allocDepth] & /@ allConsumerKids]
            ];
            status = Which[
                preserved === 1, "Preserved",
                freeable  === 1, "Freeable",
                producerKid === 0, "External",
                allConsumerKids === {}, "Live",
                True, "Live"
            ];
            <|
                "id"              -> bufId,
                "backend"         -> backendId,
                "nbytes"          -> nbytes,
                "refcount"        -> refcount,
                "preserved"       -> preserved,
                "freeable"        -> freeable,
                "alias_tids"      -> tids,
                "producer_kid"    -> producerKid,
                "consumer_kids"   -> allConsumerKids,
                "alloc_depth"     -> allocDepth,
                "last_use_depth"  -> lastUseDepth,
                "alive_span"      -> lastUseDepth - allocDepth + 1,
                "status"          -> status
            |>
        ]],
        Values[byBuf]
    ]
]

(* === TMemoryPlan[] entry ===
   Snapshots the 5 mp1 bridge tables and stitches them into a
   TMemoryPlan[<|"Kernels", "Tens", "Bufs"|>] object. *)
TMemoryPlan[] := Module[
    {kernels, tens, cpuBufs, metalBufs, kernelDepths, kernelRecords, tensRecords, bufRecords},
    kernels   = TKernelTable[];
    tens      = TTensTable[];
    cpuBufs   = TCpuBufTable[];
    metalBufs = TMetalBufTable[];
    kernelDepths = computeKernelDepths[kernels, tens];
    kernelRecords = MapIndexed[
        With[{kid = First[#2]}, <|
            "id"             -> kid,
            "n_inputs"       -> #1[[1]],
            "output_tid"     -> #1[[2]],
            "fired"          -> #1[[3]],
            "spliced"        -> #1[[4]],
            "consumer_count" -> #1[[5]],
            "output_numel"   -> #1[[6]],
            "output_dtype"   -> #1[[7]],
            "input_tids"     -> TKernelInputs[kid],
            "depth"          -> kernelDepths[kid]
        |>] &,
        kernels
    ];
    tensRecords = MapIndexed[
        With[{tid = First[#2]}, <|
            "id"              -> tid,
            "producer_kid"    -> #1[[1]],
            "buf_id"          -> #1[[2]],
            "dtype"           -> #1[[3]],
            "view_numel"      -> #1[[4]],
            "view_contiguous" -> #1[[5]],
            "refcount"        -> #1[[6]],
            "backend_id"      -> #1[[7]]
        |>] &,
        tens
    ];
    bufRecords = collateBufs[kernels, tens, kernelDepths, cpuBufs, metalBufs];
    TMemoryPlan[<|
        "Kernels" -> kernelRecords,
        "Tens"    -> tensRecords,
        "Bufs"    -> bufRecords
    |>]
]

(* === TMemoryPlanReport ===
   Text summary parallel to memory-probe.wls.  Five sections:
   header (counts + total live bytes), top-5 largest, top-5
   longest-lived, status histogram, backend breakdown. *)
backendName[1] = "CPU";
backendName[2] = "Metal";
backendName[_] = "?";

formatBytes[n_] := Round[n / 1024.0, 0.1];

TMemoryPlanReport[TMemoryPlan[a_Association]] := Module[
    {bufs = a["Bufs"], kernels = a["Kernels"], byStatus, byBackend,
     totalBytes, topByBytes, topBySpan},
    byStatus  = KeySort @ Counts[#["status"] & /@ bufs];
    byBackend = KeySort @ Counts[backendName[#["backend"]] & /@ bufs];
    totalBytes = Total[#["nbytes"] & /@ bufs];
    topByBytes = TakeLargestBy[bufs, #["nbytes"] &, UpTo[5]];
    topBySpan  = TakeLargestBy[bufs, #["alive_span"] &, UpTo[5]];
    Column[{
        Row[{"TMemoryPlan: ",
             Length[kernels], " kernels, ",
             Length[bufs],    " bufs, ",
             formatBytes[totalBytes], " KiB live"}],
        Row[{"  by status:  ", byStatus}],
        Row[{"  by backend: ", byBackend}],
        Row[{"  top-5 by bytes:"}],
        Column[Function[b, Row[{"    buf ", b["id"], " (",
            backendName[b["backend"]], "): ",
            formatBytes[b["nbytes"]], " KiB, ",
            "depth [", b["alloc_depth"], "..", b["last_use_depth"], "], ",
            b["status"]}]] /@ topByBytes],
        Row[{"  top-5 by alive span:"}],
        Column[Function[b, Row[{"    buf ", b["id"], " (",
            backendName[b["backend"]], "): span ",
            b["alive_span"], ", ",
            formatBytes[b["nbytes"]], " KiB, ",
            b["status"]}]] /@ topBySpan]
    }]
]

End[];
EndPackage[];
