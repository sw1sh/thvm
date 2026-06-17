---
Template: Symbol
Name: TBookCtr
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TBookCtr
Keywords: [ctr, book_heap, AOT, Metal, constructor]
SeeAlso: [TCtr, TBookRead, TMatCtr]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TBookCtr]()[*label*, *c*<sub>1</sub>, *c*<sub>2</sub>, ...]</code> constructs a `TAG_CTR` in `BOOK_HEAP` rather than the dynamic heap.

## Details & Options

- Use when constructing CTR inputs destined for the Metal AOT path: the kernel's heap `MTLBuffer` is bound to `BOOK_HEAP`, so destructure derefs only resolve for `VAL`s in that range.
- For ordinary (host-side) construction use [TCtr]() -- it allocates in the dynamic heap that all WNF reductions also write to.
- Read a book-heap cell with `TBookRead`.

## Basic Examples

A two-field book-CTR survives the live runtime state. Its tag is `TAG_CTR`:

```wl
TReset[];
bc = TBookCtr[5, TNum[1], TNum[2]];
bc["tagName"]
```
<!-- => "CTR" -->

## Properties and Relations

`TBookCtr` lives on the AOT codepath (see Phase 7 iter T in the trajectory); a destructure on the result follows the same `APP-MAT-CTR-MAT` rule as a dynamic [TCtr]() because both share the underlying `TAG_CTR` layout.
