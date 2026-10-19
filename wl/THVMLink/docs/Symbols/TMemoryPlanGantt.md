---
Template: Symbol
Name: TMemoryPlanGantt
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMemoryPlanGantt
Keywords: [memory, Gantt chart, buffer lifetime, memory plan, visualization, peak memory]
SeeAlso: [TMemoryPlan, TMemoryPlanReport, TScheduleGraph, TMaterialize, TReset]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMemoryPlanGantt]()[*plan*]</code> draws the lifetimes of the buffers of the memory plan *plan* as a Gantt chart over the depths of the kernel graph.

<!-- #| annotation: 26.10.19: The chart packs each buffer into a row with a linear-scan slot allocator (Preserved buffers keep their slot forever), so overlapping rows show the reuse an allocator could make; the dashed red line marks the peak depth. The headroom percentage in the title can carry float noise such as 2.9000000000000004 (reported 26.10.19). -->

## Details & Options

- *plan* is a memory plan object as <code>[TMemoryPlan]()[]</code> gives it.
- The horizontal axis is the depth in the kernel graph. Each buffer is a bar from the depth where it is first written to the depth after its last read, and the bars are packed into rows so that buffers in use at different depths can share a row.
- Each bar is colored by the buffer's status, with a legend of the statuses present. A tooltip on a bar gives the buffer's id, size, dtype, status, depths, producing and consuming kernels, and tensors.
- The title gives the backend, the numbers of buffers and kernels, the total size and the number of depths, and the peak size with its depth and the headroom an allocator reusing buffers could save. A dashed line marks the peak depth.
- The result is a [Legended]() [Graphics](). For a plan with no buffers it is an empty [Graphics]().
- The following options can be given:

  | | | |
  | --- | --- | --- |
  | `"TopN"` | 40 | the number of largest buffers to draw |
  | `"BarHeight"` | `"Linear"` | bar heights proportional to the buffer size, or to $\log_2(1 + \text{size})$ with `"Log"` |

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

The buffer lifetimes of the schedule:

```wl
TMemoryPlanGantt[TMemoryPlan[]]
```

<!-- => a Legended Graphics titled "TMemoryPlan / CPU - 4 bufs / 3 kernels / 140B total / depth 2" and "peak concurrent: 136B at depth 0 (slot-reuse headroom 2.9%)", with one bar for each of the four buffers, colored by the statuses External and Live of the legend -->

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

The chart of its seven buffers:

```wl
TMemoryPlanGantt[TMemoryPlan[]]
```

<!-- => a Legended Graphics titled "TMemoryPlan / CPU - 7 bufs / 2 kernels / 1.52KiB total / depth 2" and "peak concurrent: 1.51KiB at depth 0", with a bar for each of the seven buffers and a status legend -->

---

Reset the runtime:

```wl
TReset[];
```

A plan with no buffers gives an empty graphic:

```wl
TMemoryPlanGantt[TMemoryPlan[]]
```

<!-- => an empty Graphics -->

## Options

### TopN

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

Draw only the two largest of the four buffers; a third title line says how much of the memory they cover:

```wl
TMemoryPlanGantt[TMemoryPlan[], "TopN" -> 2]
```

<!-- => a Legended Graphics whose title ends "showing top 2 of 4 bufs (= 94.3% of bytes)" -->

### BarHeight

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

With `"Log"`, the bar heights grow with the logarithm of the size, so small buffers stay visible next to large ones; the vertical axis is labeled "stacked Log2[1+nbytes], linear-scan slot allocator":

```wl
TMemoryPlanGantt[TMemoryPlan[], "BarHeight" -> "Log"]
```

<!-- => a Legended Graphics with the vertical axis label "stacked Log2[1+nbytes], linear-scan slot allocator" -->

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

The chart is a legended graphic:

```wl
Head[TMemoryPlanGantt[TMemoryPlan[]]]
```

<!-- => Legended -->

[TMemoryPlanReport]() gives the same plan as text, including the peak the chart's dashed line marks:

```wl
TMemoryPlanReport[TMemoryPlan[]]
```

<!-- => a Column starting "TMemoryPlan: 3 kernels, 4 bufs, 140B live", with the peak of 136 bytes at depth 0 -->

## Possible Issues

The association inside a plan is not itself a plan, and [TMemoryPlanGantt]() returns unevaluated:

```wl
TMemoryPlanGantt[<|"Bufs" -> {}|>]
```

<!-- => TMemoryPlanGantt[<|"Bufs" -> {}|>] -->
