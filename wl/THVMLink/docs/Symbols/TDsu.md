---
Template: Symbol
Name: TDsu
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TDsu
Keywords: [dynamic label, superposition, label, interaction combinator]
SeeAlso: [TSup, TDdu, TDup, TFreshLabel, TCollapse]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TDsu]()[*label*, *a*, *b*]</code> builds a superposition of the terms *a* and *b* whose label is the value of the term *label*, computed during reduction.

<!-- #| annotation: 26.10.19: TDsu is HVM4's DSU (dynamic superposition), built by thvm_wl_term_new_dsu as a three-cell node [label, a, b]. wnf forces the label strictly first and then fires DSU-NUM (rewrite to SUP^n), DSU-ERA or DSU-SUP (src/interact/dsu_num.c, dsu_era.c, dsu_sup.c); any other label head leaves the DSU stuck. The DownValue requires three TTerms, so the bare-Integer lifting of the other constructors does not apply. Term[] prints a DSU as its tag, location and ext only. -->

## Details & Options

- A dynamic-label superposition (DSU) first reduces *label*. When the label is a number <code>[TNum]()[*n*]</code>, the DSU becomes the superposition <code>[TSup]()[*n*, *a*, *b*]</code>.
- When the label reduces to the eraser <code>[TEra]()[]</code>, the DSU reduces to the eraser.
- When the label reduces to a superposition, the DSU becomes a superposition of two DSUs, one for each branch of the label.
- A label that reduces to anything else, such as a lambda, leaves the DSU unreduced.
- The label decides how the superposition meets a duplication: a <code>[TDup]()</code> with the same label takes one branch per projection, and one with a different label copies both branches.
- All three arguments are [TTerm]() objects; a number is written <code>[TNum]()[*n*]</code>.
- The canonical form of an unreduced DSU, `Term["DSU", loc, ext]`, does not show its arguments.

## Basic Examples

A superposition of 10 and 20 whose label is the number 3:

```wl
d = TDsu[TNum[3], TNum[10], TNum[20]]
```
<!-- => a TTerm summary box with tag DSU -->

Reducing it resolves the label and gives an ordinary superposition with label 3:

```wl
Term[TWnf[d]]
```
<!-- => Term["SUP", 3, Term["NUM", 10], Term["NUM", 20]] -->

## Scope

The label may be any term that reduces to a number:

```wl
Term[TWnf[TDsu[TNum[1] + 2, TNum[10], TNum[20]]]]
```
<!-- => Term["SUP", 3, Term["NUM", 10], Term["NUM", 20]] -->

---

Collapse a dynamic-label superposition to its branches:

```wl
TTermVal /@ TCollapse[TDsu[TNum[3], TNum[10], TNum[20]]]
```
<!-- => {10, 20} -->

---

An eraser as the label erases the superposition:

```wl
Term[TWnf[TDsu[TEra[], TNum[10], TNum[20]]]]
```
<!-- => Term["ERA"] -->

---

A superposed label gives one superposition for each of its branches:

```wl
TTermVal /@ TCollapse[TDsu[TSup[1, 2], TNum[10], TNum[20]]]
```
<!-- => {10, 20, 10, 20} -->

## Properties and Relations

Once its label is a number, a dynamic-label superposition is the same as <code>[TSup]()</code> with that label:

```wl
Term[TSup[3, TNum[10], TNum[20]]]
```
<!-- => Term["SUP", 3, Term["NUM", 10], Term["NUM", 20]] -->

---

A duplication with the resolved label takes one branch per projection, so adding the projections gives 10 + 20:

```wl
TTermVal /@ TCollapse[Plus @@ TDup[3, TDsu[TNum[3], TNum[10], TNum[20]]]]
```
<!-- => {30} -->

---

A duplication with another label copies the superposition, and the copies take the same branch:

```wl
TTermVal /@ TCollapse[Plus @@ TDup[4, TDsu[TNum[3], TNum[10], TNum[20]]]]
```
<!-- => {20, 40} -->

## Possible Issues

Bare integers are not read as numbers, and [TDsu]() with them returns unevaluated:

```wl
TDsu[3, 10, 20]
```
<!-- => TDsu[3, 10, 20] -->

---

A label that does not reduce to a number, an eraser or a superposition leaves the term unreduced:

```wl
Term[TWnf[TDsu[TLam[x, x], TNum[10], TNum[20]]]]
```
<!-- => Term["DSU", loc, 0], with loc a heap location -->
