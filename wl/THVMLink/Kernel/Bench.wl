(* ::Package:: *)
(* Bench.wl -- reusable benchmark harness.

   TBench[spec] runs `NumSteps` iterations of a caller-supplied
   step function and reports a stable Association of metrics
   suitable for diffing across commits:

     {
       "name", "backend", "n_steps",
       "wall_time_ms", "ms_per_step",
       "kernel_count", "ten_count",
       "total_live_kib", "peak_concurrent_kib",
       "slot_reuse_headroom_pct"
     }

   spec keys:
     "Name"     -- String label for the bench (e.g. "lenet-mnist").
     "InitFn"   -- Function[] that calls TInit[] + sets up host state
                   and returns the initial `hosts` value.
     "StepFn"   -- Function[hosts, t] that runs one training step
                   (Adam-style is typical) and returns updated hosts.
                   `t` is 0-indexed.
     "NumSteps" -- Integer count of steps to execute.

   The InitFn is responsible for backend selection (TInit picks up
   DEV); the bench reads the active backend from
   CURRENT_BACKEND for the report.

   TBenchReport[bench]      -- formats the Association as a Column
                               of Row entries for stdout / notebook.
   TBenchExport[bench, file] -- writes the same Column to `file` so
                                a CI run can drop a snapshot to disk
                                and the next commit can diff it. *)

BeginPackage["THVMLink`"];

TBench::usage = "TBench[spec] runs `NumSteps` of a step function under TInit + the active backend, then snapshots TMemoryPlan and returns an Association with wall_time_ms / ms_per_step / peak_concurrent_kib / total_live_kib / kernel_count / ten_count / slot_reuse_headroom_pct / backend.  spec keys: \"Name\", \"InitFn\", \"StepFn\", \"NumSteps\".";

TBenchReport::usage = "TBenchReport[bench] returns a Column rendering of a TBench result Association suitable for stdout.";

TBenchExport::usage = "TBenchExport[bench, file] writes TBenchReport[bench] to `file` (plain text), so subsequent runs can diff against the prior snapshot.";

(* Forward-declare bridge symbols owned by THVMLink.wl + MemoryPlan.wl *)
{TMemoryPlan, TKernelCount, TTensCount, TTotalBufBytes};

Begin["`Private`"];

backendNameLabel[1] = "CPU";
backendNameLabel[2] = "Metal";
backendNameLabel[_] = "?";

formatKib[n_] := Round[n / 1024.0, 0.1];

TBench[spec_Association] := Block[{
    name = spec["Name"],
    initFn = spec["InitFn"],
    stepFn = spec["StepFn"],
    nSteps = spec["NumSteps"],
    hosts,
    t0,
    t1,
    wallMs,
    msPerStep,
    plan,
    bufs,
    peak,
    totalKib,
    headroomPct,
    backendId,
    backend
},
    hosts = initFn[];
    t0 = AbsoluteTime[];
    Do[
        hosts = stepFn[hosts, t],
        {t, 0, nSteps - 1}
    ];
    t1 = AbsoluteTime[];
    wallMs = (t1 - t0) * 1000.0;
    msPerStep = If[ nSteps > 0, wallMs / nSteps, 0];
    plan = First @ TMemoryPlan[];
    bufs = plan["Bufs"];
    peak = plan["Peak"];
    totalKib = formatKib[peak["total_bytes"]];
    headroomPct = If[ peak["total_bytes"] > 0,
        Round[
            100. (peak["total_bytes"] - peak["peak_bytes"]) / peak["total_bytes"],
            0.1
        ],
        0
    ];
    backendId = If[ bufs === {},
        0,
        First @ Commonest[#["backend"] & /@ bufs]
    ];
    backend = backendNameLabel[backendId];
    <|
        "name" -> name,
        "backend" -> backend,
        "n_steps" -> nSteps,
        "wall_time_ms" -> Round[wallMs, 0.1],
        "ms_per_step" -> Round[msPerStep, 0.1],
        "kernel_count" -> Length[plan["Kernels"]],
        "ten_count" -> Length[plan["Tens"]],
        "total_live_kib" -> totalKib,
        "peak_concurrent_kib" -> formatKib[peak["peak_bytes"]],
        "slot_reuse_headroom_pct" -> headroomPct
    |>
]

TBenchReport[bench_Association] := Column[{
    Row[{"TBench: ", bench["name"], " / ", bench["backend"], " / ", bench["n_steps"], " steps"}],
    Row[{"  wall_time_ms       : ", bench["wall_time_ms"]}],
    Row[{"  ms_per_step        : ", bench["ms_per_step"]}],
    Row[{"  kernel_count       : ", bench["kernel_count"]}],
    Row[{"  ten_count          : ", bench["ten_count"]}],
    Row[{"  total_live_kib     : ", bench["total_live_kib"]}],
    Row[{"  peak_concurrent_kib: ", bench["peak_concurrent_kib"]}],
    Row[{"  slot_reuse_headroom_pct: ", bench["slot_reuse_headroom_pct"]}]
}]

TBenchExport[bench_Association, file_String] :=
    Export[file, ToString[TBenchReport[bench], OutputForm], "Text"]

End[];
EndPackage[];
