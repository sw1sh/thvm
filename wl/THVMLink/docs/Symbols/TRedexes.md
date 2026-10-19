---
Template: Symbol
Name: TRedexes
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TRedexes
Keywords: [redex, list, heap, multiway, reduction order]
SeeAlso: [TInteract, TStep, TWnf, THeapDiagram]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TRedexes]()[]</code> gives the list of redexes stored in the cells of the live heap.

<code>[TRedexes]()[*t*]</code> also includes the redexes reachable from the term *t*, *t* itself among them.

## Details & Options

- A redex is an active pair the runtime can fire: an application of a lambda or a matcher, an integer operation on two numbers, a duplication projection of a superposition, a lambda or a constructor, and similar pairs.
- Each entry is a [TTerm]() for the redex, which [TInteract]() fires.
- A term held only by Wolfram Language, such as the root a constructor such as [TApp]() gives, is not stored in a heap cell, so only <code>[TRedexes]()[*t*]</code> lists it.
- [TStep]() and [TWnf]() always fire the redex at the head. [TRedexes]() with [TInteract]() fires redexes in any order.

## Basic Examples

Start from an empty heap so the listing covers only this example:

```wl
TReset[];
```

An application of a lambda is a redex:

```wl
TRedexes[TApp[TLam[x, x], TNum[5]]]
```

<!-- => a list of one TTerm summary box showing tag APP -->

The canonical form of a redex:

```wl
Term /@ TRedexes[TNum[3] + TNum[4]]
```

<!-- => {Term["OP2", "+", Term["NUM", 3], Term["NUM", 4]]} -->

## Scope

Start from an empty heap:

```wl
TReset[];
```

Bind an application of a lambda:

```wl
t = TApp[TLam[x, x], TNum[5]]
```

<!-- => a TTerm summary box showing tag APP -->

The application is held only by Wolfram Language, so the heap stores no redex:

```wl
TRedexes[]
```

<!-- => {} -->

With the term given, the application is listed:

```wl
TRedexes[t]
```

<!-- => a list of one TTerm summary box showing tag APP -->

---

Start from an empty heap:

```wl
TReset[];
```

A lambda stores its body in the heap:

```wl
f = TLam[y, TNum[1] + TNum[2]]
```

<!-- => a TTerm summary box showing tag LAM -->

The sum in the body is a redex in the heap:

```wl
Term /@ TRedexes[]
```

<!-- => {Term["OP2", "+", Term["NUM", 1], Term["NUM", 2]]} -->

---

Start from an empty heap:

```wl
TReset[];
```

A projection of a duplication of a superposition is a redex:

```wl
Term /@ TRedexes[First[TDup[1, TSup[1, 1, 2]]]]
```

<!-- => {Term["DP0", 1, Term["SUP", 1, Term["NUM", 1], Term["NUM", 2]]]} -->

---

Start from an empty heap:

```wl
TReset[];
```

An application of a matcher to a number is a redex:

```wl
Term /@ TRedexes[TApp[TMatNum[0, TNum[1], TLam[k, TNum[2]]], TNum[0]]]
```

<!-- => {Term["APP", Term["MAT", 0, Term["NUM", 1], Term["LAM", 0, Term["NUM", 2]]], Term["NUM", 0]]} -->

## Properties and Relations

Start from an empty heap:

```wl
TReset[];
```

A superposition stores two sums in the heap:

```wl
s = TSup[9, TNum[1] + TNum[2], TNum[3] * TNum[4]]
```

<!-- => a TTerm summary box showing tag SUP -->

Both sums are redexes:

```wl
Term /@ TRedexes[]
```

<!-- => {Term["OP2", "+", Term["NUM", 1], Term["NUM", 2]], Term["OP2", "*", Term["NUM", 3], Term["NUM", 4]]} -->

[TInteract]() fires the first one, the sum:

```wl
Term[TInteract[First[TRedexes[]]]["result"]]
```

<!-- => Term["NUM", 3] -->

The product is the only redex left:

```wl
Term /@ TRedexes[]
```

<!-- => {Term["OP2", "*", Term["NUM", 3], Term["NUM", 4]]} -->

The superposition now holds the reduced sum:

```wl
Term[s]
```

<!-- => Term["SUP", 9, Term["NUM", 3], Term["OP2", "*", Term["NUM", 3], Term["NUM", 4]]] -->

## Possible Issues

Start from an empty heap:

```wl
TReset[];
```

Bind a lambda whose body holds a sum:

```wl
f = TLam[y, TNum[1] + TNum[2]]
```

<!-- => a TTerm summary box showing tag LAM -->

The listing for another term also includes the redex stored in the lambda's body:

```wl
Term /@ TRedexes[TNum[3] + TNum[4]]
```

<!-- => {Term["OP2", "+", Term["NUM", 3], Term["NUM", 4]], Term["OP2", "+", Term["NUM", 1], Term["NUM", 2]]} -->
