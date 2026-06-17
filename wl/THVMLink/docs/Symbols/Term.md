---
Template: Symbol
Name: Term
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/Term
Keywords: [canonical, term, walk, snapshot, multiway, vertex identity]
SeeAlso: [TTermExpr, TTermTree, TInitialize, TContextSnapshot, TMultiwayGraph]
RelatedGuides: [THVMLink]
---

## Usage

<code>[Term]()[*t*]</code> walks the live heap from the `TTerm` *t* and returns a fully unrolled nested `Term[head, args...]` expression -- the **structural canonical form** (LAMs / DUPs carry a binder id so VAR / DP references resolve back).

<code>[Term]()[*tag*, *ext*, *val*]</code> / <code>[Term]()[*tag*, *ext*, *val*, *sub*]</code> is the *snapshot cell* form used inside `TContext["Cells"]`. *tag* is a `String` (`"LAM"`, `"APP"`, `"SUP"`, ...), *val* is a heap loc, *ext* is the secondary field (op code, dtype, label, ...). The 4-arg form also stores the `SUB` flag.

## Details & Options

- The walk is recursive: each compound's children unroll, then their children, ..., so the result is a serialisable tree of strings + integers.
- Used as **vertex identity** in [TMultiwayGraph](): two terms with the same canonical form compare equal via `===` regardless of heap layout.
- The snapshot form is the cell-level view used by [TContextSnapshot]() -- a hand-authored `Term[<|"tag", "ext", "val", "sub"|>]` is accepted and normalised to the positional form.

## Basic Examples

The canonical form of a simple OP2 graph:

```wl
TReset[];
Term[TOp2["+", TNum[3], TNum[4]]]
```
<!-- => Term["OP2", "+", Term["NUM", 3], Term["NUM", 4]] -->

A lambda surfaces its body with the binder loc as the LAM's first slot:

```wl
TReset[];
Term[TLam[x, TOp2["+", x, TNum[1]]]]
```
<!-- => Term["LAM", 0, Term["OP2", "+", Term["VAR", 0], Term["NUM", 1]]] -->

## Properties and Relations

[TTermExpr]() is the *tag-only* projection of the same walk (just heads, no canonical wrapper). For visual inspection use [TTermTree](). For the cell-level snapshot built on the multi-arg `Term` form see [TContextSnapshot]() and [TInitialize]().
