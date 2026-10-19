---
Template: Symbol
Name: TCollapse
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TCollapse
Keywords: [collapse, superposition, enumerate, leaves, multicomputation, interaction combinator]
SeeAlso: [TSup, TCnf, TEra, TDup, TMultiTrace, TMultiwayGraph]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TCollapse]()[*t*]</code> gives the list of leaves of the superposition tree of the term *t*.

<code>[TCollapse]()[*t*, *n*]</code> gives at most *n* leaves.

<!-- #| annotation: 26.10.19: TCollapse wraps thvm_collapse (src/collapse/_.c), a recursive walk that cnfs the current term, recurses into both cells of a SUP, drops an ERA and emits anything else, filling a C buffer of cap entries; the LibraryLink entry maps cap 0 or cap > 2^20 to the default 65536. Using cnf rather than wnf at each step is what drives DP-rooted branches through their duplications, and it is why a shared variable collapses to the diagonal. Prior art: HVM4's collapse readback; in the multicomputation reading it is the observer that reads out the leaves of a multiway evolution. -->

## Details & Options

- [TCollapse]() reduces *t* to collapsed normal form, as <code>[TCnf]()</code> does. A superposition at the top is split into its two branches, which are collapsed in turn; the eraser <code>[TEra]()[]</code> contributes no leaf; any other term is a leaf.
- The result is a [List]() of [TTerm]() leaves, in the order of the superposition tree. <code>[TTermVal]()</code> or <code>[Term]()</code> reads them.
- A value shared between two uses, through a lambda variable or a duplication, takes the same branch in both uses, so squaring a superposition of 5 and 7 gives 25 and 49 and not the products 35.
- Two independent superpositions give every combination of their branches.
- The default maximum is 65536 leaves. A maximum *n* of 0, or of more than $2^{20}$, uses the default.

## Basic Examples

Collapse a superposition of 1 and 2 with 3 added:

```wl
TCollapse[TSup[1, 2] + 3]
```
<!-- => {TNum summary box 4, TNum summary box 5} -->

Read the leaves as numbers:

```wl
TTermVal /@ TCollapse[TSup[1, 2] + 3]
```
<!-- => {4, 5} -->

## Scope

A superposition of a list collapses to the elements of the list:

```wl
TTermVal /@ TCollapse[TSup[{5, 7, 9}]]
```
<!-- => {5, 7, 9} -->

---

Give at most three leaves:

```wl
TTermVal /@ TCollapse[TSup[{1, 2, 3, 4, 5, 6}], 3]
```
<!-- => {1, 2, 3} -->

---

Two independent superpositions give every combination:

```wl
TTermVal /@ TCollapse[TSup[1, 2] + TSup[10, 20]]
```
<!-- => {11, 21, 12, 22} -->

---

Branches that are erasers contribute no leaves:

```wl
TTermVal /@ TCollapse[TSup[{1, TEra[], 3, TEra[]}]]
```
<!-- => {1, 3} -->

---

A term without a superposition is its own single leaf:

```wl
TTermVal /@ TCollapse[TNum[5]]
```
<!-- => {5} -->

---

The eraser alone has no leaves:

```wl
TCollapse[TEra[]]
```
<!-- => {} -->

---

A superposition inside a constructor gives one constructor per branch:

```wl
Term /@ TCollapse[TCtr[1, TSup[1, 2], TNum[3]]]
```
<!-- => {Term["CTR", 1, Term["NUM", 1], Term["NUM", 3]], Term["CTR", 1, Term["NUM", 2], Term["NUM", 3]]} -->

---

A superposition of two functions applied to 10 gives one result per function:

```wl
TTermVal /@ TCollapse[TApp[TSup[TLam[x, x + 1], TLam[x, 2 x]], 10]]
```
<!-- => {11, 20} -->

## Properties and Relations

A variable used twice takes the same branch in both uses:

```wl
TTermVal /@ TCollapse[TLam[x, x * x][TSup[5, 7]]]
```
<!-- => {25, 49} -->

---

The number of leaves of independent superpositions is the product of their branch counts:

```wl
Length[TCollapse[TSup[{1, 2, 3}] + TSup[{10, 20, 30}] + TSup[{100, 200, 300}]]]
```
<!-- => 27 -->

---

<code>[TCnf]()</code> performs the first step of the walk, lifting the superposition to the top:

```wl
Term[TCnf[TSup[1, 2] + 3]]
```
<!-- => Term["SUP", L, Term["OP2", "+", Term["NUM", 1], Term["DP0", L, Term["NUM", 3]]], Term["OP2", "+", Term["NUM", 2], Term["DP1", L, Term["NUM", 3]]]] -->

## Possible Issues

A maximum of 0 does not mean no leaves; it means the default maximum:

```wl
Length[TCollapse[TSup[{1, 2, 3}], 0]]
```
<!-- => 3 -->
