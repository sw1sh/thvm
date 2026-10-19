---
Template: Symbol
Name: TMemoryPlan
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMemoryPlan
Keywords: [memory, memory plan, buffer, lifetime, alive span, peak memory, schedule]
SeeAlso: [TMemoryPlanReport, TMemoryPlanGantt, TScheduleGraph, TMaterialize, TKernelCount, TReset]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMemoryPlan]()[]</code> gives a snapshot of the kernels, tensors and buffers of the current schedule, with the range of kernel-graph depths over which each buffer is in use.

<!-- #| annotation: 26.10.19: TMemoryPlan reads the KERNELS, TENS and CPU and Metal buffer side tables and derives each kernel's topological depth from the producer_kid and input_tids edges; TenDescs that share a buf_id collapse into one Bufs record whose alias_tids lists them, so views and constants never double-count. "Peak" is the lower bound a perfect slot-reusing allocator would need. The snapshot covers everything scheduled since the last TReset, which is why the examples reset first. formatBytes in the summary box and in TMemoryPlanReport prints small byte counts as "140.B" and "4.00B" (reported 26.10.19). -->

## Details & Options

- The result is a [TMemoryPlan]() object, displayed as a summary box with the numbers of kernels and buffers, the total size of the buffers and the backend that holds them.
- The association inside the object, <code>[First]()[*plan*]</code>, has the keys `"Kernels"`, `"Tens"`, `"Bufs"` and `"Peak"`.
- `"Kernels"` has one record per kernel, with its `"id"`, its `"input_tids"` and `"output_tid"`, and its `"depth"` in the kernel graph: 0 for a kernel that reads only tensors no kernel produces, and one more than the deepest kernel it reads from otherwise.
- `"Tens"` has one record per tensor, with the buffer it lives in, as `"buf_id"`, and the kernel that produces it, as `"producer_kid"`.
- `"Bufs"` has one record per buffer, with its size `"nbytes"`, its `"dtype"`, the depths `"alloc_depth"` and `"last_use_depth"` at which it is first written and last read, their span `"alive_span"`, and its `"status"`. Tensors that share a buffer, such as views, are listed together in `"alias_tids"`.
- The status of a buffer is `"External"` for an input that no kernel produces, `"Live"` for a buffer a kernel produces, and `"Preserved"`, `"Freeable"` or `"Dead"` for buffers the runtime keeps, may reuse, or has released.
- `"Peak"` holds `"peak_bytes"`, the largest total size of the buffers in use at any one depth, the depth `"peak_depth"` where it occurs, and `"total_bytes"`, the total size of all buffers. The difference is what an allocator reusing buffers could save.
- The snapshot covers every kernel scheduled since the runtime was last reset with <code>[TReset]()</code>. Taken after <code>[TMaterialize]()</code>, it shows the schedule before anything runs.
- <code>[TMemoryPlanReport]()</code> summarizes a plan as text, and <code>[TMemoryPlanGantt]()</code> draws its buffer lifetimes.

## Basic Examples

Reset the runtime, so the plan below covers only this schedule:

```wl
TReset[];
```

A tensor of 32 elements, 128 bytes:

```wl
a = TTensorCreate[ConstantArray[1., 32]]
```

<!-- => a TTensor summary box: shape 32, dtype f32 -->

Schedule the product of two sums, without running it:

```wl
TMaterialize[Total[a^2]*Total[a]]
```

<!-- => a TUOp summary box of kind KERNEL -->

The memory plan of the schedule:

```wl
plan = TMemoryPlan[]
```

<!-- => a TMemoryPlan summary box: kernels 3, bufs 4, live 140.B, backend CPU -->

## Scope

Reset the runtime:

```wl
TReset[];
```

A tensor of 32 elements:

```wl
a = TTensorCreate[ConstantArray[1., 32]]
```

<!-- => a TTensor summary box: shape 32, dtype f32 -->

Schedule the product of two sums:

```wl
TMaterialize[Total[a^2]*Total[a]]
```

<!-- => a TUOp summary box of kind KERNEL -->

The plan:

```wl
plan = TMemoryPlan[]
```

<!-- => a TMemoryPlan summary box: kernels 3, bufs 4 -->

Its parts:

```wl
Keys[First[plan]]
```

<!-- => {"Kernels", "Tens", "Bufs", "Peak"} -->

The two sums read only the input and sit at depth 0; the product reads both sums and sits at depth 1:

```wl
#["depth"] & /@ First[plan]["Kernels"]
```

<!-- => {0, 0, 1} -->

The product reads the tensors the two sums write:

```wl
First[plan]["Kernels"][[3]]["input_tids"]
```

<!-- => {2, 3} -->

The buffers, with their sizes, the depths over which they are in use, and their status:

```wl
Dataset[First[plan]["Bufs"]][All, {"id", "nbytes", "alloc_depth", "last_use_depth", "status"}]
```

<!-- => a Dataset of 4 rows: {1, 128, 0, 0, "External"}, {2, 4, 0, 1, "Live"}, {3, 4, 0, 1, "Live"}, {4, 4, 1, 1, "Live"} -->

The peak and the total size:

```wl
First[plan]["Peak"]
```

<!-- => <|"peak_bytes" -> 136, "peak_depth" -> 0, "total_bytes" -> 140|> -->

## Applications

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

The memory the schedule needs at its peak:

```wl
First[TMemoryPlan[]]["Peak"]["peak_bytes"]
```

<!-- => 1548 -->

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

The plan has one kernel record for each kernel the runtime counts:

```wl
Length[First[TMemoryPlan[]]["Kernels"]] == TKernelCount[] - 1
```

<!-- => True -->

[TMemoryPlanReport]() summarizes the plan as text:

```wl
TMemoryPlanReport[TMemoryPlan[]]
```

<!-- => a Column starting "TMemoryPlan: 3 kernels, 4 bufs, 140.B live", then the status and backend counts, the peak of 136 bytes at depth 0, and the largest and longest-lived buffers -->

## Possible Issues

Reset the runtime:

```wl
TReset[];
```

A tensor:

```wl
a = TTensorCreate[ConstantArray[1., 32]]
```

<!-- => a TTensor summary box: shape 32, dtype f32 -->

Schedule one kernel:

```wl
TMaterialize[a^2]
```

<!-- => a TUOp summary box of kind KERNEL -->

A plan taken now:

```wl
plan = TMemoryPlan[]
```

<!-- => a TMemoryPlan summary box: kernels 1 -->

Schedule another kernel:

```wl
TMaterialize[a + 1]
```

<!-- => a TUOp summary box of kind KERNEL -->

The plan is a snapshot and still has one kernel; a new <code>[TMemoryPlan]()[]</code> has both:

```wl
Length[First[plan]["Kernels"]] < Length[First[TMemoryPlan[]]["Kernels"]]
```

<!-- => True -->
