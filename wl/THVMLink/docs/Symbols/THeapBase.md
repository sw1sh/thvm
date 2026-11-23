---
Template: Symbol
Name: THeapBase
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapBase
Keywords: [heap, lower bound, base, semi-space, garbage collection]
SeeAlso: [THeapPos, THeapRead, THeapAlloc, TGCCollect]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapBase]()[]</code> gives the lower bound of the active region of the dynamic heap.

<!-- #| annotation: 26.11.23: THeapBase wraps the C heap's first allocatable cell of the from-space, gc_space_base(gc_from_start()): 1 while the low semi-space is active, since location 0 reads as "none" and never holds a node, and the start of the high semi-space, 2^27 cells with the default heap size, while that one is active. thvm_init and TReset start in the low semi-space, and TGCCollect and the realize-boundary collection end in it (gc_collect_low in src/heap/collect.c copies the live cells back), so the base reads 1 after each of them. A bare gc_collect, which other parts of the runtime call, swaps the semi-spaces and can leave the high one active. Heap walkers (THeap, THeapGraph[], TContextSnapshot) iterate from THeapBase[] to THeapPos[] - 1 for that reason. -->

## Details & Options

- The live cells of the dynamic heap are at the locations from <code>[THeapBase]()[]</code> up to <code>[THeapPos]()[]</code> - 1.
- The heap is split into two halves for garbage collection. <code>[THeapBase]()[]</code> is 1 while the first half is active, since location 0 never holds a cell, and the start of the second half, 134217728 with the default heap size, while the second half is active.
- A collection with <code>[TGCCollect]()</code>, like the one the runtime runs after <code>[TRealize]()</code>, ends with the live cells in the first half, so the base is 1 after it. <code>[TReset]()</code> sets the base back to 1 too.

## Basic Examples

Start from an empty heap:

```wl
TReset[];
```

The active region starts at location 1:

```wl
THeapBase[]
```
<!-- => 1 -->

## Scope

Start from an empty heap:

```wl
TReset[];
```

Run a garbage collection, which gives the new heap position:

```wl
TGCCollect[]
```
<!-- => 1 -->

The collection ends in the first half of the heap, so the base is still 1:

```wl
THeapBase[]
```
<!-- => 1 -->

## Applications

Start from an empty heap:

```wl
TReset[];
```

Build a superposition:

```wl
s = TSup[1, 2]
```
<!-- => a TTerm summary box with tag SUP -->

Read every live cell, from the base up to the position:

```wl
Term /@ THeapRead /@ Range[THeapBase[], THeapPos[] - 1]
```
<!-- => {Term["NUM", 1], Term["NUM", 2]} -->

A collection keeps the cells of the held term and gives the position just past them:

```wl
TGCCollect[]
```
<!-- => 3 -->

The same walk still finds them:

```wl
Term /@ THeapRead /@ Range[THeapBase[], THeapPos[] - 1]
```
<!-- => {Term["NUM", 1], Term["NUM", 2]} -->

## Properties and Relations

Start from an empty heap:

```wl
TReset[];
```

The active region is empty when the base and the position agree:

```wl
THeapPos[] - THeapBase[]
```
<!-- => 0 -->
