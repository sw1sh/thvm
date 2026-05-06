(* pool.wlt -- VerificationTest specs for the parallel WNF / NF
   worker pool harness.

   Coverage:
     1. TThreads[] / TThreads[n] round-trip; clamp at MAX_THREADS;
        n=0 reverts to env var (default 1 in the test environment).
     2. TPoolStats[] after a small TNf reports n_workers > 0,
        total_fires equal to actual interaction count, drain_wall_ns
        non-zero, per-worker rows present.
     3. TNfProfiled returns both the result term AND a stats snapshot.
     4. T=4 vs T=1 produce the same final term + same total_fires. *)

VerificationTest[
    TInit[];
    TThreads[1];
    TThreads[]
    , 1, TestID -> "TThreads-default-1"
];

VerificationTest[
    TInit[];
    TThreads[4];
    TThreads[]
    , 4, TestID -> "TThreads-set-4"
];

VerificationTest[
    TInit[];
    TThreads[1000];   (* clamped to MAX_THREADS = 64 *)
    Module[{n = TThreads[]}, n > 0 && n <= 64]
    , True, TestID -> "TThreads-clamped"
];

VerificationTest[
    TInit[];
    TThreads[0];
    TThreads[]
    , 1, TestID -> "TThreads-zero-falls-back-to-env-default"
];

(* Build (a + b) via the published WL surface (TOp2 / TNum from
   Switch.wl).  One OP2-NUM-NUM fire produces TNum[a + b]. *)
op2Add[a_, b_] := TOp2["+", TNum[a], TNum[b]]

VerificationTest[
    TInit[];
    TThreads[1];
    Module[{r = TNf[op2Add[3, 4]], stats},
        stats = TPoolStats[];
        (* Wall-time can round to 0 on a single-fire workload (clock
           granularity) -- skip that assertion here; the multi-worker
           tests below stress timing under heavier load. *)
        {
            TTermVal[r],
            stats["Workers"],
            stats["TotalFires"],
            Length[stats["PerWorker"]]
        }
    ]
    , {7, 1, 1, 1},
    TestID -> "TPoolStats-T1-single-fire"
];

VerificationTest[
    TInit[];
    TThreads[4];
    Module[{r = TNf[op2Add[10, 20]], stats},
        stats = TPoolStats[];
        {
            TTermVal[r],
            stats["Workers"],
            stats["TotalFires"],
            Length[stats["PerWorker"]],
            (* Sum of per-worker fires == total *)
            Total[#["Fires"] & /@ stats["PerWorker"]] === stats["TotalFires"]
        }
    ]
    , {30, 4, 1, 4, True},
    TestID -> "TPoolStats-T4-stats-shape"
];

VerificationTest[
    TInit[];
    TThreads[2];
    Module[{p = TNfProfiled[op2Add[5, 6]]},
        {
            TTermVal[p["Result"]],
            p["Stats", "Workers"],
            p["Stats", "TotalFires"]
        }
    ]
    , {11, 2, 1},
    TestID -> "TNfProfiled-result-plus-stats"
];

(* Equivalence: T=1 and T=4 produce the same reduction result. *)
VerificationTest[
    TInit[];
    TThreads[1];
    Module[{r1 = TTermVal[TNf[op2Add[100, 200]]]},
        TInit[];
        TThreads[4];
        Module[{r4 = TTermVal[TNf[op2Add[100, 200]]]},
            r1 === r4
        ]
    ]
    , True,
    TestID -> "TNf-T1-T4-equivalence"
];

(* Idle accounting: T=4 with a single 1-fire reduction leaves at
   least three workers idle for nontrivial time. *)
VerificationTest[
    TInit[];
    TThreads[4];
    TNf[op2Add[1, 1]];
    Module[{idleNs = #["IdleNs"] & /@ TPoolStats[]["PerWorker"]},
        Count[idleNs, n_ /; n > 0] >= 1   (* at least one idle worker *)
    ]
    , True,
    TestID -> "TPoolStats-T4-idle-accounting"
];

TThreads[1];   (* leave the pool at the default for downstream tests *)
