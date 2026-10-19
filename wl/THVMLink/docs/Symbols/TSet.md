---
Template: Symbol
Name: TSet
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSet
Keywords: [tensor, assign, in place, mutate, update, buffer, optimizer step]
SeeAlso: [TTensorCreate, TRealize, TJit, TGrad, TClearGrad, Set]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TSet]()[*dst*, *src*]</code> writes the values of *src* into the buffer of the tensor *dst* in place and gives *dst*.

<!-- #| annotation: 26.10.19: TSet[dst, src] is TRealize[TAssign[dst, src]]; dst: a UOP_ASSIGN redex that, once src reduces to a TEN, memcpys src's buffer into dst's and rewrites to dst, as tinygrad's ASSIGN does. dst keeps its TenDesc id, so every handle on it sees the new values, a TJit recording that reads it replays on them, and an optimizer loop allocates no new tensor per step. A src with another number of elements, or one that is not a TTerm (a list, a number), is refused with TSet::src; one with the same number of elements in another shape is written in order. TAssign has no reference page yet. -->

## Details & Options

- *src* is a tensor or a lazy tensor graph with the shape and dtype of *dst*. It is computed, and its values are copied into the buffer of *dst*.
- *dst* keeps its identity: every expression that refers to *dst* sees the new values, including a [TJit]() closure that reads it.
- *src* may read *dst* itself, so <code>[TSet]()[*w*, *w* - *rate* *g*]</code> is a gradient-descent step in place.
- On a [TTerm]() *w*, <code>[Evaluate]()[*w*] = *expr*</code> writes into *w* the same way, rather than assigning a new value to the symbol.
- A *src* whose number of elements differs from that of *dst*, or one that is not a [TTerm](), is refused with the message `TSet::src`: the [TSet]() expression returns unevaluated and *dst* is unchanged.

## Basic Examples

A tensor:

```wl
d = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Write new values into it; the result is *d* itself:

```wl
TSet[d, TTensorCreate[{7., 8., 9.}]]
```

<!-- => the TTensor summary box of d: shape 3, dtype f32 -->

*d* now holds the new values:

```wl
Normal[d]
```

<!-- => {7., 8., 9.} -->

## Scope

A tensor:

```wl
d = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

The source may be a lazy graph that reads the target:

```wl
Normal[TSet[d, d*2]]
```

<!-- => {2., 4., 6.} -->

---

A matrix:

```wl
m = TTensorCreate[{{1., 2.}, {3., 4.}}]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f32 -->

Transpose it in place:

```wl
Normal[TSet[m, Transpose[m]]]
```

<!-- => {{1., 3.}, {2., 4.}} -->

---

An integer tensor:

```wl
n = TTensorCreate[{1, 2, 3}, "i32"]
```

<!-- => a TTensor summary box: shape 3, dtype i32 -->

Double it in place:

```wl
Normal[TSet[n, n*2]]
```

<!-- => {2, 4, 6} -->

---

A tensor:

```wl
w = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

Assigning to <code>[Evaluate]()[*w*]</code> writes into the tensor:

```wl
Evaluate[w] = w + 10
```

<!-- => the TTensor summary box of w -->

The symbol still names the same tensor, which holds the new values:

```wl
Normal[w]
```

<!-- => {11., 12., 13.} -->

## Applications

A parameter:

```wl
w = TTensorCreate[{3., -2.}]
```

<!-- => a TTensor summary box: shape 2, dtype f32 -->

Ten steps of gradient descent on $\sum_i (w_i - 1)^2$, each writing the step into *w* in place:

```wl
Do[TClearGrad[w]; TGrad[Total[(w - 1)^2]]; TSet[w, w - 0.25*TGradOf[w]], {10}]
```

The parameter approaches the minimum at 1:

```wl
Normal[w]
```

<!-- => {1.00195, 0.99707} -->

---

A tensor that a closure of no arguments reads:

```wl
xs = TTensorCreate[{1., 2., 3., 4.}]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

The closure, recording its kernel on the first application:

```wl
f = TJit[Function[{}, TRealize[xs*xs]]]
```

<!-- => a TJitClosure summary box: captured: no, ops: 0 -->

The first result:

```wl
out = f[]
```

<!-- => a TTensor summary box: shape 4, dtype f32 -->

Write new values into the input:

```wl
TSet[xs, TTensorCreate[{10., 20., 30., 40.}]]
```

<!-- => the TTensor summary box of xs -->

Replay:

```wl
f[]
```

<!-- => the TTensor summary box of out -->

The result holds the squares of the new input:

```wl
Normal[out]
```

<!-- => {100., 400., 900., 1600.} -->

## Properties and Relations

A tensor:

```wl
d = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

A second name for the same tensor:

```wl
alias = d
```

<!-- => the TTensor summary box of d -->

Write into *d*:

```wl
TSet[d, d + 1]
```

<!-- => the TTensor summary box of d -->

The other name sees the change:

```wl
Normal[alias]
```

<!-- => {2., 3., 4.} -->

The result of [TSet]() is the target itself:

```wl
TSet[d, d + 1] === d
```

<!-- => True -->

## Possible Issues

A tensor:

```wl
d = TTensorCreate[{1., 2., 3.}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

A source with a different number of elements is refused:

```wl
TSet[d, TTensorCreate[{5., 6.}]]
```

<!-- => the message TSet::src is issued and the expression returns unevaluated -->

A plain list is refused too; make it a tensor with [TTensorCreate]() first:

```wl
TSet[d, {5., 6., 7.}]
```

<!-- => the message TSet::src is issued and the expression returns unevaluated -->

The destination keeps its values:

```wl
Normal[d]
```

<!-- => {1., 2., 3.} -->
