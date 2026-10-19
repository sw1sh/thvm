---
Template: Symbol
Name: TSatEUF
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSatEUF
Keywords: [SMT, congruence closure, uninterpreted functions, QF_UF, decision procedure, equality, satisfiability]
SeeAlso: [TSmtDecide, TFindProof, CounterexampleObject, SatisfiabilityInstances]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Congruence Closure and Lazy DPLL(T)](SMT)"]
---

## Usage

<code>[TSatEUF]()[*equations*, *disequations*]</code> decides whether the list of *equations* and the list of *disequations* between terms can hold together, by congruence closure.

## Details & Options

- [TSatEUF]() decides the quantifier-free theory of equality with uninterpreted functions (QF_UF): the terms are symbols and applications of functions about which nothing is known except that equal arguments give equal values.
- *equations* is a list of `lhs == rhs` and *disequations* a list of `lhs != rhs`. Their [Inactive]() forms are accepted too.
- [TSatEUF]() merges the terms that the equations make equal, together with every pair of applications `f[x1, ...]` and `f[y1, ...]` whose arguments are equal. The input is unsatisfiable when a disequation joins two terms of the same class.
- For unsatisfiable input, [TSatEUF]() gives `<|"Status" -> "UNSAT", "Witness" -> diseq|>`, with a disequation that contradicts the equations.
- For satisfiable input, it gives `<|"Status" -> "SAT", "Classes" -> classes|>`, with the classes of equal terms.
- [TSatEUF]() always terminates and is correct for both answers.
- [TSmtDecide]() decides Boolean combinations of equations, and <code>[TFindProof]()[*goal*, *hypotheses*, [Method]() -> "SMT"]</code> decides whether a ground equation follows from ground equations.

## Basic Examples

`a == b` and `b == c` contradict `a != c`:

```wl
TSatEUF[{a == b, b == c}, {a != c}]
```

<!-- => <|"Status" -> "UNSAT", "Witness" -> a != c|> -->

---

Two equations that do not contradict anything give the classes of equal terms:

```wl
TSatEUF[{a == b, c == d}, {}]
```

<!-- => <|"Status" -> "SAT", "Classes" -> {{a, b}, {c, d}}|> -->

## Scope

Equal arguments give equal values of a function:

```wl
TSatEUF[{a == b}, {f[a] != f[b]}]
```

<!-- => <|"Status" -> "UNSAT", "Witness" -> f[a] != f[b]|> -->

---

If applying `f` three times and five times both give back `a`, then `f[a] == a`:

```wl
TSatEUF[{f[f[f[a]]] == a, f[f[f[f[f[a]]]]] == a}, {f[a] != a}]
```

<!-- => <|"Status" -> "UNSAT", "Witness" -> f[a] != a|> -->

---

A disequation between terms of different classes is satisfiable:

```wl
TSatEUF[{a == b, c == d}, {a != c}]
```

<!-- => <|"Status" -> "SAT", "Classes" -> {{a, b}, {c, d}}|> -->

---

With no equations, every term is in a class of its own:

```wl
TSatEUF[{}, {a != b}]
```

<!-- => <|"Status" -> "SAT", "Classes" -> {{a}, {b}}|> -->

---

The [Inactive]() forms of [Equal]() and [Unequal]() are accepted:

```wl
TSatEUF[{Inactive[Equal][a, b]}, {Inactive[Unequal][a, b]}]
```

<!-- => <|"Status" -> "UNSAT", "Witness" -> a != b|> -->

## Properties and Relations

[TSmtDecide]() decides the same conjunction written as a formula:

```wl
TSmtDecide[a == b && b == c && a != c]
```

<!-- => <|"Status" -> "UNSAT"|> -->

---

The `"SMT"` method of [TFindProof]() proves a ground equation from ground equations:

```wl
TFindProof[f[a] == f[c], {a == b, b == c}, Method -> "SMT"]
```

<!-- => <|"Status" -> "Proved", "Method" -> "CongruenceClosure", "Goal" -> f[a] == f[c], "Hypotheses" -> {a == b, b == c}, "Witness" -> f[a] != f[c]|> -->

## Possible Issues

Functions are uninterpreted, so [Plus]() is treated as an unknown function:

```wl
TSatEUF[{x + y == z}, {}]
```

<!-- => <|"Status" -> "SAT", "Classes" -> {{x + y, z}, {x}, {y}}|> -->

---

An equation between numbers evaluates before [TSatEUF]() sees it, here to [True](), and leaves no classes:

```wl
TSatEUF[{1 + 1 == 2}, {}]
```

<!-- => <|"Status" -> "SAT", "Classes" -> {}|> -->

---

The equations and disequations have to be given as lists, and other input returns unevaluated:

```wl
TSatEUF[a == b, a != b]
```

<!-- => TSatEUF[a == b, a != b] -->
