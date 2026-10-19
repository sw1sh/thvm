---
Template: Symbol
Name: TNf
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TNf
Keywords: [nf, normal form, redex, full reduction]
SeeAlso: [TWnf, TCnf, TCollapse, TRedexes]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TNf]()[*term*]</code> reduces *term* to full normal form and gives the reduced term.

## Details & Options

- Where [TWnf]() reduces only the head, [TNf]() also reduces inside lambda bodies, inside the branches of superpositions and inside the operands of other terms.
- [TNf]() first fires every redex stored in the live heap, then reduces the result the way [TCnf]() does, so the term it gives has no pending duplication projections.
- The sweep covers the whole heap, so it also reduces redexes inside other terms built in the same session.
- References to named definitions and lazy allocations are not unfolded eagerly, so a recursive definition does not unfold forever.

## Basic Examples

Reduce a nested integer computation:

```wl
TNf[TOp2["+", TOp2["*", 3, 4], 5]]
```

<!-- => a TNum summary box showing number 17 and dtype i32 -->

---

Its value:

```wl
TTermVal[TNf[TOp2["+", TOp2["*", 3, 4], 5]]]
```

<!-- => 17 -->

## Scope

Start from an empty heap so the binder location is predictable:

```wl
TReset[];
```

The body of a lambda is reduced:

```wl
Term[TNf[TLam[y, TNum[1] + TNum[2]]]]
```

<!-- => Term["LAM", 0, Term["NUM", 3]] -->

---

The branches of a superposition are reduced:

```wl
Term[TNf[TSup[9, TNum[1] + TNum[2], 5]]]
```

<!-- => Term["SUP", 9, Term["NUM", 3], Term["NUM", 5]] -->

---

An application whose bound variable occurs twice:

```wl
TTermVal[TNf[TApp[TLam[x, x * x], TNum[7]]]]
```

<!-- => 49 -->

## Properties and Relations

Start from an empty heap so the binder location is predictable:

```wl
TReset[];
```

[TWnf]() leaves the body of a lambda as it is:

```wl
Term[TWnf[TLam[y, TNum[1] + TNum[2]]]]
```

<!-- => Term["LAM", 0, Term["OP2", "+", Term["NUM", 1], Term["NUM", 2]]] -->

---

[TWnf]() leaves the branches of a superposition as they are:

```wl
Term[TWnf[TSup[9, TNum[1] + TNum[2], 5]]]
```

<!-- => Term["SUP", 9, Term["OP2", "+", Term["NUM", 1], Term["NUM", 2]], Term["NUM", 5]] -->

## Possible Issues

Start from an empty heap:

```wl
TReset[];
```

Bind a lambda whose body holds a sum:

```wl
other = TLam[y, TNum[3] + TNum[4]]
```

<!-- => a TTerm summary box showing tag LAM@0 -->

Reduce an unrelated term:

```wl
TNf[TNum[1]]
```

<!-- => a TNum summary box showing number 1 -->

The sweep has reduced the other lambda's body as well:

```wl
Term[other]
```

<!-- => Term["LAM", 0, Term["NUM", 7]] -->
