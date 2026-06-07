---
Template: Symbol
Name: TMatChain
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMatChain
Keywords: [match, dispatch, chain, multi-label, ADT]
SeeAlso: [TMatNum, TMatCtr, TIfZero, TCtr]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMatChain]()[<|*label*<sub>1</sub> -> *handler*<sub>1</sub>, ..., *label*<sub>k</sub> -> *handler*<sub>k</sub>|>, *fallback*]</code> builds a nested chain of [TMatNum]() atoms so a single matcher dispatches multiple constructor or NUM labels, mirroring HVM4's `λ{ #L1: h1; #L2: h2; ... }`.

## Details & Options

- Each `label -> handler` pair becomes one `TMatNum[label, handler, ...]`; pairs are folded right-to-left so the *first* (lowest-listed) label is the outermost test.
- When the applied arg is a `CTR` whose ext equals one of the labels, the corresponding handler is applied positionally to the constructor's children -- same semantics as [TMatCtr]().
- When the applied arg is a `NUM` whose value equals one of the labels, the corresponding handler is returned as-is.
- An arg matching none of the labels falls through to *fallback* via <code>[TApp]()[*fallback*, *arg*]</code>.

## Basic Examples

NUM dispatch over three labels:

```wl
TReset[];
mc = TMatChain[<|0 -> TNum[10], 1 -> TNum[20], 2 -> TNum[30]|>, TLam[k, TNum[99]]];
TTermVal @ TWnf @ TApp[mc, TNum[1]]
```
<!-- => 20 -->

## Properties and Relations

For a two-arm `0 / not-0` test prefer [TIfZero](); for a single constructor label use [TMatCtr](). `TMatChain` is what you reach for when modelling a small ADT-like case tree.
