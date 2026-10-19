---
Template: Symbol
Name: TBench
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TBench
Keywords: [benchmark, timing, training step, memory, peak memory]
SeeAlso: [TMemoryPlan, TMemoryPlanReport, TRealize, TReset, AbsoluteTiming]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TBench]()[*spec*]</code> runs the step function given in the association *spec* for a number of steps and gives an association of timing and memory measurements.

<!-- #| annotation: 26.10.19: TBench times the Do loop of StepFn with AbsoluteTime and snapshots TMemoryPlan[] afterwards, so its kernel, tensor and memory figures cover the whole session since the last TReset, not just the benchmark; the examples reset first. It is meant for diffing a stable record across commits, which is what TBenchReport and TBenchExport (no reference pages yet) format and write. The rounded times can carry float noise such as 31.900000000000002 (reported 26.10.19). -->

## Details & Options

- *spec* has the keys `"Name"`, a label, `"bench"` by default; `"InitFn"`, a function of no arguments that sets up the state; `"StepFn"`, a function of the state and the 0-based step number that runs one step and gives the new state; and `"NumSteps"`, the number of steps, a positive integer.
- A *spec* without `"InitFn"`, `"StepFn"` or a positive integer `"NumSteps"` issues the message `TBench::spec` and gives `$Failed`.
- <code>[TBench]()</code> applies `"InitFn"` once, untimed, then applies `"StepFn"` `"NumSteps"` times, timing the loop.
- The result has the keys `"name"`, `"backend"`, `"n_steps"`, `"wall_time_ms"` and `"ms_per_step"`, the time of the loop and of one step in milliseconds, `"kernel_count"` and `"ten_count"`, the numbers of kernels and tensors, and `"total_live_kib"`, `"peak_concurrent_kib"` and `"slot_reuse_headroom_pct"`, the memory figures of <code>[TMemoryPlan]()</code> in kibibytes (KiB) and percent.
- The kernel, tensor and memory figures describe everything scheduled since the runtime was last reset with <code>[TReset]()</code>, so resetting before the benchmark limits them to it.
- The times depend on the machine. A step must realize its work with <code>[TRealize]()</code> for it to be timed.
- `TBenchReport` formats the result as a [Column](), and `TBenchExport[`*result*`, `*file*`]` writes that report to a text file.

## Basic Examples

Reset the runtime, so the counts below cover only the benchmark:

```wl
TReset[];
```

Time 20 steps that each add two vectors of 1024 elements:

```wl
TBench[<|"Name" -> "add", "InitFn" -> Function[{TTensorCreate[ConstantArray[1., 1024]], TTensorCreate[ConstantArray[2., 1024]]}], "StepFn" -> Function[{h, t}, TRealize[h[[1]] + h[[2]]]; h], "NumSteps" -> 20|>]
```

<!-- => <|"name" -> "add", "backend" -> "CPU", "n_steps" -> 20, "wall_time_ms" -> a time in milliseconds, "ms_per_step" -> a time in milliseconds, "kernel_count" -> 20, "ten_count" -> 22, "total_live_kib" -> 88., "peak_concurrent_kib" -> 88., "slot_reuse_headroom_pct" -> 0.|> -->

## Scope

Reset the runtime:

```wl
TReset[];
```

The state can be a single tensor, here the parameter of ten steps of gradient descent on $\sum_i (w_i - 1)^2$:

```wl
TBench[<|"Name" -> "descent", "InitFn" -> Function[TTensorCreate[{3., -2.}]], "StepFn" -> Function[{w, t}, TClearGrad[w]; TGrad[Total[(w - 1)^2]]; TSet[w, w - 0.25*TGradOf[w]]], "NumSteps" -> 10|>]
```

<!-- => <|"name" -> "descent", "backend" -> "CPU", "n_steps" -> 10, "wall_time_ms" -> a time, "ms_per_step" -> a time, "kernel_count" -> 1, "ten_count" -> 9, ...|> -->

---

Reset the runtime:

```wl
TReset[];
```

A step that does nothing schedules no kernel, and the backend is unknown:

```wl
TBench[<|"Name" -> "noop", "InitFn" -> Function[Null], "StepFn" -> Function[{h, t}, h], "NumSteps" -> 1|>]
```

<!-- => <|"name" -> "noop", "backend" -> "?", "n_steps" -> 1, "wall_time_ms" -> 0., "ms_per_step" -> 0., "kernel_count" -> 0, "ten_count" -> 0, "total_live_kib" -> 0., "peak_concurrent_kib" -> 0., "slot_reuse_headroom_pct" -> 0|> -->

---

Without a `"Name"`, the result is labeled `"bench"`:

```wl
TBench[<|"InitFn" -> Function[Null], "StepFn" -> Function[{h, t}, h], "NumSteps" -> 1|>]["name"]
```

<!-- => "bench" -->

## Applications

Reset the runtime:

```wl
TReset[];
```

A benchmark of the vector sum:

```wl
b = TBench[<|"Name" -> "add", "InitFn" -> Function[{TTensorCreate[ConstantArray[1., 1024]], TTensorCreate[ConstantArray[2., 1024]]}], "StepFn" -> Function[{h, t}, TRealize[h[[1]] + h[[2]]]; h], "NumSteps" -> 20|>]
```

<!-- => the result association of the "add" benchmark -->

Its report, for reading or for writing to a file with `TBenchExport`:

```wl
TBenchReport[b]
```

<!-- => Column: "TBench: add / CPU / 20 steps", then one line per measurement: wall_time_ms, ms_per_step, kernel_count 20, ten_count 22, total_live_kib 88., peak_concurrent_kib 88., slot_reuse_headroom_pct 0. -->

## Properties and Relations

Reset the runtime:

```wl
TReset[];
```

A benchmark of the vector sum:

```wl
b = TBench[<|"Name" -> "add", "InitFn" -> Function[{TTensorCreate[ConstantArray[1., 1024]], TTensorCreate[ConstantArray[2., 1024]]}], "StepFn" -> Function[{h, t}, TRealize[h[[1]] + h[[2]]]; h], "NumSteps" -> 20|>]
```

<!-- => the result association of the "add" benchmark -->

Each step realized one kernel, and the kernel count is the one [TMemoryPlan]() reports for the session:

```wl
b["kernel_count"] == Length[First[TMemoryPlan[]]["Kernels"]]
```

<!-- => True -->

## Possible Issues

Running on another backend needs the benchmark inside that backend's context; this is not evaluated here:

```wl
#| eval: false
TInContext[TContextNew["metal"], TBench[<|"Name" -> "metal-add", "InitFn" -> Function[{TTensorCreate[ConstantArray[1., 1024]], TTensorCreate[ConstantArray[2., 1024]]}], "StepFn" -> Function[{h, t}, TRealize[h[[1]] + h[[2]]]; h], "NumSteps" -> 20|>]]
```

<!-- => an association whose "backend" is "Metal" -->

---

Reset the runtime:

```wl
TReset[];
```

A step that builds a graph without realizing it does no work, so its time measures nothing but the graph building:

```wl
TBench[<|"Name" -> "lazy", "InitFn" -> Function[TTensorCreate[ConstantArray[1., 1024]]], "StepFn" -> Function[{x, t}, x + x; x], "NumSteps" -> 20|>]["kernel_count"]
```

<!-- => 0 -->

---

A spec without the number of steps is refused:

```wl
TBench[<|"Name" -> "add", "InitFn" -> Function[Null], "StepFn" -> Function[{h, t}, h]|>]
```

<!-- => the message TBench::spec is issued and the result is $Failed -->
