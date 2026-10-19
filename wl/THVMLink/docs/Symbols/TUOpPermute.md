---
Template: Symbol
Name: TUOpPermute
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpPermute
Keywords: [UOp, permute, transpose, movement, axes, graph node]
SeeAlso: [TUOpReshape, TUOpReduce, TTensorShape, TRealize, Transpose]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpPermute]()[*src*, *perm*]</code> gives the graph node reordering the axes of *src*, so that axis *i* of the result is axis *perm*[[*i* + 1]] of *src*, counting axes from 0.

<!-- #| annotation: 26.10.19: TUOpPermute allocates a UOP_PERMUTE movement node with numpy's transpose(perm) convention: the result's shape is the source shape taken in the order perm. The Wolfram Language's Transpose[list, p] uses the inverse convention (level k of list goes to level p[[k]]), so TUOpPermute[t, perm] equals Transpose[Normal[t], InversePermutation[perm + 1]]. The TTerm UpValue Transpose[t, p] calls TUOpPermute[t, p - 1], so it follows numpy's convention and disagrees with the built-in Transpose for a permutation that is not its own inverse; a perm that is not a permutation (such as {1, 1}) is not checked and reads garbage (both reported 26.10.19). -->

## Details & Options

- *perm* is a permutation of `{0, 1, …, `*r*` - 1}` for a *src* of rank *r*. The shape of the result is the shape of *src* taken in the order *perm*.
- For a matrix, *perm* `{1, 0}` is the transpose, and `{0, 1}` leaves it unchanged.
- The node is lazy, with kind `PERMUTE` and one source; it moves no data and does no arithmetic. <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- <code>[Dimensions]()</code> gives the permuted shape without computing it.
- On a [TTerm]() *t*, <code>[Transpose]()[*t*]</code> builds the permutation that reverses all axes.
- The convention is that of numpy's `transpose`. In terms of the built-in [Transpose]() on the values, the result is <code>[Transpose]()[*values*, [InversePermutation]()[*perm* + 1]]</code>.

## Basic Examples

A 2 x 3 matrix:

```wl
m = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

The node for its transpose:

```wl
p = TUOpPermute[m, {1, 0}]
```

<!-- => a TUOp summary box: kind PERMUTE, srcs 1 -->

Realize it:

```wl
Normal[TRealize[p]]
```

<!-- => {{1., 4.}, {2., 5.}, {3., 6.}} -->

## Scope

A 2 x 3 matrix:

```wl
m = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

The identity permutation leaves it unchanged:

```wl
Normal[TRealize[TUOpPermute[m, {0, 1}]]]
```

<!-- => {{1., 2., 3.}, {4., 5., 6.}} -->

---

A tensor of shape 2 x 3 x 4:

```wl
t = TTensorCreate[ArrayReshape[Range[1., 24.], {2, 3, 4}]]
```

<!-- => a TTensor summary box: shape 2 x 3 x 4, dtype f32 -->

Moving the last axis to the front gives shape 4 x 2 x 3:

```wl
Dimensions[TUOpPermute[t, {2, 0, 1}]]
```

<!-- => {4, 2, 3} -->

The first slice of the result holds the first element of every row of *t*:

```wl
First[Normal[TRealize[TUOpPermute[t, {2, 0, 1}]]]]
```

<!-- => {{1., 5., 9.}, {13., 17., 21.}} -->

## Applications

A matrix:

```wl
m = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

The symmetric part of the matrix, $(m + m^T)/2$:

```wl
Normal[TRealize[TUOpMul[TUOpAdd[m, TUOpPermute[m, {1, 0}]], TUOpConst[0.5]]]]
```

<!-- => {{1., 2.5}, {2.5, 4.}} -->

## Properties and Relations

A 2 x 3 matrix:

```wl
m = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

[Transpose]() of the tensor builds the same reversal of the axes:

```wl
Normal[TRealize[TUOpPermute[m, {1, 0}]]] === Normal[TRealize[Transpose[m]]]
```

<!-- => True -->

---

A tensor of shape 2 x 3 x 4:

```wl
t = TTensorCreate[ArrayReshape[Range[1., 24.], {2, 3, 4}]]
```

<!-- => a TTensor summary box: shape 2 x 3 x 4, dtype f32 -->

The built-in [Transpose]() on the values gives the same result with the inverse permutation, counted from 1:

```wl
Normal[TRealize[TUOpPermute[t, {2, 0, 1}]]] === Transpose[Normal[t], InversePermutation[{2, 0, 1} + 1]]
```

<!-- => True -->

## Possible Issues

A tensor of shape 2 x 3 x 4:

```wl
t = TTensorCreate[ArrayReshape[Range[1., 24.], {2, 3, 4}]]
```

<!-- => a TTensor summary box: shape 2 x 3 x 4, dtype f32 -->

*perm* lists, for each axis of the result, the axis of *src* it comes from; with the built-in [Transpose]()'s reading of the same list, where each axis of *src* goes, the shape would be 3 x 4 x 2 instead:

```wl
Dimensions[TUOpPermute[t, {2, 0, 1}]]
```

<!-- => {4, 2, 3} -->
