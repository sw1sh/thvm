---
Template: Symbol
Name: THeapRead
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapRead
Keywords: [heap, read, cell, loc, low-level]
SeeAlso: [THeapSet, THeapAlloc, THeapBase, THeapPos, TBookRead]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapRead]()[*loc*]</code> returns the `TTerm` at `heap[*loc*]`.

## Details & Options

- Low-level. The runtime tracks each `Term` cell as a packed 64-bit value; `THeapRead` is the read-port of that array. The returned `TTerm` is a fresh handle wrapping the read value -- safe to compare, inspect, and feed into a follow-up reduction.
- Out-of-range loads read whatever was last there. Use [THeapBase]() / [THeapPos]() to bound your iteration.
- For `BOOK_HEAP` (the AOT-staging heap) use `TBookRead` instead.

## Basic Examples

Allocate a single cell, write a `NUM`, read it back:

```wl
TReset[];
loc = THeapAlloc[1];
THeapSet[loc, TNum[99]];
TTermVal @ THeapRead[loc]
```
<!-- => 99 -->

## Properties and Relations

[THeapSet]() is the matching write port; [THeapAlloc]() reserves the cells; [THeapBase]() / [THeapPos]() give the bounds of the live region.
