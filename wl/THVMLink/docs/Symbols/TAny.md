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

<code>[TAny]()[]</code> stands for the wildcard term `ANY`, which structural equality treats as equal to every term.

<!-- #| annotation: 26.10.19: TAny has no definitions in this version. The C reducer knows TAG_ANY (tag 18: the EQL-ANY short-circuit in src/term/eq.c and the DUP-ANY rule in src/interact/dup_any.c), but no WL constructor packs it, so TInitialize[Term["ANY"]] gives the inert TAny[]. Writing the raw word 18*2^56 with THeapSet and reading it back with THeapRead produces a working ANY term; that is how the Details below were verified. A one-line TAny[] := packTerm[0, $TagANY, 0, 0] next to TEra in THVMLink.wl would let this page demonstrate them directly. -->

## Details & Options

- An `ANY` term is a wildcard for structural equality: a <code>[TEql]()</code> with `ANY` on either side reduces to <code>[TNum]()[1]</code>, whatever the other side is.
- The comparison does not reduce the other side, so `ANY` is equal to a superposition once, not once per branch.
- Duplicating an `ANY` gives two `ANY` terms, and it takes part in no other interaction: an application or arithmetic on it stays as it is.
- The canonical form of an `ANY` term is `Term["ANY"]`, and <code>[TInitialize]()</code> gives <code>[TAny]()[]</code> for it.
- In this version <code>[TAny]()[]</code> has no definition and returns unevaluated, so it does not build an `ANY` term on the heap.

## Basic Examples

[TAny]() with no arguments returns unevaluated:

```wl
TAny[]
```
<!-- => TAny[] -->

---

Rebuilding the canonical form of an `ANY` term gives the same expression:

```wl
TInitialize[Term["ANY"]]
```
<!-- => TAny[] -->

## Properties and Relations

The eraser atom, by contrast, has a constructor, and its canonical form is `Term["ERA"]`:

```wl
Term[TEra[]]
```
<!-- => Term["ERA"] -->
