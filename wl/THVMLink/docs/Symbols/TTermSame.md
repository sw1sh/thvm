---
Template: Symbol
Name: TTermSame
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermSame
Keywords: [equality, term, structural, no reduction]
SeeAlso: [TTermEq, Term, TCnf]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermSame]()[*a*, *b*]</code> returns [True]() iff *a* and *b* are structurally equal **without further reduction**.

## Details & Options

- Compares the packed `Term`'s tag / ext / val and recurses into the children. Same-loc compounds are trivially equal.
- Cheap; use when both inputs are already CNF (or you intentionally want to compare the un-reduced shape). For general equality drive both sides through [TCnf]() first, or call [TTermEq]() which does it for you.

## Basic Examples

A `TTerm` is structurally equal to itself without reduction:

```wl
TReset[];
t = TNum[42];
TTermSame[t, t]
```
<!-- => True -->

## Properties and Relations

[TTermEq]() is the reducing variant. Compare with [Term]() (the structural canonical-form walk) which yields a plain expression `===`-comparable across processes.
