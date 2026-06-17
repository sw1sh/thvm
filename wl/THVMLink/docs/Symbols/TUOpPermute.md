---
Template: Symbol
Name: TUOpPermute
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpPermute
Keywords: [UOp, permute, transpose, movement, axes]
SeeAlso: [TUOpReshape, TUOpExpand, TTensorShape, TRealize, TMatMul]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpPermute]()[*src*, *axes*]</code> builds a `UOP_PERMUTE` node that reorders *src*'s axes according to the zero-based permutation list *axes*.

## Details & Options

- A movement op: it rewrites the axis order in the shape tracker without copying element data.
- *axes* must be a permutation of <code>[Range]()[0, *rank* - 1]</code>; for a 2-D tensor, `{1, 0}` is a transpose.
- Lazy; the permuted shape is observed via <code>[TTensorShape]()</code> after <code>[TRealize]()</code>.

## Basic Examples

Transpose a 2x3 matrix into a 3x2 matrix:

```wl
m = TUOpReshape[TTensorCreate[{1., 2., 3., 4., 5., 6.}], {2, 3}];
Normal @ TRealize @ TUOpPermute[m, {1, 0}]
```
<!-- => {{1., 4.}, {2., 5.}, {3., 6.}} -->

## Properties and Relations

The permutation reorders the shape entries accordingly:

```wl
m = TUOpReshape[TTensorCreate[{1., 2., 3., 4., 5., 6.}], {2, 3}];
TTensorShape @ TRealize @ TUOpPermute[m, {1, 0}]
```
<!-- => {3, 2} -->
