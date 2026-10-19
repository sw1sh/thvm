---
Template: Symbol
Name: TUOpConst
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpConst
Keywords: [UOp, constant, scalar, literal, dtype, graph node]
SeeAlso: [TUOpAdd, TUOpMul, TUOpNeg, TTensorCreate, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpConst]()[*value*]</code> gives the graph node for the constant *value*, of dtype `"f32"`.

<code>[TUOpConst]()[*value*, "*dtype*"]</code> gives the constant with the named *dtype*.

<!-- #| annotation: 26.10.19: TUOpConst allocates a UOP_CONST heap node holding N[value] and a dtype code. TGrad treats CONST leaves as constants, which is why a TUOpConst operand gets no gradient while a one-element TTensorCreate tensor does. A symbolic value or an unknown dtype name gives a malformed TTerm[LibraryFunction[...][...]] (reported 26.10.19). -->

## Details & Options

- *value* is a number; an exact number is converted with [N](). The default *dtype* is `"f32"`; others include `"f64"`, `"f16"`, `"i32"` and `"u8"`.
- The node is lazy, with kind `CONST` and shape `{1}`. As an operand of <code>[TUOpAdd]()</code> or <code>[TUOpMul]()</code>, it applies to every element of the other operand.
- Realizing a constant on its own gives a tensor of one element.
- A value is converted to the dtype: a real value of an integer dtype is truncated toward zero, and an out-of-range value wraps around.
- On [TTerm]() objects, arithmetic with a plain number, such as *a* + 1, makes the constant itself.
- A constant is not differentiated: <code>[TGrad]()</code> gives no gradient to it.

## Basic Examples

The constant 5:

```wl
c = TUOpConst[5.]
```

<!-- => a TUOp summary box: kind CONST, srcs 1 -->

Realized on its own, it is a tensor of one element:

```wl
Normal[TRealize[c]]
```

<!-- => {5.} -->

## Scope

An integer constant:

```wl
Normal[TRealize[TUOpConst[7, "i32"]]]
```

<!-- => {7} -->

A half-precision constant:

```wl
TRealize[TUOpConst[2.5, "f16"]]
```

<!-- => a TTensor summary box: shape 1, dtype f16 -->

An exact number is converted to a machine number, then to single precision:

```wl
Normal[TRealize[TUOpConst[Pi]]]
```

<!-- => {3.14159} -->

---

A tensor:

```wl
x = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

As an operand, a constant applies to every element:

```wl
Normal[TRealize[TUOpMul[x, TUOpConst[10.]]]]
```

<!-- => {10., 20., 30., 40.} -->

Added to every element:

```wl
Normal[TRealize[TUOpAdd[x, TUOpConst[0.5]]]]
```

<!-- => {1.5, 2.5, 3.5, 4.5} -->

## Applications

A tensor:

```wl
x = TTensorCreate[{-2., -1., 0., 1., 2.}]
```

<!-- => a TTensor summary box: shape 5, dtype f32 -->

The polynomial $3 x^2 - 1$, built from nodes with two constants:

```wl
Normal[TRealize[TUOpAdd[TUOpMul[TUOpMul[x, x], TUOpConst[3.]], TUOpConst[-1.]]]]
```

<!-- => {11., 2., -1., 2., 11.} -->

## Properties and Relations

A tensor:

```wl
x = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

Arithmetic with a plain number on the tensor makes the constant itself:

```wl
Normal[TRealize[TUOpMul[x, TUOpConst[10.]]]] === Normal[TRealize[10*x]]
```

<!-- => True -->

---

A tensor:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Differentiate the sum of $3 w$:

```wl
TGrad[TUOpReduce[TUOpMul[w, TUOpConst[3.]], 0, "SUM"]]
```

<!-- => a TUOp summary box of kind REDUCE -->

The tensor gets the gradient 3 in every position; the constant gets none:

```wl
Normal[TRealize[TGradOf[w]]]
```

<!-- => {3., 3., 3.} -->

## Possible Issues

A real value of an integer dtype is truncated toward zero:

```wl
Normal[TRealize[TUOpConst[-2.7, "i32"]]]
```

<!-- => {-2} -->

A value outside the range of a byte wraps around:

```wl
Normal[TRealize[TUOpConst[300, "u8"]]]
```

<!-- => {44} -->
