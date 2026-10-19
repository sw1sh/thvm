---
Template: Symbol
Name: TReduce
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TReduce
Keywords: [reduce, wnf, in place, inspect heap]
SeeAlso: [TWnf, THeapGraph, TTermExpr, Term, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TReduce]()[*term*]</code> reduces *term* to weak normal form in place and gives *term* itself.

## Details & Options

- [TReduce]() runs <code>[TWnf]()[*term*]</code> for its effect on the heap and gives back the handle it was given, not the reduced term.
- The reduction rewrites the cells reachable from *term*, so [Term](), [TTermExpr]() and [THeapGraph]() of *term* afterwards show the heap after the reduction.
- The reduced value is what [TWnf]() gives. The original handle does not point at it, and reducing it a second time fires on cells the first reduction already consumed.
- [TRealize]() is the tensor counterpart: it schedules and runs the kernels of a tensor computation.

## Basic Examples

Start from an empty heap so the binder location is predictable:

```wl
TReset[];
```

Bind an application of a lambda:

```wl
t = TApp[TLam[x, x + 1], TNum[41]]
```

<!-- => a TTerm summary box showing tag APP -->

Reduce it in place; [TReduce]() gives the application back:

```wl
TReduce[t]
```

<!-- => a TTerm summary box showing tag APP -->

The application now reads through the rewritten heap, where the lambda's cell holds the argument 41:

```wl
Term[t]
```

<!-- => Term["APP", Term["LAM", 0, Term["NUM", 41]], Term["NUM", 41]] -->

## Scope

Start from an empty heap so the interaction counter starts at 0:

```wl
TReset[];
```

Bind an application of a lambda:

```wl
t = TApp[TLam[x, x + 1], TNum[41]]
```

<!-- => a TTerm summary box showing tag APP -->

Reduce it in place:

```wl
u = TReduce[t]
```

<!-- => a TTerm summary box showing tag APP -->

The handle given back is the original one:

```wl
u === t
```

<!-- => True -->

The reduction fired the application and the sum:

```wl
TItrs[]
```

<!-- => 2 -->

---

Reducing a value leaves it as it is:

```wl
TReduce[TNum[5]] == TNum[5]
```

<!-- => True -->

## Properties and Relations

[TWnf]() gives the reduced term instead of the original handle:

```wl
TTermVal[TWnf[TApp[TLam[x, x + 1], TNum[41]]]]
```

<!-- => 42 -->

## Possible Issues

Bind an application of a lambda:

```wl
t = TApp[TLam[x, x + 1], TNum[41]]
```

<!-- => a TTerm summary box showing tag APP -->

Reduce it in place:

```wl
TReduce[t]
```

<!-- => a TTerm summary box showing tag APP -->

Reducing the original handle again fires on consumed cells and does not give the value 42:

```wl
TTermVal[TWnf[t]]
```

<!-- => 41 -->
