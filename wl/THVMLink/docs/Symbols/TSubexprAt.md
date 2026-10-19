---
Template: Symbol
Name: TSubexprAt
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSubexprAt
Keywords: [subexpression, path, position, navigate]
SeeAlso: [TTermSubexprs, Term, TTermExpr]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TSubexprAt]()[*term*, *path*]</code> gives the subterm of *term* at the position *path* as a [TTerm]().

## Details & Options

- *path* is a list of child offsets, as [TTermSubexprs]() lists them: 0 is the body of a LAM, 0 and 1 are the two children of an APP, SUP, OP2 or MAT, and 1, 2, ... are the fields of a CTR.
- The path `{}` gives *term* itself.
- A path that leaves the shape of *term* gives <code>[Missing]()["OutOfBounds", *path*]</code>.
- The subterm is a live [TTerm]() handle on the heap cell at that position; [TSubexprAt]() does not reduce *term*.

## Basic Examples

The second operand of the sum in the body of a lambda:

```wl
TSubexprAt[TLam[x, x + 1], {0, 1}]
```

<!-- => a TNum summary box showing number 1 and dtype i32 -->

---

The body of a lambda:

```wl
Term[TSubexprAt[TLam[x, TNum[3] * TNum[4]], {0}]]
```

<!-- => Term["OP2", "*", Term["NUM", 3], Term["NUM", 4]] -->

## Scope

The second branch of a superposition:

```wl
Term[TSubexprAt[TSup[9, 5, 7], {1}]]
```

<!-- => Term["NUM", 7] -->

---

The second field of a constructor, whose fields start at offset 1:

```wl
Term[TSubexprAt[TCtr[7, TNum[1], TNum[2]], {2}]]
```

<!-- => Term["NUM", 2] -->

---

Bind a sum:

```wl
t = TNum[3] + TNum[4]
```

<!-- => a TTerm summary box showing tag OP2 -->

The empty path gives the term itself:

```wl
TSubexprAt[t, {}] == t
```

<!-- => True -->

## Properties and Relations

The paths [TTermSubexprs]() lists are the ones [TSubexprAt]() accepts:

```wl
Keys[TTermSubexprs[TLam[x, x + 1]]]
```

<!-- => {{}, {0}, {0, 0}, {0, 1}} -->

## Possible Issues

A path past the shape of the term gives a [Missing]() value:

```wl
TSubexprAt[TLam[x, x + 1], {0, 5}]
```

<!-- => Missing["OutOfBounds", {0, 5}] -->

---

An atom has no children:

```wl
TSubexprAt[TNum[5], {0}]
```

<!-- => Missing["OutOfBounds", {0}] -->
