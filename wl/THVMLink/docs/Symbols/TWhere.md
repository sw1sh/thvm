---
Template: Symbol
Name: TWhere
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TWhere
Keywords: [where, select, mask, ternary, conditional, elementwise]
SeeAlso: [TMaximum, TMinimum, TClip, TGrad, Less, If]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TWhere]()[*cond*, *a*, *b*]</code> gives, element by element, *a* where the mask *cond* is 1 and *b* where it is 0.

<!-- #| annotation: 26.10.19: thvm has no WHERE opcode, so TWhere lowers to cond*a + (1 - cond)*b, tinygrad's Tensor.where written with arithmetic. That form is why both branches are evaluated everywhere, why a non-finite value in the unselected branch poisons the position (0*Infinity is NaN), why a mask value between 0 and 1 blends, and why the gradient of a is cond and of b is 1 - cond. A comparison mask (UOP_CMPLT from the Less UpValue) carries no gradient. Equal on two TTerms is structural identity, not a UOP_CMPEQ mask. Tensor operands of different shapes are not broadcast (reported 26.10.19). -->

## Details & Options

- *cond* is a tensor of 0s and 1s of the shape of the result, usually a comparison such as *x* < *y* or *x* < 0, which gives 1 where the comparison holds and 0 elsewhere.
- *a* and *b* are tensors of the same shape as *cond*, lazy graphs, or numbers, which apply to every element.
- <code>[TWhere]()[*cond*, *a*, *b*]</code> gives a lazy [TTerm](). <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- The result is computed as *cond* *a* + (1 - *cond*) *b*, so both branches are evaluated at every position. A mask value between 0 and 1 blends the two branches.
- Under <code>[TGrad]()</code>, *a* receives the gradient where *cond* is 1 and *b* where it is 0. A comparison mask receives no gradient.
- Both branches must be finite where they are not selected: an infinite or indeterminate value there makes that position indeterminate.

## Basic Examples

A mask:

```wl
cond = TTensorCreate[{1., 0., 1., 0.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

Values for the positions where the mask is 1:

```wl
a = TTensorCreate[{10., 20., 30., 40.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

Values for the positions where it is 0:

```wl
b = TTensorCreate[{-1., -2., -3., -4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

The selection, as a lazy graph:

```wl
w = TWhere[cond, a, b]
```

<!-- => a TUOp summary box of kind ADD -->

Its values:

```wl
Normal[TRealize[w]]
```

<!-- => {10., -2., 30., -4.} -->

## Scope

A tensor:

```wl
z = TTensorCreate[{-2., -1., 1., 2.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

A comparison is a mask of 1s where it holds:

```wl
Normal[TRealize[z < 0]]
```

<!-- => {1., 1., 0., 0.} -->

The branches may be numbers; this replaces the negative elements by 0:

```wl
Normal[TRealize[TWhere[z < 0, 0, z]]]
```

<!-- => {0., 0., 1., 2.} -->

The sign of each element:

```wl
Normal[TRealize[TWhere[z < 0, -1, 1]]]
```

<!-- => {-1., -1., 1., 1.} -->

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

A comparison of two tensors selects the larger element at each position:

```wl
Normal[TRealize[TWhere[a < b, b, a]]]
```

<!-- => {-1., 2., 9.} -->

## Applications

A tensor:

```wl
x = TTensorCreate[{-3., -1., 0., 2., 4.}]
```

<!-- => a TTensor summary box: shape 5, dtype f32 -->

A leaky rectifier, which keeps positive values and scales negative ones by 0.1:

```wl
Normal[TRealize[TWhere[x < 0, 0.1*x, x]]]
```

<!-- => {-0.3, -0.1, 0., 2., 4.} -->

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

[TMaximum]() is this selection:

```wl
Normal[TRealize[TWhere[a < b, b, a]]] === Normal[TRealize[TMaximum[a, b]]]
```

<!-- => True -->

It agrees with an [If]() on the values, element by element:

```wl
Normal[TRealize[TWhere[a < b, b, a]]] === MapThread[If[#1 < #2, #2, #1] &, {Normal[a], Normal[b]}]
```

<!-- => True -->

---

A mask and two branches:

```wl
cond = TTensorCreate[{1., 0., 1.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The first branch:

```wl
a = TTensorCreate[{5., 6., 7.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The second branch:

```wl
b = TTensorCreate[{0., 0., 0.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Differentiate the sum of the selection:

```wl
TGrad[Total[TWhere[cond, a, b]]]
```

<!-- => a TUOp summary box of kind REDUCE -->

*a* gets the gradient where the mask is 1:

```wl
Normal[TRealize[TGradOf[a]]]
```

<!-- => {1., 0., 1.} -->

*b* gets it where the mask is 0:

```wl
Normal[TRealize[TGradOf[b]]]
```

<!-- => {0., 1., 0.} -->

## Possible Issues

A mask value between 0 and 1 blends the branches instead of choosing one:

```wl
Normal[TRealize[TWhere[TTensorCreate[{0.5, 0.}], TTensorCreate[{10., 10.}], TTensorCreate[{0., 0.}]]]]
```

<!-- => {5., 0.} -->

---

A tensor whose logarithm is infinite at its second element:

```wl
p = TTensorCreate[{1., 0.}]
```

<!-- => a TTensor summary box: shape 2, dtype f32 -->

The mask selects 5 at the second position, but the unselected logarithm there is infinite, so that position becomes indeterminate and the values cannot be read:

```wl
Normal[TRealize[TWhere[TTensorCreate[{1., 0.}], Log[p], TTensorCreate[{5., 5.}]]]]
```

<!-- => the message LibraryFunction::fpexc is issued and the result is $Failed -->
