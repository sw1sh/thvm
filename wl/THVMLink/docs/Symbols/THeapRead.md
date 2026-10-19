---
Template: Symbol
Name: THeapRead
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapRead
Keywords: [heap, read, cell, location, low-level]
SeeAlso: [THeapSet, THeapAlloc, THeapBase, THeapPos, TBookRead]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapRead]()[*loc*]</code> gives the term stored at location *loc* of the dynamic heap.

<!-- #| annotation: 26.10.19: THeapRead wraps heap_read (src/heap/read.c) and packs the 64-bit word into a fresh TTerm, which adds a pin for that word. The C side checks the location against the heap capacity, not THeapPos[], so a location past THeapPos[] reads an old or empty cell without a message. After a beta reduction the cell that held the lambda body carries the substituted argument with its SUB flag set, which is what Term follows when it resolves a VAR. -->

## Details & Options

- A heap cell holds one term. The result is a new [TTerm]() for it, and reading does not change the heap.
- The cells of a node hold its children: a superposition or an application at location *loc* keeps its two children at *loc* and *loc* + 1, a lambda keeps its body at *loc*, and a constructor keeps its child count at *loc* followed by its children.
- The value field of a node term, <code>[TTermVal]()[*t*]</code>, is the location of its first cell.
- Only locations from <code>[THeapBase]()[]</code> to <code>[THeapPos]()[]</code> - 1 hold live cells. A location outside the heap issues the message `THeapRead::loc`, and the input returns unevaluated.
- The book heap of ahead-of-time (AOT) Metal kernels is read with <code>[TBookRead]()</code>.

## Basic Examples

Start from an empty heap:

```wl
TReset[];
```

Build a superposition of 1 and 2:

```wl
s = TSup[1, 2]
```
<!-- => a TTerm summary box with tag SUP -->

Its first cell holds the first branch:

```wl
THeapRead[TTermVal[s]]
```
<!-- => a TNum summary box showing the number 1 -->

The next cell holds the second branch:

```wl
Term[THeapRead[TTermVal[s] + 1]]
```
<!-- => Term["NUM", 2] -->

## Scope

An application keeps its function and its argument in two cells:

```wl
a = TApp[TLam[x, x + 1], TNum[41]]
```
<!-- => a TTerm summary box with tag APP -->

The first cell holds the function:

```wl
Term[THeapRead[TTermVal[a]]]
```
<!-- => Term["LAM", id, Term["OP2", "+", Term["VAR", id], Term["NUM", 1]]] -->

The second cell holds the argument:

```wl
Term[THeapRead[TTermVal[a] + 1]]
```
<!-- => Term["NUM", 41] -->

---

Start from an empty heap:

```wl
TReset[];
```

Reserve a cell:

```wl
loc = THeapAlloc[1]
```
<!-- => 0 -->

Write a number into it:

```wl
THeapSet[loc, TNum[99]]
```
<!-- => 1 -->

Read it back:

```wl
THeapRead[loc]
```
<!-- => a TNum summary box showing the number 99 -->

## Applications

Start from an empty heap:

```wl
TReset[];
```

Build the successor function applied to 41:

```wl
a = TApp[TLam[x, x + 1], TNum[41]]
```
<!-- => a TTerm summary box with tag APP -->

List every live cell of the heap:

```wl
Term /@ THeapRead /@ Range[THeapBase[], THeapPos[] - 1]
```
<!-- => {Term["OP2", "+", Term["VAR", 0], Term["NUM", 1]], Term["VAR", 0], Term["NUM", 1], Term["LAM", 0, Term["OP2", "+", Term["VAR", 0], Term["NUM", 1]]], Term["NUM", 41]} -->

Reduce the application:

```wl
TWnf[a]
```
<!-- => a TNum summary box showing the number 42 -->

The reduction has rewritten the cells, and the body cell of the lambda now holds the argument 41:

```wl
Term /@ THeapRead /@ Range[THeapBase[], THeapPos[] - 1]
```
<!-- => {Term["NUM", 41], Term["NUM", 41], Term["NUM", 1], Term["LAM", 0, Term["NUM", 41]], Term["NUM", 41]} -->

## Properties and Relations

<code>[TBookRead]()</code> reads the book heap, where <code>[TBookCtr]()</code> builds constructors with the same layout:

```wl
bc = TBookCtr[2, TNum[7]]
```
<!-- => a TTerm summary box with tag CTR and ext 2 -->

Read its child count and its child from the book heap:

```wl
TTermVal /@ TBookRead /@ (TTermVal[bc] + Range[0, 1])
```
<!-- => {1, 7} -->

## Possible Issues

A location outside the heap is refused:

```wl
THeapRead[-1]
```
<!-- => the message THeapRead::loc is issued and the expression returns unevaluated -->
