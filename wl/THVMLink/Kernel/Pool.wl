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

BeginPackage["WolframInstitute`THVMLink`"];

(* Forward-declare sibling-owned symbols so resolution lands in
   WolframInstitute`THVMLink` not Private`. *)
{TInit, TReset, TNf, TTerm};

GeneralUtilities`SetUsage[TThreads, "TThreads[] returns the worker count nf will use on the next call.
TThreads[n$] sets the count and returns the resolved value; n$ = 0 reverts to the $THVM_THREADS env var, and the value is clamped to the runtime's thread range."];

GeneralUtilities`SetUsage[TPoolStats, "TPoolStats[] returns the most recent nf-run worker-pool snapshot as an Association.
Pool-level keys are \"Workers\", \"DrainRounds\", \"DrainWallNs\", \"TotalFires\"; \"PerWorker\" is a list of per-worker associations keyed \"Id\", \"Fires\", \"Steals\", \"StealAttempts\", \"Pushes\", \"ActiveNs\", \"IdleNs\", \"Wakeups\", \"ItrsDelta\"."];

GeneralUtilities`SetUsage[TNfProfiled, "TNfProfiled[term$] runs TNf[term$] and returns an Association with key \"Result\" (the normalized _TTerm) and key \"Stats\" (the TPoolStats[] snapshot for that run)."];

GeneralUtilities`SetUsage[TPoolStatsReport, "TPoolStatsReport[stats$] renders a Tabular summary of a TPoolStats[] snapshot stats$: wall time, total fires, and per-worker fires, steals, and active/idle ratio."];

GeneralUtilities`SetUsage[TPoolStatsBench, "TPoolStatsBench[builder$, threadCounts$] runs TNfProfiled on builder$ under each thread count in the list threadCounts$, calling TReset between runs for a clean heap, and returns a Dataset comparing wall time, total fires, and idle ratios.
builder$ is held (HoldFirst) and re-evaluated after each TReset so the term is rebuilt against the fresh heap; pass a term-producing expression, e.g. TPoolStatsBench[treeIter[14, TNum[#]&], {1, 2, 4, 8}], or a no-arg Function, e.g. TPoolStatsBench[(treeIter[14, TNum[#]&])&, {1, 2, 4, 8}]."];

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
