---
Template: Symbol
Name: TEra
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TEra
Keywords: [eraser, prune, superposition, multicomputation, interaction combinator]
SeeAlso: [TCollapse, TSup, TDup, TAny, TIfZero]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TEra]()[]</code> builds the eraser, a term that discards what it interacts with.

<!-- #| annotation: 26.10.19: TEra packs a TAG_ERA word (tag 3, no heap cell). The rules that fire on it are APP-ERA, DUP-ERA, DSU-ERA, DDU-ERA and the EQL-ERA short-circuit (src/interact/*_era.c); there is no OP2-ERA or MAT-ERA rule, so arithmetic on an eraser stays stuck and a matcher sends it to its fallback. The collapse walk drops an ERA branch, which is what makes ERA the pruning primitive of the multicomputation layer. TEra[] + 3 does not reach TOp2: an ERA is not a numeric term to the arithmetic UpValues, and the expression falls through to the tensor UpValues. -->

## Details & Options

- The eraser is an atom in normal form: <code>[TWnf]()</code> leaves it unchanged, and its canonical form is `Term["ERA"]`.
- Applying the eraser to an argument gives the eraser, duplicating it gives two erasers, and comparing it with <code>[TEql]()</code> gives the eraser.
- A lambda whose body is the eraser discards its argument.
- <code>[TCollapse]()</code> drops a branch that reduces to the eraser, so a branch can be pruned from a superposition by reducing it to <code>[TEra]()[]</code>.
- As a child of a constructor the eraser stays in place.
- Arithmetic with <code>[TOp2]()</code> on the eraser is not reduced, so an eraser branch under arithmetic is not pruned, and a matcher applied to the eraser takes its fallback.

## Basic Examples

The eraser:

```wl
TEra[]
```
<!-- => a TTerm summary box with tag ERA -->

Its canonical form is the tag alone:

```wl
Term[TEra[]]
```
<!-- => Term["ERA"] -->

## Scope

The eraser is in normal form:

```wl
Term[TWnf[TEra[]]]
```
<!-- => Term["ERA"] -->

---

Applying the eraser gives the eraser:

```wl
Term[TWnf[TApp[TEra[], TNum[3]]]]
```
<!-- => Term["ERA"] -->

---

Duplicating the eraser gives two erasers:

```wl
Term /@ TWnf /@ TDup[TEra[]]
```
<!-- => {Term["ERA"], Term["ERA"]} -->

---

A lambda with the eraser as its body discards the argument:

```wl
Term[TWnf[TLam[x, TEra[]][TNum[5]]]]
```
<!-- => Term["ERA"] -->

---

A comparison with the eraser gives the eraser:

```wl
Term[TWnf[TEql[TEra[], TNum[2]]]]
```
<!-- => Term["ERA"] -->

---

An eraser branch of a superposition contributes no leaf:

```wl
TTermVal /@ TCollapse[TSup[TNum[5], TEra[]]]
```
<!-- => {5} -->

---

The eraser alone collapses to no leaves at all:

```wl
TCollapse[TEra[]]
```
<!-- => {} -->

---

In a constructor the eraser stays as a child:

```wl
Term[TCtr[1, TEra[]]]
```
<!-- => Term["CTR", 1, Term["ERA"]] -->

## Applications

Keep the even branches of a superposition by erasing the others:

```wl
TTermVal /@ TCollapse[TLam[x, TIfZero[TOp2["%", x, 2], x, TEra[]]][TSup[{1, 2, 3, 4, 5, 6}]]]
```
<!-- => {2, 4, 6} -->

---

Search a superposition of candidates for the one whose square is 16:

```wl
TTermVal /@ TCollapse[TLam[a, TIfZero[TOp2["==", a * a, 16], TEra[], a]][TSup[{1, 2, 3, 4, 5}]]]
```
<!-- => {4} -->

## Properties and Relations

An eraser among the functions of a superposition prunes that branch when it is applied:

```wl
Term /@ TCollapse[TApp[TSup[TLam[x, x + 1], TEra[]], TNum[1]]]
```
<!-- => {Term["NUM", 2]} -->

## Possible Issues

Arithmetic on the eraser is not reduced:

```wl
Term[TWnf[TOp2["+", TEra[], TNum[3]]]]
```
<!-- => Term["OP2", "+", Term["ERA"], Term["NUM", 3]] -->

---

So an eraser branch under arithmetic is not pruned, and it collapses to an unreduced leaf:

```wl
Term /@ TCollapse[TOp2["+", TSup[TNum[5], TEra[]], TNum[1]]]
```
<!-- => {Term["NUM", 6], Term["OP2", "+", Term["ERA"], Term["NUM", 1]]} -->

---

A matcher applied to the eraser takes its fallback, here the constant 2:

```wl
TTermVal[TWnf[TApp[TMatNum[0, TNum[1], TLam[k, TNum[2]]], TEra[]]]]
```
<!-- => 2 -->
