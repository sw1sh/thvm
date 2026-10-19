---
Template: Symbol
Name: TGradOf
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TGradOf
Keywords: [autodiff, gradient, gradient slot, backward, read]
SeeAlso: [TGrad, TClearGrad, TRealize, TSet]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TGradOf]()[*t*]</code> gives the gradient held in the gradient slot of the tensor *t*.

<!-- #| annotation: 26.10.19: TGradOf reads TenDesc.grad, the per-tensor accumulator that the leaf rule of a TGrad[y] backward walk adds into (the param.grad of PyTorch and tinygrad). The slot holds a lazy UOp term, not a buffer, which is why the result needs TRealize. -->

## Details & Options

- <code>[TGrad]()[*y*]</code> fills the gradient slot of every floating-point tensor that *y* depends on, and <code>[TClearGrad]()</code> empties it.
- The gradient is a lazy [TTerm]() with the shape of *t*. <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- Each <code>[TGrad]()</code> walk adds to the slot, so after two walks over the same loss the slot holds twice the gradient.
- <code>[TGradOf]()[*t*]</code> gives <code>[Missing]()["NoGrad"]</code> when the slot is empty.
- For a lazy expression rather than a tensor, it gives <code>[Missing]()["NotATensor", *kind*]</code>: only tensors have gradient slots.
- An argument that is not a [TTerm]() returns unevaluated.

## Basic Examples

A tensor:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Differentiate the sum of its squares, filling its gradient slot:

```wl
TGrad[Total[w^2]]
```

<!-- => a TUOp summary box of kind REDUCE -->

The gradient, a lazy tensor:

```wl
g = TGradOf[w]
```

<!-- => a TUOp summary box of kind ADD -->

Its values are $2 w$:

```wl
Normal[TRealize[g]]
```

<!-- => {2., 4., 6.} -->

## Scope

A matrix:

```wl
m = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

Differentiate the sum of its squared entries:

```wl
TGrad[Total[m^2, All]]
```

<!-- => a TUOp summary box of kind REDUCE -->

The gradient has the shape of the matrix:

```wl
Dimensions[TGradOf[m]]
```

<!-- => {2, 2} -->

Its values:

```wl
Normal[TRealize[TGradOf[m]]]
```

<!-- => {{2., 4.}, {6., 8.}} -->

---

Two tensors in one loss:

```wl
a = TTensorCreate[{1., 2.}]
```

<!-- => a TTensor summary box: shape 2, dtype f32 -->

The second:

```wl
b = TTensorCreate[{3., 4.}]
```

<!-- => a TTensor summary box: shape 2, dtype f32 -->

One walk over their inner product:

```wl
TGrad[Total[a*b]]
```

<!-- => a TUOp summary box of kind REDUCE -->

The slot of *a* holds *b*:

```wl
Normal[TRealize[TGradOf[a]]]
```

<!-- => {3., 4.} -->

The slot of *b* holds *a*:

```wl
Normal[TRealize[TGradOf[b]]]
```

<!-- => {1., 2.} -->

## Properties and Relations

A tensor:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

A loss:

```wl
loss = Total[w^3]
```

<!-- => a TUOp summary box of kind REDUCE -->

Fill the slot:

```wl
TGrad[loss]
```

<!-- => the TUOp summary box of loss -->

The slot holds the same gradient as the two-argument [TGrad]() gives directly:

```wl
Normal[TRealize[TGradOf[w]]] === Normal[TRealize[TGrad[loss, w]]]
```

<!-- => True -->

A second walk adds to the slot:

```wl
TGrad[loss]
```

<!-- => the TUOp summary box of loss -->

The slot now holds twice the gradient:

```wl
Normal[TRealize[TGradOf[w]]]
```

<!-- => {6., 24., 54.} -->

## Possible Issues

A tensor that no walk has reached has an empty slot:

```wl
TGradOf[TTensorCreate[{1., 2.}]]
```

<!-- => Missing["NoGrad"] -->

---

A lazy expression has no gradient slot:

```wl
TGradOf[TTensorCreate[{1., 2.}]^2]
```

<!-- => Missing["NotATensor", "UOP"] -->

---

A tensor:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Fill its slot:

```wl
TGrad[Total[w]]
```

<!-- => a TUOp summary box of kind REDUCE -->

After [TClearGrad](), the slot is empty again:

```wl
TGradOf[TClearGrad[w]]
```

<!-- => Missing["NoGrad"] -->
