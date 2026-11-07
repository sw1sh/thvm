---
Template: Symbol
Name: TAtpEqualQ
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpEqualQ
Keywords: [equality, entailment, word problem, normal form, completed system]
SeeAlso: [TAtpComplete, TAtpSystem, TAtpNormalForm, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpEqualQ]()[*sys*, *a*, *b*]</code> gives [True]() when the axioms of the [TAtpSystem]() *sys* entail *a* == *b*.

## Details & Options

- [TAtpEqualQ]() gives [True]() when *a* and *b* have the same normal form under *sys*.
- When *sys* is complete and the normal forms differ, the axioms do not entail *a* == *b*, and the result is a [Failure]() of type `"NotEntailed"` holding the two normal forms. This does not make *a* == *b* false: it fails in some model of the axioms, and may hold in another.
- When completion stopped at its budget and the normal forms differ, the result is [Indeterminate]().

## Basic Examples

An equation the axioms entail:

```wl
TAtpEqualQ[TAtpComplete[{ForAll[x, k[k[x]] == x]}], k[k[a]], a]
```

<!-- => True -->

---

An equation the axioms do not entail, under a complete system:

```wl
TAtpEqualQ[TAtpComplete[{ForAll[x, k[k[x]] == x]}], k[a], a]
```

<!-- => Failure["NotEntailed", <|...|>] -->

---

Under a system whose completion stopped at its budget, the answer is open:

```wl
TAtpEqualQ[TAtpComplete[{ForAll[{x, y, z}, m[m[x, y], z] == m[x, m[y, z]]], ForAll[x, m[ee, x] == x], ForAll[x, m[i[x], x] == ee]}], m[a, b], m[b, a]]
```

<!-- => Indeterminate -->
