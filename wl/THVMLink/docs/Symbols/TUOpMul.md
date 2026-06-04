---
Template: Symbol
Name: TUOpMul
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpMul
Keywords: [UOp, multiply, elementwise, tensor]
SeeAlso: [TUOpAdd, TUOpNeg, TUOpReduce, TRealize, TMaterialize, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpMul]()[*a*, *b*]</code> builds a `UOP_MUL` node representing the elementwise product of two UOp inputs.

## Details & Options

- Lazy: the node sits in the heap until <code>[TMaterialize]()</code> schedules it or <code>[TRealize]()</code> fires it.
- Broadcasts when one side carries a broadcastable shape; for an explicit broadcast wrap with <code>[TUOpExpand]()</code> first.
- Differentiable: <code>[TGrad]()</code> threads the cotangent through both inputs via the product rule.
- `TTensor` leaves and <code>[TUOpConst]()</code> leaves are accepted directly; bare integers and reals are not lifted automatically (use <code>[TUOpConst]()</code> or <code>[TTensorCreate]()</code>).

## Basic Examples

Multiply two small tensors:

```wl
a = TTensorCreate[{1., 2., 3., 4.}];
b = TTensorCreate[{10., 20., 30., 40.}];
Normal @ TRealize @ TUOpMul[a, b]
```
<!-- => {10., 40., 90., 160.} -->

## Scope

Squaring is <code>[TUOpMul]()[*x*, *x*]</code>; compose it with a reduction to form a sum of squares:

```wl
x = TTensorCreate[{1., 2., 3., 4.}];
Normal @ TRealize @ TUOpReduce[TUOpMul[x, x], 0, "SUM"]
```
<!-- => {30.} -->

## Properties and Relations

The product rule gives the gradient of a sum of squares as *2x*:

```wl
w = TRequiresGrad @ TTensorCreate[{1., 2., 3.}];
TRealize @ TGrad @ TUOpReduce[TUOpMul[w, w], 0, "SUM"];
Normal @ TRealize @ TGradOf[w]
```
<!-- => {2., 4., 6.} -->
