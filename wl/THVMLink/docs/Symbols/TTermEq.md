---
Template: Symbol
Name: TTermEq
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermEq
Keywords: [equality, term, cnf, alpha]
SeeAlso: [TTermSame, TEql, Term, TCnf]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermEq]()[*a*, *b*]</code> returns [True]() iff *a* and *b* `cnf`-reduce to structurally equal terms (modulo `VAR` alpha-aliasing), [False]() otherwise.

## Details & Options

- Drives both sides through `cnf` ([TCnf]()) so DP-rooted projections fire and `SUP` heads lift before the comparison; therefore expensive but robust.
- For the *no-reduction* variant on already-CNF'd terms use [TTermSame]().
- For an equality *term* (a value reducible inside the heap, e.g. as a theorem-proving primitive) use [TEql]().

## Basic Examples

Two equal beta-reducible forms compare True:

```wl
TReset[];
TTermEq[TApp[TLam[x, x], TNum[7]], TNum[7]]
```
<!-- => True -->

## Properties and Relations

`TTermEq` is the user-facing host-side equality used in tests and assertions; `TEql` is the heap-side equality used inside proof obligations.
