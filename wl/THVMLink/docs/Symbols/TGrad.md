---
Template: Symbol
Name: TGrad
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TGrad
Keywords: [autodiff, gradient, backward, VJP, requires grad]
SeeAlso: [TGradOf, TClearGrad, TRequiresGrad, TUOpGrad, TRealize, TAdam]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TGrad]()[$y$]</code> is `loss.backward()`: one backward walk seeded with `ones-at-y` that accumulates the cotangent of every reachable <code>[TRequiresGrad]()</code> leaf into its `TenDesc.grad`. Returns $y$ for chaining.

<code>[TGrad]()[$y$, $target$]</code> computes $\partial y / \partial$ $target$ as a target-aware VJP, returning the gradient `TTerm` without touching other leaves.

<code>[TGrad]()[$y$, $\{x_1, \ldots, x_n\}$]</code> computes $\partial y / \partial x_i$ for every target in one shared walk and returns a list of $n$ gradient `TTerm`s in target order.

## Details & Options

- The walk is lazy: it builds a single backward graph rooted at the cotangent seed, with one DUP per branch that shares an upstream factor. <code>[TRealize]()</code> forces evaluation.
- For the full-graph form, gradients accumulate into `TenDesc.grad`; read them per tensor with <code>[TGradOf]()</code> and zero them between steps with <code>[TClearGrad]()</code>.
- For the multi-target form, realize the entire list together (`TRealize[grads]`) so the shared upstream emits once.
- The default cotangent seed is `ones-at-y.shape`. For a non-default seed (e.g. an upstream gradient threaded from another source), call <code>[TUOpGrad]()</code> directly.

## Basic Examples

A single backward over a small inner product:

```wl
Needs["THVMLink`"];
TInit[];
w    = TRequiresGrad @ TTensorCreate[{1., 2., 3.}];
x    = TTensorCreate[{10., 20., 30.}];
loss = Total[w*x];
TRealize @ TGrad[loss];
TTensorData @ TRealize @ TGradOf[w]
```
<!-- => NumericArray[{10., 20., 30.}, "Real32"] -->

## Scope

Multi-target VJP returns the gradients in target order:

```wl
w1 = TRequiresGrad @ TTensorCreate[{1., 2., 3.}];
w2 = TRequiresGrad @ TTensorCreate[{4., 5., 6.}];
y  = Total[w1^2 + w2^2];
{g1, g2} = TGrad[y, {w1, w2}];
{TTensorData @ TRealize @ g1, TTensorData @ TRealize @ g2}
```
<!-- => {NumericArray[{2., 4., 6.}, "Real32"], NumericArray[{8., 10., 12.}, "Real32"]} -->

## Applications

Drive one SGD step against a tiny linear regression. Take the gradient first, then write the parameter update back through <code>[TSet]()</code> with a small learning rate:

```wl
W = TRequiresGrad @ TGlorot[{4}];
x = TTensorCreate[{1., 2., 3., 4.}];
y = TTensorCreate[{1.}];

loss = TL2Loss[ Total[W*x] - y ];
TClearGrad[W];
TRealize @ TGrad[loss];
gW = TTensorData @ TRealize @ TGradOf[W];
TSet[W, W + (-0.01)*TGradOf[W]];
{gW, TTensorData[W]}
```
<!-- => {<gradient>, <post-step W>} - the param shifted by -0.01 * gradient -->

## Properties and Relations

`TGrad[y]` is the full-graph projection; calling it again WITHOUT clearing accumulates:

```wl
w  = TRequiresGrad @ TTensorCreate[{1., 2., 3.}];
y1 = Total[w];
TClearGrad[w];
TRealize @ TGrad[y1];
g1 = TTensorData @ TRealize @ TGradOf[w];
TRealize @ TGrad[y1];
g2 = TTensorData @ TRealize @ TGradOf[w];
{g1, g2}
```
<!-- => {{1., 1., 1.}, {2., 2., 2.}} -- second TGrad accumulated again -->

## Possible Issues

Forgetting to mark a leaf with <code>[TRequiresGrad]()</code> makes its gradient slot stay `Missing["NoGrad"]`:

```wl
w    = TTensorCreate[{1., 2., 3.}];
loss = Total[w];
TRealize @ TGrad[loss];
TGradOf[w]
```
<!-- => Missing["NoGrad"] -->

---

The multi-target form requires that every target reaches the seed; an unreachable target's gradient is zero, not an error:

```wl
w        = TRequiresGrad @ TTensorCreate[{1., 2.}];
unrelated = TRequiresGrad @ TTensorCreate[{0., 0.}];
loss     = Total[w];
{gw, gu} = TGrad[loss, {w, unrelated}];
{TTensorData @ TRealize @ gw, TTensorData @ TRealize @ gu}
```
<!-- => {NumericArray[{1., 1.}, "Real32"], NumericArray[{0., 0.}, "Real32"]} -->
