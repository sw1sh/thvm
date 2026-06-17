---
Template: Symbol
Name: TTermVal
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermVal
Keywords: [val, term, inspector, packed, heap loc]
SeeAlso: [TTerm, TTermTag, TTermExt, TTermSub]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermVal]()[*term*]</code> returns the `VAL` field of *term* -- the 38-bit payload whose meaning depends on the tag.

## Details & Options

- For a compound (`APP`, `SUP`, `LAM`, `CTR`, ...), `VAL` is the heap location of the cell's children.
- For an atom like `TAG_NUM`, `VAL` is the integer value itself.
- For a `VAR`, `VAL` is the binder's heap loc.

## Basic Examples

A `TAG_NUM` holds its integer in `VAL`:

```wl
TReset[];
TTermVal[TNum[42]]
```
<!-- => 42 -->

## Properties and Relations

[TTermTag]() / [TTermExt]() / [TTermSub]() expose the other packed fields.
