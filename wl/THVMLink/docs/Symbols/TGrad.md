---
Template: Symbol
Name: TGrad
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TGrad
Keywords: [autodiff, automatic differentiation, gradient, backward, backpropagation, vector-Jacobian product]
SeeAlso: [TGradOf, TClearGrad, TRealize, TSet, Grad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TGrad]()[*y*]</code> differentiates *y* with respect to every floating-point tensor it depends on, adds each gradient to that tensor's gradient slot, and gives *y*.

<code>[TGrad]()[*y*, *x*]</code> gives the gradient of *y* with respect to the tensor *x*.

<code>[TGrad]()[*y*, *x*, *g*]</code> gives the vector-Jacobian product of the cotangent *g* with the derivative of *y* with respect to *x*.

<code>[TGrad]()[*y*, {$x_1$, $x_2$, …}]</code> gives the list of gradients of *y* with respect to each $x_i$.

<!-- #| annotation: 26.10.19: TGrad[y] is loss.backward() of PyTorch and tinygrad: it marks every reachable float TEN leaf, fires one backward walk seeded with ones at y's shape (a UOP_GRAD node reduced by wnf), and the C leaf rule accumulates each leaf's cotangent into its TenDesc.grad; there is no requires_grad flag and no tape, the backward graph is built in the heap. TGrad[y, x] and TGrad[y, x, g] use the target-aware walk (UOP_GRAD with a target), which emits the cotangent only at the target and leaves the grad slots alone. The list form with two or more targets is the auto-grad-all walk again: it clears every reachable float leaf's slot, walks once, reads the targets back with TGradOf and substitutes TZeros for a target the walk never reached. -->

## Details & Options

- The gradient of a *y* with several elements is the gradient of the sum of its elements: the default cotangent is a tensor of ones with the shape of *y*. A cotangent *g* given explicitly has the shape of *y*.
- <code>[TGrad]()[*y*]</code> differentiates every floating-point tensor that *y* depends on; no tensor needs to be marked as trainable. Each gradient is added to the tensor's gradient slot, read with <code>[TGradOf]()</code> and emptied with <code>[TClearGrad]()</code>, so a second walk adds to the first.
- <code>[TGrad]()[*y*, *x*]</code> and <code>[TGrad]()[*y*, *x*, *g*]</code> give a lazy [TTerm]() with the shape of *x* and leave every gradient slot unchanged.
- <code>[TGrad]()[*y*, {$x_1$, $x_2$, …}]</code> computes every gradient in one backward walk; realizing the whole list with one <code>[TRealize]()</code> computes the part they share once. It empties and refills the gradient slots of the tensors *y* depends on, and a target that *y* does not depend on gets a zero gradient.
- The gradients are lazy: <code>[TRealize]()</code> computes them, and <code>[Normal]()</code> reads the values back.
- The derivatives of the arithmetic, reduction, matrix and elementwise functions that build a [TTerm]() graph, such as [Plus](), [Times](), [Power](), [Total](), [Dot](), [Exp]() and [Tanh](), are all supported.

## Basic Examples

Two tensors:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The second:

```wl
x = TTensorCreate[{10., 20., 30.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Their inner product, a lazy scalar:

```wl
loss = Total[w*x]
```

<!-- => a TUOp summary box of kind REDUCE -->

Differentiating it fills the gradient slot of every tensor it depends on, and gives the loss back:

```wl
TGrad[loss]
```

<!-- => the TUOp summary box of loss -->

The gradient with respect to *w* is *x*:

```wl
Normal[TRealize[TGradOf[w]]]
```

<!-- => {10., 20., 30.} -->

The gradient with respect to *x* is *w*:

```wl
Normal[TRealize[TGradOf[x]]]
```

<!-- => {1., 2., 3.} -->

---

A tensor:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The gradient of $\sum_i w_i^3$ with respect to *w* is $3 w_i^2$:

```wl
Normal[TRealize[TGrad[Total[w^3], w]]]
```

<!-- => {3., 12., 27.} -->

## Scope

A tensor:

```wl
z = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

For a *y* with several elements, the gradient is that of their sum, here $2 z$:

```wl
Normal[TRealize[TGrad[z^2, z]]]
```

<!-- => {2., 4., 6.} -->

An explicit cotangent weights each element of *y*, giving $2 z_i g_i$:

```wl
Normal[TRealize[TGrad[z^2, z, TTensorCreate[{1., 10., 100.}]]]]
```

<!-- => {2., 40., 600.} -->

