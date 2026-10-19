---
Template: Symbol
Name: TDdu
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TDdu
Keywords: [dynamic label, duplication, label, interaction combinator]
SeeAlso: [TDup, TDsu, TSup, TFreshLabel, TCollapse]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TDdu]()[*label*, *v*, *f*]</code> duplicates the term *v* under the label given by the term *label*, computed during reduction, and applies the two-argument function *f* to the two copies.

<!-- #| annotation: 26.10.19: TDdu is HVM4's DDU (dynamic duplication), the DUP-side twin of DSU, built by thvm_wl_term_new_ddu as a three-cell node [label, val, body]. wnf forces the label strictly first and then fires DDU-NUM (rewrite to body applied to the DP0/DP1 projections of DUP^n on val), DDU-ERA or DDU-SUP (src/interact/ddu_num.c, ddu_era.c, ddu_sup.c). The DownValue requires three TTerms. Term[] prints a DDU as its tag, location and ext only. -->

## Details & Options

- A dynamic-label duplication (DDU) first reduces *label*. When the label is a number <code>[TNum]()[*n*]</code>, the DDU becomes *f* applied to the two projections of <code>[TDup]()[*n*, *v*]</code>.
- *f* is a curried function of two arguments, such as <code>[TLam]()[*a*, [TLam]()[*b*, *body*]]</code>.
- When the label reduces to the eraser <code>[TEra]()[]</code>, the DDU reduces to the eraser, and a superposed label gives a superposition of two DDUs, one for each branch of the label.
- The label decides how the copies meet a superposition in *v*: with the same label the two copies take one branch each, and with a different label each copy keeps both branches.
- All three arguments are [TTerm]() objects; a number is written <code>[TNum]()[*n*]</code>.
- The canonical form of an unreduced DDU, `Term["DDU", loc, ext]`, does not show its arguments.

## Basic Examples

Duplicate 7 under the label 3 and multiply the two copies:

```wl
d = TDdu[TNum[3], TNum[7], TLam[a, TLam[b, a * b]]]
```
<!-- => a TTerm summary box with tag DDU -->

Reducing it resolves the label, duplicates the value and applies the function:

```wl
TWnf[d]
```
<!-- => a TNum summary box showing the number 49 -->

## Scope

The label may be any term that reduces to a number:

```wl
TTermVal[TWnf[TDdu[TNum[1] + 2, TNum[7], TLam[a, TLam[b, a + b]]]]]
```
<!-- => 14 -->

---

With the label of a superposition in the value, the two copies take one branch each, giving 2 times 5:

```wl
TTermVal /@ TCollapse[TDdu[TNum[3], TSup[3, 2, 5], TLam[a, TLam[b, a * b]]]]
```
<!-- => {10} -->

---

With a different label, both copies take the same branch, giving the squares:

```wl
TTermVal /@ TCollapse[TDdu[TNum[4], TSup[3, 2, 5], TLam[a, TLam[b, a * b]]]]
```
<!-- => {4, 25} -->

---

An eraser as the label erases the duplication:

```wl
Term[TWnf[TDdu[TEra[], TNum[7], TLam[a, TLam[b, a * b]]]]]
```
<!-- => Term["ERA"] -->

---

A superposed label gives one duplication for each of its branches:

```wl
TTermVal /@ TCollapse[TDdu[TSup[1, 2], TNum[7], TLam[a, TLam[b, a * b]]]]
```
<!-- => {49, 49} -->

---

The function may build any term from the copies, here a constructor:

```wl
Term[TCnf[TDdu[TNum[3], TNum[7], TLam[a, TLam[b, TCtr[0, a, b]]]]]]
```
<!-- => Term["CTR", 0, Term["NUM", 7], Term["NUM", 7]] -->

## Properties and Relations

Once its label is a number, a dynamic-label duplication is the continuation form of <code>[TDup]()</code> with that label:

```wl
TTermVal[TWnf[TDup[3, TNum[7], #1 * #2 &]]]
```
<!-- => 49 -->

---

<code>[TDsu]()</code> is the corresponding dynamic-label superposition:

```wl
Term[TWnf[TDsu[TNum[3], TNum[10], TNum[20]]]]
```
<!-- => Term["SUP", 3, Term["NUM", 10], Term["NUM", 20]] -->

## Possible Issues

Bare integers are not read as numbers, and [TDdu]() with them returns unevaluated:

```wl
TDdu[3, 7, TLam[a, TLam[b, a * b]]]
```
<!-- => TDdu[3, 7, TTerm[...]], unevaluated -->

---

A function that is not a lambda leaves the applications to the two copies unreduced:

```wl
Term[TWnf[TDdu[TNum[3], TNum[7], TNum[1]]]]
```
<!-- => Term["APP", Term["APP", Term["NUM", 1], Term["DP0", 3, Term["NUM", 7]]], Term["DP1", 3, Term["NUM", 7]]] -->
