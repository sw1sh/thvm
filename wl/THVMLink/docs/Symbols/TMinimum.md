---
Template: Symbol
Name: TMinimum
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMinimum
Keywords: [minimum, min, elementwise, clamp, cap]
SeeAlso: [TMaximum, TClip, TWhere, Min, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMinimum]()[*a*, *b*]</code> gives the elementwise minimum of the tensors *a* and *b*.

<!-- #| annotation: 26.10.19: TMinimum mirrors tinygrad's Tensor.minimum. It lowers to TWhere[a < b, a, b] over the UOP_CMPLT mask, so thvm needs no MIN opcode; at a tie a < b is false and the select picks b, which is why the second argument gets the gradient there. The binary Min UpValue on TTerm calls it. -->

## Details & Options

- <code>[TMinimum]()[*a*, *b*]</code> gives a lazy [TTerm](). <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- *b* may be a tensor of the same shape as *a*, or a number, which is compared against every element of *a*. A number may also come first.
- When one argument is a [TTerm](), <code>[Min]()[*a*, *b*]</code> builds the same graph.
- Under <code>[TGrad]()</code>, the gradient of each element goes to the argument that is smaller there. At a tie it goes to *b*.
- An *a* and *b* that are both not [TTerm]() objects return unevaluated.

## Basic Examples

Two tensors:

```wl
a = TTensorCreate[{-1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The second:

```wl
b = TTensorCreate[{-4., -2., 9.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Their elementwise minimum, as a lazy graph:

```wl
m = TMinimum[a, b]
```

<!-- => a TUOp summary box of kind ADD -->

The values:

```wl
Normal[TRealize[m]]
```

<!-- => {-4., -2., 3.} -->

## Scope

A tensor:

```wl
x = TTensorCreate[{-3., -1., 0., 2., 5.}]
```

<!-- => a TTensor summary box: shape 5, dtype f32 -->

A number is compared against every element, here capping the values at 1:

```wl
Normal[TRealize[TMinimum[x, 1]]]
```

<!-- => {-3., -1., 0., 1., 1.} -->

The number may come first:

```wl
Normal[TRealize[TMinimum[0, x]]]
```

<!-- => {-3., -1., 0., 0., 0.} -->

---

A 2 x 2 matrix:

```wl
m = TTensorCreate[{{1., 5.}, {3., 2.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

A number is compared against every entry of a matrix:

```wl
Normal[TRealize[TMinimum[m, 2]]]
```

<!-- => {{1., 2.}, {2., 2.}} -->

## Properties and Relations

Two tensors:

```wl
a = TTensorCreate[{-1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The second:

```wl
b = TTensorCreate[{-4., -2., 9.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

[Min]() of two tensors builds the same graph:

```wl
Normal[TRealize[Min[a, b]]]
```

<!-- => {-4., -2., 3.} -->

It agrees with [Min]() on the plain lists, element by element:

```wl
Normal[TRealize[TMinimum[a, b]]] === MapThread[Min, {Normal[a], Normal[b]}]
```

<!-- => True -->

The minimum is the negated maximum of the negations:

```wl
Normal[TRealize[TMinimum[a, b]]] === Normal[TRealize[-TMaximum[-a, -b]]]
```

<!-- => True -->

---

Two tensors:

```wl
a = TTensorCreate[{-1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The second:

```wl
b = TTensorCreate[{-4., -2., 9.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Differentiate the sum of the minima:

```wl
TGrad[Total[TMinimum[a, b]]]
```

<!-- => a TUOp summary box of kind REDUCE -->

*a* is smaller only in the last position:

```wl
Normal[TRealize[TGradOf[a]]]
```

<!-- => {0., 0., 1.} -->

*b* gets the first two:

```wl
Normal[TRealize[TGradOf[b]]]
```

<!-- => {1., 1., 0.} -->

## Possible Issues

Two tensors that tie in their first element:

```wl
e = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The second:

```wl
f = TTensorCreate[{1., 5., 0.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Differentiate the sum of the minima:

```wl
TGrad[Total[TMinimum[e, f]]]
```

<!-- => a TUOp summary box of kind REDUCE -->

At the tie, the whole gradient goes to the second argument:

```wl
Normal[TRealize[TGradOf[f]]]
```

<!-- => {1., 0., 1.} -->

---

Plain lists are not tensors, and [TMinimum]() returns unevaluated:

```wl
TMinimum[{1., 2.}, {2., 1.}]
```

<!-- => TMinimum[{1., 2.}, {2., 1.}] -->
