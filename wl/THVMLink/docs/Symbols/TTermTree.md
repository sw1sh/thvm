---
Template: Symbol
Name: TTermTree
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermTree
Keywords: [tree, term, visual, ExpressionTree]
SeeAlso: [TTermExpr, Term, THeapDiagram]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermTree]()[*term*]</code> = <code>[ExpressionTree]()[[TTermExpr]()[*term*]]</code> -- the tag-name walk of *term* rendered as a Wolfram [Tree]() object for visual inspection.

## Details & Options

- Renders inline in a notebook with full `Tree` styling.
- Compare with [THeapDiagram]() (string-diagram view with named ports) and [THeapGraph]() (cell-and-wire `Graph`).

## Basic Examples

A small lambda body as a tree:

```wl
TReset[];
TTermTree[TLam[x, TOp2["+", x, TNum[1]]]]
```

## Properties and Relations

For the canonical structural form (binder ids preserved, suitable as vertex identity in [TMultiwayGraph]()) use [Term](). For the raw tag-only walk use [TTermExpr]().
