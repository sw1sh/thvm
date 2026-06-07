---
Template: Symbol
Name: TTermTag
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermTag
Keywords: [tag, term, inspector, packed]
SeeAlso: [TTerm, TTermExt, TTermVal, TTermSub, TTermExpr]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermTag]()[*term*]</code> returns the tag (an `Integer`) of *term*. Accepts either a `TTerm` or a raw `Integer` packed value.

## Details & Options

- The tag is the 5-bit field selecting the term variant: `APP` = 0, `LAM` = 1, `VAR` = 2, `ERA` = 3, `DP0` = 4, `DP1` = 5, `SUP` = 6, `DUP` = 7, `TEN` = 8, `UOP` = 9, `NUM` = 10, ... See `$TagAPP` and friends in `Kernel/THVMLink.wl`.
- For a `String` name use the indexer `*t*["tagName"]` (an alias for `TTagName[TTermTag[*t*]]`).

## Basic Examples

A LAM has tag 1:

```wl
TReset[];
TTermTag[TLam[x, x]]
```
<!-- => 1 -->

## Properties and Relations

[TTermExt](), [TTermVal](), [TTermSub]() expose the other packed fields. For the structural canonical form use [Term]().
