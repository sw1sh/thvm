---
Template: Symbol
Name: TBookCtr
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TBookCtr
Keywords: [constructor, book heap, ahead-of-time, Metal, interaction combinator]
SeeAlso: [TCtr, TBookRead, TMatCtr, THeapRead]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TBookCtr]()[*label*, $c_1$, $c_2$, …]</code> builds a constructor term with the integer *label* and the children $c_i$ in the book heap rather than the dynamic heap.

<!-- #| annotation: 26.10.19: TBookCtr allocates through thvm_wl_term_new_book_ctr into BOOK_HEAP, the region the Metal ahead-of-time path binds zero-copy as the kernel's device heap buffer, so a constructor destructured inside a dispatched kernel has to live there. The layout matches the dynamic heap's CTR layout: the child count as a NUM, then the children. BOOK_HEAP is reset only by TInitialize of a snapshot that bundles BookCells, never by TReset. Like TCtr it passes children through ttermRaw without numCoerce, so a bare Integer child is spliced in as a raw word; and Term, TWnf and the MAT rules read children from the dynamic heap, so on the host a book constructor is inspectable only cell by cell through TBookRead. -->

## Details & Options

- The book heap is a cell region separate from the dynamic heap. It holds the constructors that ahead-of-time (AOT) compiled Metal kernels read, since such a kernel's heap buffer is bound to it.
- The result is a [TTerm]() with tag `CTR`, whose extension field is *label* and whose value field is the constructor's location in the book heap.
- A constructor occupies consecutive book-heap cells: its child count, as a <code>[TNum]()</code> term, followed by its children. <code>[TBookRead]()</code> reads them back.
- The children are terms: a number is written as <code>[TNum]()[*n*]</code>.
- <code>[TReset]()</code> clears the dynamic heap but leaves the book heap as it is, so book-heap locations keep growing across resets.
- <code>[Term]()</code>, <code>[TWnf]()</code> and the matchers read children from the dynamic heap, so they do not see the children of a book-heap constructor. A constructor for host-side reduction is built with <code>[TCtr]()</code>.

## Basic Examples

A two-field constructor with label 5 in the book heap:

```wl
bc = TBookCtr[5, TNum[1], TNum[2]]
```
<!-- => a TTerm summary box with tag CTR and ext 5 -->

The extension field of the term carries the label:

```wl
TTermExt[bc]
```
<!-- => 5 -->

The cell at the constructor's location holds its child count:

```wl
TBookRead[TTermVal[bc]]
```
<!-- => a TNum summary box showing the number 2 -->

## Scope

A constructor occupies its child count followed by its children:

```wl
bc = TBookCtr[7, TNum[10], TNum[20]]
```
<!-- => a TTerm summary box with tag CTR and ext 7 -->

Read the three cells back as numbers:

```wl
TTermVal /@ TBookRead /@ (TTermVal[bc] + Range[0, 2])
```
<!-- => {2, 10, 20} -->

---

A constructor with no children occupies one cell, its child count of zero:

```wl
TTermVal[TBookRead[TTermVal[TBookCtr[4]]]]
```
<!-- => 0 -->

## Properties and Relations

<code>[TCtr]()</code> builds the same constructor in the dynamic heap, where <code>[Term]()</code> reads its children:

```wl
Term[TCtr[5, TNum[1], TNum[2]]]
```
<!-- => Term["CTR", 5, Term["NUM", 1], Term["NUM", 2]] -->

---

The dynamic heap uses the same layout, a child count followed by the children:

```wl
c = TCtr[3, TNum[10], TNum[20]]
```
<!-- => a TTerm summary box with tag CTR and ext 3 -->

Read its cells with <code>[THeapRead]()</code>:

```wl
TTermVal /@ THeapRead /@ (TTermVal[c] + Range[0, 2])
```
<!-- => {2, 10, 20} -->
