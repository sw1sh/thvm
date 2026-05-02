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

(* TMemoryPlanGantt -- declared and defined in Visualization.wl
   alongside THeapGraph / TScheduleGraph; this file owns the snapshot
   logic, helpers (linearScanPack, statusFill/Edge, formatBytes,
   peakConcurrentLive, backendsActive) it shares with the renderer. *)

(* === mp1 bridge tables ===
   Snapshot accessors over the runtime side tables (KERNELS, TENS,
   CPU/Metal bufs).  Each returns a flat list-of-rows -- consumers
   index by row schema documented in the ::usage.  Used directly
   by TMemoryPlan + indirectly by Kernel.wl, Visualization.wl,
   Heap.wl etc. (anywhere a kid -> tid -> buf walk is needed). *)
(* TKernelTable / TKernelInputs are declared in Kernel.wl (kernel
   side-table accessors).  TTensTable / TTensCount / TTotalBufBytes
   are declared in Tensor.wl (tensor side-table accessors).
   MemoryPlan.wl owns only the per-backend buf-table accessors. *)
TCpuBufTable::usage   = "TCpuBufTable[] returns a list of {nbytes, refcount, preserved, freeable, owns_data} per CPU buffer (buf_id 1 .. CPU_BUFS_NEXT - 1).";
TMetalBufTable::usage = "TMetalBufTable[] returns a list of {nbytes, refcount} per Metal buffer.  Empty when the dylib was built without Metal support.";
TMetalBufSummary::usage = "TMetalBufSummary[] returns <|\"LiveBytes\", \"RetainedBytes\", \"DeferredBytes\", \"DeferredCount\", \"FreelistCount\", \"PeakLiveBytes\", \"PeakRetainedBytes\", \"PeakDeferredBytes\"|> for the Metal buffer table.  RetainedBytes includes recycle-list buffers that no live tensor references.";
TMetalMemoryProfile::usage = "TMetalMemoryProfile[] returns a flat Metal memory profile derived from TMetalBufSummary[] and TMetalBufTable[], including buffer counts, freelist bytes, and largest live/retained buffer sizes.";

Begin["`Private`"];

(* === mp1 bridge tables: thin wrappers over the LibraryLink loaders ===
   Loader symbols ($kernelTableFn etc.) live in THVMLink.wl alongside
   every other LibraryFunctionLoad call; both files share THVMLink`Private`
   so the references resolve regardless of file load order (alphabetical:
   Kernel.wl < MemoryPlan.wl < THVMLink.wl).

   Each accessor is a snapshot, NOT a live view: the data is copied out
   of the C-side side tables on call, so mutating it in WL has no effect
   on the runtime. *)
(* TKernelTable / TKernelInputs are defined in Kernel.wl.
   TTensTable / TTensCount / TTotalBufBytes are defined in Tensor.wl. *)
TCpuBufTable[]           := (ensureInit[]; Partition[Normal @ $cpuBufTableFn[],   5])
TMetalBufTable[]         := (ensureInit[]; Partition[Normal @ $metalBufTableFn[], 2])
TMetalBufSummary[]       := Module[{v},
    ensureInit[];
    v = PadRight[Normal @ $metalBufSummaryFn[], 8, 0];
    <|"LiveBytes" -> v[[1]], "RetainedBytes" -> v[[2]],
      "DeferredBytes" -> v[[3]], "DeferredCount" -> v[[4]],
      "FreelistCount" -> v[[5]], "PeakLiveBytes" -> v[[6]],
      "PeakRetainedBytes" -> v[[7]], "PeakDeferredBytes" -> v[[8]]|>
]

