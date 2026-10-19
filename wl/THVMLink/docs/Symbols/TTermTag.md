---
Template: Symbol
Name: TTermTag
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermTag
Keywords: [tag, term, inspector, packed, term kind]
SeeAlso: [TTerm, TTagName, TTermExt, TTermVal, TTermSub, TTermExpr]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermTag]()[*term*]</code> gives the integer tag of *term*.

## Details & Options

- *term* is a [TTerm]() or its packed [Integer]() value.
- The tag selects the kind of term. The common tags are 0 `APP`, 1 `LAM`, 2 `VAR`, 3 `ERA`, 4 `DP0`, 5 `DP1`, 6 `SUP`, 7 `DUP`, 8 `TEN`, 9 `UOP`, 10 `NUM`, 13 `OP2`, 14 `MAT` and 20 `CTR`.
- [TTagName]() gives the name of a tag, and <code>*t*["tagName"]</code> gives the name of the tag of the [TTerm]() *t* directly.
- [TTermTag]() reads the packed value only. It does not reduce *term*, so an unreduced application has the `APP` tag.

## Basic Examples

A lambda has tag 1:

```wl
TTermTag[TLam[x, x]]
```

<!-- => 1 -->

---

An application has tag 0:

```wl
TTermTag[TApp[TLam[x, x], TNum[5]]]
```

<!-- => 0 -->

## Scope

A number has tag 10:

```wl
TTermTag[TNum[3]]
```

<!-- => 10 -->

---

A superposition has tag 6:

```wl
TTermTag[TSup[1, 2]]
```

<!-- => 6 -->

---

An eraser has tag 3:

```wl
TTermTag[TEra[]]
```

<!-- => 3 -->

---

An integer operation has tag 13:

```wl
TTermTag[TOp2["+", 1, 2]]
```

<!-- => 13 -->

---

The two projections of a duplication have tags 4 and 5:

```wl
TTermTag /@ TDup[TNum[1]]
```

<!-- => {4, 5} -->

---

A tensor handle has tag 8:

```wl
TTermTag[TTensorCreate[{1., 2.}]]
```

<!-- => 8 -->

---

A tensor operation has tag 9:

```wl
TTermTag[TTensorCreate[{1., 2.}] + TTensorCreate[{3., 4.}]]
```

<!-- => 9 -->

---

A packed integer value is accepted in place of a [TTerm]():

```wl
TTermTag[72057594037927936]
```

<!-- => 1 -->

## Properties and Relations

[TTagName]() turns the tag into its name:

```wl
TTagName[TTermTag[TSup[1, 2]]]
```

<!-- => "SUP" -->

---

Reducing an application changes the tag of the result:

```wl
TTermTag[TWnf[TApp[TLam[x, x], TNum[5]]]]
```

<!-- => 10 -->
