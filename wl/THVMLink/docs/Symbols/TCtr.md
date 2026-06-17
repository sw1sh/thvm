---
Template: Symbol
Name: TCtr
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TCtr
Keywords: [constructor, ctr, ADT, sum type, ctor, hvm4]
SeeAlso: [TBookCtr, TMatCtr, TMatChain, TMatNum]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TCtr]()[*label*, *c*<sub>1</sub>, *c*<sub>2</sub>, ...]</code> constructs a `TAG_CTR` cell with the given integer *label* and child terms. Arity capped at 16 (matches HVM4's CTR limit).

## Details & Options

- An IC-level [TDup]() of a `CTR` fires `DUP-CTR` via the runtime's `interact_dup_ctr` -- the constructor's children fan out under their own DUPs.
- A pattern match against the same *label* via [TMatCtr]() / [TMatChain]() destructures the children, applying the handler positionally (see [TMatNum]()).
- Construction allocates in the dynamic heap; use [TBookCtr]() for the AOT Metal path which needs cells in `BOOK_HEAP`.

## Basic Examples

A two-field constructor with integer label 3:

```wl
TReset[];
Term[TCtr[3, TNum[10], TNum[20]]]
```
<!-- => Term["CTR", 3, Term["NUM", 10], Term["NUM", 20]] -->

## Scope

Destructure with [TMatCtr](): the matcher applies its handler to the children positionally.

```wl
TReset[];
ctr  = TCtr[1, TNum[10], TNum[20]];
add2 = TMatCtr[1, TLam[a, TLam[b, TOp2["+", a, b]]], TLam[k, TNum[0]]];
TTermVal @ TWnf @ TApp[add2, ctr]
```
<!-- => 30 -->

## Properties and Relations

For multi-label dispatch (one matcher fanning to several CTR labels with their own handlers), use [TMatChain]().
