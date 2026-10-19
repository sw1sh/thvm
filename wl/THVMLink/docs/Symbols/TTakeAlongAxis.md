---
Template: Symbol
Name: TTakeAlongAxis
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTakeAlongAxis
Keywords: [take along axis, gather, index, select, policy gradient]
SeeAlso: [TGather, TWhere, TGrad, TRealize, Part]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTakeAlongAxis]()[*x*, *index*, *axis*]</code> picks one element of the tensor *x* along *axis* in every slice, at the position *index* gives for that slice.

<!-- #| annotation: 26.10.19: TTakeAlongAxis[x, index, axis] is defined as TGather[x, axis, index]; it exists so code ported from numpy's take_along_axis keeps its argument order. Everything about the lowering (one-hot mask, sum-reduce, f32 result, unchecked index length) is TGather's. -->

## Details & Options

- <code>[TTakeAlongAxis]()[*x*, *index*, *axis*]</code> is <code>[TGather]()[*x*, *axis*, *index*]</code>, with the index before the axis as in numpy's `take_along_axis`.
- *axis* counts from 0. The result has the shape of *x* with *axis* set to length 1.
- *index* is a list of integers, one per slice, or an integer-valued [TTerm]().
- The result is a lazy [TTerm](). <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- Under <code>[TGrad]()</code>, the gradient flows back to exactly the selected positions of *x*.
- An index outside the axis selects nothing, and that slice of the result is 0.

## Basic Examples

A 2 x 3 matrix:

```wl
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

Take column 2 from row 0 and column 0 from row 1, as a lazy graph:

```wl
s = TTakeAlongAxis[x, {2, 0}, 1]
```

<!-- => a TUOp summary box of kind RESHAPE -->

The selected axis keeps length 1:

```wl
Normal[TRealize[s]]
```

<!-- => {{3.}, {4.}} -->

## Scope

A 2 x 3 matrix:

```wl
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

Along axis 0 the index gives one row for each column:

```wl
Normal[TRealize[TTakeAlongAxis[x, {1, 0, 1}, 0]]]
```

<!-- => {{4., 2., 6.}} -->

The index can be an integer tensor:

```wl
Normal[TRealize[TTakeAlongAxis[x, TTensorCreate[{1, 2}, "i32"], 1]]]
```

<!-- => {{2.}, {6.}} -->

An index past the end of the axis gives 0 for that slice:

```wl
Normal[TRealize[TTakeAlongAxis[x, {5, 0}, 1]]]
```

<!-- => {{0.}, {4.}} -->

## Applications

The log-probabilities a policy assigns to three actions, for a batch of two states:

```wl
logp = TTensorCreate[{{-2., -1.5, -0.5}, {-0.25, -3., -2.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

The sum of the log-probabilities of the actions taken, 2 in state 0 and 0 in state 1:

```wl
score = Total[TTakeAlongAxis[logp, {2, 0}, 1], All]
```

<!-- => a TUOp summary box of kind REDUCE -->

Its value:

```wl
Normal[TRealize[score]]
```

<!-- => {-0.75} -->

Differentiate it:

```wl
TGrad[score]
```

<!-- => the TUOp summary box of score -->

The gradient is a one-hot row per state, at the action taken, which is the direction a policy-gradient step raises:

```wl
Normal[TRealize[TGradOf[logp]]]
```

<!-- => {{0., 0., 1.}, {1., 0., 0.}} -->

## Properties and Relations

A 2 x 3 matrix:

```wl
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

[TGather]() with the axis before the index gives the same values:

```wl
Normal[TRealize[TTakeAlongAxis[x, {2, 0}, 1]]] === Normal[TRealize[TGather[x, 1, {2, 0}]]]
```

<!-- => True -->

## Possible Issues

A plain nested list is not a tensor, and [TTakeAlongAxis]() returns unevaluated:

```wl
TTakeAlongAxis[{{1., 2., 3.}, {4., 5., 6.}}, {2, 0}, 1]
```

<!-- => TTakeAlongAxis[{{1., 2., 3.}, {4., 5., 6.}}, {2, 0}, 1] -->
