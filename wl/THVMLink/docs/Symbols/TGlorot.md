---
Template: Symbol
Name: TGlorot
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TGlorot
Keywords: [init, Glorot, Xavier, He, weight, random]
SeeAlso: [TZeros, TOnes, TMatMul, TTensorShape]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TGlorot]()[*shape*]</code> returns a fresh f32 `TTensor` of the given *shape*, filled with samples from a normal distribution `N(0, Sqrt[2 / fan_in])` (Glorot/Xavier-He initialization).

## Details & Options

- `fan_in` is the product of all dimensions after the first, so a `{K, N}` weight draws from `N(0, Sqrt[2 / N-of-trailing-dims])`.
- Suitable for ReLU and linear-layer weight initialization; pair with <code>[TZeros]()</code> for the bias.
- Always f32 (see <code>[TTensorDType]()</code>); the values are random, so realize and read shape rather than exact data.
- Use it directly as a trainable parameter; <code>[TGrad]()</code> auto-grads every float leaf, so no marking is needed.

## Basic Examples

Create a 2x4 weight and check its shape:

```wl
TTensorShape @ TRealize @ TGlorot[{2, 4}]
```
<!-- => {2, 4} -->

## Properties and Relations

A Glorot weight is f32:

```wl
TTensorDType @ TRealize @ TGlorot[{2, 4}]
```
<!-- => f32 -->

---

It slots directly into a <code>[TMatMul]()</code> as the right-hand weight:

```wl
x = TTensorCreate[{{1., 2., 3.}}];
W = TGlorot[{3, 4}];
TTensorShape @ TRealize @ TMatMul[x, W]
```
<!-- => {1, 4} -->
