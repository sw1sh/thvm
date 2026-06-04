---
Template: Symbol
Name: TScheduleGraph
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TScheduleGraph
Keywords: [schedule, graph, kernel, visualization, DAG]
SeeAlso: [TMemoryPlan, TMemoryPlanGantt, TKernel, TMaterialize, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TScheduleGraph]()[]</code> returns a [Graph]() of the live kernel schedule: one vertex per emitted kernel, with directed edges from producer kernel to consumer kernel labeled by the connecting tensor id.

## Details & Options

- External inputs (tensors with no producer kernel, such as weights and host tensors) appear as cyan `TEN`-shaped vertices when `"ShowExternalInputs" -> True` (the default).
- Disconnected kernels render as isolated vertices.
- Accepts all standard [Graph]() options.
- Build a schedule first by realizing a graph (or with <code>[TMaterialize]()</code>); the function reads the current side tables.

## Basic Examples

Build a small schedule, then count the kernel vertices in the graph:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize @ (Total[x^2] * Total[x]);
VertexCount @ TScheduleGraph[]
```
<!-- => 3 -->

## Scope

The call itself renders the schedule as a directed [Graph](); the last cell is the graphic:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize @ (Total[x^2] * Total[x]);
TScheduleGraph[]
```
<!-- => Graph[...] - producer-to-consumer kernel DAG with labeled tensor-id edges -->
