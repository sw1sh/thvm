---
Template: Symbol
Name: TUOpReshape
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpReshape
Keywords: [UOp, reshape, movement, shape]
SeeAlso: [TUOpPermute, TUOpExpand, TTensorShape, TRealize, TMatMul]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpReshape]()[*src*, *shape*]</code> builds a `UOP_RESHAPE` node that reinterprets *src* with the integer list *shape*, preserving element order.

## Details & Options

- A movement op: it changes the shape metadata without recomputing or copying element data, so the total number of elements must be preserved.
- Lazy; the new shape is observed via <code>[TTensorShape]()</code> after <code>[TRealize]()</code>.
- Frequently paired with <code>[TUOpPermute]()</code> and <code>[TUOpExpand]()</code> to set up the broadcast shapes that <code>[TMatMul]()</code> lowers to.

## Basic Examples

Reshape a length-6 vector into a 2x3 matrix:

```wl
x = TTensorCreate[{1., 2., 3., 4., 5., 6.}];
Normal @ TRealize @ TUOpReshape[x, {2, 3}]
```
<!-- => {{1., 2., 3.}, {4., 5., 6.}} -->

## Properties and Relations

The reshape preserves row-major element order, so the reported shape is exactly the requested one:

```wl
x = TTensorCreate[{1., 2., 3., 4., 5., 6.}];
TTensorShape @ TRealize @ TUOpReshape[x, {3, 2}]
```
<!-- => {3, 2} -->
