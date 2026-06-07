---
Template: Symbol
Name: TSup
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSup
Keywords: [superposition, sup, label, multicomputation, parallelism]
SeeAlso: [TDup, TCollapse, TMultiwayGraph, TFreshLabel, TDsu]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TSup]()[*a*, *b*]</code> constructs a `TAG_SUP` superposition of *a* and *b* with a fresh integer label.

<code>[TSup]()[*label*, *a*, *b*]</code> uses an explicit *label*.

<code>[TSup]()[{*x*<sub>1</sub>, *x*<sub>2</sub>, ..., *x*<sub>n</sub>}]</code> is the *n*-way variant -- a left-fold of <code>[TSup]()[*x*<sub>i</sub>, *x*<sub>i+1</sub>]</code> with fresh labels at each level.

## Details & Options

- A superposition is "two values at once" along the *label* dimension; bare-Integer children are coerced to `TAG_NUM` via the constructor sugar.
- Labels matter for downstream `DUP-SUP` interactions: a `DUP` of the same label *annihilates* (pairs diagonally), a different label *commutes* (cross-products). See [TDup]() and the multicomputation guide.
- Operators slide into the branches via `OP2-SUP` and `APP-SUP`, so <code>[TSup]()[1, 2] + 3</code> reduces to a `SUP` of the per-branch sums.
- Read the leaves with [TCollapse]().

## Basic Examples

A two-way superposition is itself a `TTerm`:

```wl
Term[TSup[5, 7]]
```
<!-- => Term["SUP", 2, Term["NUM", 5], Term["NUM", 7]] -->

The list form is a fresh-label left-fold:

```wl
TReset[];
Term[TSup[{5, 7, 9}]]
```
<!-- => Term["SUP", 2, Term["SUP", 1, Term["NUM", 5], Term["NUM", 7]], Term["NUM", 9]] -->

## Scope

[TCollapse]() enumerates the leaves:

```wl
TReset[];
TTermVal /@ TCollapse[TSup[{5, 7, 9}]]
```
<!-- => {5, 7, 9} -->

Arithmetic distributes through superposition (the `OP2-SUP` slide):

```wl
TReset[];
TTermVal /@ TCollapse[TSup[1, 2] + 3]
```
<!-- => {4, 5} -->

## Properties and Relations

`TSup` is the source of multicomputation parallelism; observe the slice evolution with [TMultiTrace]() and view it as a graph with [TMultiwayGraph](). For a label resolved at reduction time (a dynamic-label SUP) use [TDsu]().
