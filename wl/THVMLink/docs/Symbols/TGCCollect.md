---
Template: Symbol
Name: TGCCollect
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TGCCollect
Keywords: [gc, garbage collection, Cheney, heap, compact]
SeeAlso: [TGCCount, TExternPinCount, THeapBase, THeapPos]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TGCCollect]()[]</code> runs a Cheney semi-space collection of the dynamic heap and returns the new `HEAP_NEXT` (live cell count).

## Details & Options

- Walks every pinned `TTerm` (the external-caller pin table -- see [TExternPinCount]()) and every reachable cell, copies them to the *to-space*, and updates pointers in place. After the call [THeapBase]() and [THeapPos]() may both have shifted.
- Cells unreachable from a pinned root or another live cell are *freed* -- the to-space ends up smaller than the from-space if there was any garbage.
- Triggered automatically when the dyn heap fills past its threshold; the explicit form is for tests and for snapshot-capture flows that want a compacted heap.

## Properties and Relations

[TGCCount]() reports how many collections have run. [TExternPinCount]() is the WL-side pin table size. The collector itself lives in `src/heap/` (Cheney from-space/to-space layout).
