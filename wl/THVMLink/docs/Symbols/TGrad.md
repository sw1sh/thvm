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

<code>[TGrad]()[*y*]</code> is `loss.backward()`: one backward walk seeded with `ones-at-y` that accumulates the cotangent of every reachable <code>[TRequiresGrad]()</code> leaf into its `TenDesc.grad`. Returns *y* for chaining.

<code>[TGrad]()[*y*, *target*]</code> computes the gradient of *y* with respect to *target* as a target-aware vector-Jacobian product (VJP), returning the gradient `TTerm` without touching other leaves.

<code>[TGrad]()[*y*, {*x*<sub>1</sub>, ..., *x*<sub>n</sub>}]</code> computes the gradient of *y* with respect to each *x*<sub>i</sub> in one shared walk and returns a list of *n* gradient `TTerm`s in target order.

## Details & Options

- The walk is lazy: it builds a single backward graph rooted at the cotangent seed, with one DUP per branch that shares an upstream factor. <code>[TRealize]()</code> forces evaluation.
- For the full-graph form, gradients accumulate into `TenDesc.grad`; read them per tensor with <code>[TGradOf]()</code> and zero them between steps with <code>[TClearGrad]()</code>.
- For the multi-target form, realize the entire list together (`TRealize[grads]`) so the shared upstream emits once.
- The default cotangent seed is `ones-at-y.shape`. For a non-default seed (e.g. an upstream gradient threaded from another source), call <code>[TUOpGrad]()</code> directly.

## Basic Examples

A single backward over a small inner product:

```wl
w    = TRequiresGrad @ TTensorCreate[{1., 2., 3.}];
x    = TTensorCreate[{10., 20., 30.}];
loss = Total[w*x];
TRealize @ TGrad[loss];
Normal @ TRealize @ TGradOf[w]
```
<!-- => {10., 20., 30.} -->

## Scope

Multi-target VJP returns the gradients in target order:

```wl
w1 = TRequiresGrad @ TTensorCreate[{1., 2., 3.}];
w2 = TRequiresGrad @ TTensorCreate[{4., 5., 6.}];
y  = Total[w1^2 + w2^2];
{g1, g2} = TGrad[y, {w1, w2}];
Normal @ TRealize @ g1
```
<!-- => {2., 4., 6.} -->

```wl
Normal @ TRealize @ g2
```
<!-- => {8., 10., 12.} -->

## Applications

Drive one SGD step against a tiny linear regression. <code>[TL2Loss]()</code> wraps the squared-error of the residual into a scalar loss. Take the gradient first, then write the parameter update back through <code>[TSet]()</code> with a small learning rate:

```wl
W = TRequiresGrad @ TGlorot[{4}];
x = TTensorCreate[{1., 2., 3., 4.}];
y = TTensorCreate[{1.}];

loss = TL2Loss[ Total[W*x] - y ];
TClearGrad[W];
TRealize @ TGrad[loss];
gW = Normal @ TRealize @ TGradOf[W]
```
<!-- => the linear-regression gradient, a length-4 list (varies with the Glorot draw) -->

Apply the update; the original `W` `TTerm` still points at the same `TenDesc`, now shifted by the learning rate times the gradient:

```wl
TSet[W, W + (-0.01)*TGradOf[W]];
Normal @ W
```
<!-- => the post-step W, a length-4 list -->

## Properties and Relations

<code>[TGrad]()[*y*]</code> is the full-graph projection; calling it again WITHOUT clearing accumulates. The first walk:

```wl
w  = TRequiresGrad @ TTensorCreate[{1., 2., 3.}];
y1 = Total[w];
TClearGrad[w];
TRealize @ TGrad[y1];
Normal @ TRealize @ TGradOf[w]
```
<!-- => {1., 1., 1.} -->

A second <code>[TGrad]()</code> without a <code>[TClearGrad]()</code> in between accumulates on top:

```wl
TRealize @ TGrad[y1];
Normal @ TRealize @ TGradOf[w]
```
<!-- => {2., 2., 2.} -- the second walk accumulated again -->

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

The multi-target form requires that every target reaches the seed; an unreachable target's gradient is zero, not an error. The reachable target:

```wl
w        = TRequiresGrad @ TTensorCreate[{1., 2.}];
unrelated = TRequiresGrad @ TTensorCreate[{0., 0.}];
loss     = Total[w];
{gw, gu} = TGrad[loss, {w, unrelated}];
Normal @ TRealize @ gw
```
<!-- => {1., 1.} -->

and the unreachable one comes back all zeros:

```wl
Normal @ TRealize @ gu
```
<!-- => {0., 0.} -->
