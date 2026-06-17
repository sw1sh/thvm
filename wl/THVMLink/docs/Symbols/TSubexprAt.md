---
Template: Symbol
Name: TSubexprAt
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSubexprAt
Keywords: [subexpression, path, locator, navigate]
SeeAlso: [TTermSubexprs, Term, TTermExpr]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TSubexprAt]()[*term*, *path*]</code> navigates *term* along *path* (a `List` of integer heap-offset hops) and returns the subterm as a `TTerm`. Returns <code>[Missing]()["OutOfBounds", path]</code> when the route does not fit the term shape.

## Details & Options

- *path* is the same kind of locator [TTermSubexprs]() enumerates: pre-order DFS, root = `{}`.
- Spot-fetch variant of [TTermSubexprs](); use this when you already know which position you want.

## Basic Examples

Reach into a small lambda's OP2 NUM:

```wl
TReset[];
Term @ TSubexprAt[TLam[x, TOp2["+", x, TNum[1]]], {0, 1}]
```
<!-- => Term["NUM", 1] -->

## Properties and Relations

[TTermSubexprs]() enumerates every (path, TTerm) pair under a root.
