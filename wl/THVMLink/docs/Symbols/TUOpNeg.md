---
Template: Symbol
Name: TUOpNeg
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpNeg
Keywords: [UOp, negate, unary, elementwise]
SeeAlso: [TUOpAdd, TUOpMul, TUOpConst, TRealize, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpNeg]()[*a*]</code> builds a `UOP_NEG` node representing the elementwise negation of a UOp input.

## Details & Options

- A unary elementwise op: the result has the same shape and dtype as *a*.
- Lazy: nothing is computed until <code>[TRealize]()</code> fires the graph.
- Differentiable: the backward rule negates the cotangent.
- Combined with <code>[TUOpAdd]()</code> it expresses subtraction, <code>[TUOpAdd]()[*a*, [TUOpNeg]()[*b*]]</code>.

## Basic Examples

Negate a small tensor:

```wl
a = TTensorCreate[{1., 2., 3., 4.}];
Normal @ TRealize @ TUOpNeg[a]
```
<!-- => {-1., -2., -3., -4.} -->

## Properties and Relations

Two negations cancel, recovering the original buffer:

```wl
a = TTensorCreate[{1., 2., 3.}];
Normal @ TRealize @ TUOpNeg[TUOpNeg[a]]
```
<!-- => {1., 2., 3.} -->

---

Subtraction is addition of a negation:

```wl
a = TTensorCreate[{10., 20., 30.}];
b = TTensorCreate[{1., 2., 3.}];
Normal @ TRealize @ TUOpAdd[a, TUOpNeg[b]]
```
<!-- => {9., 18., 27.} -->
