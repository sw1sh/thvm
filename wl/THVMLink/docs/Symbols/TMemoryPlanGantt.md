---
Template: Symbol
Name: TMemoryPlanGantt
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMemoryPlanGantt
Keywords: [memory, Gantt, buffer, lifecycle, visualization]
SeeAlso: [TMemoryPlan, TMemoryPlanReport, TScheduleGraph, TKernel, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMemoryPlanGantt]()[*plan*]</code> returns a Gantt-style chart of buffer lifecycles for a memory plan *plan*.

## Details & Options

- The X axis is topological depth on the kernel DAG; the Y axis is one row per buffer (sorted by allocation depth, then by size descending).
- Each bar spans `[alloc_depth, last_use_depth]` and is colored by status: blue Preserved, green Freeable, gray Live, orange External, red Dead.
- Hover tooltips expose buffer id, size, dtype, status, depths, and alias ids.
- Option `"BarHeight"`: `"Log"` (default; height proportional to <code>[Log2]()[1 + *nbytes*]</code>) or `"Uniform"` (all bars one unit tall).
- Pass a plan from <code>[TMemoryPlan]()</code>.

## Basic Examples

Realize a small graph, snapshot the plan, and render the Gantt chart; the result is a legended graphic:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize @ (Total[x^2] * Total[x]);
TMemoryPlanGantt @ TMemoryPlan[]
```
<!-- => Legended[Graphics[...], ...] - buffer-lifecycle bars colored by status -->

## Scope

Set `"BarHeight" -> "Uniform"` for equal-height rows:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize @ (Total[x^2] * Total[x]);
Head @ TMemoryPlanGantt[TMemoryPlan[], "BarHeight" -> "Uniform"]
```
<!-- => Legended -->
