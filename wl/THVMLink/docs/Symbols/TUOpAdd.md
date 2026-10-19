---
Template: Symbol
Name: TUOpAdd
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpAdd
Keywords: [UOp, add, sum, elementwise, graph node, tensor]
SeeAlso: [TUOpMul, TUOpNeg, TUOpConst, TUOpReduce, TRealize, Plus]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpAdd]()[*a*, *b*]</code> gives the graph node for the elementwise sum of *a* and *b*.

<!-- #| annotation: 26.10.19: TUOpAdd allocates a UOP_ADD heap node over the raw terms of its operands (thvm_wl_uop_binary) with no lifting or shape check; the Plus UpValue on TTerm is the user surface and lifts numbers with liftNumeric. A bare integer operand is read as a raw term id and builds a meaningless node, and tensors of different shapes are not broadcast but read past the smaller buffer (both reported 26.10.19), so the page sticks to TUOpConst for numbers and to equal shapes. -->

## Details & Options

- *a* and *b* are tensors, other graph nodes, or constants made with <code>[TUOpConst]()</code>. A number must be made a constant with <code>[TUOpConst]()</code> first.
- The node is lazy, with kind `ADD` and the two operands as sources. <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- Tensor operands have the same shape. A <code>[TUOpConst]()</code> constant is added to every element.
- On [TTerm]() objects, *a* + *b* builds the same kind of node and converts numbers to constants itself; *a* - *b* adds *a* to the negation of *b*.
- Under <code>[TGrad]()</code>, the gradient passes unchanged to both operands.

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

The node for their sum:

```wl
s = TUOpAdd[a, b]
```

<!-- => a TUOp summary box: kind ADD, srcs 2 -->

Realize it:

```wl
Normal[TRealize[s]]
```

<!-- => {11., 22., 33., 44.} -->

## Scope

A tensor:

```wl
a = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

A constant is added to every element:

```wl
Normal[TRealize[TUOpAdd[a, TUOpConst[0.5]]]]
```

<!-- => {1.5, 2.5, 3.5, 4.5} -->

An operand may itself be a node, here a product:

```wl
Normal[TRealize[TUOpAdd[TUOpMul[a, a], a]]]
```

<!-- => {2., 6., 12., 20.} -->

---

Two matrices of the same shape:

```wl
m = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

Add the matrix to itself:

```wl
Normal[TRealize[TUOpAdd[m, m]]]
```

<!-- => {{2., 4.}, {6., 8.}} -->

---

Integer tensors add exactly:

```wl
Normal[TRealize[TUOpAdd[TTensorCreate[{1, 2}], TTensorCreate[{10, 20}]]]]
```

<!-- => {11, 22} -->

## Applications

A tensor:

```wl
x = TTensorCreate[{0., 1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

The affine map $2 x + 1$, built from nodes:

```wl
Normal[TRealize[TUOpAdd[TUOpMul[x, TUOpConst[2.]], TUOpConst[1.]]]]
```

<!-- => {1., 3., 5., 7.} -->

## Properties and Relations

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

[Plus]() on the tensors computes the same sum:

```wl
Normal[TRealize[TUOpAdd[a, b]]] === Normal[TRealize[a + b]]
```

<!-- => True -->

The sum does not depend on the order of the operands:

```wl
Normal[TRealize[TUOpAdd[a, b]]] === Normal[TRealize[TUOpAdd[b, a]]]
```

<!-- => True -->

---

A tensor:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Differentiate the sum of $w + w^2$:

```wl
TGrad[TUOpReduce[TUOpAdd[w, TUOpMul[w, w]], 0, "SUM"]]
```

<!-- => a TUOp summary box of kind REDUCE -->

The gradient is $1 + 2 w$:

```wl
Normal[TRealize[TGradOf[w]]]
```

<!-- => {3., 5., 7.} -->

## Possible Issues

A tensor:

```wl
a = TTensorCreate[{1., 2.}]
```

<!-- => a TTensor summary box: shape 2, dtype f32 -->

Each application builds a new node, so two equal sums are different expressions:

```wl
TUOpAdd[a, a] === TUOpAdd[a, a]
```

<!-- => False -->

Their values agree:

```wl
Normal[TRealize[TUOpAdd[a, a]]] === Normal[TRealize[TUOpAdd[a, a]]]
```

<!-- => True -->
