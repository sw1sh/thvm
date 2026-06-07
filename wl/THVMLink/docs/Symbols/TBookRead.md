---
Template: Symbol
Name: TBookRead
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TBookRead
Keywords: [book heap, read, AOT, Metal, ctr]
SeeAlso: [TBookCtr, THeapRead, TCtr]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TBookRead]()[*loc*]</code> returns the `TTerm` at `book_heap[*loc*]`.

## Details & Options

- The *book heap* is a separate cell region used for AOT-staging: the Metal kernel's heap `MTLBuffer` is bound to `BOOK_HEAP`, so anything destructured by the dispatched kernel must live there.
- Use [TBookCtr]() to build a `TAG_CTR` directly in book heap; use `TBookRead` to inspect what landed.
- For the dynamic heap use [THeapRead]() instead.

## Basic Examples

A book-CTR can be inspected through `TBookRead` once you know its cell loc -- typically by reading the constructor's `VAL` field. (Most users instead let the AOT pipeline manage the book heap and only read it back when debugging a stuck kernel.)

```wl
TReset[];
bc = TBookCtr[5, TNum[1], TNum[2]];
bc["tagName"]
```
<!-- => "CTR" -->

## Properties and Relations

[THeapRead]() / [THeapSet]() / [THeapAlloc]() are the dynamic-heap analogues.
