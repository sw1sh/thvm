---
Template: Symbol
Name: TDup
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TDup
Keywords: [duplication, projections, sharing, superposition, label, interaction combinator]
SeeAlso: [TSup, TCollapse, TCnf, TFreshLabel, TDdu, TLam]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TDup]()[*v*]</code> duplicates the term *v* under a fresh label and gives the list of its two projections.

<code>[TDup]()[*label*, *v*]</code> duplicates *v* under the integer *label*.

<code>[TDup]()[*v*, *f*]</code> applies *f* to the two projections of a duplication of *v*.

<code>[TDup]()[*label*, *v*, *f*]</code> applies *f* to the two projections of a duplication of *v* under *label*.

<code>[TDup]()[{*p*, *q*}]</code> gives the duplication node that the projections *p* and *q* belong to.

<!-- #| annotation: 26.10.19: TDup allocates one heap cell holding v and gives DP0/DP1 terms that share it (ext = label, val = the cell); the copying is lazy and fires as DUP-NUM, DUP-LAM, DUP-CTR, DUP-SUP (annihilate on equal labels, commute otherwise), DUP-ERA and the other src/interact/dup_*.c rules when a projection is driven, which happens during cnf readback. The {p, q} form only retags a matching DP0/DP1 pair to DUP, without touching the heap, and gives $Failed for a non-matching pair. Auto-DUP labels for multi-use lambda binders come from 0x10000 upward (src/lam/auto_dup.c), apart from the TFreshLabel range. Projections of a tensor are numeric terms to the arithmetic UpValues, so p + q over tensor projections builds an OP2, not a UOp. Prior art: the duplicator agent of interaction combinators and HVM4's labelled DUP. -->

## Details & Options

- A duplication shares *v* between two uses. Its projections, with tags `DP0` and `DP1`, each stand for a copy of *v*, and the copying takes place when a projection is reduced.
- The fresh label is drawn from <code>[TFreshLabel]()</code>. A bare integer *v* is read as <code>[TNum]()[*v*]</code>.
- A number is copied as it is, a constructor or a lambda is copied together with its body, and the eraser gives two erasers.
- When *v* is a superposition with the same label as the duplication, the first projection takes the first branch and the second projection the second branch.
- When *v* is a superposition with another label, each projection is a copy of the whole superposition, and <code>[TCollapse]()</code> takes the same branch in both copies.
- In <code>[TDup]()[*v*, *f*]</code>, *f* is any function of two arguments, such as a pure [Function]().
- <code>[TDup]()[{*p*, *q*}]</code> requires the two projections of one duplication, in order, and gives `$Failed` for any other pair.
- A lambda whose variable occurs more than once in its body duplicates its argument automatically, with labels from a range separate from <code>[TFreshLabel]()</code>.

## Basic Examples

Duplicate the number 7:

```wl
TDup[TNum[7]]
```
<!-- => {a TTerm summary box with tag DP0, a TTerm summary box with tag DP1} -->

The two projections share one copy of the value and carry the label of the duplication:

```wl
Term /@ TDup[TNum[7]]
```
<!-- => {Term["DP0", L, Term["NUM", 7]], Term["DP1", L, Term["NUM", 7]]}, with L the fresh label -->

Each projection reduces to the value:

```wl
Term /@ TWnf /@ TDup[TNum[7]]
```
<!-- => {Term["NUM", 7], Term["NUM", 7]} -->

## Scope

Duplicate under an explicit label:

```wl
Term /@ TDup[3, TNum[7]]
```
<!-- => {Term["DP0", 3, Term["NUM", 7]], Term["DP1", 3, Term["NUM", 7]]} -->

---

Pass the projections to a function, here addition:

```wl
TWnf[TDup[TNum[7], #1 + #2 &]]
```
<!-- => a TNum summary box showing the number 14 -->

---

Use an explicit label and a function together:

```wl
TTermVal[TWnf[TDup[9, TNum[7], #1 * #2 &]]]
```
<!-- => 49 -->

---

A bare integer is read as a number term:

```wl
Term /@ TWnf /@ TDup[5]
```
<!-- => {Term["NUM", 5], Term["NUM", 5]} -->

---

A duplicated function can be applied to two different arguments:

```wl
TTermVal /@ TWnf /@ TDup[TLam[x, x + 1], {TApp[#1, TNum[1]], TApp[#2, TNum[10]]} &]
```
<!-- => {2, 11} -->

---

A constructor is copied with its children:

```wl
Term /@ TCnf /@ TDup[TCtr[2, TNum[1], TNum[2]]]
```
<!-- => {Term["CTR", 2, Term["NUM", 1], Term["NUM", 2]], Term["CTR", 2, Term["NUM", 1], Term["NUM", 2]]} -->

---

The eraser is copied to two erasers:

```wl
Term /@ TWnf /@ TDup[TEra[]]
```
<!-- => {Term["ERA"], Term["ERA"]} -->

---

The pair of projections determines its duplication node:

```wl
pq = TDup[3, TNum[4]]
```
<!-- => {a TTerm summary box with tag DP0, a TTerm summary box with tag DP1} -->

Recover the node from the pair:

```wl
Term[TDup[pq]]
```
<!-- => Term["DUP", loc, Term["NUM", 4]], with loc the shared cell -->

## Properties and Relations

With the label of the superposition, the first projection takes the first branch:

```wl
Term[TCnf[First[TDup[7, TSup[7, 1, 2]]]]]
```
<!-- => Term["NUM", 1] -->

---

The second projection takes the second branch:

```wl
Term[TCnf[Last[TDup[7, TSup[7, 1, 2]]]]]
```
<!-- => Term["NUM", 2] -->

---

So their sum is 1 + 2:

```wl
TTermVal /@ TCollapse[Plus @@ TDup[7, TSup[7, 1, 2]]]
```
<!-- => {3} -->

---

With another label each projection copies the superposition, and the sum takes the same branch twice:

```wl
TTermVal /@ TCollapse[Plus @@ TDup[8, TSup[7, 1, 2]]]
```
<!-- => {2, 4} -->

---

A lambda that uses its variable twice holds an automatic duplication of it:

```wl
Term[TLam[x, x * x]]
```
<!-- => Term["LAM", id, Term["OP2", "*", Term["DP0", L, Term["VAR", id]], Term["DP1", L, Term["VAR", id]]]], with L at least 65536 -->

## Possible Issues

Projections of two different duplications do not form a pair:

```wl
TDup[{First[TDup[TNum[1]]], Last[TDup[TNum[2]]]}]
```
<!-- => $Failed -->
