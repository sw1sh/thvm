---
Template: Symbol
Name: TClip
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TClip
Keywords: [clip, clamp, bound, range, elementwise]
SeeAlso: [TMaximum, TMinimum, TWhere, Clip, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TClip]()[*x*, *lo*, *hi*]</code> clamps every element of the tensor *x* into the range from *lo* to *hi*.

<!-- #| annotation: 26.10.19: TClip mirrors tinygrad's Tensor.clamp. It lowers to TWhere[x < lo, lo, x] followed by TWhere[hi < x, hi, x], so it needs no clamp opcode, the lower bound wins when lo > hi, and the gradient rule is the one TWhere gives each select. A None or infinite bound is dropped at construction, before any node is built. -->

## Details & Options

- <code>[TClip]()[*x*, *lo*, *hi*]</code> gives a lazy [TTerm]() for the clamped tensor. <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- *lo* and *hi* are numbers, compared against every element of *x*.
- The lower bound is applied first and the upper bound second, so when *lo* exceeds *hi* every element becomes *hi*.
- A bound given as [None]() or as a positive or negative [Infinity]() is skipped, giving a one-sided clamp. With both bounds skipped, *x* itself is given back.
- On a [TTerm](), <code>[Clip]()[*x*, {*lo*, *hi*}]</code> builds the same graph as <code>[TClip]()[*x*, *lo*, *hi*]</code>, and <code>[Clip]()[*x*]</code> the same as <code>[TClip]()[*x*, -1, 1]</code>.
- Under <code>[TGrad]()</code>, the gradient passes through unchanged where *x* lies inside the range and is zero where *x* was clamped.
- An *x* that is not a [TTerm]() returns unevaluated.

## Basic Examples

A ramp from -3 to 3:

```wl
x = TTensorCreate[{-3., -2., -1., 0., 1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 7, dtype f32 -->

Clamping it into $[-1, 1]$ gives a lazy graph:

```wl
c = TClip[x, -1, 1]
```

<!-- => a TUOp summary box of kind ADD -->

Realizing the graph and reading its buffer gives the clamped values:

```wl
Normal[TRealize[c]]
```

<!-- => {-1., -1., -1., 0., 1., 1., 1.} -->

## Scope

A ramp from -3 to 3:

```wl
x = TTensorCreate[{-3., -2., -1., 0., 1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 7, dtype f32 -->

An [Infinity]() upper bound is skipped, so this floors at 0 and leaves the top of the ramp alone:

```wl
Normal[TRealize[TClip[x, 0, Infinity]]]
```

<!-- => {0., 0., 0., 0., 1., 2., 3.} -->

A [None]() lower bound is skipped too, so this only caps at 1:

```wl
Normal[TRealize[TClip[x, None, 1]]]
```

<!-- => {-3., -2., -1., 0., 1., 1., 1.} -->

With both bounds skipped, the tensor itself comes back:

```wl
TClip[x, None, None] === x
```

<!-- => True -->

The bounds need not be integers:

```wl
Normal[TRealize[TClip[x, -1.5, 0.5]]]
```

<!-- => {-1.5, -1.5, -1., 0., 0.5, 0.5, 0.5} -->

---

A matrix is clamped element by element:

```wl
m = TTensorCreate[{{-2., 0.5}, {3., -0.25}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

Clamp it into $[0, 1]$:

```wl
Normal[TRealize[TClip[m, 0, 1]]]
```

<!-- => {{0., 0.5}, {1., 0.}} -->

## Properties and Relations

A ramp from -3 to 3:

```wl
x = TTensorCreate[{-3., -2., -1., 0., 1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 7, dtype f32 -->

On a [TTerm](), [Clip]() with a range list builds the same graph as [TClip]():

```wl
Normal[TRealize[Clip[x, {-1, 1}]]]
```

<!-- => {-1., -1., -1., 0., 1., 1., 1.} -->

[Clip]() with no range clamps into $[-1, 1]$, the default of the built-in [Clip]():

```wl
Normal[TRealize[Clip[x]]]
```

<!-- => {-1., -1., -1., 0., 1., 1., 1.} -->

Clamping is a [TMaximum]() with the lower bound followed by a [TMinimum]() with the upper bound:

```wl
Normal[TRealize[TClip[x, -1, 2]]] === Normal[TRealize[TMinimum[TMaximum[x, -1], 2]]]
```

<!-- => True -->

---

A tensor with one element below, one inside and one above $[-1, 1]$:

```wl
y = TTensorCreate[{-3., 0.5, 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Differentiate the sum of the clamped values:

```wl
TGrad[Total[TClip[y, -1, 1]]]
```

<!-- => a TUOp summary box for the lazy sum -->

The gradient passes through only where the input was inside the range, here the middle element:

```wl
Normal[TRealize[TGradOf[y]]]
```

<!-- => {0., 1., 0.} -->

## Possible Issues

A ramp from -3 to 3:

```wl
x = TTensorCreate[{-3., -2., -1., 0., 1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 7, dtype f32 -->

When the lower bound exceeds the upper bound, the upper bound applies last and every element becomes *hi*:

```wl
Normal[TRealize[TClip[x, 1, -1]]]
```

<!-- => {-1., -1., -1., -1., -1., -1., -1.} -->

---

A plain list is not a tensor, and [TClip]() returns unevaluated:

```wl
TClip[{-3., 0.5, 3.}, -1, 1]
```

<!-- => TClip[{-3., 0.5, 3.}, -1, 1] -->
