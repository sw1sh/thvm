---
Template: Symbol
Name: TMinimum
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMinimum
Keywords: [minimum, min, elementwise, clamp, RL]
SeeAlso: [TMaximum, TWhere, TClip, TReLU, TUOpCmplt, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMinimum]()[*a*, *b*]</code> is the elementwise minimum of *a* and *b*. It lowers to <code>[TWhere]()[*a* < *b*, *a*, *b*]</code> over thvm's `UOP_CMPLT` mask, so it needs no `MIN` opcode. *b* may be a scalar, which broadcasts.

## Details & Options

- Built from the comparison mask <code>[TUOpCmplt]()</code> (the `a < b` mask) composed through <code>[TWhere]()</code>; it mirrors tinygrad's `Tensor.minimum` and is the dual of <code>[TMaximum]()</code>.
- Also installed as the binary `Min` UpValue on `TTerm`s, so <code>[Min]()[*a*, *b*]</code> builds the same graph when at least one argument is a `TTerm`.
- Differentiable: <code>[TGrad]()</code> routes each output's cotangent to whichever input was smaller, since the underlying <code>[TWhere]()</code> mask carries no gradient.
- A scalar second argument broadcasts, so <code>[TMinimum]()[*x*, 1]</code> is a ceiling at 1 (the upper half of <code>[TClip]()</code>).

## Basic Examples

The elementwise smaller of two tensors:

```wl
a = TTensorCreate[{-1., 2., 3.}];
b = TTensorCreate[{-4., -2., 9.}];
Normal @ TRealize @ TMinimum[a, b]
```
<!-- => {-4., -2., 3.} -->

## Scope

The second argument may be a scalar; it broadcasts. Capping at 1 ceilings the larger values:

```wl
x = TTensorCreate[{-3., -1., 0., 2., 5.}];
Normal @ TRealize @ TMinimum[x, 1]
```
<!-- => {-3., -1., 0., 1., 1.} -->

## Properties and Relations

The binary `Min` UpValue on `TTerm`s calls <code>[TMinimum]()</code>, so ordinary `Min` notation builds the same graph:

```wl
a = TTensorCreate[{-1., 2., 3.}];
b = TTensorCreate[{-4., -2., 9.}];
Normal @ TRealize @ Min[a, b]
```
<!-- => {-4., -2., 3.} -->

---

The cotangent flows only to the smaller input. Here *b* wins at positions 0 and 1, *a* at position 2:

```wl
a = TTensorCreate[{-1., 2., 3.}];
b = TTensorCreate[{-4., -2., 9.}];
TRealize @ TGrad @ Total[TMinimum[a, b]];
Normal @ TRealize @ TGradOf[a]
```
<!-- => {0., 0., 1.} -->
