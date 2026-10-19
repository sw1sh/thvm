---
Template: Symbol
Name: TGather
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TGather
Keywords: [gather, index, select, one-hot, take along axis, policy gradient]
SeeAlso: [TTakeAlongAxis, TWhere, TGrad, TRealize, Part]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TGather]()[*x*, *axis*, *index*]</code> picks one element of the tensor *x* along *axis* in every slice, at the position *index* gives for that slice.

<!-- #| annotation: 26.10.19: TGather follows tinygrad's gather lowering: an arange along axis compared for equality with the index (a UOP_CMPEQ one-hot mask), multiplied by x and sum-reduced over axis. There is no gather opcode, which is why an index outside the axis matches nothing and gives 0, why the result is f32 (the mask is f32), and why the gradient scatters back through the mask to exactly the selected positions. The index length is not checked against the slice count. -->

## Details & Options

- *axis* counts from 0. The result has the shape of *x* with *axis* set to length 1, and its element at position 0 of *axis* is the element of *x* at position *index* of *axis* in the same slice.
- *index* is a list of integers, one per slice, either flat or shaped like the result. It can also be an integer-valued [TTerm]() of dtype `"i32"` or `"f32"`.
- <code>[TGather]()[*x*, *axis*, *index*]</code> gives a lazy [TTerm](). <code>[TRealize]()</code> computes it, and <code>[Normal]()</code> reads the values back.
- Under <code>[TGrad]()</code>, the gradient flows back to exactly the selected positions of *x* and is zero everywhere else.
- An index outside the range from 0 to the axis length minus 1 selects nothing, and that slice of the result is 0.
- The result has dtype `"f32"`, also when *x* is an `"i32"` tensor.
- <code>[TTakeAlongAxis]()[*x*, *index*, *axis*]</code> is the same selection with the arguments in the order of numpy's `take_along_axis`.
- An *x* that is not a [TTerm]() returns unevaluated.

## Basic Examples

A 2 x 3 matrix:

```wl
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

Gather column 2 from row 0 and column 0 from row 1, as a lazy graph:

```wl
g = TGather[x, 1, {2, 0}]
```

<!-- => a TUOp summary box of kind RESHAPE -->

The gathered axis keeps length 1, so the result is a 2 x 1 matrix:

```wl
Normal[TRealize[g]]
```

<!-- => {{3.}, {4.}} -->

## Scope

A 2 x 3 matrix:

```wl
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

Along axis 0 the slices are the columns, so the index gives one row per column:

```wl
Normal[TRealize[TGather[x, 0, {1, 0, 1}]]]
```

<!-- => {{4., 2., 6.}} -->

The index may also be shaped like the result:

```wl
Normal[TRealize[TGather[x, 1, {{2}, {0}}]]]
```

<!-- => {{3.}, {4.}} -->

An integer tensor works as the index:

```wl
idx = TTensorCreate[{1, 2}, "i32"]
```

<!-- => a TTensor summary box: shape 2, dtype i32 -->

Gather with it:

```wl
Normal[TRealize[TGather[x, 1, idx]]]
```

<!-- => {{2.}, {6.}} -->

---

A 2 x 3 x 2 tensor:

```wl
t = TTensorCreate[ArrayReshape[Range[1., 12.], {2, 3, 2}]]
```

<!-- => a TTensor summary box: shape 2 x 3 x 2, dtype f32 -->

Gathering along the last axis takes one index for each of the 2 x 3 slices:

```wl
Normal[TRealize[TGather[t, 2, {{1, 0, 1}, {0, 0, 1}}]]]
```

<!-- => {{{2.}, {3.}, {6.}}, {{7.}, {9.}, {12.}}} -->

## Applications

The log-probabilities a policy assigns to three actions, for a batch of two states:

```wl
logp = TTensorCreate[{{-2., -1.5, -0.5}, {-0.25, -3., -2.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

Gathering along the action axis with the actions 2 and 0 taken in the two states picks the log-probability of each action taken, the term a policy gradient differentiates:

```wl
Normal[TRealize[TGather[logp, 1, {2, 0}]]]
```

<!-- => {{-0.5}, {-0.25}} -->

## Properties and Relations

A 2 x 3 matrix:

```wl
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

Differentiate the sum of the gathered elements:

```wl
TGrad[Total[TGather[x, 1, {2, 0}], All]]
```

<!-- => a TUOp summary box for the lazy sum -->

The gradient is 1 at each selected position and 0 elsewhere:

```wl
Normal[TRealize[TGradOf[x]]]
```

<!-- => {{0., 0., 1.}, {1., 0., 0.}} -->

---

A 2 x 3 matrix:

```wl
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

[TTakeAlongAxis]() takes the index before the axis and gives the same values:

```wl
Normal[TRealize[TTakeAlongAxis[x, {2, 0}, 1]]] === Normal[TRealize[TGather[x, 1, {2, 0}]]]
```

<!-- => True -->

## Possible Issues

A 2 x 3 matrix:

```wl
x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype f32 -->

An index past the end of the axis selects nothing, and that slice comes out 0 rather than issuing a message:

```wl
Normal[TRealize[TGather[x, 1, {5, 0}]]]
```

<!-- => {{0.}, {4.}} -->

---

An integer matrix:

```wl
n = TTensorCreate[{{1, 2, 3}, {4, 5, 6}}, "i32"]
```

<!-- => a TTensor summary box: shape 2 x 3, dtype i32 -->

The gathered values come back as reals:

```wl
Normal[TRealize[TGather[n, 1, {2, 0}]]]
```

<!-- => {{3.}, {4.}} -->
