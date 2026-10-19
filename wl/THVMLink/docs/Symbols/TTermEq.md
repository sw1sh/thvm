---
Template: Symbol
Name: TTermEq
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermEq
Keywords: [equality, term, reduction, structural, cnf]
SeeAlso: [TTermSame, TEql, Term, TCnf, TCollapse]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermEq]()[*a*, *b*]</code> gives [True]() if the terms *a* and *b* reduce to the same structure, and [False]() otherwise.

## Details & Options

- *a* and *b* are [TTerm]() handles, or both packed [Integer]() values.
- Both terms are reduced in place by the readback reduction of [TCnf]() before they are compared, so an application, an integer operation or a duplication projection compares by its result.
- The comparison is structural: equal tags, equal EXT fields and equal children. Numbers compare by value and dtype.
- Variables compare by binder. Two separately built lambdas have distinct binders, so they compare unequal even when they are alpha-equivalent.
- A superposition compares as a superposition, by its label and its branches; [TCollapse]() lists the values it stands for.
- [TTermSame]() compares without reducing. [TEql]() builds an equality test as a term that reduces inside the heap.

## Basic Examples

An application of the identity reduces to its argument:

```wl
TTermEq[TApp[TLam[x, x], TNum[7]], TNum[7]]
```

<!-- => True -->

---

A sum reduces to its value:

```wl
TTermEq[TNum[3] + TNum[4], TNum[7]]
```

<!-- => True -->

---

A sum and a different number are not equal:

```wl
TTermEq[TNum[3] + TNum[4], TNum[8]]
```

<!-- => False -->

## Scope

Two superpositions with the same label and branches are equal:

```wl
TTermEq[TSup[1, 1, 2], TSup[1, 1, 2]]
```

<!-- => True -->

---

Superpositions with different labels are not:

```wl
TTermEq[TSup[1, 1, 2], TSup[2, 1, 2]]
```

<!-- => False -->

---

Constructors compare by label and fields:

```wl
TTermEq[TCtr[1, TNum[2], TNum[3]], TCtr[1, TNum[2], TNum[3]]]
```

<!-- => True -->

---

The order of the fields matters:

```wl
TTermEq[TCtr[1, TNum[2], TNum[3]], TCtr[1, TNum[3], TNum[2]]]
```

<!-- => False -->

---

Two erasers are equal:

```wl
TTermEq[TEra[], TEra[]]
```

<!-- => True -->

---

Packed integer values are accepted in place of [TTerm]() handles:

```wl
TTermEq[TNum[7]["raw"], TNum[7]["raw"]]
```

<!-- => True -->

## Properties and Relations

[TTermSame]() does not reduce, so an application differs from its value:

```wl
TTermSame[TApp[TLam[x, x], TNum[7]], TNum[7]]
```

<!-- => False -->

---

Superpositions with different labels stand for the same values when their [TCollapse]() lists agree:

```wl
(TTermVal /@ TCollapse[TSup[1, 1, 2] + 3]) === (TTermVal /@ TCollapse[TSup[2, 4, 5]])
```

<!-- => True -->

## Possible Issues

Two separately built identity functions have distinct binders, so they are not equal:

```wl
TTermEq[TLam[x, x], TLam[y, y]]
```

<!-- => False -->

---

A [TTerm]() and a bare integer together return unevaluated:

```wl
TTermEq[TNum[7], 7]
```

<!-- => TTermEq[TTerm[...], 7], unevaluated, with the first argument shown as a TNum summary box -->
