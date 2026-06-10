(* ::Package:: *)
(* Pool.wl: WL harness for the parallel WNF / NF worker pool.

   `nf` runs on a single thread by default; setting THVM_THREADS at
   the env level OR calling TThreads[n] from WL routes nf through
   wnf_pool_drain_parallel with N workers (worker 0 = caller, the
   rest pthread-spawned).  Per-run stats land in a process-global
   snapshot that TPoolStats[] reads out as an Association.

   Surface:

   TThreads[]         get the current worker count
   TThreads[n]        set it (0 reverts to env-var precedence;
                      clamped to [1, MAX_THREADS]).  Returns the
                      resolved count.
   TPoolStats[]       latest snapshot as an Association with keys
                      "Workers"     -> Integer,
                      "DrainRounds" -> Integer,
                      "DrainWallNs" -> Integer,
                      "TotalFires"  -> Integer,
                      "PerWorker"   -> {<|"Id", "Fires", "Steals",
                                           "StealAttempts", "Pushes",
                                           "ActiveNs", "IdleNs",
                                           "Wakeups", "ItrsDelta"|>, ...}
   TNfProfiled[t]     equivalent to TNf[t] but returns
                      <|"Result" -> _TTerm, "Stats" -> TPoolStats[]|>.
   TPoolStatsReport[s] short Tabular summary of a stats snapshot.
   TPoolStatsBench[t, ns] runs TNfProfiled[t] for each thread count
                      in `ns` (a list of integers); restores the
                      runtime between trials with TReset.  Returns a
                      Tabular comparing wall, fires, idle/active. *)

BeginPackage["THVMLink`"];

(* Forward-declare sibling-owned symbols so resolution lands in
   THVMLink` not Private`. *)
{TInit, TReset, TNf, TTerm};

TThreads::usage = "TThreads[] returns the worker count nf will use on the next call.  TThreads[n] sets it (0 to revert to env var $THVM_THREADS; clamped to [1, MAX_THREADS]).";

TPoolStats::usage = "TPoolStats[] returns the most recent nf-run worker-pool snapshot as an Association: \"Workers\" (count), \"DrainRounds\", \"DrainWallNs\", \"TotalFires\", and \"PerWorker\" (a list of per-worker stat associations: \"Id\", \"Fires\", \"Steals\", \"StealAttempts\", \"Pushes\", \"ActiveNs\", \"IdleNs\", \"Wakeups\", \"ItrsDelta\").";

TNfProfiled::usage = "TNfProfiled[term] runs TNf[term] and returns <|\"Result\" -> _TTerm, \"Stats\" -> TPoolStats[]|>.";

TPoolStatsReport::usage = "TPoolStatsReport[stats] renders a Tabular summary of a TPoolStats[] snapshot: wall time, total fires, per-worker fires, steals, active / idle ratio.";

TPoolStatsBench::usage = "TPoolStatsBench[builder, threadCounts_List] runs TNfProfiled[builder[]] under each thread count in `threadCounts`, calling TReset between runs to ensure a clean heap.  `builder` is a HoldFirst term-producing expression (or a no-arg Function); it must be re-evaluated AFTER TReset so the term is rebuilt against the fresh heap.  Returns a Tabular comparing wall time, total fires, and idle ratios.  Example: TPoolStatsBench[treeIter[14, TNum[#]&], {1, 2, 4, 8}] when treeIter is HoldFirst-safe; or pass a Function: TPoolStatsBench[(treeIter[14, TNum[#]&])&, {1, 2, 4, 8}].";

Begin["`Private`"];

(* === LibraryLink loaders ============================================ *)

$poolSetThreadsFn       := $poolSetThreadsFn       = load[
    "thvm_wl_pool_set_threads", {Integer}, Integer];
$poolGetThreadsFn       := $poolGetThreadsFn       = load[
    "thvm_wl_pool_get_threads", {}, Integer];
$poolStatsPoolFieldFn   := $poolStatsPoolFieldFn   = load[
    "thvm_wl_pool_stats_pool_field", {Integer}, Integer];
$poolStatsWorkerFieldFn := $poolStatsWorkerFieldFn = load[
    "thvm_wl_pool_stats_worker_field", {Integer, Integer}, Integer];

ensureInit[];   (* Pool helpers all need the runtime up. *)

(* === Field codes: mirror thvm_wl_pool_stats_*_field switches ===== *)

(* Pool-level scalars. *)
$poolFieldNWorkers     = 0;
$poolFieldDrainRounds  = 1;
$poolFieldDrainWallNs  = 2;
$poolFieldTotalFires   = 3;

(* Per-worker fields. *)
$workerFieldFires         = 0;
$workerFieldSteals        = 1;
$workerFieldStealAttempts = 2;
$workerFieldPushes        = 3;
$workerFieldActiveNs      = 4;
$workerFieldIdleNs        = 5;
$workerFieldWakeups       = 6;
$workerFieldItrsDelta     = 7;

(* === API =========================================================== *)

TThreads[]            := (ensureInit[]; $poolGetThreadsFn[])
TThreads[n_Integer]   := (ensureInit[]; $poolSetThreadsFn[n])

