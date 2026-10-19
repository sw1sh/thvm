---
Template: Symbol
Name: TVarFor
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TVarFor
Keywords: [var, variable, binder, lambda, heap]
SeeAlso: [TLam, TApp, THeapAlloc, THeapRead, TTermVal]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TVarFor]()[*loc*]</code> gives a variable term for the lambda whose binder is the heap cell at location *loc*.

## Details & Options

- A variable term has the VAR tag, and its VAL field is *loc*.
- [TLam]() builds its variable this way: it allocates the binder cell, writes the body there with the bound symbol replaced by <code>[TVarFor]()[*loc*]</code>, and gives a LAM term pointing at that cell.
- Reading a variable goes through its binder cell. Once an application of the lambda has fired, the cell holds the argument with the SUB flag set, and the variable reads as the argument.
- [TVarFor]() only builds the variable term; it does not allocate or write the binder cell.

## Basic Examples

A variable for the binder at location 0:

```wl
TVarFor[0]
```

<!-- => a TTerm summary box showing tag VAR@0 -->

---

It has the VAR tag:

```wl
TTagName[TTermTag[TVarFor[0]]]
```

<!-- => "VAR" -->

---

Its VAL field is the binder location:

```wl
TTermVal[TVarFor[7]]
```

<!-- => 7 -->

## Scope

Bind an identity lambda:

```wl
f = TLam[x, x]
```

<!-- => a TTerm summary box showing tag LAM -->

The lambda's cell holds the variable for that same cell:

```wl
THeapRead[TTermVal[f]] == TVarFor[TTermVal[f]]
```

<!-- => True -->

Apply the lambda to 5 and reduce:

```wl
TWnf[TApp[f, TNum[5]]]
```

<!-- => a TNum summary box showing number 5 -->

The variable now reads as the argument:

```wl
Term[TVarFor[TTermVal[f]]]
```

<!-- => Term["NUM", 5] -->

## Properties and Relations

Start from an empty heap so the binder location is predictable:

```wl
TReset[];
```

The body of an identity lambda is the variable for the lambda's own cell, here location 0:

```wl
TTermExpr[TLam[x, x]]
```

<!-- => "LAM"["VAR"[1]] -->
