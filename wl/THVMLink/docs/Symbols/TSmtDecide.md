---
Template: Symbol
Name: TSmtDecide
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSmtDecide
Keywords: [SMT, DPLL(T), congruence closure, QF_UF, Boolean formula, decision procedure, satisfiability]
SeeAlso: [TSatEUF, TFindProof, SatisfiabilityInstances, CounterexampleObject]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Congruence Closure and Lazy DPLL(T)](SMT)"]
---

## Usage

<code>[TSmtDecide]()[*formula*]</code> decides whether the Boolean combination *formula* of equations between terms is satisfiable.

## Details & Options

- *formula* combines equations `lhs == rhs` and disequations `lhs != rhs` between terms with [And](), [Or](), [Not](), [Implies](), [Equivalent]() and [Xor](). The functions in the terms are uninterpreted, as in [TSatEUF]().
- For a satisfiable formula, [TSmtDecide]() gives `<|"Status" -> "SAT", "Model" -> model|>`, where *model* gives the truth value of each equation.
- For an unsatisfiable formula, it gives `<|"Status" -> "UNSAT"|>`.
- [TSmtDecide]() replaces each equation by a Boolean variable, finds a truth assignment with [SatisfiabilityInstances](), and checks it against the theory of equality with [TSatEUF](). An assignment that contradicts the theory is excluded, and the search continues.
- [TSmtDecide]() always terminates and is correct for both answers. A formula with *n* equations can need up to $2^n$ truth assignments.
- A formula is valid when its negation is unsatisfiable.

## Basic Examples

A formula that is unsatisfiable, since every case of the disjunction contradicts a disequation:

```wl
TSmtDecide[(a == b || b == c) && a != b && b != c]
```

<!-- => <|"Status" -> "UNSAT"|> -->

---

A satisfiable formula gives the truth value of each equation:

```wl
TSmtDecide[(a == b || c == d) && c == e]
```

<!-- => <|"Status" -> "SAT", "Model" -> <|a == b -> False, c == d -> True, c == e -> True|>|> -->

## Scope

A truth assignment that the theory of equality rules out is excluded:

```wl
TSmtDecide[((a == b && c == d) || x == y) && f[a, c] != f[b, d] && x != y]
```

<!-- => <|"Status" -> "UNSAT"|> -->

---

Equal arguments give equal values of a function:

```wl
TSmtDecide[a == b && f[a] != f[b]]
```

<!-- => <|"Status" -> "UNSAT"|> -->

---

[Implies]() is satisfiable when its premise is false:

```wl
TSmtDecide[Implies[a == b, f[a] == f[b]]]
```

<!-- => <|"Status" -> "SAT", "Model" -> <|a == b -> False, f[a] == f[b] -> False|>|> -->

---

[Equivalent]() is satisfiable when both sides are false:

```wl
TSmtDecide[Equivalent[a == b, b == c]]
```

<!-- => <|"Status" -> "SAT", "Model" -> <|a == b -> False, b == c -> False|>|> -->

---

[Xor]() of two equations is unsatisfiable when their right sides are equal:

```wl
TSmtDecide[Xor[a == b, a == c] && b == c]
```

<!-- => <|"Status" -> "UNSAT"|> -->

## Properties and Relations

The negation of a valid formula is unsatisfiable:

```wl
TSmtDecide[Not[Implies[a == b, f[a] == f[b]]]]
```

<!-- => <|"Status" -> "UNSAT"|> -->

---

[TSatEUF]() decides a conjunction of equations and disequations given as lists:

```wl
TSatEUF[{a == b}, {f[a] != f[b]}]
```

<!-- => <|"Status" -> "UNSAT", "Witness" -> f[a] != f[b]|> -->

## Possible Issues

Quantifiers are not interpreted, so a quantified equation is treated as a single Boolean variable:

```wl
TSmtDecide[ForAll[x, f[x] == x]]
```

<!-- => <|"Status" -> "SAT", "Model" -> <|f[x] == x -> True|>|> -->

---

An equation between numbers evaluates before [TSmtDecide]() sees it:

```wl
TSmtDecide[1 + 1 == 2 && a != b]
```

<!-- => <|"Status" -> "SAT", "Model" -> <|a != b -> True|>|> -->
