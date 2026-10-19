---
Template: Symbol
Name: THeapAlloc
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapAlloc
Keywords: [heap, allocate, cell, location, bump allocator]
SeeAlso: [THeapSet, THeapRead, THeapPos, THeapBase, TGCCollect]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapAlloc]()[*n*]</code> reserves *n* consecutive cells of the dynamic heap and gives the location of the first.

<!-- #| annotation: 26.10.19: THeapAlloc wraps heap_alloc, a bump allocator over the active semi-space (src/heap/alloc.c), and every heap-term constructor goes through it (heapWith in THVMLink.wl). The cells are not cleared, and a cell that no held TTerm reaches is not a GC root. A negative size is passed straight to the C side and was not probed: negative locations crash the kernel in THeapRead, so the page keeps to n >= 0. -->

## Details & Options

- The cells are taken from <code>[THeapPos]()[]</code> onward, and <code>[THeapPos]()[]</code> advances by *n*.
- The new cells are not initialized; <code>[THeapSet]()</code> writes them and <code>[THeapRead]()</code> reads them.
- <code>[THeapAlloc]()[0]</code> gives the current position and reserves nothing.
- The term constructors, such as <code>[TSup]()</code>, <code>[TApp]()</code> and <code>[TLam]()</code>, allocate their cells the same way.
- A garbage collection with <code>[TGCCollect]()</code> keeps an allocated cell only when a term held by the Wolfram Language reaches it.

## Basic Examples

Start from an empty heap:

```wl
TReset[];
```

Reserve two cells, which start at location 0:

```wl
THeapAlloc[2]
```
<!-- => 0 -->

The next free location has moved past them:

```wl
THeapPos[]
```
<!-- => 2 -->

The next allocation starts where the previous one ended:

```wl
THeapAlloc[3]
```
<!-- => 2 -->

## Scope

Start from an empty heap:

```wl
TReset[];
```

Reserve two cells:

```wl
loc = THeapAlloc[2]
```
<!-- => 0 -->

Write a number into the first:

```wl
THeapSet[loc, TNum[3]]
```
<!-- => 1 -->

Write a number into the second:

```wl
THeapSet[loc + 1, TNum[4]]
```
<!-- => 1 -->

Read both cells back:

```wl
Term /@ THeapRead /@ (loc + Range[0, 1])
```
<!-- => {Term["NUM", 3], Term["NUM", 4]} -->

---

Start from an empty heap:

```wl
TReset[];
```

An allocation of no cells gives the current position:

```wl
THeapAlloc[0]
```
<!-- => 0 -->

Nothing was reserved:

```wl
THeapPos[]
```
<!-- => 0 -->

## Properties and Relations

Start from an empty heap:

```wl
TReset[];
```

A superposition is a node of two cells:

```wl
TSup[1, 2]
```
<!-- => a TTerm summary box with tag SUP -->

Building it reserved those two cells:

```wl
THeapPos[]
```
<!-- => 2 -->

The next allocation follows them:

```wl
THeapAlloc[1]
```
<!-- => 2 -->
