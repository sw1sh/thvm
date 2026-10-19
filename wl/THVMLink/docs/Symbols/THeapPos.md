---
Template: Symbol
Name: THeapPos
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapPos
Keywords: [heap, upper bound, position, next free cell, bump allocator]
SeeAlso: [THeapBase, THeapAlloc, THeapRead, TGCCollect]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapPos]()[]</code> gives the next free location of the dynamic heap, the upper bound of its active region.

<!-- #| annotation: 26.10.19: THeapPos wraps HEAP_NEXT, the bump pointer of the active semi-space. Node sizes follow the heap layout of each tag: LAM 1 cell (the body), APP, SUP, OP2 and MAT 2, CTR one plus its arity, DUP 1; NUM, ERA and the other atoms are packed into the term word and take no cell. TGCCollect leaves it just past the last evacuated cell. -->

## Details & Options

- The live cells are at the locations from <code>[THeapBase]()[]</code> up to <code>[THeapPos]()[]</code> - 1.
- Every allocation advances it: <code>[THeapAlloc]()[*n*]</code> by *n*, and a term constructor by the number of cells of its node.
- A lambda takes one cell, an application or a superposition two, and a constructor one more than its number of children. Numbers and the eraser are stored in the term itself and take no cell.
- <code>[TReset]()</code> sets it to 1, since location 0 never holds a cell, and after <code>[TGCCollect]()</code> it is just past the last live cell in the other half of the heap.

## Basic Examples

Start from an empty heap:

```wl
TReset[];
```

The next free location is 1:

```wl
THeapPos[]
```
<!-- => 1 -->

Reserve two cells:

```wl
THeapAlloc[2]
```
<!-- => 1 -->

The position has moved past them:

```wl
THeapPos[]
```
<!-- => 3 -->

## Scope

Start from an empty heap:

```wl
TReset[];
```

A lambda is a node of one cell, its body:

```wl
TLam[x, x]
```
<!-- => a TTerm summary box with tag LAM -->

The position has advanced by one:

```wl
THeapPos[]
```
<!-- => 2 -->

A constructor with two children takes three cells, its child count and the children:

```wl
TCtr[1, TNum[2], TNum[3]]
```
<!-- => a TTerm summary box with tag CTR and ext 1 -->

The position has advanced by three:

```wl
THeapPos[]
```
<!-- => 5 -->

A number is stored in the term itself:

```wl
TNum[5]
```
<!-- => a TNum summary box showing the number 5 -->

The position has not moved:

```wl
THeapPos[]
```
<!-- => 5 -->

## Properties and Relations

Start from an empty heap:

```wl
TReset[];
```

Build a superposition, a node of two cells:

```wl
s = TSup[1, 2]
```
<!-- => a TTerm summary box with tag SUP -->

The active region holds those two cells:

```wl
THeapPos[] - THeapBase[]
```
<!-- => 2 -->

After a garbage collection the position is in the other half of the heap:

```wl
TGCCollect[]
```
<!-- => 134217730 -->

The size of the active region is unchanged:

```wl
THeapPos[] - THeapBase[]
```
<!-- => 2 -->
