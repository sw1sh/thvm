---
Template: Symbol
Name: TBench
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TBench
Keywords: [benchmark, timing, training, memory, peak]
SeeAlso: [TBenchReport, TBenchExport, TProfile, TMemoryPlan, THotCountersDelta]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TBench]()[$spec$]</code> runs `NumSteps` of a step function under <code>[TInit]()</code> and the active backend, then snapshots <code>[TMemoryPlan]()</code> and returns an Association of timing + memory metrics.

`spec` keys: `"Name"`, `"InitFn"`, `"StepFn"`, `"NumSteps"`.

## Details & Options

- The returned Association carries: `wall_time_ms`, `ms_per_step`, `peak_concurrent_kib`, `total_live_kib`, `kernel_count`, `ten_count`, `slot_reuse_headroom_pct`, `backend`.
- `"InitFn"` runs once before the loop (model build, dataset load); `"StepFn"` runs `"NumSteps"` times under timing.
- <code>[TBenchReport]()[result]</code> formats the Association as a Column suitable for stdout.
- <code>[TBenchExport]()[result, file]</code> writes the report to `file` (plain text) so subsequent runs can diff against the prior snapshot.

## Basic Examples

Bench a one-line elementwise step.  `InitFn` runs once before timing starts and seeds the host-side `$x` / `$y` tensors; `StepFn` is the body whose wallclock + memory footprint are recorded:

```wl
Needs["THVMLink`"];
TBench[<|
    "Name"     -> "elementwise-add",
    "InitFn"   -> Function[Null,
        TInit[];
        $x = TTensorCreate[ConstantArray[1., {1024}]];
        $y = TTensorCreate[ConstantArray[2., {1024}]]
    ],
    "StepFn"   -> Function[Null, TRealize @ TUOpAdd[$x, $y]],
    "NumSteps" -> 20
|>]
```
<!-- => <|"wall_time_ms" -> _, "ms_per_step" -> _, "peak_concurrent_kib" -> _, ...|> -->

## Scope

Format the result for terminal output:

```wl
TBenchReport @ TBench[<|
    "Name"     -> "noop",
    "InitFn"   -> Function[Null, TInit[]],
    "StepFn"   -> Function[Null, Null],
    "NumSteps" -> 1
|>]
```
<!-- => Column[{"noop", "wall_time_ms 0.0", ...}] -->

## Applications

Export the report so a later run can diff against it:

```wl
file = FileNameJoin[{$TemporaryDirectory, "bench.txt"}];
TBenchExport[
    TBench[<|
        "Name"     -> "linear",
        "InitFn"   -> Function[Null,
            TInit[];
            $W = TGlorot[{16, 32}];
            $b = TZeros[{32}];
            $x = TTensorCreate[ConstantArray[1., {16}]]
        ],
        "StepFn"   -> Function[Null, TRealize @ TLinear[$x, $W, $b]],
        "NumSteps" -> 20
    |>],
    file
];
StringTake[Import[file, "Text"], UpTo[120]]
```
<!-- => "linear\nwall_time_ms ...\n..." -->

## Properties and Relations

`TBench` and <code>[THotCountersDelta]()</code> are complementary - `TBench` measures wallclock + memory for a whole step, `THotCountersDelta` counts substitution / kernel / grad / replay events inside a step:

```wl
THotCountersDelta["bench-window",
    TBench[<|
        "Name"     -> "elementwise-add",
        "InitFn"   -> Function[Null,
            TInit[];
            $x = TTensorCreate[ConstantArray[1., {1024}]];
            $y = TTensorCreate[ConstantArray[2., {1024}]]
        ],
        "StepFn"   -> Function[Null, TRealize @ TUOpAdd[$x, $y]],
        "NumSteps" -> 5
    |>]
]
```
<!-- => <|"Label" -> "bench-window", "WallMs" -> _, "Counters" -> <|heap_replace_cascade -> _, ...|>|> -->

## Possible Issues

Backend selection follows the active context; switch first when benching a different device.  `InitFn` is responsible for the `TInit[]` call; once the context's default device is set, `TInit[]` brings the matching back end up.  This cell is intentionally not evaluated at doc-build time so a half-shut-down Metal command buffer cannot orphan GPU work between cells; run it interactively against the live runtime:

```wl
#| eval: False
metal = TContextNew["metal"];
TInContext[ metal,
    TBench[<|
        "Name"     -> "metal-add",
        "InitFn"   -> Function[Null,
            TInit[];
            $x = TTensorCreate[ConstantArray[1., {1024}]];
            $y = TTensorCreate[ConstantArray[2., {1024}]]
        ],
        "StepFn"   -> Function[Null, TRealize @ TUOpAdd[$x, $y]],
        "NumSteps" -> 20
    |>]
]
```
<!-- => the Association's "backend" field reports "Metal" -->