Elementwise functions differentiate too; the derivative of [Exp]() is itself:

```wl
Normal[TRealize[TGrad[Exp[z], z]]]
```

<!-- => {2.71828, 7.38906, 20.0855} -->

---

Two tensors:

```wl
w1 = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The second:

```wl
w2 = TTensorCreate[{4., 5., 6.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The gradients of one loss with respect to both, from one walk:

```wl
grads = TGrad[Total[w1^2 + w2^2], {w1, w2}]
```

<!-- => a list of two TUOp summary boxes -->

Realize the list together and read both:

```wl
Normal /@ TRealize[grads]
```

<!-- => {{2., 4., 6.}, {8., 10., 12.}} -->

---

A 2 x 2 weight matrix:

```wl
m = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

An input row:

```wl
v = TTensorCreate[{{1., 1.}}]
```

<!-- => a TTensor summary box: shape 1 x 2, dtype f32 -->

The gradient of the summed product with respect to the input row is the row sums of the matrix:

```wl
Normal[TRealize[TGrad[Total[v . m, All], v]]]
```

<!-- => {{3., 7.}} -->

## Applications

Gradient descent on $\sum_i (w_i - 1)^2$ from a starting point:

```wl
w = TTensorCreate[{3., -2.}]
```

<!-- => a TTensor summary box: shape 2, dtype f32 -->

Ten steps with rate 0.25, each clearing the gradient slot, differentiating, and writing the step into *w* in place with [TSet]():

```wl
Do[TClearGrad[w]; TGrad[Total[(w - 1)^2]]; TSet[w, w - 0.25*TGradOf[w]], {10}]
```

The parameters approach the minimum at 1:

```wl
Normal[w]
```

<!-- => {1.00195, 0.99707} -->

## Properties and Relations

A tensor:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The gradient agrees with the symbolic gradient [Grad]() of the same function, $3 w_i^2$:

```wl
Normal[TRealize[TGrad[Total[w^3], w]]] == (Grad[Total[{a, b, c}^3], {a, b, c}] /. {a -> 1., b -> 2., c -> 3.})
```

<!-- => True -->

---

A tensor:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The loss:

```wl
loss = Total[w^2]
```

<!-- => a TUOp summary box of kind REDUCE -->

A gradient with respect to *w*:

```wl
Normal[TRealize[TGrad[loss, w]]]
```

<!-- => {2., 4., 6.} -->

It leaves the gradient slot of *w* empty:

```wl
TGradOf[w]
```

<!-- => Missing["NoGrad"] -->

The one-argument form fills it:

```wl
TGrad[loss]
```

<!-- => the TUOp summary box of loss -->

Read the slot:

```wl
Normal[TRealize[TGradOf[w]]]
```

<!-- => {2., 4., 6.} -->

A second walk adds to it:

```wl
TGrad[loss]
```

<!-- => the TUOp summary box of loss -->

The slot now holds twice the gradient:

```wl
Normal[TRealize[TGradOf[w]]]
```

<!-- => {4., 8., 12.} -->

## Possible Issues

Two tensors, the second unrelated to the loss below:

```wl
w = TTensorCreate[{1., 2.}]
```

<!-- => a TTensor summary box: shape 2, dtype f32 -->

The unrelated one:

```wl
u = TTensorCreate[{0., 0.}]
```

<!-- => a TTensor summary box: shape 2, dtype f32 -->

A walk over $\sum_i w_i$:

```wl
TGrad[Total[w]]
```

<!-- => a TUOp summary box of kind REDUCE -->

The unrelated tensor gets no gradient:

```wl
TGradOf[u]
```

<!-- => Missing["NoGrad"] -->

In the list form, an unrelated target gets a zero gradient instead:

```wl
Normal /@ TRealize[TGrad[Total[w], {w, u}]]
```

<!-- => {{1., 1.}, {0., 0.}} -->

---

A tensor:

```wl
v = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Fill its gradient slot:

```wl
TGrad[Total[v^2]]
```

<!-- => a TUOp summary box of kind REDUCE -->

The list form empties the slots before its walk, so after it the slot of *v* holds only the list form's gradient, not the sum of both:

```wl
Normal /@ TRealize[TGrad[Total[v^2], {v, TTensorCreate[{5., 5., 5.}]}]]
```

<!-- => {{2., 4., 6.}, {0., 0., 0.}} -->

Read the slot:

```wl
Normal[TRealize[TGradOf[v]]]
```

<!-- => {2., 4., 6.} -->
