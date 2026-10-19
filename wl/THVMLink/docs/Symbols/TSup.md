---
Template: Symbol
Name: TSup
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSup
Keywords: [superposition, sup, label, multicomputation, branches]
SeeAlso: [TDup, TCollapse, TFreshLabel, TDsu, TMultiTrace, TMultiwayGraph]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TSup]()[*a*, *b*]</code> gives a superposition of the terms *a* and *b* with a fresh label.

<code>[TSup]()[*label*, *a*, *b*]</code> gives a superposition with the integer label *label*.

<code>[TSup]()[{$x_1$, $x_2$, …}]</code> gives the nested superposition of the $x_i$, with a fresh label at each level.

## Details & Options

- A superposition stands for both of its branches at once. [TCollapse]() lists the values it stands for.
- A bare [Integer]() branch is read as <code>[TNum]()[*i*]</code>.
- Without an explicit label, the superposition takes the next value of the fresh-label counter of [TFreshLabel](), which [TReset]() restarts at 1.
- The list form folds from the left: <code>[TSup]()[{*a*, *b*, *c*}]</code> is <code>[TSup]()[[TSup]()[*a*, *b*], *c*]</code>. A list of one element gives the term of that element, and an empty list issues the message `TSup::empty`.
- Integer operations, applications and matchers distribute over a superposition, giving a superposition of the results.
- Labels decide how superpositions combine. Two with the same label combine branch by branch; two with different labels give every combination.
- A duplication ([TDup]()) with the same label as a superposition takes its two branches apart; with a different label, it duplicates the whole superposition.
- [TMultiTrace]() and [TMultiwayGraph]() show how the branches evolve during reduction.

## Basic Examples

A superposition of 5 and 7 with label 9:

```wl
TSup[9, 5, 7]
```

<!-- => a TTerm summary box showing tag SUP and ext 9 -->

---

Its canonical form shows the label and both branches:

```wl
Term[TSup[9, 5, 7]]
```

<!-- => Term["SUP", 9, Term["NUM", 5], Term["NUM", 7]] -->

---

[TCollapse]() lists the values it stands for:

```wl
TTermVal /@ TCollapse[TSup[9, 5, 7]]
```

<!-- => {5, 7} -->

## Scope

Start from an empty heap, which restarts the fresh-label counter:

```wl
TReset[];
```

A superposition without a label takes label 1:

```wl
Term[TSup[5, 7]]
```

<!-- => Term["SUP", 1, Term["NUM", 5], Term["NUM", 7]] -->

---

Start from an empty heap:

```wl
TReset[];
```

The list form nests from the left, with a fresh label at each level:

```wl
Term[TSup[{5, 7, 9}]]
```

<!-- => Term["SUP", 2, Term["SUP", 1, Term["NUM", 5], Term["NUM", 7]], Term["NUM", 9]] -->

---

A three-way superposition stands for three values:

```wl
TTermVal /@ TCollapse[TSup[{5, 7, 9}]]
```

<!-- => {5, 7, 9} -->

---

A list of one element gives the term of that element, with no superposition:

```wl
Term[TSup[{5}]]
```

<!-- => Term["NUM", 5] -->

---

The branches can be any terms:

```wl
TTermVal /@ TCollapse[TSup[TNum[3] + TNum[4], TNum[6] * 7]]
```

<!-- => {7, 42} -->

---

Arithmetic distributes over the branches:

```wl
TTermVal /@ TCollapse[TSup[1, 2] + 3]
```

<!-- => {4, 5} -->

---

So does application, here of a superposition of two functions:

```wl
TTermVal /@ TCollapse[TApp[TSup[TLam[x, x + 1], TLam[x, x + 10]], TNum[5]]]
```

<!-- => {6, 15} -->

## Properties and Relations

Two superpositions with the same label combine branch by branch:

```wl
TTermVal /@ TCollapse[TSup[1, 1, 2] + TSup[1, 10, 20]]
```

<!-- => {11, 22} -->

---

Two superpositions with different labels give every combination:

```wl
TTermVal /@ TCollapse[TSup[1, 1, 2] + TSup[2, 10, 20]]
```

<!-- => {11, 21, 12, 22} -->

---

Duplicate a superposition with a duplication of the same label:

```wl
{d0, d1} = TDup[1, TSup[1, 1, 2]]
```

<!-- => a list of two TTerm summary boxes, tags DP0 and DP1 -->

The first projection takes the first branch:

```wl
TTermVal[TWnf[d0]]
```

<!-- => 1 -->

The second projection takes the second branch:

```wl
TTermVal[TWnf[d1]]
```

<!-- => 2 -->

---

[TTermExt]() gives the label:

```wl
TTermExt[TSup[9, 5, 7]]
```

<!-- => 9 -->

## Possible Issues

A superposition of no terms is refused:

```wl
TSup[{}]
```

<!-- => the message TSup::empty is issued and the expression returns unevaluated -->
