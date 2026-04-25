(* bench.wlt -- TBench / TBenchReport / TBenchExport (bm1).
   Smoke-test: run TBench on a tiny ADD-only "training step" and
   verify the returned Association has all expected keys + values
   in sane ranges. *)

VerificationTest[
    bench = TBench[<|
        "Name" -> "smoke-add",
        "InitFn" -> Function[
            TInit[];
            TReset[];
            {
                TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"],
                TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"]
            }
        ],
        "StepFn" -> Function[{hosts, t},
            TRealize @ TUOpAdd[hosts[[1]], hosts[[2]]];
            hosts
        ],
        "NumSteps" -> 3
    |>];
    Sort[Keys[bench]],
    Sort[{
        "name", "backend", "n_steps",
        "wall_time_ms", "ms_per_step",
        "kernel_count", "ten_count",
        "total_live_kib", "peak_concurrent_kib",
        "slot_reuse_headroom_pct"
    }],
    TestID -> "bench/smoke-add-association-keys"
]

VerificationTest[
    bench = TBench[<|
        "Name" -> "smoke-add",
        "InitFn" -> Function[
            TInit[];
            TReset[];
            {
                TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"],
                TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"]
            }
        ],
        "StepFn" -> Function[{hosts, t},
            TRealize @ TUOpAdd[hosts[[1]], hosts[[2]]];
            hosts
        ],
        "NumSteps" -> 3
    |>];
    {
        bench["name"] === "smoke-add",
        bench["n_steps"] === 3,
        bench["wall_time_ms"] >= 0,
        bench["ms_per_step"]  >= 0,
        bench["kernel_count"] >= 1,
        bench["ten_count"]    >= 2,
        bench["total_live_kib"] >= 0,
        IntegerQ[bench["kernel_count"]],
        IntegerQ[bench["ten_count"]]
    },
    {True, True, True, True, True, True, True, True, True},
    TestID -> "bench/smoke-add-values-sane"
]

VerificationTest[
    (* TBenchReport returns a Column expression. *)
    bench = TBench[<|
        "Name"     -> "smoke-add",
        "InitFn"   -> Function[
            TInit[];
            TReset[];
            {
                TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"],
                TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"]
            }
        ],
        "StepFn"   -> Function[{hosts, t},
            TRealize @ TUOpAdd[hosts[[1]], hosts[[2]]]; hosts
        ],
        "NumSteps" -> 1
    |>];
    Head @ TBenchReport[bench],
    Column,
    TestID -> "bench/report-head-is-column"
]

VerificationTest[
    (* TBenchExport writes a file we can re-read. *)
    bench = TBench[<|
        "Name"     -> "smoke-export",
        "InitFn"   -> Function[
            TInit[];
            TReset[];
            {
                TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"],
                TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"]
            }
        ],
        "StepFn"   -> Function[{hosts, t},
            TRealize @ TUOpAdd[hosts[[1]], hosts[[2]]]; hosts
        ],
        "NumSteps" -> 1
    |>];
    Module[{tmp = FileNameJoin[{$TemporaryDirectory,
                                 "thvm-bench-smoke-" <> ToString[$ProcessID] <> ".txt"}],
            content},
        TBenchExport[bench, tmp];
        content = Import[tmp, "Text"];
        DeleteFile[tmp];
        StringContainsQ[content, "TBench: smoke-export"]
    ],
    True,
    TestID -> "bench/export-writes-readable-file"
]
