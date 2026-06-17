---
Template: Symbol
Name: TKernelCount
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TKernelCount
Keywords: [kernel, count, side table, codegen]
SeeAlso: [TKernel, TKernelSource, TKernelFlops, TRealize, TScheduleGraph]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TKernelCount]()[]</code> returns the number of compiled kernel entries currently in the kernel side table.

## Details & Options

- The count grows as <code>[TRealize]()</code> kernelizes new UOp graphs; each distinct kernel is appended once.
- A kernel id (`kid`) for the most recently emitted kernel is <code>[TKernelCount]()[] - 1</code>; feed it to <code>[TKernelSource]()</code>, <code>[TKernelFlops]()</code>, <code>[TKernelProposed]()</code>, or <code>[TKernel]()</code>.
- A fresh <code>[TInit]()</code> leaves a single bootstrap entry, so the count begins at 1.

## Basic Examples

A fresh runtime holds the bootstrap entry only:

```wl
TInit[];
TKernelCount[]
```
<!-- => 1 -->

## Properties and Relations

Realizing a graph appends its kernel and increments the count:

```wl
before = TKernelCount[];
x = TTensorCreate[Range[1., 8.]];
TRealize[Total[x^2]];
TKernelCount[] - before
```
<!-- => 1 -->
