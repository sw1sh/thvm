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

<!-- #| annotation: 26.10.19: THeapBase wraps the C heap's from-space start: 0 after thvm_init or TReset, and gc_from_start() once a collection has swapped the semi-spaces, which is 2^27 cells with the default heap size. Heap walkers (THeap, THeapGraph[], TContextSnapshot) iterate from THeapBase[] to THeapPos[] - 1 for that reason. -->

## Details & Options

- The live cells of the dynamic heap are at the locations from <code>[THeapBase]()[]</code> up to <code>[THeapPos]()[]</code> - 1.
- The heap is split into two halves for garbage collection. <code>[THeapBase]()[]</code> is 0 while the first half is active, and the start of the second half, 134217728 with the default heap size, while the second half is active.
- Each <code>[TGCCollect]()</code> moves the live cells to the other half, and <code>[TReset]()</code> sets the base back to 0.

## Basic Examples

Start from an empty heap:

```wl
TReset[];
```

The active region starts at location 0:

```wl
THeapBase[]
```
<!-- => 0 -->

## Scope

Start from an empty heap:

```wl
TReset[];
```

A garbage collection moves the heap to its second half:

```wl
TGCCollect[]
```
<!-- => 134217728 -->

The base has moved with it:

```wl
THeapBase[]
```
<!-- => 134217728 -->

A second collection moves the heap back:

```wl
TGCCollect[]
```
<!-- => 0 -->

The base is 0 again:

```wl
THeapBase[]
```
<!-- => 0 -->

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

After a collection the same cells sit in the other half of the heap:

```wl
TGCCollect[]
```
<!-- => 134217730 -->

The same walk finds them there:

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
