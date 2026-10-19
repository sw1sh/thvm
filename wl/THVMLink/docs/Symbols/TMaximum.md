---
Template: Symbol
Name: TMaximum
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMaximum
Keywords: [maximum, max, elementwise, rectifier, ReLU, clamp]
SeeAlso: [TMinimum, TClip, TWhere, Max, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMaximum]()[*a*, *b*]</code> gives the elementwise maximum of the tensors *a* and *b*.

<!-- #| annotation: 26.10.19: TMaximum mirrors tinygrad's Tensor.maximum. It lowers to TWhere[a < b, b, a] over the UOP_CMPLT mask, so thvm needs no MAX opcode; the select is what decides the gradient at a tie (the first argument wins, since a < b is false there). The binary Max UpValue on TTerm calls it. -->

## Details & Options

- <code>[TMaximum]()[*a*, *b*]</code> gives a lazy [TTerm](). <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- *b* may be a tensor of the same shape as *a*, or a number, which is compared against every element of *a*. A number may also come first.
- When one argument is a [TTerm](), <code>[Max]()[*a*, *b*]</code> builds the same graph.
- Under <code>[TGrad]()</code>, the gradient of each element goes to the argument that is larger there. At a tie it goes to *a*.
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

Their elementwise maximum, as a lazy graph:

```wl
m = TMaximum[a, b]
```

<!-- => a TUOp summary box of kind ADD -->

The values:

```wl
Normal[TRealize[m]]
```

<!-- => {-1., 2., 9.} -->

## Scope

A tensor:

```wl
x = TTensorCreate[{-3., -1., 0., 2., 5.}]
```

<!-- => a TTensor summary box: shape 5, dtype f32 -->

The maximum with the number 0, compared against every element, is the rectified linear unit:

```wl
Normal[TRealize[TMaximum[x, 0]]]
```

<!-- => {0., 0., 0., 2., 5.} -->

The number may come first:

```wl
Normal[TRealize[TMaximum[1, x]]]
```

<!-- => {1., 1., 1., 2., 5.} -->

---

A 2 x 2 matrix:

```wl
m = TTensorCreate[{{1., 5.}, {3., 2.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

A number is compared against every entry of a matrix:

```wl
Normal[TRealize[TMaximum[m, 2]]]
```

<!-- => {{2., 5.}, {3., 2.}} -->

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

[Max]() of two tensors builds the same graph:

```wl
Normal[TRealize[Max[a, b]]]
```

<!-- => {-1., 2., 9.} -->

It agrees with [Max]() on the plain lists, element by element:

```wl
Normal[TRealize[TMaximum[a, b]]] === MapThread[Max, {Normal[a], Normal[b]}]
```

<!-- => True -->

The maximum is the [TWhere]() select it is built from:

```wl
Normal[TRealize[TMaximum[a, b]]] === Normal[TRealize[TWhere[a < b, b, a]]]
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

Differentiate the sum of the maxima:

```wl
TGrad[Total[TMaximum[a, b]]]
```

<!-- => a TUOp summary box of kind REDUCE -->

*a* is larger in the first two positions and gets the gradient there:

```wl
Normal[TRealize[TGradOf[a]]]
```

<!-- => {1., 1., 0.} -->

*b* gets the rest:

```wl
Normal[TRealize[TGradOf[b]]]
```

<!-- => {0., 0., 1.} -->

## Possible Issues

Two tensors that tie in their first element:

```wl
c = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The second:

```wl
d = TTensorCreate[{1., 5., 0.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Differentiate the sum of the maxima:

```wl
TGrad[Total[TMaximum[c, d]]]
```

<!-- => a TUOp summary box of kind REDUCE -->

At the tie, the whole gradient goes to the first argument:

```wl
Normal[TRealize[TGradOf[c]]]
```

<!-- => {1., 0., 1.} -->

---

Plain lists are not tensors, and [TMaximum]() returns unevaluated:

```wl
TMaximum[{1., 2.}, {2., 1.}]
```

<!-- => TMaximum[{1., 2.}, {2., 1.}] -->
