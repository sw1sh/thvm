---
Template: Symbol
Name: TStack
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TStack
Keywords: [stack, eliminator, frames, wnf, bounded reduction, debug]
SeeAlso: [TStep, TWnf, TItrs, TReset]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TStack]()[]</code> gives the eliminator frames still pending when the last bounded reduction stopped.

## Details & Options

- A bounded reduction is <code>[TWnf]()[*term*, *n*]</code> or [TStep](). It stops after its budget of interactions, possibly with eliminators still waiting on the head, such as an application whose function has not yet reduced to a lambda.
- Each frame is a [TTerm]() for a waiting eliminator, such as an application, an integer operation or a duplication projection, listed from the innermost outward.
- The frames are written back into the heap when the reduction stops, so reducing the term the bounded reduction gave continues where it stopped.
- [TStack]() gives `{}` when the last reduction reached weak normal form within its budget, after an unbounded [TWnf](), and after [TReset]().

## Basic Examples

Start from an empty heap so the binder locations are predictable:

```wl
TReset[];
```

Stop an application of a curried three-argument function after one interaction:

```wl
TWnf[TApp[TApp[TApp[TLam[a, TLam[b, TLam[c, a]]], TNum[1]], TNum[2]], TNum[3]], 1]
```

<!-- => a TTerm summary box showing tag APP -->

Two applications are still waiting:

```wl
TStack[]
```

<!-- => a list of two TTerm summary boxes showing tag APP -->

Their canonical forms, the inner application first:

```wl
Term /@ TStack[]
```

<!-- => {Term["APP", Term["LAM", 1, Term["LAM", 2, Term["NUM", 1]]], Term["NUM", 2]], Term["APP", Term["APP", Term["LAM", 1, Term["LAM", 2, Term["NUM", 1]]], Term["NUM", 2]], Term["NUM", 3]]} -->

## Scope

Stop a sum after its first operand has reduced:

```wl
TWnf[TOp2["+", TApp[TLam[a, a], TNum[1]], TNum[2]], 1]
```

<!-- => a TTerm summary box showing tag OP2 -->

The sum is waiting, with the reduced operand written back:

```wl
Term /@ TStack[]
```

<!-- => {Term["OP2", "+", Term["NUM", 1], Term["NUM", 2]]} -->

---

A reduction that completes within its budget leaves nothing pending:

```wl
TWnf[TApp[TLam[x, x + 1], TNum[41]], 5]
```

<!-- => a TNum summary box showing number 42 -->

No frames are pending:

```wl
TStack[]
```

<!-- => {} -->

## Properties and Relations

Stop an application of a lambda after one interaction:

```wl
r = TWnf[TApp[TLam[x, x + 1], TNum[41]], 1]
```

<!-- => a TTerm summary box showing tag OP2 -->

Reducing the result finishes the computation:

```wl
TTermVal[TWnf[r]]
```

<!-- => 42 -->

The unbounded reduction leaves nothing pending:

```wl
TStack[]
```

<!-- => {} -->

---

Stop a curried application after one interaction:

```wl
TWnf[TApp[TApp[TLam[a, TLam[b, a]], TNum[1]], TNum[2]], 1]
```

<!-- => a TTerm summary box showing tag APP -->

One application is pending:

```wl
Length[TStack[]]
```

<!-- => 1 -->

Reset the heap:

```wl
TReset[];
```

[TReset]() drops the pending frames:

```wl
TStack[]
```

<!-- => {} -->
