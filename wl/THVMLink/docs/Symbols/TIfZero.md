---
Template: Symbol
Name: TIfZero
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TIfZero
Keywords: [conditional, zero test, branch, matcher, control flow]
SeeAlso: [TMatNum, TMatChain, TMatCtr, TOp2, TEql, TEra]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TIfZero]()[*n*, *then*, *else*]</code> builds a conditional term that reduces to *then* when the term *n* reduces to 0 and to *else* otherwise.

<!-- #| annotation: 26.10.19: TIfZero is sugar, defined in Switch.wl as TApp[TMatNum[0, then, TLam[ignored, else]], n]: the discarding lambda consumes the number that the MAT-MIS fallback passes on. The APP lifts a bare Integer counter through numCoerce, but TMatNum and TLam do not lift their arguments (TMatNum passes ttermRaw, TLam's 1-cell THeapSet writes a raw word), so bare Integer branches are spliced in as raw heap words and only look right by accident; the page always writes TNum[n]. A CTR with label 0 matches MAT 0 by the APP-MAT-CTR rule, so it counts as zero. -->

## Details & Options

- [TIfZero]() builds the application of a matcher, <code>[TApp]()[[TMatNum]()[0, *then*, [TLam]()[*k*, *else*]], *n*]</code>, in which *else* is wrapped in a lambda that discards the matched value.
- *n* is reduced first. The number 0 selects *then*, and any other number selects *else*.
- Only the selected branch is reduced; the other one is discarded.
- A superposition as *n* distributes the conditional over its branches.
- A bare integer *n* is read as a number term, but the branches are terms and a number branch is written <code>[TNum]()[*k*]</code>.
- A comparison such as <code>[TOp2]()["<", *a*, *b*]</code> or <code>[TEql]()[*a*, *b*]</code> gives 1 when it holds and 0 when it does not, so on a comparison *then* is the branch for a false comparison.

## Basic Examples

A conditional on the number 0:

```wl
c = TIfZero[TNum[0], TNum[10], TNum[20]]
```
<!-- => a TTerm summary box with tag APP -->

Zero selects the first branch:

```wl
TWnf[c]
```
<!-- => a TNum summary box showing the number 10 -->

## Scope

Any other number selects the second branch:

```wl
TTermVal[TWnf[TIfZero[TNum[5], TNum[10], TNum[20]]]]
```
<!-- => 20 -->

---

The test value is reduced first, here 3 - 3:

```wl
TTermVal[TWnf[TIfZero[TNum[3] - 3, TNum[10], TNum[20]]]]
```
<!-- => 10 -->

---

A bare integer test value is read as a number:

```wl
TTermVal[TWnf[TIfZero[0, TNum[10], TNum[20]]]]
```
<!-- => 10 -->

---

A superposed test value gives one result per branch:

```wl
TTermVal /@ TCollapse[TIfZero[TSup[TNum[0], TNum[3]], TNum[10], TNum[20]]]
```
<!-- => {10, 20} -->

---

A comparison that fails gives 0, so 5 < 3 selects the first branch:

```wl
TTermVal[TWnf[TIfZero[TOp2["<", TNum[5], TNum[3]], TNum[10], TNum[20]]]]
```
<!-- => 10 -->

---

In a lambda the conditional keeps the even branches of a superposition and erases the odd ones:

```wl
TTermVal /@ TCollapse[TLam[x, TIfZero[TOp2["%", x, 2], x, TEra[]]][TSup[{1, 2, 3, 4}]]]
```
<!-- => {2, 4} -->

## Properties and Relations

The conditional is an application of a matcher on 0:

```wl
Term[TIfZero[TNum[0], TNum[10], TNum[20]]]
```
<!-- => Term["APP", Term["MAT", 0, Term["NUM", 10], Term["LAM", id, Term["NUM", 20]]], Term["NUM", 0]] -->

---

The same matcher written with <code>[TMatNum]()</code> gives the same result:

```wl
TTermVal[TWnf[TApp[TMatNum[0, TNum[10], TLam[k, TNum[20]]], TNum[5]]]]
```
<!-- => 20 -->

---

Start from a reset runtime, so the interaction counter is at 0:

```wl
TReset[];
```

The first branch is selected, and the costly second branch is never reduced:

```wl
TTermVal[TWnf[TIfZero[TNum[0], TNum[1], TLam[f, TLam[y, f[f[f[y]]]]][TLam[z, z * 2]][TNum[1]]]]]
```
<!-- => 1 -->

A single interaction was needed:

```wl
TItrs[]
```
<!-- => 1 -->

## Possible Issues

A constructor with label 0 matches the 0 of the matcher, so it selects the first branch:

```wl
TTermVal[TWnf[TIfZero[TCtr[0], TNum[10], TNum[20]]]]
```
<!-- => 10 -->

---

The eraser is not a number and selects the second branch:

```wl
TTermVal[TWnf[TIfZero[TEra[], TNum[10], TNum[20]]]]
```
<!-- => 20 -->
