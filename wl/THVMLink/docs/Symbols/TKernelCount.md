---
Template: Symbol
Name: TKernelCount
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernelCount
Keywords: [kernel, count, kernel table, kernel id, schedule]
SeeAlso: [TKernel, TRealize, TMaterialize, TScheduleGraph, TReset]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernelCount]()[]</code> gives the number of entries in the kernel table of the runtime.

<!-- #| annotation: 26.10.19: TKernelCount reads KERNELS_NEXT on the C side. Entry 0 of the KernelEntry table is reserved, so a fresh or reset runtime reports 1 and live kernel ids run from 1 to TKernelCount[] - 1. The table persists across pages of a docs build kernel, which is why the Basic Examples reset it first. -->

## Details & Options

- Each kernel that <code>[TMaterialize]()</code> or <code>[TRealize]()</code> schedules adds one entry, and kernel ids are given out as 1, 2, 3, ... in that order.
- The table also holds one reserved entry, so the count is one more than the number of kernels, and the newest kernel has id <code>[TKernelCount]()[] - 1</code>.
- A graph that fuses into one kernel adds one entry; a graph with several stages adds one entry per kernel.
- Realizing a graph again schedules its kernels again, so the count grows with every <code>[TRealize]()</code> that has work to do. Realizing a tensor that already holds its values adds nothing.
- <code>[TReset]()[]</code> empties the table, leaving only the reserved entry.

## Basic Examples

Reset the runtime, so the counts below start from an empty table:

```wl
TReset[];
```

The table holds only its reserved entry:

```wl
TKernelCount[]
```

<!-- => 1 -->

Realize an elementwise sum, which compiles and runs one kernel:

```wl
TRealize[TTensorCreate[{1., 2.}] + TTensorCreate[{3., 4.}]]
```

<!-- => a TTensor summary box: shape 2, dtype f32 -->

The table has one more entry:

```wl
TKernelCount[]
```

<!-- => 2 -->

## Scope

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The count so far, which depends on everything realized earlier in the session:

```wl
n = TKernelCount[]
```

<!-- => a positive integer -->

The product of two sums needs a kernel for each sum and one for the product:

```wl
TRealize[Total[x^2]*Total[x]]
```

<!-- => a TTensor summary box for a scalar, dtype f32 -->

Three entries were added:

```wl
TKernelCount[] - n
```

<!-- => 3 -->

---

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The count so far:

```wl
n = TKernelCount[]
```

<!-- => a positive integer -->

Scheduling a graph without running it also adds its kernel:

```wl
TMaterialize[x - 1]
```

<!-- => a TUOp summary box of kind KERNEL -->

One entry was added:

```wl
TKernelCount[] - n
```

<!-- => 1 -->

## Properties and Relations

Realize a sum:

```wl
TRealize[TTensorCreate[{1., 2., 3.}] + TTensorCreate[{10., 20., 30.}]]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The newest kernel has id <code>[TKernelCount]()[] - 1</code>:

```wl
TKernel[TKernelCount[] - 1]
```

<!-- => a TKernel summary box: 2 inputs, output numel 3, output dtype f32 -->

## Possible Issues

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The square of it, as a lazy graph:

```wl
e = x^2
```

<!-- => a TUOp summary box of kind MUL -->

The count so far:

```wl
n = TKernelCount[]
```

<!-- => a positive integer -->

Realizing the same graph twice schedules its kernel twice:

```wl
Normal[TRealize[e]] === Normal[TRealize[e]]
```

<!-- => True -->

Two entries were added:

```wl
TKernelCount[] - n
```

<!-- => 2 -->
