---
Template: Symbol
Name: TMaterialize
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMaterialize
Keywords: [materialize, schedule, kernelize, fusion, kernel, lazy graph]
SeeAlso: [TRealize, TKernel, TWnf, TScheduleGraph, TMemoryPlan, TKernelCount]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMaterialize]()[*expr*]</code> schedules the lazy tensor graph *expr* into kernels and gives the resulting kernel term, without running any kernel.

<!-- #| annotation: 26.10.19: TMaterialize is the schedule, kernelize and linearize rewrite of tinygrad's scheduler, run directly on the heap (thvm_materialize) with no wnf: UOP subgraphs are rewritten in place to UOP_KERNEL cells, each emitted kernel gets a KernelEntry and a pinned heap cell, and the returned term is the last kernel, whose first cell is the pre-allocated empty output TEN and second the NUM kernel id. TRealize[expr] is TWnf[TMaterialize[expr]] plus a buffer-pool sweep at exit. TMaterialize of a list or of a non-TTerm expression gives a malformed TTerm[LibraryFunction[...][...]] (reported 26.10.19); the list form exists only on TRealize. -->

## Details & Options

- Scheduling groups the graph into kernels, fusing chains of elementwise operations into one kernel, and adds each kernel to the kernel table. No kernel runs.
- The result is a [TTerm]() of kind `KERNEL`, the last kernel of the schedule, whose inputs refer to the kernels before it. <code>[TKernel]()[[TMaterialize]()[*expr*]]</code> gives it as a kernel object.
- <code>[TWnf]()</code> of the result runs the scheduled kernels and gives the output tensor. <code>[TRealize]()[*expr*]</code> does both steps at once.
- After <code>[TMaterialize]()</code>, <code>[TScheduleGraph]()</code> and <code>[TMemoryPlan]()</code> show the kernels and buffers of the schedule before anything runs.
- A tensor that already holds its values needs no kernel and materializes to a tensor.
- Each <code>[TMaterialize]()</code> schedules new kernels, also for a graph scheduled before.

## Basic Examples

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

Schedule its elementwise square:

```wl
s = TMaterialize[x^2]
```

<!-- => a TUOp summary box of kind KERNEL -->

The scheduled kernel has not run:

```wl
TKernel[s]["DispatchCount"]
```

<!-- => 0 -->

Run it with [TWnf]() and read the result:

```wl
Normal[TWnf[s]]
```

<!-- => {1., 4., 9., 16., 25., 36., 49., 64.} -->

## Scope

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The kernel count so far:

```wl
n = TKernelCount[]
```

<!-- => a positive integer -->

A chain of elementwise operations fuses into one kernel:

```wl
TMaterialize[Exp[x]*2 + 1]
```

<!-- => a TUOp summary box of kind KERNEL -->

One kernel was added:

```wl
TKernelCount[] - n
```

<!-- => 1 -->

---

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

The kernel count so far:

```wl
n = TKernelCount[]
```

<!-- => a positive integer -->

The product of two sums needs one kernel per sum and one for the product:

```wl
s = TMaterialize[Total[x^2]*Total[x]]
```

<!-- => a TUOp summary box of kind KERNEL -->

Three kernels were added:

```wl
TKernelCount[] - n
```

<!-- => 3 -->

The result is the last kernel, which reads the outputs of the two sums:

```wl
TKernel[s]["InputCount"]
```

<!-- => 2 -->

Running it runs all three, giving $204 \cdot 36$:

```wl
Normal[TWnf[s]]
```

<!-- => {7344.} -->

---

A tensor that already holds its values materializes to a tensor:

```wl
TMaterialize[TTensorCreate[{1., 2., 3.}]]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

## Applications

Reset the runtime, so the schedule graph below shows only this schedule:

```wl
TReset[];
```

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

Schedule the product of two sums:

```wl
TMaterialize[Total[x^2]*Total[x]]
```

<!-- => a TUOp summary box of kind KERNEL -->

The schedule before anything runs: the input tensor feeds the two sum kernels, which feed the product:

```wl
TScheduleGraph[GraphLayout -> {"LayeredDigraphEmbedding", "Orientation" -> Left}, ImageSize -> 400]
```

<!-- => a Graph with vertices t1, k1, k2, k3 and edges t1 -> k1, t1 -> k2, k1 -> k3, k2 -> k3 -->

## Properties and Relations

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

Materializing and then running gives the same values as [TRealize]():

```wl
Normal[TWnf[TMaterialize[x^2 + x]]] === Normal[TRealize[x^2 + x]]
```

<!-- => True -->

## Possible Issues

A tensor:

```wl
x = TTensorCreate[Range[1., 8.]]
```

<!-- => a TTensor summary box: shape 8, dtype f32 -->

A lazy graph:

```wl
e = x^2
```

<!-- => a TUOp summary box of kind MUL -->

The kernel count so far:

```wl
n = TKernelCount[]
```

<!-- => a positive integer -->

Scheduling the same graph twice adds its kernel twice:

```wl
TKernel[TMaterialize[e]]["Kid"] < TKernel[TMaterialize[e]]["Kid"]
```

<!-- => True -->

Two kernels were added:

```wl
TKernelCount[] - n
```

<!-- => 2 -->
