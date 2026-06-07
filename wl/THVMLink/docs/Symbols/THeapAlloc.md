---
Template: Symbol
Name: THeapAlloc
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapAlloc
Keywords: [heap, allocate, cell, loc, low-level]
SeeAlso: [THeapSet, THeapRead, THeapBase, THeapPos, TGCCollect]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapAlloc]()[*size*]</code> reserves *size* consecutive cells in the dynamic heap and returns the base loc.

## Details & Options

- Bump-pointer allocator -- O(1). The reserved cells are *not* zero-initialised; subsequent [THeapSet]() calls must write each one before any reader expects a defined `Term`.
- Allocation respects the active heap region: <code>[THeapBase]() <= loc < [THeapPos]()</code> after the call.
- The Cheney semi-space collector ([TGCCollect]()) compacts live cells when triggered; freshly allocated cells survive the next GC pass iff they are reachable from a `TTerm` the WL caller still holds (or from another live cell).

## Basic Examples

A two-cell allocation returns the base loc of the new region:

```wl
TReset[];
base = THeapAlloc[2];
{base, THeapPos[]}
```
<!-- => {0, 2} -->

## Properties and Relations

The user-facing constructors ([TApp](), [TSup](), [TCtr](), ...) wrap `THeapAlloc` + [THeapSet](); reach for the primitives directly only for low-level surgery and tests.
