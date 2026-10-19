---
Template: Symbol
Name: TUOpNeg
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpNeg
Keywords: [UOp, negate, minus, unary, elementwise, graph node]
SeeAlso: [TUOpAdd, TUOpMul, TUOpConst, TRealize, Minus, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpNeg]()[*a*]</code> gives the graph node for the elementwise negation of *a*.

<!-- #| annotation: 26.10.19: TUOpNeg allocates a UOP_NEG heap node. The Minus UpValue on TTerm maps to it, but -a in the Wolfram Language parses as Times[-1, a], so the sugar -a builds a MUL by the constant -1 instead; a - b builds an ADD with that product. Both give the same values. -->

## Details & Options

- *a* is a tensor or another graph node. The node is lazy, with kind `NEG` and one source; it has the shape and dtype of *a*.
- <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- On a [TTerm]() *a*, -*a* gives the same values, built as a product with the constant -1, and *a* - *b* is the sum of *a* and the negation of *b*.
- Under <code>[TGrad]()</code>, the gradient is negated.

## Basic Examples

A tensor:

```wl
a = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

The node for its negation:

```wl
n = TUOpNeg[a]
```

<!-- => a TUOp summary box: kind NEG, srcs 1 -->

Realize it:

```wl
Normal[TRealize[n]]
```

<!-- => {-1., -2., -3., -4.} -->

## Scope

An integer tensor:

```wl
Normal[TRealize[TUOpNeg[TTensorCreate[{1, -2, 3}]]]]
```

<!-- => {-1, 2, -3} -->

---

A matrix:

```wl
m = TTensorCreate[{{1., -2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

Negate it:

```wl
Normal[TRealize[TUOpNeg[m]]]
```

<!-- => {{-1., 2.}, {-3., -4.}} -->

## Applications

Two tensors:

```wl
a = TTensorCreate[{10., 20., 30.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The second:

```wl
b = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Their difference, as the sum of *a* and the negation of *b*:

```wl
Normal[TRealize[TUOpAdd[a, TUOpNeg[b]]]]
```

<!-- => {9., 18., 27.} -->

## Properties and Relations

A tensor:

```wl
a = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The sugar -*a* gives the same values:

```wl
Normal[TRealize[TUOpNeg[a]]] === Normal[TRealize[-a]]
```

<!-- => True -->

Two negations give back the original values:

```wl
Normal[TRealize[TUOpNeg[TUOpNeg[a]]]]
```

<!-- => {1., 2., 3.} -->

---

A tensor:

```wl
u = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Differentiate the sum of the negation:

```wl
TGrad[TUOpReduce[TUOpNeg[u], 0, "SUM"]]
```

<!-- => a TUOp summary box of kind REDUCE -->

The gradient is -1 in every position:

```wl
Normal[TRealize[TGradOf[u]]]
```

<!-- => {-1., -1., -1.} -->

## Possible Issues

A tensor:

```wl
a = TTensorCreate[{1., 2.}]
```

<!-- => a TTensor summary box: shape 2, dtype f32 -->

The sugar -*a* is a product with -1, so its node has kind `MUL`, not `NEG`:

```wl
-a
```

<!-- => a TUOp summary box: kind MUL, srcs 2 -->
