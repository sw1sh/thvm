---
Template: Symbol
Name: TUOpMul
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpMul
Keywords: [UOp, multiply, product, elementwise, graph node, tensor]
SeeAlso: [TUOpAdd, TUOpNeg, TUOpConst, TUOpReduce, TRealize, Times]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpMul]()[*a*, *b*]</code> gives the graph node for the elementwise product of *a* and *b*.

<!-- #| annotation: 26.10.19: TUOpMul allocates a UOP_MUL heap node over the raw terms of its operands with no lifting or shape check; the Times UpValue on TTerm is the user surface. A product followed by a SUM reduce is the pattern TMatMul lowers to and the CPU dispatcher matches for BLAS. A bare number operand is read as a raw term id, and tensors of different shapes are not broadcast (reported 26.10.19). -->

## Details & Options

- *a* and *b* are tensors, other graph nodes, or constants made with <code>[TUOpConst]()</code>. A number must be made a constant with <code>[TUOpConst]()</code> first.
- The node is lazy, with kind `MUL` and the two operands as sources. <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- Tensor operands have the same shape. A <code>[TUOpConst]()</code> constant scales every element.
- On [TTerm]() objects, *a* *b* builds the same kind of node and converts numbers to constants itself, and *a*^2 is a product of *a* with itself.
- Under <code>[TGrad]()</code>, the gradient follows the product rule: each operand receives the gradient times the other operand.

## Basic Examples

A tensor:

```wl
a = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

Another:

```wl
b = TTensorCreate[{10., 20., 30., 40.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

The node for their product:

```wl
p = TUOpMul[a, b]
```

<!-- => a TUOp summary box: kind MUL, srcs 2 -->

Realize it:

```wl
Normal[TRealize[p]]
```

<!-- => {10., 40., 90., 160.} -->

## Scope

A tensor:

```wl
a = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

A constant scales every element:

```wl
Normal[TRealize[TUOpMul[a, TUOpConst[10.]]]]
```

<!-- => {10., 20., 30., 40.} -->

The product of a tensor with itself is its square:

```wl
Normal[TRealize[TUOpMul[a, a]]]
```

<!-- => {1., 4., 9., 16.} -->

---

A matrix:

```wl
m = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

The elementwise product of the matrix with its transpose:

```wl
Normal[TRealize[TUOpMul[m, TUOpPermute[m, {1, 0}]]]]
```

<!-- => {{1., 6.}, {6., 16.}} -->

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

A product followed by a sum is their inner product:

```wl
Normal[TRealize[TUOpReduce[TUOpMul[a, b], 0, "SUM"]]]
```

<!-- => {300.} -->

## Properties and Relations

A tensor:

```wl
a = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

[Times]() on the tensor computes the same product:

```wl
Normal[TRealize[TUOpMul[a, a]]] === Normal[TRealize[a^2]]
```

<!-- => True -->

---

A tensor:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Differentiate the sum of squares:

```wl
TGrad[TUOpReduce[TUOpMul[w, w], 0, "SUM"]]
```

<!-- => a TUOp summary box of kind REDUCE -->

By the product rule, the gradient is $2 w$:

```wl
Normal[TRealize[TGradOf[w]]]
```

<!-- => {2., 4., 6.} -->

## Possible Issues

An integer tensor:

```wl
n = TTensorCreate[{1, 2, 3}]
```

<!-- => a TTensor summary box: shape 3, dtype i32 -->

An integer tensor times a real constant gives integers, each product truncated toward zero:

```wl
Normal[TRealize[TUOpMul[n, TUOpConst[2.5]]]]
```

<!-- => {2, 5, 7} -->

The product written with `*` gives reals instead, as in tinygrad:

```wl
Normal[TRealize[n*2.5]]
```

<!-- => {2.5, 5., 7.5} -->
