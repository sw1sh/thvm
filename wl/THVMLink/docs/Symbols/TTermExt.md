---
Template: Symbol
Name: TTermExt
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermExt
Keywords: [ext, term, inspector, packed, label, opcode]
SeeAlso: [TTerm, TTermTag, TTermVal, TTermSub]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermExt]()[*term*]</code> returns the `EXT` field (an `Integer`) of *term*. Accepts either a `TTerm` or a raw `Integer` packed value.

## Details & Options

- The `EXT` is the 18-bit secondary field whose meaning depends on the tag: the SUP/DUP label, the OP2 / MAT opcode, the CTR constructor label, the UOP opcode, the CTR arity, ...
- For a `TAG_SUP` or `TAG_DUP`, `TTermExt` gives the label that drives `DUP-SUP-ANN` vs `DUP-SUP-COM`.

## Basic Examples

The label of a freshly built SUP:

```wl
TReset[];
TTermExt[TSup[1, 2]]
```
<!-- => 1 -->

## Properties and Relations

[TTermTag]() identifies the variant; `TTermExt` is the per-variant secondary field. For NUM values the integer is in `VAL` instead -- see [TTermVal]().
