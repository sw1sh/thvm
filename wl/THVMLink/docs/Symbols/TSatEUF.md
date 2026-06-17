---
Template: Symbol
Name: TSatEUF
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSatEUF
Keywords: [SMT, congruence closure, QF_UF, decision procedure, Downey-Sethi-Tarjan, equality]
SeeAlso: [TSmtDecide, TFindProof, SatisfiabilityInstances]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TSatEUF]()[$equalities$, $disequalities$]</code> decides the quantifier-free first-order theory of equality with uninterpreted functions (QF_UF) via congruence closure.

$equalities$ is a list of `Equal[lhs, rhs]` literals; $disequalities$ is a list of `Unequal[lhs, rhs]` literals.  Returns an Association: `<|"Status" -> "UNSAT", "Witness" -> diseq|>` when the union of equalities collapses some disequality, or `<|"Status" -> "SAT", "Classes" -> {{...}, ...}|>` with the inferred equivalence classes of every subterm seen.

## Details & Options

- The implementation is Downey-Sethi-Tarjan: each subterm starts in its own union-find class with a use-list of compound parents whose arguments mention it.  Each merge unions two classes and propagates congruence: for every pair `(f[x1, ..., xn], f[y1, ..., yn])` in the merged class's use-list with matching head and arity, if all `xi` and `yi` are equivalent the parents merge too.  Path-compressed find + union-by-rank give the standard near-linear bound.
- Sound and complete decision procedure for QF_UF.  Always terminates.
- Atoms are uninterpreted: symbols (`a`, `b`, ...), nullary functions (`f[]`), and compound applications (`f[a, b]`) are all treated the same way.  No theory-specific reasoning beyond equality plus congruence.
- For Boolean combinations of equality atoms, see [TSmtDecide]().  For ground entailment queries, use [TFindProof]() with `Method -> "SMT"` (decision) or the `"Counterexample"` output kind (refuting model).  For variable-bearing axioms reach for [TFindProof]()'s default unfailing Knuth-Bendix completion engine.

## Basic Examples

The transitivity case:

```wl
TSatEUF[{a == b, b == c}, {a != c}]
```
<!-- => <|"Status" -> "UNSAT", "Witness" -> a != c|> -->

Congruence propagation on a single merge:

```wl
TSatEUF[{a == b}, {f[a] != f[b]}]
```
<!-- => <|"Status" -> "UNSAT", "Witness" -> f[a] != f[b]|> -->

A satisfiable input returns the inferred classes:

```wl
TSatEUF[{a == b, c == d}, {}]
```
<!-- => <|"Status" -> "SAT", "Classes" -> {{a, b}, {c, d}}|> -->

## Properties & Relations

- [TSmtDecide]() is the natural lift to Boolean combinations of equality atoms via lazy DPLL(T).  The propositional kernel is Wolfram's [SatisfiabilityInstances](); each candidate model is theory-checked here through [TSatEUF]().
- [TFindProof]() with `Method -> "SMT"` reduces entailment to satisfiability: `H1, ..., Hn |= G` becomes `H1 && ... && Hn && !G`, then dispatched to [TSmtDecide]() (Boolean goal) or [TSatEUF]() (equality-literal goal).
- For variable-bearing equational axioms (universally-quantified), congruence closure is the wrong tool: reach for the unfailing Knuth-Bendix completion at [TFindProof]() instead.

## Possible Issues

- Inputs are typed as Wolfram `Equal` / `Unequal` heads, not `Inactive[Equal]` / `Inactive[Unequal]`.  The decision procedure handles both forms, but the canonical input convention is bare `==` / `!=`.
- Symbols carry no semantic meaning to congruence closure; arithmetic, lists, and other built-in operations are not interpreted.  `TSatEUF[{1 + 1 == 2}, {}]` reports the inferred classes literally - it does not know that `1 + 1` equals `2`.
