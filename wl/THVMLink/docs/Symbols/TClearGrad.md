---
Template: Symbol
Name: TClearGrad
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TClearGrad
Keywords: [autodiff, gradient, zero grad, reset, optimizer]
SeeAlso: [TGrad, TGradOf, TSet, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TClearGrad]()[*t*]</code> clears the gradient accumulated on the tensor *t* and gives *t*.

<!-- #| annotation: 26.10.19: TClearGrad is the zero_grad step of a PyTorch or tinygrad optimizer loop. It empties the tensor descriptor's grad slot on the C side rather than writing zeros into a buffer, which is why TGradOf reads Missing["NoGrad"] afterwards instead of a zero tensor. -->

## Details & Options

- Each <code>[TGrad]()</code> walk adds its gradient to the tensor's gradient slot, so a second walk over the same loss doubles what <code>[TGradOf]()</code> reads. <code>[TClearGrad]()[*t*]</code> empties the slot between walks.
- After clearing, <code>[TGradOf]()[*t*]</code> gives <code>[Missing]()["NoGrad"]</code> until the next <code>[TGrad]()</code> fills the slot again.
- <code>[TClearGrad]()[*t*]</code> gives *t* itself, the same [TTerm]() with the same buffer, so it composes inside a larger expression.
- A tensor that holds no gradient is given back unchanged.
- An argument that is not a [TTerm]() returns unevaluated.

## Basic Examples

A tensor whose gradient is taken below:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTerm tensor summary box: shape 3, dtype f32 -->

The gradient of the sum of its squares fills the gradient slot of *w*; [TGrad]() gives the loss back:

```wl
TGrad[Total[w^2]]
```

<!-- => a TTerm UOp summary box for the lazy loss Total[w^2] -->

The slot holds $2 w$:

```wl
Normal[TRealize[TGradOf[w]]]
```

<!-- => {2., 4., 6.} -->

Clearing the gradient gives *w* back:

```wl
TClearGrad[w]
```

<!-- => the same TTerm tensor summary box as w -->

The slot is empty again:

```wl
TGradOf[w]
```

<!-- => Missing["NoGrad"] -->

## Scope

A tensor that was never differentiated has no gradient to clear and is given back unchanged:

```wl
v = TTensorCreate[{5., 6.}]
```

<!-- => a TTerm tensor summary box: shape 2, dtype f32 -->

Clearing it gives the same tensor:

```wl
TClearGrad[v] === v
```

<!-- => True -->

## Applications

A parameter tensor for one step of gradient descent on the loss $\sum_i w_i^2$:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTerm tensor summary box: shape 3, dtype f32 -->

Take the gradient of the loss:

```wl
TGrad[Total[w^2]]
```

<!-- => a TTerm UOp summary box for the lazy loss -->

Step against the gradient with rate 0.25, writing the result into *w* in place with [TSet]():

```wl
Normal[TSet[w, w - 0.25*TGradOf[w]]]
```

<!-- => {0.5, 1., 1.5} -->

Clear the gradient, so the next step starts from an empty slot:

```wl
TGradOf[TClearGrad[w]]
```

<!-- => Missing["NoGrad"] -->

## Properties and Relations

A tensor, to show that without clearing a second [TGrad]() walk adds to the first:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTerm tensor summary box: shape 3, dtype f32 -->

The loss:

```wl
loss = Total[w^2]
```

<!-- => a TTerm UOp summary box for the lazy loss -->

The first walk:

```wl
TGrad[loss]
```

<!-- => the loss's TTerm UOp summary box -->

A second walk over the same loss:

```wl
TGrad[loss]
```

<!-- => the loss's TTerm UOp summary box -->

The slot holds the sum of both gradients, $4 w$:

```wl
Normal[TRealize[TGradOf[w]]]
```

<!-- => {4., 8., 12.} -->

---

A tensor, to show that clearing between two walks leaves only the second gradient:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTerm tensor summary box: shape 3, dtype f32 -->

The loss:

```wl
loss = Total[w^2]
```

<!-- => a TTerm UOp summary box for the lazy loss -->

The first walk:

```wl
TGrad[loss]
```

<!-- => the loss's TTerm UOp summary box -->

Clear the slot:

```wl
TClearGrad[w]
```

<!-- => the TTerm tensor summary box of w -->

The second walk:

```wl
TGrad[loss]
```

<!-- => the loss's TTerm UOp summary box -->

The slot holds only $2 w$:

```wl
Normal[TRealize[TGradOf[w]]]
```

<!-- => {2., 4., 6.} -->

## Possible Issues

A plain list is not a tensor, and [TClearGrad]() returns unevaluated:

```wl
TClearGrad[{1., 2., 3.}]
```

<!-- => TClearGrad[{1., 2., 3.}] -->
