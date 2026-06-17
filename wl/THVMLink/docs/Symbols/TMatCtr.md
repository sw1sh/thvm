---
Template: Symbol
Name: TMatCtr
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMatCtr
Keywords: [match, ctr, constructor, destructure, pattern]
SeeAlso: [TMatNum, TMatChain, TCtr, TIfZero]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMatCtr]()[*ctorName*, *handler*, *fallback*]</code> -- sugar alias for [TMatNum]() with the intent of destructuring a [TCtr]() whose label is *ctorName* (an anonymous CTR uses `0`). When applied to a matching `CTR`, applies *handler* positionally to each child via an `APP` chain.

## Details & Options

- Same primitive as [TMatNum](): the dispatch is by the applied arg's tag and ext, the destructure layout matches HVM4's `APP-MAT-CTR-MAT`.
- *handler* should be a curried lambda whose arity equals the constructor's child count.
- A `SUP` on the arg side commutes via `APP-MAT-SUP`.

## Basic Examples

A two-field constructor destructured into the sum of its children:

```wl
TReset[];
ctr  = TCtr[1, TNum[10], TNum[20]];
add2 = TMatCtr[1, TLam[a, TLam[b, TOp2["+", a, b]]], TLam[k, TNum[0]]];
TTermVal @ TWnf @ TApp[add2, ctr]
```
<!-- => 30 -->

## Properties and Relations

`TMatCtr` is used by [TGrad]() (multi-target form) to bind the list of gradient results into a body lambda without an indexed projection primitive. For a chain of constructor labels with their own handlers use [TMatChain]().
