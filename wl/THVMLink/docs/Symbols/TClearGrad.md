---
Template: Symbol
Name: TClearGrad
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TClearGrad
Keywords: [autodiff, gradient, zero grad, reset, optimizer]
SeeAlso: [TGrad, TGradOf, TSet, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TClearGrad]()[*t*]</code> zeros tensor *t*'s accumulated gradient and returns *t*.

## Details & Options

- The analogue of PyTorch `zero_grad`; call it between optimizer steps so gradients from successive backward passes do not accumulate.
- Afterwards <code>[TGradOf]()</code> reports <code>[Missing]()["NoGrad"]</code> until the next <code>[TGrad]()</code> walk repopulates the slot.
- Returns its argument for chaining.

## Basic Examples

Clearing returns the tensor for chaining:

```wl
w = TTensorCreate[{1., 2., 3.}];
TRealize @ TGrad @ Total[w^2];
MatchQ[TClearGrad[w], _TTerm]
```
<!-- => True -->

## Properties and Relations

After clearing, the gradient slot is empty again:

```wl
w = TTensorCreate[{1., 2., 3.}];
TRealize @ TGrad @ Total[w^2];
TClearGrad[w];
TGradOf[w]
```
<!-- => Missing[NoGrad] -->
