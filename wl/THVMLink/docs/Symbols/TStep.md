---
Template: Symbol
Name: TStep
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TStep
Keywords: [step, single interaction, trace, debug]
SeeAlso: [TWnf, TStack, TItrs, TInteract, TMultiTrace]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TStep]()[*term*]</code> fires at most one interaction at the head of *term* and gives the partly reduced term.

## Details & Options

- <code>[TStep]()[*term*]</code> is <code>[TWnf]()[*term*, 1]</code>.
- An interaction is one rewrite, such as an application of a lambda, an integer operation, a duplication or a match, and it adds 1 to [TItrs](). Walking down to the redex at the head is not an interaction.
- When the head is already a value, [TStep]() gives the term back and fires nothing.
- When the step stops short of a value, [TStack]() gives the eliminators still waiting on the head. Stepping the result continues the reduction.
- Repeated [TStep]() walks a reduction one interaction at a time. [TMultiTrace]() records such a walk with its events, and [TInteract]() fires a chosen redex instead of the one at the head.

## Basic Examples

One step of an application fires the application and leaves the sum:

```wl
TStep[TApp[TLam[x, x + 1], TNum[41]]]
```

<!-- => a TTerm summary box showing tag OP2 -->

---

The canonical form of that step:

```wl
Term[TStep[TApp[TLam[x, x + 1], TNum[41]]]]
```

<!-- => Term["OP2", "+", Term["NUM", 41], Term["NUM", 1]] -->

## Scope

Start from an empty heap so the interaction counter starts at 0:

```wl
TReset[];
```

The first step:

```wl
s1 = TStep[TApp[TLam[x, x + 1], TNum[41]]]
```

<!-- => a TTerm summary box showing tag OP2 -->

It fired one interaction:

```wl
TItrs[]
```

<!-- => 1 -->

The second step does the sum:

```wl
s2 = TStep[s1]
```

<!-- => a TNum summary box showing number 42 -->

Two interactions have fired:

```wl
TItrs[]
```

<!-- => 2 -->

A number is a value, so a further step gives it back:

```wl
TStep[s2] == s2
```

<!-- => True -->

No interaction fired:

```wl
TItrs[]
```

<!-- => 2 -->

---

Start from an empty heap so the binder locations are predictable:

```wl
TReset[];
```

One step of a curried application fires the inner application:

```wl
Term[TStep[TApp[TApp[TLam[a, TLam[b, a]], TNum[1]], TNum[2]]]]
```

<!-- => Term["APP", Term["LAM", 1, Term["NUM", 1]], Term["NUM", 2]] -->

The outer application is still pending:

```wl
Term /@ TStack[]
```

<!-- => {Term["APP", Term["LAM", 1, Term["NUM", 1]], Term["NUM", 2]]} -->

## Properties and Relations

[TStep]() is [TWnf]() with a budget of one interaction:

```wl
Term[TStep[TNum[3] + TNum[4]]] === Term[TWnf[TNum[3] + TNum[4], 1]]
```

<!-- => True -->

---

Stepping until nothing changes reaches the weak normal form:

```wl
TTermVal[FixedPoint[TStep, TApp[TLam[x, x * 2], TNum[21]], 10, SameTest -> Equal]]
```

<!-- => 42 -->
