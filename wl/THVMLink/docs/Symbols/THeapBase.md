---
Template: Symbol
Name: THeapBase
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapBase
Keywords: [heap, lower bound, base, Cheney]
SeeAlso: [THeapPos, THeapRead, THeapAlloc, TGCCollect]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapBase]()[]</code> returns the lower bound of the active heap region.

## Details & Options

- Equal to `0` in pre-Cheney layouts and immediately after `TInit[]`.
- Equal to `gc_from_start()` once the GC has swapped semi-spaces, so a portable heap iterator walks <code>[Range](){}[[THeapBase]()[], [THeapPos]()[] - 1]</code> to cover live cells.
- Pair with [THeapPos]() to bound any low-level walk.

## Basic Examples

```wl
TReset[];
THeapBase[]
```
<!-- => 0 -->

## Properties and Relations

[THeapPos]() is the upper bound. [TGCCollect]() may shift the base when it swaps semi-spaces.
