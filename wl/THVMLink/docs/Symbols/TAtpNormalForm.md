---
Template: Symbol
Name: TAtpNormalForm
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpNormalForm
Keywords: [normal form, simplification, rewriting, ordered rewriting, completed system]
SeeAlso: [TAtpComplete, TAtpSystem, TAtpEqualQ, TKnuthBendixCompletion, ReplaceRepeated]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpNormalForm]()[*sys*, *term*]</code> gives the normal form of *term* under the [TAtpSystem]() *sys*.

<code>[TAtpNormalForm]()[*sys*, {*term*<sub>1</sub>, *term*<sub>2</sub>, ...}]</code> gives the normal forms of several terms in one call.

## Details & Options

- The normal form is found by ordered rewriting: a rule `l -> r` rewrites an instance of `l`, and an unorientable equation rewrites an instance in whichever direction makes the term smaller in the system's ordering.
- The symbols and numbers of *term* that do not occur in the axioms are constants, ordered below every symbol of the axioms and among themselves in canonical order, so that the normal form does not depend on how *term* is written.
- When the system is complete, equal terms have the same normal form; otherwise the normal form is equal to *term* but need not be unique.

## Basic Examples

Simplify a group expression under the completed group axioms:

```wl
TAtpNormalForm[TAtpComplete[{ForAll[{x, y, z}, m[m[x, y], z] == m[x, m[y, z]]], ForAll[x, m[ee, x] == x], ForAll[x, m[i[x], x] == ee]}], m[a, i[a]]]
```

<!-- => ee -->

---

Arguments of a commutative, associative operator come out in canonical order:

```wl
TAtpNormalForm[TAtpComplete[{ForAll[{x, y}, h[x, y] == h[y, x]], ForAll[{x, y, z}, h[h[x, y], z] == h[x, h[y, z]]]}], {h[c, h[b, a]], h[h[a, c], b]}]
```

<!-- => {h[a, h[b, c]], h[a, h[b, c]]} -->

## Scope

Heads and numbers outside the axioms are kept as they are:

```wl
TAtpNormalForm[TAtpComplete[{ForAll[{x, y, z}, m[m[x, y], z] == m[x, m[y, z]]], ForAll[x, m[ee, x] == x], ForAll[x, m[i[x], x] == ee]}], m[c, m[i[c], Power[y, 2]]]]
```

<!-- => y^2 -->
