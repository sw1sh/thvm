---
Template: Symbol
Name: TExternPinCount
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TExternPinCount
Keywords: [pin, gc, extern, term, handle]
SeeAlso: [TTerm, TTermUnpin, TGCCollect]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TExternPinCount]()[]</code> returns the current number of entries in the external-caller pin table.

## Details & Options

- Each `TTerm` the WL side holds adds one pin so the Cheney collector keeps the cell alive. The pin drops when Wolfram garbage-collects the wrapper, and the count goes down.
- Use this to observe that standard WL GC has dropped no-longer-referenced `TTerm` wrappers between evaluations (compare the count before and after a forced `GC[]`).

## Basic Examples

A freshly built `TTerm` adds at least one pin:

```wl
TReset[];
TExternPinCount[]
```
<!-- => 0 -->

## Properties and Relations

[TTermUnpin]() explicitly drops a pin without dropping the WL wrapper. The Cheney collector ([TGCCollect]()) walks pinned roots when it compacts the heap.
