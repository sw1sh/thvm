---
Template: Symbol
Name: TTermSubexprs
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermSubexprs
Keywords: [subexpression, path, walk, enumerate, locator]
SeeAlso: [TSubexprAt, TTermExpr, TTermTree, Term]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermSubexprs]()[*term*]</code> returns a `List` of `path -> TTerm` rules covering every position reachable from *term*, pre-order DFS. *path* is a `List` of integer heap-offset hops (the root has empty path).

## Details & Options

- Sibling of [TTermExpr]() / [TTermTree](): same traversal, different output shape. Pairs each position locator with the *live subterm* (as a fresh `TTerm`) so callers can substitute or compare at specific positions.
- Companion to [TSubexprAt](): use this to enumerate positions, then [TSubexprAt]() to fetch a single one by path.

## Basic Examples

A small lambda body has a handful of sub-positions:

```wl
TReset[];
Length @ TTermSubexprs[TLam[x, TOp2["+", x, TNum[1]]]]
```
<!-- => 4 -->

## Properties and Relations

[TSubexprAt]() is the spot-fetch variant. For an inert-tree projection use [TTermTree](); for the canonical form use [Term]().