readWorkerStats[id_Integer] := <|
    "Id"            -> id,
    "Fires"         -> $poolStatsWorkerFieldFn[id, $workerFieldFires],
    "Steals"        -> $poolStatsWorkerFieldFn[id, $workerFieldSteals],
    "StealAttempts" -> $poolStatsWorkerFieldFn[id, $workerFieldStealAttempts],
    "Pushes"        -> $poolStatsWorkerFieldFn[id, $workerFieldPushes],
    "ActiveNs"      -> $poolStatsWorkerFieldFn[id, $workerFieldActiveNs],
    "IdleNs"        -> $poolStatsWorkerFieldFn[id, $workerFieldIdleNs],
    "Wakeups"       -> $poolStatsWorkerFieldFn[id, $workerFieldWakeups],
    "ItrsDelta"     -> $poolStatsWorkerFieldFn[id, $workerFieldItrsDelta]
|>

TPoolStats[] := Module[{nw},
    ensureInit[];
    nw = $poolStatsPoolFieldFn[$poolFieldNWorkers];
    <|
        "Workers"     -> nw,
        "DrainRounds" -> $poolStatsPoolFieldFn[$poolFieldDrainRounds],
        "DrainWallNs" -> $poolStatsPoolFieldFn[$poolFieldDrainWallNs],
        "TotalFires"  -> $poolStatsPoolFieldFn[$poolFieldTotalFires],
        "PerWorker"   -> Table[readWorkerStats[i], {i, 0, nw - 1}]
    |>
]

TNfProfiled[t_] := Module[{result},
    ensureInit[];
    result = TNf[t];
    <| "Result" -> result, "Stats" -> TPoolStats[] |>
]

(* === Renderer ====================================================== *)

formatNs[ns_Integer] := Which[
    ns >= 10^9, ToString[N[ns / 10.^9, 3]] <> " s",
    ns >= 10^6, ToString[N[ns / 10.^6, 3]] <> " ms",
    ns >= 10^3, ToString[N[ns / 10.^3, 3]] <> " us",
    True,       ToString[ns] <> " ns"
]

ratioPercent[a_, b_] := If[ b > 0, ToString[Round[100. a / b, 0.1]] <> "%", "-"]

TPoolStatsReport[s_Association] := Module[{wall, perW, totalActive, totalIdle},
    wall        = s["DrainWallNs"];
    perW        = s["PerWorker"];
    totalActive = Total[#["ActiveNs"] & /@ perW];
    totalIdle   = Total[#["IdleNs"]   & /@ perW];
    Column[{
        Style["Pool stats", Bold],
        Row[{"workers: ", s["Workers"],
             "  drain rounds: ", s["DrainRounds"],
             "  wall: ", formatNs[wall],
             "  total fires: ", s["TotalFires"]}],
        Row[{"active / wall: ", formatNs[totalActive], " (",
             ratioPercent[totalActive, wall * s["Workers"]],
             " of N*wall)",
             "   idle: ", formatNs[totalIdle]}],
        Tabular[
            Prepend[
                (w |-> {
                    w["Id"], w["Fires"],
                    w["Pushes"],
                    w["Steals"], w["StealAttempts"],
                    w["Wakeups"],
                    formatNs[w["ActiveNs"]], formatNs[w["IdleNs"]]
                }) /@ perW,
                {"id", "fires", "pushes", "steals", "stealAttempts",
                 "wakeups", "active", "idle"}],
            TableHeadings -> Automatic
        ]
    }]
]

(* === Bench helper ================================================== *)

SetAttributes[TPoolStatsBench, HoldFirst];
TPoolStatsBench[builder_, threadCounts_List] := Module[
    {rows, prevThreads = TThreads[]},
    rows = (n |-> Module[ {p, t},
        TReset[];
        TThreads[n];
        (* Rebuild the term against the fresh heap on every iteration: the
           previous run's TReset wiped the heap, so any term built before
           this Module-iter is now a dangling Term value. *)
        t = builder;
        p = TNfProfiled[t];
        <|
            "Threads"     -> n,
            "DrainWallNs" -> p["Stats", "DrainWallNs"],
            "TotalFires"  -> p["Stats", "TotalFires"],
            "ActiveNs"    -> Total[#["ActiveNs"] & /@ p["Stats", "PerWorker"]],
            "IdleNs"      -> Total[#["IdleNs"]   & /@ p["Stats", "PerWorker"]]
        |>
    ]) /@ threadCounts;
    TThreads[prevThreads];
    Dataset[
        AssociationThread[
            {"threads", "wall", "fires", "active", "idle", "idle%"},
            #] & /@ (r |-> {
                r["Threads"],
                formatNs[r["DrainWallNs"]],
                r["TotalFires"],
                formatNs[r["ActiveNs"]],
                formatNs[r["IdleNs"]],
                ratioPercent[r["IdleNs"], r["ActiveNs"] + r["IdleNs"]]
            }) /@ rows
    ]
]

End[];
EndPackage[];
