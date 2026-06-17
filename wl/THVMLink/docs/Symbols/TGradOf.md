---
Template: Symbol
Name: TGradOf
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TGradOf
Keywords: [autodiff, gradient, accumulator, backward, read]
SeeAlso: [TGrad, TClearGrad, TRealize, TSet]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TGradOf]()[*t*]</code> returns the lazy gradient term accumulated into tensor *t* by a backward walk.

## Details & Options

- Reads `TenDesc.grad`, the chain-rule accumulator populated by a <code>[TGrad]()</code> backward pass over a graph that depends on *t*.
- Returns <code>[Missing]()["NoGrad"]</code> when no gradient has been accumulated (or after <code>[TClearGrad]()</code>).
- The returned term is lazy; wrap it in <code>[TRealize]()</code> to materialize the gradient buffer, then read with <code>[Normal]()</code> or <code>[TTensorData]()</code>.

## Basic Examples

After a backward pass, read the gradient of a sum of squares (*2w*):

```wl
w = TTensorCreate[{1., 2., 3.}];
TRealize @ TGrad @ Total[w^2];
Normal @ TRealize @ TGradOf[w]
```
<!-- => {2., 4., 6.} -->

## Possible Issues

Before any backward pass, the gradient is missing:

```wl
v = TTensorCreate[{1., 2.}];
TGradOf[v]
```
<!-- => Missing[NoGrad] -->
