---
Template: Symbol
Name: TTensorShape
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTensorShape
Keywords: [tensor, shape, dimensions, rank]
SeeAlso: [TTensorData, TTensorDType, TTensorCreate, TUOpReshape, TUOpPermute]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTensorShape]()[*t*]</code> returns tensor *t*'s shape as a list of integers.

## Details & Options

- The length of the returned list is the tensor's rank; an empty list `{}` denotes a scalar.
- Works on a realized tensor; for a UOp graph, realize it first with <code>[TRealize]()</code> (movement ops such as <code>[TUOpReshape]()</code> and <code>[TUOpPermute]()</code> change the shape).

## Basic Examples

Shape of a rank-1 tensor:

```wl
TTensorShape @ TTensorCreate[{1., 2., 3., 4.}]
```
<!-- => {4} -->

## Scope

A reshape changes the reported shape:

```wl
x = TTensorCreate[{1., 2., 3., 4., 5., 6.}];
TTensorShape @ TRealize @ ArrayReshape[x, {2, 3}]
```
<!-- => {2, 3} -->

---

A permute swaps the axes:

```wl
x = TTensorCreate[{1., 2., 3., 4., 5., 6.}];
TTensorShape @ TRealize @ Transpose[ArrayReshape[x, {2, 3}]]
```
<!-- => {3, 2} -->
