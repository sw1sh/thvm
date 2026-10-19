---
Template: Symbol
Name: TAny
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAny
Keywords: [any, wildcard, structural equality, interaction combinator]
SeeAlso: [TEql, TEra, TInitialize, Term]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAny]()[]</code> gives the wildcard term `ANY`, which structural equality treats as equal to every term.

## Details & Options

- An `ANY` term is an atom of the heap, with tag 18 and no value.
- `ANY` is a wildcard for structural equality: a <code>[TEql]()</code> with `ANY` on either side reduces to the number 1, true, whatever the other side is.
- The comparison does not reduce the other side, so `ANY` is equal to a superposition once, not once per branch.
- Duplicating an `ANY` gives two `ANY` terms, and it takes part in no other interaction: an application of it, or an operation of <code>[TOp2]()</code> on it, stays as it is.
- The canonical form of an `ANY` term is `Term["ANY"]`, and <code>[TInitialize]()</code> rebuilds an `ANY` term from it.

## Basic Examples

The wildcard term:

```wl
TAny[]
```
<!-- => a TTerm summary box showing tag ANY -->

---

Its canonical form:

```wl
Term[TAny[]]
```
<!-- => Term["ANY"] -->

## Scope

`ANY` is equal to a number:

```wl
TTermVal[TWnf[TEql[TAny[], TNum[3]]]]
```
<!-- => 1 -->

---

`ANY` is equal to a lambda, on either side of the comparison:

```wl
TTermVal[TWnf[TEql[TLam[x, x], TAny[]]]]
```
<!-- => 1 -->

---

A superposition is compared once, not once for each branch:

```wl
TTermVal /@ TCollapse[TEql[TAny[], TSup[1, 2]]]
```
<!-- => {1} -->

---

Each copy of a duplicated `ANY` is `ANY`:

```wl
Term[TWnf[First[TDup[TAny[]]]]]
```
<!-- => Term["ANY"] -->

---

An application of `ANY` stays as it is:

```wl
Term[TWnf[TApp[TAny[], TNum[3]]]]
```
<!-- => Term["APP", Term["ANY"], Term["NUM", 3]] -->

---

So does an integer operation on it:

```wl
Term[TWnf[TOp2["+", TAny[], TNum[3]]]]
```
<!-- => Term["OP2", "+", Term["ANY"], Term["NUM", 3]] -->

## Properties and Relations

<code>[TInitialize]()</code> rebuilds an `ANY` term from its canonical form:

```wl
Term[TInitialize[Term["ANY"]]]
```
<!-- => Term["ANY"] -->

---

The tag of an `ANY` term is 18:

```wl
TTermTag[TAny[]]
```
<!-- => 18 -->

---

A superposition compared with one of its own branches is equal in one branch only, unlike with `ANY`:

```wl
TTermVal /@ TCollapse[TEql[TNum[1], TSup[1, 2]]]
```
<!-- => {1, 0} -->

---

The eraser is the other atom with no value; its canonical form is `Term["ERA"]`:

```wl
Term[TEra[]]
```
<!-- => Term["ERA"] -->
