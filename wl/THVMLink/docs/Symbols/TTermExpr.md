---
Template: Symbol
Name: TTermExpr
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermExpr
Keywords: [tree, expr, walk, tags, diff, snapshot]
SeeAlso: [TTermTree, Term, TTermSubexprs, TSubexprAt]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermExpr]()[*term*]</code> walks the heap from *term* and returns a nested expression whose heads are tag-name `String`s: `"LAM"`, `"APP"`, `"SUP"`, `"DUP"`, `"DP0"`, `"DP1"`, `"VAR"`, `"ERA"`, ...

## Details & Options

- Useful for snapshotting / diffing the pre- and post-reduction states by direct `===` equality (the heads are atoms, not heap-loc-tagged structures, so two structurally equal terms compare equal).
- Compare with [Term](), which carries binder ids on `LAM` / `DUP` and is the canonical form used as vertex identity in [TMultiwayGraph](). `TTermExpr` is the lighter, tag-only projection.

## Basic Examples

A LAM body comes through with tag-name heads:

```wl
TReset[];
TTermExpr[TLam[x, TOp2["+", x, TNum[1]]]]
```
<!-- => "LAM"["OP2"["+", "VAR"[0], "NUM"[1]]] -->

## Properties and Relations

[TTermTree]() wraps `TTermExpr` in a [Tree]() for visual rendering. `TTermSubexprs` enumerates `path -> TTerm` rules so a position-locator can navigate into the live term.
