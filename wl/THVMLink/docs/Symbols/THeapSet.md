---
Template: Symbol
Name: THeapSet
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/THeapSet
Keywords: [heap, write, cell, location, low-level]
SeeAlso: [THeapRead, THeapAlloc, THeapBase, THeapPos, TTerm]
RelatedGuides: [THVMLink]
---

## Usage

<code>[THeapSet]()[*loc*, *t*]</code> writes the term *t* into location *loc* of the dynamic heap.

<!-- #| annotation: 26.10.19: THeapSet wraps heap_set (src/heap/set.c) and writes ttermRaw[t] verbatim: unlike the constructors it does not apply numCoerce, so THeapSet[loc, 42] stores the raw word 42, which the reducer decodes as an APP at location 42. That is deliberate (it is the escape hatch the constructors themselves build on) and is also how a raw tag word can be planted for testing. Like THeapRead it does not bounds-check loc. -->

## Details & Options

- The term is written as it is, replacing whatever the cell held, and the result is 1.
- Cells are reserved first with <code>[THeapAlloc]()</code>, and <code>[THeapRead]()</code> reads them back.
- Writing into a cell of an existing node changes that node, for instance one branch of a superposition.
- A bare integer is written as a raw heap word, not as a number; a number is written as <code>[TNum]()[*n*]</code>.
- The location is not checked: write only locations from <code>[THeapBase]()[]</code> to <code>[THeapPos]()[]</code> - 1.

## Basic Examples

Start from an empty heap:

```wl
TReset[];
```

Reserve a cell:

```wl
loc = THeapAlloc[1]
```
<!-- => 0 -->

Write the number 7 into it:

```wl
THeapSet[loc, TNum[7]]
```
<!-- => 1 -->

Read it back:

```wl
Term[THeapRead[loc]]
```
<!-- => Term["NUM", 7] -->

## Scope

Build a superposition of 1 and 2:

```wl
s = TSup[1, 2]
```
<!-- => a TTerm summary box with tag SUP -->

Replace its second branch with 5:

```wl
THeapSet[TTermVal[s] + 1, TNum[5]]
```
<!-- => 1 -->

The superposition now collapses to 1 and 5:

```wl
TTermVal /@ TCollapse[s]
```
<!-- => {1, 5} -->

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

Any term can be stored, here the eraser:

```wl
THeapSet[loc, TEra[]]
```
<!-- => 1 -->

The cell holds the eraser:

```wl
Term[THeapRead[loc]]
```
<!-- => Term["ERA"] -->

## Properties and Relations

Build a constructor with one child:

```wl
c = TCtr[1, TNum[2]]
```
<!-- => a TTerm summary box with tag CTR and ext 1 -->

Its child sits after the child count:

```wl
THeapSet[TTermVal[c] + 1, TNum[9]]
```
<!-- => 1 -->

The constructor now holds the new child:

```wl
Term[c]
```
<!-- => Term["CTR", 1, Term["NUM", 9]] -->

## Possible Issues

Start from an empty heap:

```wl
TReset[];
```

Reserve a cell:

```wl
loc = THeapAlloc[1]
```
<!-- => 0 -->

Write a bare integer:

```wl
THeapSet[loc, 42]
```
<!-- => 1 -->

The cell does not hold the number 42 but a raw word that reads as an application:

```wl
TTagName[TTermTag[THeapRead[loc]]]
```
<!-- => "APP" -->
