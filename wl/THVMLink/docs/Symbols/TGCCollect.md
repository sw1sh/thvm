---
Template: Symbol
Name: TGCCollect
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TGCCollect
Keywords: [garbage collection, Cheney, semi-space, heap, compact]
SeeAlso: [TExternPinCount, THeapBase, THeapPos, TReset, TTerm]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TGCCollect]()[]</code> runs a garbage collection of the dynamic heap and gives the new heap position.

<!-- #| annotation: 26.11.23: TGCCollect wraps gc_collect_low (src/heap/collect.c). gc_collect is a Cheney copying collector over two equal semi-spaces of the dynamic heap: it evacuates every cell reachable from the root set (the extern pin table, the DEFS table, the last wnf stack, tensor grad slots and kernel source terms) to to-space with forward pointers, re-keys the lam_shape side table and swaps the spaces. gc_collect_low runs it and, when that leaves the live cells in the high semi-space (which starts at 2^27 cells with the default heap size, THVM_HEAP_CELLS), runs it once more to copy them back, so a collection ends with the live cells at the bottom of the heap, where the scheduler's scratch arrays, which are indexed by heap location and span HEAP_NEXT, stay proportional to the live cells. The pair counts as one collection in TGCCount. The realize loop collects through the same function once HEAP_NEXT passes a high-water mark. A TTerm wrapper is a root only while the Wolfram Language holds it, since its managed handle releases the pin when the wrapper is dropped, so garbage is the cells of terms whose wrappers are gone and what the runtime allocated without handing a wrapper back, such as the internal cells of a reduction. -->

## Details & Options

- The heap is split into two halves. The collector copies the cells reachable from the terms the Wolfram Language holds into the empty half and updates the references, then copies them back when that half is the second one. A collection so ends with the live cells packed at the start of the first half and the rest of the heap free.
- Terms held by the Wolfram Language stay valid across a collection, and their wrappers follow the cells to their new locations.
- The result is <code>[THeapPos]()[]</code> after the collection, one more than the number of live cells. The live cells occupy the locations from <code>[THeapBase]()[]</code>, which is 1 after a collection, up to it.
- Every [TTerm]() wrapper the Wolfram Language still holds is a root. The cells freed are the ones no held term reaches: the cells of terms whose wrappers were dropped, and the ones the runtime allocated without giving out a term for them, such as the intermediate cells of a reduction.
- The runtime also collects on its own after <code>[TRealize]()</code> once the heap passes a high-water mark, and that collection ends in the first half too. <code>[TReset]()</code> empties the heap.

## Basic Examples

Start from an empty heap:

```wl
TReset[];
```

Build a term to keep:

```wl
x = TSup[1, 2] + 3
```
<!-- => a TTerm summary box with tag OP2 -->

Collapsing a sum of two superpositions of ten numbers leaves its intermediate cells on the heap:

```wl
Total[TTermVal /@ TCollapse[TSup[Range[10]] + TSup[Range[10]]]]
```
<!-- => 1100 -->

The heap now holds over a thousand cells:

```wl
THeapPos[]
```
<!-- => 1132 -->

Collect the garbage:

```wl
TGCCollect[]
```
<!-- => 5 -->

The live cells that remain are the four cells of the term kept:

```wl
THeapPos[] - THeapBase[]
```
<!-- => 4 -->

The term kept across the collection still reduces:

```wl
TTermVal /@ TCollapse[x]
```
<!-- => {4, 5} -->

## Scope

Start from an empty heap:

```wl
TReset[];
```

Hold a superposition, a node of two cells:

```wl
s = TSup[1, 2]
```
<!-- => a TTerm summary box with tag SUP -->

A collection packs the live cells at the start of the heap and gives the position just past them:

```wl
TGCCollect[]
```
<!-- => 3 -->

The heap base stays at 1:

```wl
THeapBase[]
```
<!-- => 1 -->

## Properties and Relations

Start from an empty heap:

```wl
TReset[];
```

Build a term:

```wl
x = TSup[1, 2] + 3
```
<!-- => a TTerm summary box with tag OP2 -->

Its wrapper is the only root of the collection:

```wl
TExternPinCount[]
```
<!-- => 1 -->

Collect:

```wl
TGCCollect[]
```
<!-- => 5 -->

Every cell of the held term survives:

```wl
THeapPos[] - THeapBase[]
```
<!-- => 4 -->

## Possible Issues

Start from an empty heap:

```wl
TReset[];
```

Build fifty superpositions and keep them in a variable:

```wl
sups = Table[TSup[i, i + 1], {i, 50}];
```

They occupy a hundred cells:

```wl
THeapPos[]
```
<!-- => 101 -->

Collect:

```wl
TGCCollect[]
```
<!-- => 101 -->

Each superposition is still pinned by its wrapper, so none of its cells is freed:

```wl
THeapPos[] - THeapBase[]
```
<!-- => 100 -->

Clear the variable, which drops the wrappers:

```wl
Clear[sups];
```

The next collection frees every cell:

```wl
TGCCollect[]
```
<!-- => 1 -->
