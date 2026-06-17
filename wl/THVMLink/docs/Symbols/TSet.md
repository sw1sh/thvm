---
Template: Symbol
Name: TSet
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSet
Keywords: [tensor, assign, in place, mutate, buffer]
SeeAlso: [TTensorData, TTensorCreate, TJit, TClearGrad, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TSet]()[*dst*, *src*]</code> writes the bytes of *src* into *dst*'s backing buffer in place, keeping *dst*'s tensor id so existing references observe the new contents.

## Details & Options

- Equivalent to <code>[TRealize]()[[TAssign]()[*dst*, *src*]]; *dst*</code>; *dst* retains its `TenDesc` id, so callers still holding it see the update.
- Also installed as the [Set]() UpValue on a literal `TTensor` left-hand side, so <code>[Evaluate]()[*w*] = *expr*</code> mutates *w* in place rather than rebinding the symbol.
- The mutation makes it the natural way to feed new inputs into a <code>[TJit]()</code> closure between replays.

## Basic Examples

Overwrite a tensor's buffer in place:

```wl
d = TTensorCreate[{1., 2., 3.}];
TSet[d, TTensorCreate[{7., 8., 9.}]];
Normal @ d
```
<!-- => {7., 8., 9.} -->

## Applications

Mutate a captured input and replay a <code>[TJit]()</code> closure; the captured result updates in place:

```wl
xFeed = TTensorCreate[{1., 2., 3., 4.}];
fFeed = TJit[Function[{}, TRealize @ (xFeed*xFeed)]];
out = fFeed[];
TSet[xFeed, TTensorCreate[{10., 20., 30., 40.}]];
fFeed[];
Normal @ out
```
<!-- => {100., 400., 900., 1600.} -->

The closure carries the captured step - each <code>[TSet]()</code>-then-replay is one epoch of a training loop:

```wl
fFeed
```
<!-- => TJitClosure summary box: captured: yes, ops: 1 -->
