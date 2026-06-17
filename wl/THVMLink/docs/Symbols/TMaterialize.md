---
Template: Symbol
Name: TMaterialize
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMaterialize
Keywords: [materialize, schedule, kernelize, linearize, DAG]
SeeAlso: [TRealize, TScheduleGraph, TMemoryPlan, TUOpMul, TKernel]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMaterialize]()[*expr*]</code> runs the schedule, kernelize, and linearize rewrite on *expr* and returns the scheduled DAG term without firing any kernel.

## Details & Options

- [TMaterialize]() is the first half of <code>[TRealize]()</code> (<code>[TRealize]()[*expr*]</code> is <code>[TWnf]()[[TMaterialize]()[*expr*]]</code>); it rewrites UOPs to `UOP_KERNEL`s in place but never dispatches them.
- Use it to inspect the schedule before dispatch, e.g. with <code>[TScheduleGraph]()</code> or <code>[TMemoryPlan]()</code>, which read the materialized side tables.
- The returned term is a `TTerm`; passing it through [TWnf]() (or simply calling <code>[TRealize]()</code> on the original expression) fires the kernels and yields a readable tensor.

## Basic Examples

Materialize a graph; the result is a scheduled term, not yet a buffer:

```wl
x = TTensorCreate[{1., 2., 3., 4.}];
MatchQ[TMaterialize @ (x*x), _TTerm]
```
<!-- => True -->

## Properties and Relations

Materializing then weak-head normalizing produces the same buffer as a direct <code>[TRealize]()</code>:

```wl
x = TTensorCreate[{1., 2., 3., 4.}];
Normal @ TWnf @ TMaterialize @ (x*x)
```
<!-- => {1., 4., 9., 16.} -->
