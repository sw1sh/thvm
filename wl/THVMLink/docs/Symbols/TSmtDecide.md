---
Template: Symbol
Name: TSmtDecide
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSmtDecide
Keywords: [SMT, DPLL(T), congruence closure, QF_UF, Boolean combination, decision procedure]
SeeAlso: [TSatEUF, TFindProof, SatisfiabilityInstances]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TSmtDecide]()[$formula$]</code> decides the satisfiability of a Boolean combination of equality atoms by lazy DPLL(T) over congruence closure.

$formula$ is any combination of `Equal[lhs, rhs]` / `Unequal[lhs, rhs]` atoms via `And`, `Or`, `Not`, `Implies`, `Equivalent`, `Xor`.  Returns `<|"Status" -> "SAT", "Model" -> assoc|>` with a satisfying assignment of the equality atoms, or `<|"Status" -> "UNSAT"|>` when no assignment works.

## Details & Options

- Each equality / disequality atom is abstracted as a fresh propositional variable.  The Boolean abstraction is handed to Wolfram's [SatisfiabilityInstances]() as the propositional kernel; every candidate model is theory-checked through [TSatEUF]().  On a T-conflict the exact assignment is forbidden by a blocking clause and the kernel is re-queried.
- The loop terminates: there are <code>2^|atoms|</code> assignments and each iteration prunes one.  Sound and complete for QF_UF.
- Operates over Wolfram's `Equal` / `Unequal` heads directly (no `Inactive` wrapping required).
- For raw congruence closure on a flat (in)equality list, use [TSatEUF]().  For ground entailment queries, use [TFindProof]() with `Method -> "SMT"` (decision) or the `"Counterexample"` output kind (refuting model).  For variable-bearing axioms reach for [TFindProof]()'s default completion engine.

## Basic Examples

A simple UNSAT case (transitivity collision):

```wl
TSmtDecide[(a == b || b == c) && a != b && b != c]
```
<!-- => <|"Status" -> "UNSAT"|> -->

The theory-conflict path exercised inside a disjunction:

```wl
TSmtDecide[((a == b && c == d) || x == y) && f[a, c] != f[b, d] && x != y]
```
<!-- => <|"Status" -> "UNSAT"|> -->

A SAT verdict carries a satisfying assignment:

```wl
TSmtDecide[(a == b || c == d) && c == e]
```
<!-- => <|"Status" -> "SAT", "Model" -> <|...|>|> -->

## Properties & Relations

- [TSatEUF]() is the inner theory solver.  [TSmtDecide]() wraps it in the lazy DPLL(T) loop.
- [TFindProof]() with `Method -> "SMT"` rephrases entailment `H1, ..., Hn |= G` as satisfiability of <code>H1 && ... && Hn && !G</code> and dispatches to either [TSatEUF]() (single equality literal goal) or [TSmtDecide]() (Boolean goal).
- The propositional shell is Wolfram's [SatisfiabilityInstances](); the implementation does no Tseitin transformation, so deeply-nested formulas with many atoms can hit the <code>2^|atoms|</code> worst case before all conflicts are blocked.

## Possible Issues

- Quantified formulas are out of scope.  Reach for [TFindProof]() (unfailing Knuth-Bendix completion) for the equational fragment with universal axioms.
- Equality atoms on built-in operators (e.g. `1 + 1 == 2`) are NOT interpreted: congruence closure treats `Plus` as uninterpreted.  Mix integers in only if the formula does not rely on arithmetic.
- A formula whose propositional kernel has exponentially many candidate models takes proportional time, since the lazy approach pays for each one.  An eager T-propagation pass would amortise this; not currently implemented.
