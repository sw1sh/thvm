---
Template: Symbol
Name: TClip
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TClip
Keywords: [clip, clamp, bound, range, RL, PPO]
SeeAlso: [TMaximum, TMinimum, TWhere, TReLU, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TClip]()[*x*, *lo*, *hi*]</code> clamps *x* into the range `[lo, hi]` elementwise. It lowers as a lower bound then an upper bound: <code>[TWhere]()[*x* < *lo*, *lo*, *x*]</code> followed by <code>[TWhere]()[*hi* < *x*, *hi*, *x*]</code> (tinygrad's clamp order). *lo* and *hi* are scalars.

## Details & Options

- The lower bound is applied first, then the upper, matching tinygrad's `clamp`.
- Either bound may be `None` or `+/-Infinity` to skip it, giving a one-sided clamp: <code>[TClip]()[*x*, *lo*, Infinity]</code> floors at *lo* only, and <code>[TClip]()[*x*, -Infinity, *hi*]</code> ceilings at *hi* only.
- Also installed as the `Clip` UpValue on `TTerm`s: <code>[Clip]()[*x*, {*lo*, *hi*}]</code> equals <code>[TClip]()[*x*, *lo*, *hi*]</code>, and <code>[Clip]()[*x*]</code> equals <code>[TClip]()[*x*, -1, 1]</code> (the default range, as for built-in <code>[Clip]()</code>).
- Differentiable: <code>[TGrad]()</code> passes the cotangent through unchanged inside the range and zeroes it where *x* was clamped, since each bound is a <code>[TWhere]()</code> select.

## Basic Examples

Clamp a ramp into `[-1, 1]`:

```wl
x = TTensorCreate[{-3., -2., -1., 0., 1., 2., 3.}];
Normal @ TRealize @ TClip[x, -1, 1]
```
<!-- => {-1., -1., -1., 0., 1., 1., 1.} -->

## Scope

An `Infinity` bound is skipped, giving a one-sided clamp. Flooring at 0 leaves the upper end untouched:

```wl
x = TTensorCreate[{-3., -2., -1., 0., 1., 2., 3.}];
Normal @ TRealize @ TClip[x, 0, Infinity]
```
<!-- => {0., 0., 0., 0., 1., 2., 3.} -->

## Properties and Relations

The `Clip` UpValue on `TTerm`s dispatches to <code>[TClip]()</code>, so the range-list form reads like built-in <code>[Clip]()</code>:

```wl
x = TTensorCreate[{-3., -2., -1., 0., 1., 2., 3.}];
Normal @ TRealize @ Clip[x, {-1, 1}]
```
<!-- => {-1., -1., -1., 0., 1., 1., 1.} -->

---

The bare <code>[Clip]()[*x*]</code> form clamps into the default `[-1, 1]`:

```wl
x = TTensorCreate[{-3., 0.5, 3.}];
Normal @ TRealize @ Clip[x]
```
<!-- => {-1., 0.5, 1.} -->

## Possible Issues

Clamping zeroes the gradient where *x* lands outside the range. Here only the middle element, which was already inside `[-1, 1]`, passes its cotangent through:

```wl
x = TTensorCreate[{-3., 0.5, 3.}];
TRealize @ TGrad @ Total[TClip[x, -1, 1]];
Normal @ TRealize @ TGradOf[x]
```
<!-- => {0., 1., 0.} -->
