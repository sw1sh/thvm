---
Template: Symbol
Name: TTermSubexprs
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TTermSubexprs
Keywords: [subexpression, path, walk, enumerate, position]
SeeAlso: [TSubexprAt, TTermExpr, TTermTree, Term]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TTermSubexprs]()[*term*]</code> gives a list of rules <code>*path* -> *sub*</code>, one for each position reachable from *term*, where *sub* is the [TTerm]() at *path*.

## Details & Options

- The positions are listed in depth-first pre-order, starting with *term* itself at the path `{}`.
- A path is a list of child offsets: 0 is the body of a LAM, 0 and 1 are the two children of an APP, SUP, OP2 or MAT, and 1, 2, ... are the fields of a CTR.
- Each *sub* is a live [TTerm]() handle on the heap cell at that position.
- [TSubexprAt]() gives the subterm at one path.
- The walk reads the heap as it is and does not reduce *term*.

## Basic Examples

The positions of a sum:

```wl
TTermSubexprs[TNum[3] + TNum[4]]
```

<!-- => {{} -> an OP2 TTerm summary box, {0} -> a TNum summary box showing 3, {1} -> a TNum summary box showing 4} -->

---

The paths of a lambda whose body is a sum:

```wl
Keys[TTermSubexprs[TLam[x, x + 1]]]
```

<!-- => {{}, {0}, {0, 0}, {0, 1}} -->

## Scope

The canonical forms of the subterms of a superposition:

```wl
Term /@ Values[TTermSubexprs[TSup[9, 5, 7]]]
```

<!-- => {Term["SUP", 9, Term["NUM", 5], Term["NUM", 7]], Term["NUM", 5], Term["NUM", 7]} -->

---

The fields of a constructor start at offset 1:

```wl
Keys[TTermSubexprs[TCtr[7, TNum[1], TNum[2]]]]
```

<!-- => {{}, {1}, {2}} -->

---

An atom has only the root position:

```wl
Keys[TTermSubexprs[TNum[5]]]
```

<!-- => {{}} -->

## Properties and Relations

Bind a sum:

```wl
t = TNum[3] + TNum[4]
```

<!-- => a TTerm summary box showing tag OP2 -->

[TSubexprAt]() at a listed path gives the same subterm:

```wl
TSubexprAt[t, {1}] == Lookup[TTermSubexprs[t], Key[{1}]]
```

<!-- => True -->
