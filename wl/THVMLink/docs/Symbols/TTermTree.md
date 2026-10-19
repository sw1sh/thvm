---
Template: Symbol
Name: TTermTree
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermTree
Keywords: [tree, term, visual, ExpressionTree]
SeeAlso: [TTermExpr, Term, THeapGraph, THeapDiagram]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermTree]()[*term*]</code> gives the tag-name walk of *term* as a [Tree]().

## Details & Options

- <code>[TTermTree]()[*term*]</code> is <code>[ExpressionTree]()[[TTermExpr]()[*term*]]</code>.
- Each compound cell is an inner node labeled by its tag name. Operator names, labels, numbers and variable locations are leaves.
- The result is an ordinary [Tree](), so functions such as [TreeLeaves](), [TreeData]() and [TreeDepth]() apply to it.
- [THeapGraph]() and [THeapDiagram]() draw the same heap as an interaction net, with shared cells and wires.

## Basic Examples

The tree of a sum:

```wl
TTermTree[TNum[3] + TNum[4]]
```

<!-- => a Tree with root OP2, whose children are the leaf +, and two NUM nodes over the leaves 3 and 4 -->

---

The tree of a product of two sums:

```wl
TTermTree[(TNum[1] + TNum[2]) * (TNum[3] + TNum[4])]
```

<!-- => a Tree with root OP2 over the leaf * and two OP2 subtrees, one per sum -->

## Scope

A superposition is a node over its label and its two branches:

```wl
TTermTree[TSup[9, TNum[3] + TNum[4], TNum[5] * TNum[6]]]
```

<!-- => a Tree with root SUP over the leaf 9 and two OP2 subtrees -->

## Properties and Relations

The leaves of the tree of a sum hold its operator and its two numbers:

```wl
TreeData /@ TreeLeaves[TTermTree[TNum[3] + TNum[4]]]
```

<!-- => {"+", 3, 4} -->

---

The tree of a sum has depth 2:

```wl
TreeDepth[TTermTree[TNum[3] + TNum[4]]]
```

<!-- => 2 -->

---

The tree is the [ExpressionTree]() of [TTermExpr]():

```wl
TTermTree[TNum[3] + TNum[4]] === ExpressionTree[TTermExpr[TNum[3] + TNum[4]]]
```

<!-- => True -->
