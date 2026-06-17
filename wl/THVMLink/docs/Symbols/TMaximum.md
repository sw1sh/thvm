---
Template: Symbol
Name: TMaximum
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMaximum
Keywords: [maximum, max, elementwise, ReLU, clamp, RL]
SeeAlso: [TMinimum, TWhere, TClip, TReLU, TUOpCmplt, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMaximum]()[*a*, *b*]</code> is the elementwise maximum of *a* and *b*. It lowers to <code>[TWhere]()[*a* < *b*, *b*, *a*]</code> over thvm's `UOP_CMPLT` mask, so it needs no `MAX` opcode. *b* may be a scalar, which broadcasts.

## Details & Options

- Built from the comparison mask <code>[TUOpCmplt]()</code> (the `a < b` mask) composed through <code>[TWhere]()</code>; it mirrors tinygrad's `Tensor.maximum`.
- Also installed as the binary `Max` UpValue on `TTerm`s, so <code>[Max]()[*a*, *b*]</code> builds the same graph when at least one argument is a `TTerm`.
- Differentiable: <code>[TGrad]()</code> routes each output's cotangent to whichever input won, since the underlying <code>[TWhere]()</code> mask carries no gradient.
- A scalar second argument broadcasts, so <code>[TMaximum]()[*x*, 0]</code> is a floor at 0 (the <code>[TReLU]()</code> shape).

## Basic Examples

The elementwise larger of two tensors:

```wl
a = TTensorCreate[{-1., 2., 3.}];
b = TTensorCreate[{-4., -2., 9.}];
Normal @ TRealize @ TMaximum[a, b]
```
<!-- => {-1., 2., 9.} -->

## Scope

The second argument may be a scalar; it broadcasts. Flooring at 0 reproduces <code>[TReLU]()</code>:

```wl
x = TTensorCreate[{-3., -1., 0., 2., 5.}];
Normal @ TRealize @ TMaximum[x, 0]
```
<!-- => {0., 0., 0., 2., 5.} -->

## Properties and Relations

The binary `Max` UpValue on `TTerm`s calls <code>[TMaximum]()</code>, so ordinary `Max` notation builds the same graph:

```wl
a = TTensorCreate[{-1., 2., 3.}];
b = TTensorCreate[{-4., -2., 9.}];
Normal @ TRealize @ Max[a, b]
```
<!-- => {-1., 2., 9.} -->

---

The cotangent flows only to the winning input, so the gradient of a sum of maxima is a 0/1 selector. Here *a* wins at positions 0 and 1, *b* at position 2:

```wl
a = TTensorCreate[{-1., 2., 3.}];
b = TTensorCreate[{-4., -2., 9.}];
TRealize @ TGrad @ Total[TMaximum[a, b]];
Normal @ TRealize @ TGradOf[a]
```
<!-- => {1., 1., 0.} -->
