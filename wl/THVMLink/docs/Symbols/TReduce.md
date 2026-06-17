---
Template: Symbol
Name: TReduce
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TReduce
Keywords: [reduce, wnf, in-place, chaining]
SeeAlso: [TWnf, THeapGraph, TTermExpr, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TReduce]()[*term*]</code> reduces *term* to WNF in-place and returns *term* (the original root, useful as a seed for [THeapGraph]() or [TTermExpr]() after reduction).

## Details & Options

- Sugar over [TWnf]() that throws away the reduced-root return value and threads *term* through for chaining.
- The heap mutates: re-reading *term* after a `TReduce` reflects the post-reduction state. This is the typical seed for "inspect the heap after the reduction" workflows.

## Basic Examples

`TReduce` runs the WNF reduction and returns the seed for chaining. The interaction counter records the fires:

```wl
TReset[];
TReduce[TApp[TLam[x, TOp2["+", x, TNum[1]]], TNum[41]]];
TItrs[]
```
<!-- => 2 -->

## Properties and Relations

[TRealize]() is `[TWnf]()` composed with `TMaterialize` -- use it for tensor-bearing UOP graphs. `TReduce` is the bare WNF + chain.
