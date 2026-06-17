---
Template: Symbol
Name: TReset
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TReset
Keywords: [reset, heap, init, scratch]
SeeAlso: [TInit, TItrs, TFreshLabel, TFree]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TReset]()[]</code> zeroes the heap, the WNF stack, and the interaction counter -- the cheapest way to start a clean reduction without tearing down the runtime.

## Details & Options

- Survives the kernel table: compiled kernels, the AOT cache, and the tensor pool are *not* wiped. To fully reset use `TFree[]` followed by [TInit]().
- The fresh-label counter ([TFreshLabel]()) goes back to 1.
- Use between independent examples in a script so heap loc numbering is predictable.

## Basic Examples

After a reduction the interaction counter has bumped:

```wl
TReset[];
TWnf[TApp[TLam[x, x], TNum[1]]];
TItrs[]
```
<!-- => 1 -->

`TReset` zeroes it back:

```wl
TReset[];
TItrs[]
```
<!-- => 0 -->

## Properties and Relations

[TInit]() is the one-shot allocator setup (idempotent). `TReset` is what you call *between* reductions to keep state predictable.
