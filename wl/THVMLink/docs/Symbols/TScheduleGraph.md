---
Template: Symbol
Name: TScheduleGraph
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TScheduleGraph
Keywords: [schedule, kernel graph, dependency graph, DAG, visualization, kernel]
SeeAlso: [TMaterialize, TKernel, TKernelCount, TMemoryPlan, TMemoryPlanGantt, Graph]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TScheduleGraph]()[]</code> gives the graph of the kernels of the current schedule, with an edge from each kernel to every kernel that reads its output.

<!-- #| annotation: 26.10.19: The graph is built from the KernelEntry table (kids 1 .. TKernelCount[] - 1) and the TENS producer_kid / input_tids edges; an input tensor with no producer kernel becomes a TEN-shaped vertex. Two defects are reported 26.10.19: the kernel vertex label reads "k2 (2[OpCount]ops)" because scheduleKernelLabel indexes decodeKernelInfo[kid][[1]] as if it were an association, and the edges carry no tensor-id labels although the usage message says they do. -->

## Details & Options

- The result is a directed acyclic [Graph](). Each kernel is a vertex named `"k`*id*`"`, showing the kernel id, its output shape and its dtype.
- An input tensor that no kernel produces, such as a weight or data created with <code>[TTensorCreate]()</code>, is a vertex named `"t`*id*`"`, drawn in the shape of a tensor, with an edge to each kernel that reads it.
- A kernel that neither reads from nor feeds another kernel is an isolated vertex.
- The graph covers every kernel scheduled since the runtime was last reset with <code>[TReset]()</code>. Taken after <code>[TMaterialize]()</code>, it shows the schedule before anything runs.
- The following options can be given, as well as the options of [Graph]():

  | | | |
  | --- | --- | --- |
  | `"ShowExternalInputs"` | [True]() | whether to include the vertices of input tensors |

## Basic Examples

Reset the runtime, so the graph below shows only this schedule:

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

The input feeds the two sum kernels, which feed the product kernel:

```wl
TScheduleGraph[GraphLayout -> {"LayeredDigraphEmbedding", "Orientation" -> Left}, ImageSize -> 400]
```

<!-- => a Graph drawn left to right: the tensor vertex t1 {32} f32, edges to the kernels k1 and k2 with output {} f32, and edges from both to k3 -->

## Scope

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

The vertices:

```wl
VertexList[TScheduleGraph[]]
```

<!-- => {"k1", "k2", "k3", "t1"} -->

The edges:

```wl
EdgeList[TScheduleGraph[]]
```

<!-- => {DirectedEdge["t1", "k1"], DirectedEdge["t1", "k2"], DirectedEdge["k1", "k3"], DirectedEdge["k2", "k3"]} -->

Schedule a kernel that does not depend on the others:

```wl
TMaterialize[a + 1]
```

<!-- => a TUOp summary box of kind KERNEL -->

It joins the graph with an edge from the input only:

```wl
EdgeList[TScheduleGraph[]]
```

<!-- => {DirectedEdge["t1", "k1"], DirectedEdge["t1", "k2"], DirectedEdge["k1", "k3"], DirectedEdge["k2", "k3"], DirectedEdge["t1", "k4"]} -->

---

Reset the runtime:

```wl
TReset[];
```

An empty schedule gives an empty graph:

```wl
TScheduleGraph[]
```

<!-- => Graph[{}, {}] -->

## Options

### ShowExternalInputs

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

Without the input tensors, only the kernels and the edges between them remain:

```wl
TScheduleGraph["ShowExternalInputs" -> False, GraphLayout -> {"LayeredDigraphEmbedding", "Orientation" -> Left}, ImageSize -> 300]
```

<!-- => a Graph with the vertices k1, k2, k3 and the edges k1 -> k3 and k2 -> k3 -->

## Applications

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

An order in which the kernels can run, every kernel after the kernels it reads:

```wl
TopologicalSort[TScheduleGraph["ShowExternalInputs" -> False]]
```

<!-- => {"k1", "k2", "k3"} -->

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

The kernel graph has a vertex for each kernel the runtime counts:

```wl
VertexCount[TScheduleGraph["ShowExternalInputs" -> False]] == TKernelCount[] - 1
```

<!-- => True -->

It has no cycles:

```wl
AcyclicGraphQ[TScheduleGraph[]]
```

<!-- => True -->

## Possible Issues

A tensor, in a session that has already scheduled other kernels:

```wl
a = TTensorCreate[ConstantArray[1., 32]]
```

<!-- => a TTensor summary box: shape 32, dtype f32 -->

The count of kernels so far, which depends on the session:

```wl
n = TKernelCount[] - 1
```

<!-- => a nonnegative integer -->

Schedule one more kernel:

```wl
TMaterialize[a^2]
```

<!-- => a TUOp summary box of kind KERNEL -->

The graph shows every kernel since the last reset, not only the latest:

```wl
VertexCount[TScheduleGraph["ShowExternalInputs" -> False]] == n + 1
```

<!-- => True -->
