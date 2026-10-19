---
Template: Symbol
Name: TTermExpr
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermExpr
Keywords: [tree, expression, walk, tag names, structure]
SeeAlso: [TTermTree, Term, TTermSubexprs, TSubexprAt, TTagName]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermExpr]()[*term*]</code> gives a nested expression whose heads are the tag names of the heap cells reachable from *term*.

## Details & Options

- Each compound cell appears as its tag name applied to its fields and children, such as `"OP2"["+", "NUM"[1], "NUM"[2]]`.
- A LAM shows only its body, a VAR the location of its binder, a SUP its label and branches, a DP0 or DP1 its label and the duplicated term, and a NUM its value.
- An eraser appears as the string `"ERA"`.
- A cell reached a second time along the same path appears as `"Cycle"[*loc*]` instead of being walked again.
- [TTermExpr]() reads the heap as it is and does not reduce *term*. Unlike [Term](), it does not follow a variable to a substituted value.
- [TTermTree]() renders the same expression as a [Tree]().

## Basic Examples

The walk of a sum:

```wl
TTermExpr[TNum[3] + TNum[4]]
```

<!-- => "OP2"["+", "NUM"[3], "NUM"[4]] -->

---

A superposition shows its label and its two branches:

```wl
TTermExpr[TSup[9, 5, 7]]
```

<!-- => "SUP"[9, "NUM"[5], "NUM"[7]] -->

## Scope

Start from an empty heap so the variable location is predictable:

```wl
TReset[];
```

A lambda shows its body, where the variable is the location of its binder:

```wl
TTermExpr[TLam[x, x + 1]]
```

<!-- => "LAM"["OP2"["+", "VAR"[0], "NUM"[1]]] -->

---

A matcher shows the value it tests for, its handler and its fallback:

```wl
TTermExpr[TMatNum[0, TNum[1], TLam[k, TNum[2]]]]
```

<!-- => "MAT"[0, "NUM"[1], "LAM"["NUM"[2]]] -->

---

A constructor shows its label and its fields:

```wl
TTermExpr[TCtr[7, TNum[1], TNum[2]]]
```

<!-- => "CTR"[7, "NUM"[1], "NUM"[2]] -->

---

A duplication projection shows its label and the term it duplicates:

```wl
TTermExpr[Last[TDup[5, TSup[1, 1, 2]]]]
```

<!-- => "DP1"[5, "SUP"[1, "NUM"[1], "NUM"[2]]] -->

---

An eraser is a plain string:

```wl
TTermExpr[TEra[]]
```

<!-- => "ERA" -->

## Properties and Relations

Walks of separately built terms with the same structure are identical:

```wl
TTermExpr[TNum[3] + TNum[4]] === TTermExpr[TNum[3] + TNum[4]]
```

<!-- => True -->

---

[TTermTree]() is the [ExpressionTree]() of the walk:

```wl
TTermTree[TNum[3] + TNum[4]] === ExpressionTree[TTermExpr[TNum[3] + TNum[4]]]
```

<!-- => True -->

---

[Term]() gives the same structure with [Term]() heads:

```wl
Term[TNum[3] + TNum[4]]
```

<!-- => Term["OP2", "+", Term["NUM", 3], Term["NUM", 4]] -->

## Possible Issues

Variables carry the location of their binder, so walks of two separately built lambdas differ:

```wl
TTermExpr[TLam[x, x]] === TTermExpr[TLam[y, y]]
```

<!-- => False -->
