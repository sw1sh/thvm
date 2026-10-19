---
Template: Symbol
Name: TFreshLabel
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFreshLabel
Keywords: [label, superposition, duplication, fresh, counter]
SeeAlso: [TSup, TDup, TDsu, TDdu, TReset]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFreshLabel]()[]</code> gives the next label from the runtime's label counter and advances the counter.

<!-- #| annotation: 26.10.19: The counter lives on the WL side ($labelCounters in THVMLink.wl), one entry per runtime context slot; TReset and TInit both set it back to 1, and TContextSnapshot/TInitialize save and restore it as "Labels". The automatic DUPs inserted for a lambda variable used more than once take labels from 0x10000 upward on the C side (src/lam/auto_dup.c), so the two ranges do not collide unless a page draws 65535 fresh labels. -->

## Details & Options

- A label is a positive integer that tags a superposition or a duplication. A duplication meeting a superposition with the same label takes one branch per projection; with different labels it copies the superposition.
- <code>[TSup]()</code> and <code>[TDup]()</code> without an explicit label draw their label from [TFreshLabel](), so they advance the counter too.
- The counter starts at 1, and <code>[TReset]()</code> sets it back to 1. There is one counter for each runtime context.
- The automatic duplications that a lambda inserts for a variable used more than once take labels from 65536 upward, apart from this counter.

## Basic Examples

Start from a reset runtime, so the counter is at 1:

```wl
TReset[];
```

The first fresh label is 1:

```wl
TFreshLabel[]
```
<!-- => 1 -->

The next one is 2:

```wl
TFreshLabel[]
```
<!-- => 2 -->

## Scope

Start from a reset runtime:

```wl
TReset[];
```

A superposition without an explicit label takes the next fresh label:

```wl
Term[TSup[1, 2]]
```
<!-- => Term["SUP", 1, Term["NUM", 1], Term["NUM", 2]] -->

So does a duplication:

```wl
Term /@ TDup[TNum[5]]
```
<!-- => {Term["DP0", 2, Term["NUM", 5]], Term["DP1", 2, Term["NUM", 5]]} -->

The counter has moved past both labels:

```wl
TFreshLabel[]
```
<!-- => 3 -->

## Applications

Start from a reset runtime:

```wl
TReset[];
```

Draw one label to share between a superposition and a duplication:

```wl
l = TFreshLabel[]
```
<!-- => 1 -->

With the shared label the two projections take one branch each, so their sum is 1 + 2:

```wl
TTermVal /@ TCollapse[Plus @@ TDup[l, TSup[l, 1, 2]]]
```
<!-- => {3} -->

---

Labels drawn separately differ, and the duplication copies the superposition instead:

```wl
TTermVal /@ TCollapse[Plus @@ TDup[TFreshLabel[], TSup[TFreshLabel[], 1, 2]]]
```
<!-- => {2, 4} -->

## Properties and Relations

The automatic duplication in a lambda that squares its variable uses a label of at least 65536:

```wl
Term[TLam[x, x * x]]
```
<!-- => Term["LAM", id, Term["OP2", "*", Term["DP0", L, Term["VAR", id]], Term["DP1", L, Term["VAR", id]]]], with L at least 65536 -->

---

Start from a reset runtime:

```wl
TReset[];
```

Draw a few labels:

```wl
Table[TFreshLabel[], 3]
```
<!-- => {1, 2, 3} -->

After a reset the counter starts over:

```wl
TReset[];
```

The next label is 1 again:

```wl
TFreshLabel[]
```
<!-- => 1 -->
