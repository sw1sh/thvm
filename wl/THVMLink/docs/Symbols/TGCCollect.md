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

<!-- #| annotation: 26.10.19: TGCCollect wraps gc_collect (src/heap/collect.c), a Cheney copying collector over two equal semi-spaces of the dynamic heap: it evacuates every cell reachable from the root set (the extern pin table, the DEFS table, the last wnf stack, tensor grad slots and kernel source terms) to to-space with forward pointers, re-keys the lam_shape side table and swaps the spaces. The second semi-space starts at 2^27 cells with the default heap size (THVM_HEAP_CELLS). The realize loop also collects on its own once HEAP_NEXT passes a high-water mark. Because every TTerm wrapper is a root and the managed handles were not observed to release in probes, garbage is only what the runtime allocated without handing a wrapper back, such as the internal cells of a reduction. -->

## Details & Options

- The collector copies the cells reachable from the terms the Wolfram Language holds into the other half of the heap, updates the references, and frees the cells left behind.
- Terms held by the Wolfram Language stay valid across a collection, and their wrappers follow the cells to their new locations.
- The result is <code>[THeapPos]()[]</code> after the collection. The live cells occupy the locations from <code>[THeapBase]()[]</code> up to it, in the half of the heap that was empty before the collection.
- Every [TTerm]() wrapper is a root, so the cells freed are the ones the runtime allocated without giving out a term for them, such as the intermediate cells of a reduction.
- The runtime also collects on its own after <code>[TRealize]()</code> once the heap passes a high-water mark. <code>[TReset]()</code> empties the heap and moves it back to its first half.

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
<!-- => 1131 -->

Collect the garbage:

```wl
TGCCollect[]
```
<!-- => 134217770 -->

The live cells that remain:

```wl
THeapPos[] - THeapBase[]
```
<!-- => 42 -->

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

The first collection after a reset moves the heap to its second half:

```wl
TGCCollect[]
```
<!-- => 134217728 -->

The next collection moves it back to the first half:

```wl
TGCCollect[]
```
<!-- => 0 -->

The heap base follows:

```wl
THeapBase[]
```
<!-- => 0 -->

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

Its wrappers are the roots of the collection:

```wl
TExternPinCount[]
```
<!-- => 5 -->

Collect:

```wl
TGCCollect[]
```
<!-- => 134217732 -->

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

Build fifty superpositions without keeping them:

```wl
Table[TSup[i, i + 1], {i, 50}];
```

They occupy a hundred cells:

```wl
THeapPos[]
```
<!-- => 100 -->

Collect:

```wl
TGCCollect[]
```
<!-- => 134217828 -->

Each superposition is still pinned by its wrapper, so none of its cells is freed:

```wl
THeapPos[] - THeapBase[]
```
<!-- => 100 -->
