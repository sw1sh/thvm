---
Template: Symbol
Name: TMemoryPlanReport
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMemoryPlanReport
Keywords: [memory, report, buffer, summary, status]
SeeAlso: [TMemoryPlan, TMemoryPlanGantt, TScheduleGraph, TKernel, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMemoryPlanReport]()[*plan*]</code> returns a [Column]() summarizing a memory plan *plan*: the top-5 largest buffers by size, the top-5 longest-lived by alive span, the count by status, and the total live bytes for the active backend.

## Details & Options

- Pass a plan object, typically <code>[TMemoryPlan]()[]</code> (see <code>[TMemoryPlan]()</code>).
- For a graphical view of the same buffer lifecycles, use <code>[TMemoryPlanGantt]()</code>.
- Build a schedule first by realizing a graph; the report reads the current side tables.

## Basic Examples

Realize a small graph, snapshot the plan, and render the report; the result is a [Column]():

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize @ (Total[x^2] * Total[x]);
TMemoryPlanReport @ TMemoryPlan[]
```
<!-- => Column[...] - largest/longest-lived buffers, status counts, total live bytes -->

## Properties and Relations

The report is a [Column](), while <code>[TMemoryPlanGantt]()</code> on the same plan is a legended graphic:

```wl
x = TTensorCreate[Range[1., 8.]];
TRealize @ (Total[x^2] * Total[x]);
Head @ TMemoryPlanReport[TMemoryPlan[]]
```
<!-- => Column -->
