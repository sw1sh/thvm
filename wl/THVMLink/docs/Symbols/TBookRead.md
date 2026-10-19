---
Template: Symbol
Name: TBookRead
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TBookRead
Keywords: [book heap, read, cell, ahead-of-time, Metal, constructor]
SeeAlso: [TBookCtr, THeapRead, TCtr, TTermVal]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TBookRead]()[*loc*]</code> gives the term stored at location *loc* of the book heap.

<!-- #| annotation: 26.10.19: TBookRead wraps thvm_wl_book_read, the read port of BOOK_HEAP, the cell region the Metal ahead-of-time path binds as the kernel's device heap. It is the only host-side view of a book constructor's children: Term and the reducer resolve a CTR's value field against the dynamic heap. There is no bounds check on loc, and location 0 is never allocated (book locations start at 1). -->

## Details & Options

- The book heap is a cell region separate from the dynamic heap, holding the constructors built by <code>[TBookCtr]()</code> for ahead-of-time (AOT) compiled Metal kernels.
- The result is a new [TTerm]() for the word stored at *loc*. Reading does not change the book heap.
- A book-heap constructor whose value field is *loc* stores its child count at *loc* and its children at the locations after it.
- <code>[TReset]()</code> does not clear the book heap, so a location keeps its contents across resets.
- The dynamic heap is read with <code>[THeapRead]()</code>.

## Basic Examples

Build a constructor in the book heap:

```wl
bc = TBookCtr[7, TNum[10], TNum[20]]
```
<!-- => a TTerm summary box with tag CTR and ext 7 -->

The cell at its location holds the child count:

```wl
TBookRead[TTermVal[bc]]
```
<!-- => a TNum summary box showing the number 2 -->

The next cell holds the first child:

```wl
Term[TBookRead[TTermVal[bc] + 1]]
```
<!-- => Term["NUM", 10] -->

## Scope

Read every cell of a constructor in one pass:

```wl
bc = TBookCtr[3, TNum[4], TNum[5], TNum[6]]
```
<!-- => a TTerm summary box with tag CTR and ext 3 -->

The child count is followed by the three children:

```wl
TTermVal /@ TBookRead /@ (TTermVal[bc] + Range[0, 3])
```
<!-- => {3, 4, 5, 6} -->

---

A child that is itself a constructor reads back as a `CTR` term:

```wl
TBookRead[TTermVal[TBookCtr[1, TCtr[2, TNum[5]]]] + 1]
```
<!-- => a TTerm summary box with tag CTR and ext 2 -->

## Properties and Relations

<code>[THeapRead]()</code> reads the dynamic heap, where <code>[TCtr]()</code> builds constructors with the same layout:

```wl
c = TCtr[1, TNum[8], TNum[9]]
```
<!-- => a TTerm summary box with tag CTR and ext 1 -->

Its cells read back the same way:

```wl
TTermVal /@ THeapRead /@ (TTermVal[c] + Range[0, 2])
```
<!-- => {2, 8, 9} -->
