---
Template: Symbol
Name: THeapPos
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapPos
Keywords: [heap, upper bound, position, bump pointer]
SeeAlso: [THeapBase, THeapRead, THeapAlloc, TGCCollect]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapPos]()[]</code> returns the next free heap location -- the upper bound of the active heap region.

## Details & Options

- A [THeapAlloc]()`[*n*]` call bumps `THeapPos[]` by *n*.
- Iterate over live cells with <code>[Range](){}[[THeapBase]()[], [THeapPos]()[] - 1]</code>.
- After [TReset]() and on first init, `THeapPos[] == THeapBase[] == 0`.

## Basic Examples

A two-cell allocation moves the position:

```wl
TReset[];
THeapAlloc[2];
THeapPos[]
```
<!-- => 2 -->

## Properties and Relations

[THeapBase]() is the lower bound; [TGCCollect]() compacts the live cells and may shift both bounds.
