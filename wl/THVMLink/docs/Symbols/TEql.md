---
Template: Symbol
Name: TEql
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TEql
Keywords: [equality, structural equality, comparison, superposition, interaction combinator]
SeeAlso: [TOp2, TTermEq, TIfZero, TSup, TAny]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TEql]()[*a*, *b*]</code> builds a term that reduces to 1 when the terms *a* and *b* are structurally equal and to 0 otherwise.

<!-- #| annotation: 26.10.19: TEql wraps thvm_wl_term_new_eql (TAG_EQL) and lifts bare Integers with numCoerce, since it bypasses heapWith. The reducer is strict on both sides; NUM-NUM compares the val field, EQL-SUP commutes by cloning the other side through a DUP (src/term/eq.c and the wnf EQL frame), ERA short-circuits to ERA and ANY to NUM(1). EQL-CTR, EQL-LAM and tensor equality are not implemented, so such a comparison stays an EQL cell. Prior art: HVM4's structural equality on terms; TOp2["=="] is the integer-only compare it generalizes. -->

## Details & Options

- Both sides are reduced before they are compared, and two numbers are compared by value. The result is <code>[TNum]()[1]</code> or <code>[TNum]()[0]</code>.
- A bare integer on either side is read as a <code>[TNum]()</code> term.
- A superposition on either side distributes the comparison over its branches, so the result is a superposition of ones and zeros.
- With the eraser <code>[TEra]()[]</code> on either side the result is the eraser, and with the wildcard term <code>[TAny]()[]</code> the result is 1.
- Constructors, lambdas and tensors are not compared in this version: such a comparison stays an unreduced term with tag `EQL`.
- The canonical form of an unreduced comparison, `Term["EQL", loc, 0]`, does not show its sides.
- <code>[TTermEq]()</code> compares two terms in the Wolfram Language and gives [True]() or [False]() directly.

## Basic Examples

Compare two equal numbers:

```wl
e = TEql[TNum[1], TNum[1]]
```
<!-- => a TTerm summary box with tag EQL -->

The comparison reduces to 1:

```wl
TWnf[e]
```
<!-- => a TNum summary box showing the number 1 -->

## Scope

Two different numbers compare to 0:

```wl
TTermVal[TWnf[TEql[TNum[1], TNum[2]]]]
```
<!-- => 0 -->

---

Bare integers are read as numbers:

```wl
TTermVal[TWnf[TEql[4, 4]]]
```
<!-- => 1 -->

---

Each side is reduced before the comparison:

```wl
TTermVal[TWnf[TEql[TNum[2] + 2, 4]]]
```
<!-- => 1 -->

---

A superposition compares branch by branch:

```wl
TTermVal /@ TCollapse[TEql[TSup[1, 2], 2]]
```
<!-- => {0, 1} -->

---

With superpositions on both sides every pair of branches is compared:

```wl
TTermVal /@ TCollapse[TEql[TSup[1, 2], TSup[1, 2]]]
```
<!-- => {1, 0, 0, 1} -->

---

A comparison in a lambda body tests each branch of a superposed argument:

```wl
TTermVal /@ TCollapse[TLam[x, TEql[x, 2]][TSup[{1, 2, 3}]]]
```
<!-- => {0, 1, 0} -->

---

The eraser on either side gives the eraser:

```wl
Term[TWnf[TEql[TEra[], TNum[2]]]]
```
<!-- => Term["ERA"] -->

---

The wildcard term on either side gives 1:

```wl
TTermVal[TWnf[TEql[TNum[2], TAny[]]]]
```
<!-- => 1 -->

## Properties and Relations

On numbers and superpositions the comparison agrees with the integer comparison <code>[TOp2]()["==", *a*, *b*]</code>:

```wl
TTermVal /@ TCollapse[TOp2["==", TSup[1, 2], 2]]
```
<!-- => {0, 1} -->

---

<code>[TTermEq]()</code> compares two terms directly:

```wl
TTermEq[TNum[2] + 2, TNum[4]]
```
<!-- => True -->

---

A comparison gives 0 or 1, so it can drive <code>[TIfZero]()</code>; 3 and 4 differ, so the first branch is taken:

```wl
TTermVal[TWnf[TIfZero[TEql[TNum[3], TNum[4]], TNum[10], TNum[20]]]]
```
<!-- => 10 -->

## Possible Issues

Constructors are not compared, and the comparison stays unreduced:

```wl
Term[TWnf[TEql[TCtr[1, TNum[2]], TCtr[1, TNum[2]]]]]
```
<!-- => Term["EQL", loc, 0], with loc a heap location -->

---

Lambdas are not compared either:

```wl
Term[TWnf[TEql[TLam[x, x], TLam[y, y]]]]
```
<!-- => Term["EQL", loc, 0], with loc a heap location -->
