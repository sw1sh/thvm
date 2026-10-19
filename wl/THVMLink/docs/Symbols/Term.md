---
Template: Symbol
Name: Term
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/Term
Keywords: [canonical form, term, walk, snapshot cell, multiway, vertex identity]
SeeAlso: [TTerm, TTermExpr, TTermTree, TInitialize, TContextSnapshot, TMultiwayGraph]
RelatedGuides: [THVMLink]
---

## Usage

<code>[Term]()[*t*]</code> gives the nested canonical form of the [TTerm]() *t*.

<code>[Term]()["*tag*", *ext*, *val*]</code> represents one heap cell of a [TContext]() snapshot.

<code>[Term]()["*tag*", *ext*, *val*, *sub*]</code> represents a heap cell whose SUB flag is *sub*.

<code>[Term]()[<|"tag" -> "*tag*", "ext" -> *ext*, "val" -> *val*|>]</code> gives the positional cell form.

## Details & Options

- <code>[Term]()[*t*]</code> walks the heap from *t* and gives a nested expression of [Term]() heads whose first argument is a tag name, such as <code>[Term]()["OP2", "+", [Term]()["NUM", 1], [Term]()["NUM", 2]]</code>.
- A lambda appears as <code>[Term]()["LAM", *loc*, *body*]</code>, where *loc* is the location of its binder, and each occurrence of its variable as <code>[Term]()["VAR", *loc*]</code>.
- A superposition carries its label, <code>[Term]()["SUP", *label*, *a*, *b*]</code>, and a duplication projection its label and the duplicated term.
- A variable or projection whose cell holds a substituted value appears as that value.
- The walk stops at depth 16, where a deeper subterm appears as <code>[Term]()["..."]</code>.
- The canonical form is an ordinary expression: it compares with [SameQ](), serializes, and is the vertex identity of [TMultiwayGraph]().
- <code>[TInitialize]()[*expr*]</code> rebuilds a canonical form *expr* into a live [TTerm]().
- The cell forms are what [TContextSnapshot]() records under `"Cells"`. *ext* is the EXT field, given by name for an operator or a dtype, and *val* is a heap location or an atomic payload.
- The association form is normalized to the positional form, dropping a `"sub"` of 0.

## Basic Examples

The canonical form of a sum:

```wl
Term[TNum[3] + TNum[4]]
```

<!-- => Term["OP2", "+", Term["NUM", 3], Term["NUM", 4]] -->

---

Start from an empty heap so the binder location is predictable:

```wl
TReset[];
```

A lambda carries the location of its binder, which its variable refers back to:

```wl
Term[TLam[x, x + 1]]
```

<!-- => Term["LAM", 1, Term["OP2", "+", Term["VAR", 1], Term["NUM", 1]]] -->

## Scope

A superposition carries its label:

```wl
Term[TSup[9, 5, 7]]
```

<!-- => Term["SUP", 9, Term["NUM", 5], Term["NUM", 7]] -->

---

A duplication projection carries its label and the term it duplicates:

```wl
Term[First[TDup[5, TSup[1, 1, 2]]]]
```

<!-- => Term["DP0", 5, Term["SUP", 1, Term["NUM", 1], Term["NUM", 2]]] -->

---

A constructor carries its label and its fields:

```wl
Term[TCtr[7, TNum[1], TNum[2]]]
```

<!-- => Term["CTR", 7, Term["NUM", 1], Term["NUM", 2]] -->

---

Start from an empty heap, which also restarts the tensor ids:

```wl
TReset[];
```

A tensor operation shows its operator and the ids of its tensor operands:

```wl
Term[TTensorCreate[{1., 2.}] + TTensorCreate[{3., 4.}]]
```

<!-- => Term["UOP", "ADD", Term["TEN", 1], Term["TEN", 2]] -->

---

Start from an empty heap:

```wl
TReset[];
```

Bind a lambda:

```wl
f = TLam[x, x]
```

<!-- => a TTerm summary box showing tag LAM@0 -->

Apply it to 5 and reduce:

```wl
TWnf[TApp[f, TNum[5]]]
```

<!-- => a TNum summary box showing number 5 -->

The lambda's variable now reads as the substituted value:

```wl
Term[f]
```

<!-- => Term["LAM", 1, Term["NUM", 5]] -->

---

A heap cell of a snapshot is inert:

```wl
Term["NUM", "i32", 5]
```

<!-- => Term["NUM", "i32", 5] -->

---

The association form of a cell is normalized to the positional form:

```wl
Term[<|"tag" -> "NUM", "ext" -> "i32", "val" -> 5|>]
```

<!-- => Term["NUM", "i32", 5] -->

---

A set SUB flag is kept as a fourth argument:

```wl
Term[<|"tag" -> "NUM", "ext" -> "i32", "val" -> 5, "sub" -> 1|>]
```

<!-- => Term["NUM", "i32", 5, 1] -->

---

Start from an empty heap:

```wl
TReset[];
```

The cells of a snapshot of a lambda:

```wl
Values[First[TContextSnapshot[TLam[x, x + 1]]]["Cells"]]
```

<!-- => {Term["OP2", "+", 2], Term["VAR", 0, 1], Term["NUM", "i32", 1]} -->

## Properties and Relations

Canonical forms of separately built terms with the same structure are identical:

```wl
Term[TNum[3] + TNum[4]] === Term[TNum[3] + TNum[4]]
```

<!-- => True -->

---

[TInitialize]() rebuilds a canonical form into a live term:

```wl
TInitialize[Term["OP2", "+", Term["NUM", 3], Term["NUM", 4]]]
```

<!-- => a TTerm summary box showing tag OP2 -->

---

The rebuilt term reduces like the original:

```wl
TTermVal[TWnf[TInitialize[Term["OP2", "+", Term["NUM", 3], Term["NUM", 4]]]]]
```

<!-- => 7 -->

---

[TTermExpr]() gives the same walk with tag names as heads:

```wl
TTermExpr[TNum[3] + TNum[4]]
```

<!-- => "OP2"["+", "NUM"[3], "NUM"[4]] -->

## Possible Issues

Two separately built lambdas have different binders, so their canonical forms differ:

```wl
Term[TLam[x, x]] === Term[TLam[y, y]]
```

<!-- => False -->

---

A term nested deeper than 16 levels is cut off:

```wl
Term[Fold[#1 + #2 &, TNum[0], Range[18]]]
```

<!-- => Term["OP2", "+", Term["OP2", "+", ... Term["OP2", "+", Term["..."], Term["..."]], Term["NUM", 4]], ... Term["NUM", 17]], Term["NUM", 18]]: sixteen OP2 levels, then Term["..."] for the two innermost operands -->
