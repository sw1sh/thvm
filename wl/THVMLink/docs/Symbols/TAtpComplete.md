---
Template: Symbol
Name: TAtpComplete
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpComplete
Keywords: [Knuth-Bendix completion, rewrite system, normal form, simplification, equational theory, Waldmeister]
SeeAlso: [TAtpSystem, TAtpNormalForm, TAtpEqualQ, TKnuthBendixCompletion, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpComplete]()[*axioms*]</code> completes the equations *axioms* and gives a [TAtpSystem]() that keeps the completed system in the engine.

<code>[TAtpComplete]()[*axioms*, *order*]</code> completes under the reduction ordering *order*.

<code>[TAtpComplete]()["*theory*", ...]</code> completes the axioms of [AxiomaticTheory]()["*theory*"].

## Details & Options

- [TAtpComplete]() runs the unfailing Knuth-Bendix completion of [TKnuthBendixCompletion]() once and keeps its result, so that [TAtpNormalForm]() and [TAtpEqualQ]() answer many questions about one theory without completing again.
- The *axioms* and *order* are as in [TKnuthBendixCompletion](): a precedence list {*s*<sub>1</sub>, *s*<sub>2</sub>, ...} gives the lexicographic path ordering, a list {{*s*<sub>1</sub>, *w*<sub>1</sub>}, ...} the Knuth-Bendix ordering with weights *w*<sub>i</sub>, and the default is the Knuth-Bendix ordering with weight 1 for every symbol.
- Completion stops when every critical pair is joined, or when it reaches [MaxSteps]() selected equations or its allocation budget. Both budgets give the same result on every machine; [TimeConstraint]() is not used by default.
- The following options can be given:

| | | |
| --- | --- | --- |
| [MaxSteps]() | 2000 | maximum number of equations completion selects |
| `"AllocationLimit"` | [Automatic]() | heap cells completion may allocate; [Automatic]() is 2<sup>24</sup>, [None]() no limit |
| [TimeConstraint]() | [Infinity]() | seconds completion may run |
| [Method]() | `"Waldmeister"` | completion configuration, any [Method]() setting of [TFindProof]() |

- The completed system lives in the engine until the [TAtpSystem]() is no longer referenced.

## Basic Examples

Complete the axioms of a commutative, associative operator:

```wl
TAtpComplete[{ForAll[{x, y}, h[x, y] == h[y, x]], ForAll[{x, y, z}, h[h[x, y], z] == h[x, h[y, z]]]}]
```

<!-- => TAtpSystem[...] -->

---

The completed system is ground convergent:

```wl
TAtpComplete[{ForAll[{x, y}, h[x, y] == h[y, x]], ForAll[{x, y, z}, h[h[x, y], z] == h[x, h[y, z]]]}]["Complete"]
```

<!-- => True -->

## Scope

Complete the group axioms; without a finite completed system under the default ordering, completion stops at its budget:

```wl
TAtpComplete[{ForAll[{x, y, z}, m[m[x, y], z] == m[x, m[y, z]]], ForAll[x, m[ee, x] == x], ForAll[x, m[i[x], x] == ee]}]["Status"]
```

<!-- => "BudgetExhausted" -->

---

Under the lexicographic path ordering with the inverse greatest, the group axioms complete:

```wl
TAtpComplete[{ForAll[{x, y, z}, m[m[x, y], z] == m[x, m[y, z]]], ForAll[x, m[ee, x] == x], ForAll[x, m[i[x], x] == ee]}, {i, m, ee}]["Status"]
```

<!-- => "Saturated" -->
