---
Template: Symbol
Name: TMemoryPlanReport
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMemoryPlanReport
Keywords: [memory, memory plan, report, buffer, peak memory, summary]
SeeAlso: [TMemoryPlan, TMemoryPlanGantt, TScheduleGraph, TMaterialize, TReset]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMemoryPlanReport]()[*plan*]</code> gives a text summary of the memory plan *plan*.

<!-- #| annotation: 26.10.19: The report is the notebook form of the memory-probe script's text summary. Its peak line is peakConcurrentLive over the Bufs records, the same numbers as the plan's "Peak" entry, and the headroom is total_bytes - peak_bytes, what a slot-reusing allocator could save. Small sizes print as "140.B" and the headroom percentage can carry float noise such as 2.9000000000000004 (reported 26.10.19). -->

## Details & Options

- *plan* is a memory plan object as <code>[TMemoryPlan]()[]</code> gives it.
- The report is a [Column]() whose first line gives the numbers of kernels and buffers and their total size. The next lines count the buffers by status and by backend.
- The peak line gives the largest total size of the buffers in use at one depth of the kernel graph, the depth where it occurs, and the headroom, the part of the total size an allocator reusing buffers could save.
- The report then lists the five largest buffers, each with its size, the depths over which it is in use and its status, and the five buffers in use over the most depths.
- Sizes are given in bytes (B), kibibytes (KiB), mebibytes (MiB) or gibibytes (GiB).
- An argument that is not a memory plan returns unevaluated.

## Basic Examples

Reset the runtime, so the plan below covers only this schedule:

```wl
TReset[];
```

A tensor of 32 elements:

```wl
a = TTensorCreate[ConstantArray[1., 32]]
```

<!-- => a TTensor summary box: shape 32, dtype f32 -->

Schedule the product of two sums, without running it:

```wl
TMaterialize[Total[a^2]*Total[a]]
```

<!-- => a TUOp summary box of kind KERNEL -->

The report of the memory plan:

```wl
TMemoryPlanReport[TMemoryPlan[]]
```

<!-- => Column: "TMemoryPlan: 3 kernels, 4 bufs, 140.B live" / "by status: <|External -> 1, Live -> 3|>" / "by backend: <|CPU -> 4|>" / "peak concurrent live: 136.B at depth 0 (slot-reuse headroom: 4.00B = 2.9% of total)" / "top-5 by bytes:" buf 1 128 B External, bufs 2, 3, 4 at 4 B Live / "top-5 by alive span:" bufs 2 and 3 with span 2 first -->

## Scope

Reset the runtime:

```wl
TReset[];
```

An input and a weight matrix:

```wl
x = TTensorCreate[ConstantArray[0.5, {16, 16}]]
```

<!-- => a TTensor summary box: shape 16 x 16, dtype f32 -->

The weight:

```wl
w = TTensorCreate[ConstantArray[0.25, {16, 4}]]
```

<!-- => a TTensor summary box: shape 16 x 4, dtype f32 -->

Schedule one layer and a sum over its output:

```wl
TMaterialize[Total[Tanh[x . w], All]]
```

<!-- => a TUOp summary box of kind KERNEL -->

The report; the 1 KiB input is the largest buffer:

```wl
TMemoryPlanReport[TMemoryPlan[]]
```

<!-- => Column: "TMemoryPlan: 2 kernels, 7 bufs, 1.52KiB live", peak 1.51KiB at depth 0, largest buf 1 at 1.00KiB External, then the 256 B buffers of the weight and the layer output -->

---

Reset the runtime:

```wl
TReset[];
```

The report of an empty schedule:

```wl
TMemoryPlanReport[TMemoryPlan[]]
```

<!-- => Column: "TMemoryPlan: 0 kernels, 0 bufs, 0B live", empty status and backend counts, peak 0B, empty buffer lists -->

## Properties and Relations

Reset the runtime:

```wl
TReset[];
```

A tensor:

```wl
a = TTensorCreate[ConstantArray[1., 32]]
```

<!-- => a TTensor summary box: shape 32, dtype f32 -->

Schedule the product of two sums:

```wl
TMaterialize[Total[a^2]*Total[a]]
```

<!-- => a TUOp summary box of kind KERNEL -->

The peak line of the report shows the `"Peak"` entry of the plan:

```wl
First[TMemoryPlan[]]["Peak"]
```

<!-- => <|"peak_bytes" -> 136, "peak_depth" -> 0, "total_bytes" -> 140|> -->

[TMemoryPlanGantt]() draws the same plan as a chart:

```wl
TMemoryPlanGantt[TMemoryPlan[]]
```

<!-- => a Legended Graphics titled "TMemoryPlan / CPU - 4 bufs / 3 kernels / 140.B total / depth 2", with a status legend; its buffer bars are currently not drawn (paclet bug reported 26.10.19) -->

## Possible Issues

The association inside a plan is not itself a plan, and [TMemoryPlanReport]() returns unevaluated:

```wl
TMemoryPlanReport[<|"Bufs" -> {}|>]
```

<!-- => TMemoryPlanReport[<|"Bufs" -> {}|>] -->
