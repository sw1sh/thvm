---
Template: Symbol
Name: TUOpReduce
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpReduce
Keywords: [UOp, reduce, sum, max, axis, reduction, graph node]
SeeAlso: [TUOpMul, TUOpAdd, TUOpReshape, TRealize, Total, ArrayReduce]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpReduce]()[*src*, *axis*, "*kind*"]</code> gives the graph node reducing *src* along *axis*, by `"SUM"` or `"MAX"`.

<!-- #| annotation: 26.10.19: TUOpReduce allocates a UOP_REDUCE heap node with the reduce-op code and axis. The reduced axis is dropped from the shape (no keepdim); the Total and ArrayReduce[Total, ...] UpValues build the same node, and the f16/bf16 sum path upcasts to f32 around it (tinygrad's sum_acc_dtype). Two defects are reported 26.10.19: a kind other than "SUM" or "MAX" (such as "PROD") gives a malformed TTerm, and an axis past the rank leaves src unreduced with no message. -->

## Details & Options

- *axis* counts from 0 and must be less than the rank of *src*. The reduced axis is removed from the shape: reducing a 2 x 3 matrix along axis 0 gives shape `{3}`, and reducing a vector gives a scalar.
- *kind* is `"SUM"`, the sum of the elements along the axis, or `"MAX"`, their largest value.
- The node is lazy, with kind `REDUCE` and one source. <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back; a scalar reads back as a list of one element.
- On a [TTerm]() *t*, <code>[Total]()[*t*]</code> builds the `"SUM"` reduction along axis 0, <code>[ArrayReduce]()[[Total](), *t*, *k*]</code> the one along axis *k* - 1, and <code>[Total]()[*t*, [All]()]</code> sums every element.
- Under <code>[TGrad]()</code>, a `"SUM"` passes the gradient to every element it reduced, and a `"MAX"` passes it to the largest element only.

## Basic Examples

A tensor:

```wl
x = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

The node for the sum of its elements:

```wl
r = TUOpReduce[x, 0, "SUM"]
```

<!-- => a TUOp summary box: kind REDUCE, srcs 1 -->

Realize it:

```wl
Normal[TRealize[r]]
```

<!-- => {10.} -->

## Scope

A tensor:

```wl
x = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

The largest element:

```wl
Normal[TRealize[TUOpReduce[x, 0, "MAX"]]]
```

<!-- => {4.} -->

---

A 2 x 3 matrix:

```wl
m = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

Reducing along axis 0 sums the columns and removes that axis:

```wl
Normal[TRealize[TUOpReduce[m, 0, "SUM"]]]
```

<!-- => {5., 7., 9.} -->

Reducing along axis 1 sums the rows:

```wl
Normal[TRealize[TUOpReduce[m, 1, "SUM"]]]
```

<!-- => {6., 15.} -->

The largest element of each row:

```wl
Normal[TRealize[TUOpReduce[m, 1, "MAX"]]]
```

<!-- => {3., 6.} -->

The shape of the row sums, known without computing them:

```wl
Dimensions[TUOpReduce[m, 1, "SUM"]]
```

<!-- => {2} -->

## Applications

Two vectors:

```wl
a = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

The second:

```wl
b = TTensorCreate[{10., 20., 30., 40.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

Their inner product is the sum of their elementwise product:

```wl
Normal[TRealize[TUOpReduce[TUOpMul[a, b], 0, "SUM"]]]
```

<!-- => {300.} -->

## Properties and Relations

A 2 x 3 matrix:

```wl
m = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

[Total]() of the tensor is the sum along axis 0:

```wl
Normal[TRealize[TUOpReduce[m, 0, "SUM"]]] === Normal[TRealize[Total[m]]]
```

<!-- => True -->

[ArrayReduce]() counts axes from 1:

```wl
Normal[TRealize[TUOpReduce[m, 1, "SUM"]]] === Normal[TRealize[ArrayReduce[Total, m, 2]]]
```

<!-- => True -->

It agrees with [Total]() on the plain matrix:

```wl
Normal[TRealize[TUOpReduce[m, 0, "SUM"]]] === Total[Normal[m]]
```

<!-- => True -->

---

A tensor:

```wl
v = TTensorCreate[{1., 5., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Differentiate its largest element:

```wl
TGrad[TUOpReduce[v, 0, "MAX"]]
```

<!-- => a TUOp summary box of kind REDUCE -->

Only the largest element gets a gradient:

```wl
Normal[TRealize[TGradOf[v]]]
```

<!-- => {0., 1., 0.} -->

## Possible Issues

A tensor:

```wl
x = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Reducing a vector gives a scalar, of shape `{}`:

```wl
TTensorShape[TRealize[TUOpReduce[x, 0, "SUM"]]]
```

<!-- => {} -->

Its values still read back as a list of one element:

```wl
Normal[TRealize[TUOpReduce[x, 0, "SUM"]]]
```

<!-- => {6.} -->
