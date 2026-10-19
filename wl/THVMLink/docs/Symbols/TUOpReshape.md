---
Template: Symbol
Name: TUOpReshape
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpReshape
Keywords: [UOp, reshape, movement, shape, view, graph node]
SeeAlso: [TUOpPermute, TUOpReduce, TTensorShape, TRealize, ArrayReshape]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpReshape]()[*src*, *shape*]</code> gives the graph node viewing *src* with the given *shape*, keeping the order of the elements.

<!-- #| annotation: 26.10.19: TUOpReshape allocates a UOP_RESHAPE movement node; it changes the view, not the data, and fuses into the kernel that reads it. The ArrayReshape UpValue on TTerm builds the same node. The element count is not checked: a shape with more elements than src reads past the end of its buffer (reported 26.10.19). -->

## Details & Options

- *shape* is a list of positive integers whose product is the number of elements of *src*.
- The elements keep their order, row by row, as in <code>[ArrayReshape]()</code>; only the way they are grouped into axes changes.
- The node is lazy, with kind `RESHAPE` and one source; it moves no data and does no arithmetic. <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- <code>[Dimensions]()</code> gives the new shape without computing it.
- On a [TTerm]() *t*, <code>[ArrayReshape]()[*t*, *shape*]</code> builds the same node.
- Under <code>[TGrad]()</code>, the gradient is reshaped back to the shape of *src*.

## Basic Examples

A vector of six elements:

```wl
x = TTensorCreate[{1., 2., 3., 4., 5., 6.}]
```

<!-- => a TTensor summary box: shape 6, dtype f32 -->

The node viewing it as a 2 x 3 matrix:

```wl
r = TUOpReshape[x, {2, 3}]
```

<!-- => a TUOp summary box: kind RESHAPE, srcs 1 -->

Realize it:

```wl
Normal[TRealize[r]]
```

<!-- => {{1., 2., 3.}, {4., 5., 6.}} -->

## Scope

A vector of six elements:

```wl
x = TTensorCreate[{1., 2., 3., 4., 5., 6.}]
```

<!-- => a TTensor summary box: shape 6, dtype f32 -->

The same elements as a 3 x 2 matrix:

```wl
Normal[TRealize[TUOpReshape[x, {3, 2}]]]
```

<!-- => {{1., 2.}, {3., 4.}, {5., 6.}} -->

As a column:

```wl
Normal[TRealize[TUOpReshape[x, {6, 1}]]]
```

<!-- => {{1.}, {2.}, {3.}, {4.}, {5.}, {6.}} -->

As a tensor of rank 3:

```wl
Dimensions[TUOpReshape[x, {1, 2, 3}]]
```

<!-- => {1, 2, 3} -->

---

A matrix:

```wl
m = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

Flatten it to a vector:

```wl
Normal[TRealize[TUOpReshape[m, {4}]]]
```

<!-- => {1., 2., 3., 4.} -->

## Applications

A vector of six elements:

```wl
x = TTensorCreate[{1., 2., 3., 4., 5., 6.}]
```

<!-- => a TTensor summary box: shape 6, dtype f32 -->

Sums of consecutive pairs, by viewing the vector as three rows of two and summing each row:

```wl
Normal[TRealize[TUOpReduce[TUOpReshape[x, {3, 2}], 1, "SUM"]]]
```

<!-- => {3., 7., 11.} -->

## Properties and Relations

A vector of six elements:

```wl
x = TTensorCreate[{1., 2., 3., 4., 5., 6.}]
```

<!-- => a TTensor summary box: shape 6, dtype f32 -->

[ArrayReshape]() on the tensor builds the same view:

```wl
Normal[TRealize[TUOpReshape[x, {2, 3}]]] === Normal[TRealize[ArrayReshape[x, {2, 3}]]]
```

<!-- => True -->

It agrees with [ArrayReshape]() on the plain list:

```wl
Normal[TRealize[TUOpReshape[x, {2, 3}]]] === ArrayReshape[Normal[x], {2, 3}]
```

<!-- => True -->

---

A tensor:

```wl
r = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

Differentiate the sum of three times its 2 x 2 view:

```wl
TGrad[TUOpReduce[TUOpReduce[TUOpMul[TUOpReshape[r, {2, 2}], TUOpConst[3.]], 1, "SUM"], 0, "SUM"]]
```

<!-- => a TUOp summary box of kind REDUCE -->

The gradient comes back in the shape of the original vector:

```wl
Normal[TRealize[TGradOf[r]]]
```

<!-- => {3., 3., 3., 3.} -->

## Possible Issues

A 2 x 3 matrix, to show that a reshape regroups the elements in order and does not transpose:

```wl
m = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

Viewed as 3 x 2, its rows are cut differently from the rows of its transpose:

```wl
Normal[TRealize[TUOpReshape[m, {3, 2}]]]
```

<!-- => {{1., 2.}, {3., 4.}, {5., 6.}} -->
