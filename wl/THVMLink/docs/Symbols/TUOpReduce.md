---
Template: Symbol
Name: TUOpReduce
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpReduce
Keywords: [UOp, reduce, sum, max, axis]
SeeAlso: [TUOpMul, TUOpAdd, TUOpReshape, TRealize, TMatMul, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpReduce]()[*src*, *axis*, *kind*]</code> builds a `UOP_REDUCE` node that reduces *src* along *axis* with *kind* either `"SUM"` or `"MAX"`.

## Details & Options

- *axis* is zero-based; the reduced axis collapses to length 1 in the result.
- *kind* is `"SUM"` (additive) or `"MAX"` (maximum); `"SUM"` is the building block of <code>[TMatMul]()</code> after an elementwise <code>[TUOpMul]()</code>.
- Lazy: nothing reduces until <code>[TRealize]()</code> fires the graph.
- Differentiable: the `"SUM"` backward broadcasts the cotangent; the `"MAX"` backward routes it to the arg-max element.

## Basic Examples

Sum a tensor along its only axis:

```wl
x = TTensorCreate[{1., 2., 3., 4.}];
Normal @ TRealize @ TUOpReduce[x, 0, "SUM"]
```
<!-- => {10.} -->

## Scope

The `"MAX"` reduction returns the largest element:

```wl
x = TTensorCreate[{1., 2., 3., 4.}];
Normal @ TRealize @ TUOpReduce[x, 0, "MAX"]
```
<!-- => {4.} -->

## Properties and Relations

A multiply followed by a `"SUM"` reduce is a dot product:

```wl
a = TTensorCreate[{1., 2., 3., 4.}];
b = TTensorCreate[{10., 20., 30., 40.}];
Normal @ TRealize @ TUOpReduce[TUOpMul[a, b], 0, "SUM"]
```
<!-- => {300.} -->