TMetalMemoryProfile[]    := Module[{
    summary,
    bufs,
    liveBufs,
    retainedBufs
},
    summary = TMetalBufSummary[];
    bufs = TMetalBufTable[];
    liveBufs = Select[bufs, #[[2]] > 0 &];
    retainedBufs = Select[bufs, #[[1]] > 0 &];
    Join[
        summary,
        <|"BufferCount" -> Length[bufs],
          "LiveBuffers" -> Length[liveBufs],
          "RetainedBuffers" -> Length[retainedBufs],
          "FreelistBytes" -> Max[0,
              summary["RetainedBytes"] - summary["LiveBytes"]],
          "LargestLiveBytes" -> If[liveBufs === {}, 0,
              Max[liveBufs[[All, 1]]]],
          "LargestRetainedBytes" -> If[retainedBufs === {}, 0,
              Max[retainedBufs[[All, 1]]]]|>
    ]
]

(* === Topological depth ===
   depth[kid] = 1 + max(depth of producer kernel of any input tid).
   External tids (producer_kid == 0) contribute 0.  Memoized via a
   local Association so the cost stays linear in the DAG size even
   when multiple consumers share a producer. *)
computeKernelDepths[kernels_, tens_] := Block[{
    nKernels = Length[kernels],
    depthCache,
    depth,
    kernelInputs,
    producerOf
},
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
        Block[{producerKids = DeleteCases[producerOf /@ kernelInputs[kid], 0]},
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
(* dtypeName covers the full DT_* enum; THVMLink.wl wires the table.
   No fallback here -- the THVMLink.wl table covers every wired slot,
   and an unknown integer should surface as the symbolic dtypeName[n]
   so callers can spot it (rather than collide with a string). *)

collateBufs[kernels_, tens_, kernelDepths_, cpuBufs_, metalBufs_] := Block[{
    byBuf,
    consumersOf
},
    (* Pre-compute consumer kernels per tid: for each kernel kid,
       look up its input_tids and accumulate (tid -> {kids...}). *)
    consumersOf = <||>;
    Do[
        Block[{inps = TKernelInputs[kid]},
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
        Function[tids, Block[{
            firstTid = First[tids],
            backendId,
            bufId,
            bufRow,
            nbytes,
            refcount,
            preserved,
            freeable,
            producerKid,
            allConsumerKids,
            allocDepth,
            lastUseDepth,
            status
        },
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
                (* All aliased tids share the same dtype (tensor_view_of
                   inherits it).  Take the first tid's dtype as
                   authoritative.  Without this the renderer's
                   tooltip showed `Missing[KeyAbsent, dtype]`. *)
                "dtype"           -> dtypeName[tens[[firstTid, 3]]],
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
TMemoryPlan[] := Block[{
    kernels,
    tens,
    cpuBufs,
    metalBufs,
    kernelDepths,
    kernelRecords,
    tensRecords,
    bufRecords
},
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
        "Bufs"    -> bufRecords,
        (* "Peak" is the {peak_bytes, peak_depth, total_bytes}
           summary -- pre-computed so external consumers (TBench,
           dashboards) don't have to re-walk Bufs.  Cheap: linear
           in the number of distinct depths. *)
        "Peak"    -> peakConcurrentLive[bufRecords]
    |>]
]

(* === TMemoryPlanReport ===
   Text summary parallel to memory-probe.wls.  Five sections:
   header (counts + total live bytes), top-5 largest, top-5
   longest-lived, status histogram, backend breakdown. *)
backendName[1] = "CPU";
backendName[2] = "Metal";
backendName[_] = "?";

(* Significant-digit byte formatter.  "0.0KiB" labels were
   useless on small intermediates (4-byte counters, 64-byte
   activations); pick a unit so the mantissa always sits in
   [1, 1024) and round to 3 sig figs.  Returns a string like
   "12B" / "5.43KiB" / "1.20MiB". *)
formatBytes[0 | 0.] := "0B";
formatBytes[n_?NumericQ] := Block[{x = N[Abs[n]], unit, scale, mantissa, digits},
    {unit, scale} = Which[
        x >= 1024.^3, {"GiB", 1024.^3},
        x >= 1024.^2, {"MiB", 1024.^2},
        x >= 1024.,   {"KiB", 1024.},
        True,         {"B",   1.}
    ];
    mantissa = x / scale;
    (* 3 sig figs: keep 2 decimals < 10, 1 decimal < 100, 0 decimals >= 100. *)
    digits = Which[mantissa < 10, 2, mantissa < 100, 1, True, 0];
    ToString[NumberForm[Sign[n] mantissa, {Infinity, digits}]] <> unit
];

(* Peak concurrent live bytes: at each topological depth t, sum
   nbytes of every buf whose alive interval covers t.  The max
   over t is the LOWER BOUND on bytes a perfect slot-reusing
   memory planner would need -- two bufs whose intervals don't
   overlap can share a single physical slot.  The gap between
   peak and total is the slot-reuse headroom (= bytes the
   current "no-reuse" allocator burns unnecessarily).  Returns
   <|"peak_bytes", "peak_depth", "total_bytes"|>. *)
peakConcurrentLive[bufs_] := If[
    bufs === {},
    <|"peak_bytes" -> 0, "peak_depth" -> 0, "total_bytes" -> 0|>,
    Block[{maxDepth, perDepth, totalBytes, peakBytes, peakDepth},
        maxDepth = Max[#["last_use_depth"] & /@ bufs];
        perDepth = Table[
            Total @ Cases[bufs,
                b_ /; b["alloc_depth"] <= t <= b["last_use_depth"]
                    :> b["nbytes"]
            ],
            {t, 0, maxDepth}
        ];
        peakBytes = Max[perDepth];
        peakDepth = First @ FirstPosition[perDepth, peakBytes] - 1;
        totalBytes = Total[#["nbytes"] & /@ bufs];
        <|
            "peak_bytes"  -> peakBytes,
            "peak_depth"  -> peakDepth,
            "total_bytes" -> totalBytes
        |>
    ]
]

TMemoryPlanReport[TMemoryPlan[a_Association]] := Block[{
    bufs = a["Bufs"],
    kernels = a["Kernels"],
    byStatus,
    byBackend,
    totalBytes,
    topByBytes,
    topBySpan,
    peak,
    savingsBytes,
    savingsPct
},
    byStatus  = KeySort @ Counts[#["status"] & /@ bufs];
    byBackend = KeySort @ Counts[backendName[#["backend"]] & /@ bufs];
    totalBytes = Total[#["nbytes"] & /@ bufs];
    topByBytes = TakeLargestBy[bufs, #["nbytes"] &, UpTo[5]];
    topBySpan  = TakeLargestBy[bufs, #["alive_span"] &, UpTo[5]];
    peak       = peakConcurrentLive[bufs];
    savingsBytes = formatBytes[peak["total_bytes"] - peak["peak_bytes"]];
    savingsPct = If[ peak["total_bytes"] > 0,
        Round[100. (peak["total_bytes"] - peak["peak_bytes"]) / peak["total_bytes"], 0.1],
        0
    ];
    Column[{
        Row[{"TMemoryPlan: ",
             Length[kernels], " kernels, ",
             Length[bufs],    " bufs, ",
             formatBytes[totalBytes], " live"}],
        Row[{"  by status:  ", byStatus}],
        Row[{"  by backend: ", byBackend}],
        Row[{"  peak concurrent live: ",
             formatBytes[peak["peak_bytes"]], " at depth ", peak["peak_depth"],
             "  (slot-reuse headroom: ", savingsBytes, " = ",
             savingsPct, "% of total)"}],
        Row[{"  top-5 by bytes:"}],
        Column[Function[b, Row[{"    buf ", b["id"], " (",
            backendName[b["backend"]], "): ",
            formatBytes[b["nbytes"]], ", ",
            "depth [", b["alloc_depth"], "..", b["last_use_depth"], "], ",
            b["status"]}]] /@ topByBytes],
        Row[{"  top-5 by alive span:"}],
        Column[Function[b, Row[{"    buf ", b["id"], " (",
            backendName[b["backend"]], "): span ",
            b["alive_span"], ", ",
            formatBytes[b["nbytes"]], ", ",
            b["status"]}]] /@ topBySpan]
    }]
]

(* === TMemoryPlanGantt -- Graphics-based renderer ===
   X-axis: topological depth on the kernel DAG (Min..Max + 1).
   Y-axis: one row per buf, ordered by (alloc_depth ascending,
   nbytes descending) so producer-cluster groupings stay visually
   contiguous.  Each bar is a Tooltip-wrapped Rectangle whose
   width spans [alloc_depth, last_use_depth + 1] and whose height
   defaults to Log2[1 + nbytes] (toggle via "BarHeight" option). *)
(* Each status carries a {fill, edge} pair so every buffer reads
   as a delimited rounded card (pastel fill + saturated border)
   even when stacked densely.  Mirrors the tinygrad memory-plan
   diagram: weights = blue, activations = warm hues. *)
statusFill[status_String] := Switch[status,
    "Preserved",
        LightDarkSwitched[Lighter[StandardBlue,   0.7], Darker[StandardBlue,   0.45]],
    "Freeable",
        LightDarkSwitched[Lighter[StandardGreen,  0.7], Darker[StandardGreen,  0.45]],
    "External",
        LightDarkSwitched[Lighter[StandardOrange, 0.7], Darker[StandardOrange, 0.45]],
    "Dead",
        LightDarkSwitched[Lighter[StandardRed,    0.7], Darker[StandardRed,    0.45]],
    _,   (* "Live" or unknown *)
        LightDarkSwitched[Lighter[StandardGray,   0.7], Darker[StandardGray,   0.45]]
]

statusEdge[status_String] := Switch[status,
    "Preserved", LightDarkSwitched[StandardBlue,   Lighter[StandardBlue,   0.2]],
    "Freeable",  LightDarkSwitched[StandardGreen,  Lighter[StandardGreen,  0.2]],
    "External",  LightDarkSwitched[StandardOrange, Lighter[StandardOrange, 0.2]],
    "Dead",      LightDarkSwitched[StandardRed,    Lighter[StandardRed,    0.2]],
    _,           LightDarkSwitched[StandardGray,   Lighter[StandardGray,   0.2]]
]

backendsActive[bufs_] := DeleteDuplicates[#["backend"] & /@ bufs] /. {
    {} -> "(none)", {1} -> "CPU", {2} -> "Metal", {1, 2} | {2, 1} -> "CPU + Metal"
}

(* Linear-scan slot allocator: assigns each buf a vertical y-range
   such that height = nbytes and two bufs whose alive intervals
   don't overlap may share the same y-range (memory slot reuse).
   The total y-extent at any x = peakConcurrentLive's peak_bytes;
   the visualization makes slot reuse visible -- a tall bar with
   multiple short cards stacked along its lifetime is one slot
   serving several bufs in turn.

   Greedy first-fit: sort by alloc_depth, then for each buf find
   the first existing slot whose freeAfterDepth < alloc_depth AND
   capacity >= nbytes; if none, open a new slot at the top.
   Returns the input bufs with an additional "y_range" -> {y0, y1}
   key.  Not optimal (true bin-packing is NP-hard) but matches
   what tinygrad's runtime allocator does in practice. *)
(* barHeightFor[nbytes, mode] -- maps a buf's nbytes to its
   y-extent on the chart.  "Linear" is faithful to the actual byte
   count (good when bufs are roughly uniform in size); "Log" uses
   Log2[1 + nbytes] so a 4-byte buf and a 4-MiB buf are both
   visible without one disappearing into a sliver (good for graphs
   with mixed-magnitude bufs like LeNet, which has 0.001 KiB
   intermediates next to 120 KiB weights).  Default "Log". *)
barHeightFor[nbytes_?NumericQ, "Log"]    := Log2[1.0 + nbytes]
barHeightFor[nbytes_?NumericQ, "Linear"] := nbytes
barHeightFor[nbytes_?NumericQ, _]        := Log2[1.0 + nbytes]

linearScanPack[bufs_, barHeightMode_:"Log"] := Block[{
    sorted,
    slots = {},
    yMax = 0,
    out = {},
    foreverDepth
},
    (* Preserved bufs (weights, optimizer state) live across the
       realize call boundary -- their last_use_depth in this snapshot
       is just the latest depth at which a kernel READ them, not the
       depth at which they're free.  A future training step's
       backward pass will read them again.  Treat their slot as
       permanently occupied by setting the slot's last-use to a
       sentinel beyond any depth in the chart. *)
    foreverDepth = Max[Append[#["last_use_depth"] & /@ bufs, 0]] + 10000;
    sorted = SortBy[bufs, {#["alloc_depth"] &, -#["nbytes"] &}];
    Do[
        Block[{
            b = sorted[[i]],
            h,
            fitIdx,
            slot,
            y0,
            y1,
            slotEnd
        },
            h = barHeightFor[b["nbytes"], barHeightMode];
            (* Slot's last_use is foreverDepth for Preserved bufs --
               makes the slot ineligible for reuse downstream. *)
            slotEnd = If[ b["status"] === "Preserved",
                          foreverDepth, b["last_use_depth"]];
            fitIdx = SelectFirst[
                Range[Length[slots]],
                Function[k,
                    slots[[k, 3]] < b["alloc_depth"]
                    && (slots[[k, 2]] - slots[[k, 1]]) >= h
                ],
                Missing[]
            ];
            If[ MissingQ[fitIdx],
                y0 = yMax;
                y1 = y0 + h;
                yMax = y1;
                AppendTo[slots, {y0, y1, slotEnd}];
                ,
                slot = slots[[fitIdx]];
                y0 = slot[[1]];
                y1 = y0 + h;
                slots[[fitIdx, 3]] = slotEnd;
            ];
            AppendTo[out, Append[b, "y_range" -> {y0, y1}]]
        ],
        {i, Length[sorted]}
    ];
    {out, yMax}
]

(* TMemoryPlanGantt[plan] is defined in Visualization.wl.  The
   linearScanPack / barHeightFor / statusFill / statusEdge /
   peakConcurrentLive helpers above stay here -- TMemoryPlanReport
   shares them with the renderer. *)

End[];
EndPackage[];
