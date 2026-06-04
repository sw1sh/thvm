---
Template: Symbol
Name: TRequiresGrad
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TRequiresGrad
Keywords: [autodiff, requires grad, parameter, leaf, backward]
SeeAlso: [TGrad, TGradOf, TClearGrad, TSet, TGlorot]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TRequiresGrad]()[*t*]</code> marks tensor *t* as a parameter to differentiate and returns *t* for chaining.

<code>[TRequiresGrad]()[*t*, [True]() | [False]()]</code> sets the flag explicitly.

## Details & Options

- Sets `TenDesc.requires_grad`, the canonical "this tensor is a parameter" flag consulted by the backward leaf rule; it mirrors PyTorch / tinygrad `.requires_grad_()`.
- A <code>[TGrad]()</code> backward walk accumulates a gradient into every tensor marked this way; read it back with <code>[TGradOf]()</code>.
- Returns its argument, so it composes inline, e.g. <code>*w* = [TRequiresGrad]() @ [TGlorot]()[{3, 4}]</code>.

## Basic Examples

Marking a tensor returns the tensor for chaining:

```wl
w = TRequiresGrad @ TTensorCreate[{1., 2., 3.}];
MatchQ[w, _TTerm]
```
<!-- => True -->

## Properties and Relations

A marked tensor accumulates a gradient under <code>[TGrad]()</code>; here the gradient of a sum of squares is *2w*:

```wl
w = TRequiresGrad @ TTensorCreate[{1., 2., 3.}];
TRealize @ TGrad @ Total[w^2];
Normal @ TRealize @ TGradOf[w]
```
<!-- => {2., 4., 6.} -->
