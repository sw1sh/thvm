---
Template: Symbol
Name: TTermSame
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermSame
Keywords: [equality, term, structural, no reduction]
SeeAlso: [TTermEq, Term, TCnf, TWnf]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermSame]()[*a*, *b*]</code> gives [True]() if the terms *a* and *b* have the same structure as they stand, and [False]() otherwise.

## Details & Options

- *a* and *b* are [TTerm]() handles, or both packed [Integer]() values.
- [TTermSame]() does not reduce. It compares the tag, EXT field and value of atoms, and recurses into the children of compound terms; two handles on the same cell are equal.
- Variables compare by binder. Two separately built lambdas have distinct binders, so they compare unequal even when they are alpha-equivalent.
- An unreduced term compares by its shape, so an application differs from its value. [TTermEq]() reduces both terms first.

## Basic Examples

Two separately built sums have the same structure:

```wl
TTermSame[TNum[3] + TNum[4], TNum[3] + TNum[4]]
```

<!-- => True -->

---

An unreduced sum is not the same as its value:

```wl
TTermSame[TNum[3] + TNum[4], TNum[7]]
```

<!-- => False -->

## Scope

Bind a lambda:

```wl
t = TLam[x, x]
```

<!-- => a TTerm summary box showing tag LAM -->

A term is the same as itself:

```wl
TTermSame[t, t]
```

<!-- => True -->

---

Superpositions compare by label and branches:

```wl
TTermSame[TSup[1, 1, 2], TSup[1, 1, 2]]
```

<!-- => True -->

---

The order of the branches matters:

```wl
TTermSame[TSup[1, 1, 2], TSup[1, 2, 1]]
```

<!-- => False -->

---

Constructors compare by label and fields:

```wl
TTermSame[TCtr[1, TNum[2], TNum[3]], TCtr[1, TNum[2], TNum[3]]]
```

<!-- => True -->

---

Packed integer values are accepted in place of [TTerm]() handles:

```wl
TTermSame[TNum[7]["raw"], TNum[7]["raw"]]
```

<!-- => True -->

## Properties and Relations

[TTermEq]() reduces both sides, so a sum equals its value:

```wl
TTermEq[TNum[3] + TNum[4], TNum[7]]
```

<!-- => True -->

---

After [TWnf]() the reduced sum is the same as its value:

```wl
TTermSame[TWnf[TNum[3] + TNum[4]], TNum[7]]
```

<!-- => True -->

## Possible Issues

Two separately built identity functions have distinct binders, so they are not the same:

```wl
TTermSame[TLam[x, x], TLam[y, y]]
```

<!-- => False -->
