---
Template: TechNote
Name: SMT
Title: Congruence Closure and Lazy DPLL(T)
Context: WolframInstitute`THVMLink`ATP`
ContextPath: [Global`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/SMT
Keywords: [SMT, satisfiability modulo theories, congruence closure, QF_UF, DPLL(T), Downey-Sethi-Tarjan, TPTP, equational, decision procedure]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, Disproof, TPTPImport]
---

A ground problem is one whose equations contain no variables. Whether a ground goal follows from ground hypotheses is a question in QF_UF, the quantifier-free theory of equality with uninterpreted functions, and congruence closure decides it completely, in time close to linear in the number of subterms. The completion engine that [TFindProof]() uses by default is built for axioms with universally quantified variables; on a ground problem it can spend its time on critical pairs that do not matter. Three functions decide ground problems directly, in the manner of an SMT (satisfiability modulo theories) solver:

| | |
| --- | --- |
| <code>[TSatEUF]()[*equalities*, *disequalities*]</code> | decide whether ground equalities and disequalities can hold together |
| <code>[TSmtDecide]()[*formula*]</code> | decide whether a Boolean combination of ground equations can hold |
| <code>[TFindProof]()[*goal*, *hypotheses*, [Method]() -> "SMT"]</code> | decide whether *goal* follows from ground *hypotheses* |

Every symbol is uninterpreted: `a`, `b` and compound terms such as `f[a, b]` stand for unknown values of unknown functions, related only by the equations given.

## Congruence closure

The examples share one list of hypotheses:

```wl
hyps = {a == b, b == c}
```
<!-- => {a == b, b == c} -->

[TSatEUF]() takes a list of equalities and a list of disequalities. Adding the disequality `a != c` to the hypotheses makes them contradictory, and the result names the disequality that fails:

```wl
TSatEUF[hyps, {a != c}]
```
<!-- => <|"Status" -> "UNSAT", "Witness" -> a != c|> -->

A function gives equal values on equal arguments, so `f[a] != f[c]` contradicts the hypotheses too. This is the congruence step: once `a` and `c` are in one class, `f[a]` and `f[c]` are merged as well:

```wl
TSatEUF[hyps, {f[a] != f[c]}]
```
<!-- => <|"Status" -> "UNSAT", "Witness" -> f[a] != f[c]|> -->

A satisfiable input gives the equivalence classes of its subterms:

```wl
TSatEUF[hyps, {a != d}]
```
<!-- => <|"Status" -> "SAT", "Classes" -> {{a, b, c}, {d}}|> -->

The algorithm is that of Downey, Sethi and Tarjan. Each subterm starts in a class of its own, and merging two classes also merges the compound terms whose arguments have become equal. With path compression and union by rank, the running time is close to linear.

## Boolean combinations

[TSmtDecide]() decides formulas built from equations and disequations with [And](), [Or](), [Not](), [Implies](), [Equivalent]() and [Xor](). Each disjunct of this formula is ruled out by one of the disequations:

```wl
TSmtDecide[(a == b || b == c) && a != b && b != c]
```
<!-- => <|"Status" -> "UNSAT"|> -->

A satisfiable formula comes with a truth value for each of its atoms:

```wl
TSmtDecide[(a == b || c == d) && c == e]
```
<!-- => <|"Status" -> "SAT", "Model" -> <|a == b -> False, c == d -> True, c == e -> True|>|> -->

The procedure is lazy DPLL(T), the Davis-Putnam-Logemann-Loveland search modulo a theory T. Each atom is replaced by a propositional variable, [SatisfiabilityInstances]() proposes a truth assignment for the propositional formula, and [TSatEUF]() checks the equations that assignment asserts. An assignment that contradicts them is excluded by a blocking clause, and the next one is requested; there are finitely many assignments, so the search ends. Here the propositional formula is satisfiable, but every assignment that satisfies it contradicts congruence, since `a == b` and `c == d` force `f[a, c] == f[b, d]`:

```wl
TSmtDecide[((a == b && c == d) || x == y) && f[a, c] != f[b, d] && x != y]
```
<!-- => <|"Status" -> "UNSAT"|> -->

A formula is valid when its negation is unsatisfiable. That `a == b` implies `f[a] == f[b]` is valid:

```wl
TSmtDecide[!Implies[a == b, f[a] == f[b]]]
```
<!-- => <|"Status" -> "UNSAT"|> -->

## Entailment

The `"SMT"` method of [TFindProof]() decides whether a goal follows from hypotheses, by deciding whether the hypotheses together with the negated goal are satisfiable. When they are not, the goal is proved, and the negated goal is recorded as the witness:

```wl
TFindProof[a == c, hyps, Method -> "SMT"]
```
<!-- => <|"Status" -> "Proved", "Method" -> "CongruenceClosure", "Goal" -> a == c, "Hypotheses" -> {a == b, b == c}, "Witness" -> a != c|> -->

Congruence carries the hypotheses through functions:

```wl
TFindProof[f[a] == f[c], hyps, Method -> "SMT"]
```
<!-- => <|"Status" -> "Proved", "Method" -> "CongruenceClosure", "Goal" -> f[a] == f[c], "Hypotheses" -> {a == b, b == c}, "Witness" -> f[a] != f[c]|> -->

When the goal does not follow, the result is a [CounterexampleObject](), a finite model in which the hypotheses hold and the goal fails:

```wl
TFindProof[a == d, hyps, Method -> "SMT"]
```

The `"Counterexample"` spec gives the same object without the method setting, since [TFindProof]() uses congruence closure for a ground problem on its own:

```wl
co = TFindProof[a == d, hyps, "Counterexample"]
```

Its model is the quotient of the terms by the hypotheses: a domain of two elements, with `a`, `b` and `c` in one and `d` in the other:

```wl
co["Model"]
```
<!-- => <|a -> 0, b -> 0, c -> 0, d -> 1|> -->

The counterexample checks itself. Its verification function evaluates the hypotheses in the model:

```wl
co["VerificationFunction"][]
```
<!-- => True -->

Its falsification function evaluates the goal in the model:

```wl
co["FalsificationFunction"][]
```
<!-- => False -->

A Boolean goal is decided by DPLL(T), as the `"Method"` entry records:

```wl
TFindProof[Implies[a == b && b == c, a == c], {}, Method -> "SMT"]
```
<!-- => <|"Status" -> "Proved", "Method" -> "DPLL(T)+CongruenceClosure", "Goal" -> Implies[a == b && b == c, a == c], "Hypotheses" -> {}|> -->

A Boolean goal that does not follow is refuted by a truth assignment to its atoms:

```wl
TFindProof[Implies[a == b, a == c], {}, Method -> "SMT"]
```

Without the method setting, [TFindProof]() proves a ground goal with its completion engine and gives a [ProofObject]():

```wl
TFindProof[a == c, hyps]
```

The `"SMT"` method gives an [Association]() instead of a [ProofObject]() because its evidence is a contradiction found by congruence closure, not a chain of rewrites.

## Problems in the TPTP format

A problem in the TPTP (Thousands of Problems for Theorem Provers) format can be given to [TFindProof]() as text. With the `"SMT"` method, a ground problem in clause normal form (`cnf`) is decided by congruence closure:

```wl
TFindProof["cnf(a1, axiom, a = b).
cnf(a2, axiom, b = c).
cnf(g, negated_conjecture, a != c).", Method -> "SMT"]
```
<!-- => <|"Status" -> "Proved", "Method" -> "CongruenceClosure", "Goal" -> a == c, "Hypotheses" -> {a == b, b == c}, "Witness" -> a != c|> -->

A problem in first-order form (`fof`) is decided the same way:

```wl
TFindProof["fof(a1, axiom, a = b).
fof(g, conjecture, f(a) = f(b)).", Method -> "SMT"]
```
<!-- => <|"Status" -> "Proved", "Method" -> "CongruenceClosure", "Goal" -> f[a] == f[b], "Hypotheses" -> {a == b}, "Witness" -> f[a] != f[b]|> -->

A problem with variables is not ground, and the `"SMT"` method gives `$Failed` for it:

```wl
TFindProof["cnf(a1, axiom, and(X, Y) = and(Y, X)).
cnf(g, negated_conjecture, and(a, b) != and(b, a)).", Method -> "SMT"]
```
<!-- => $Failed -->

Without the method setting, the completion engine proves the same problem:

```wl
TFindProof["cnf(a1, axiom, and(X, Y) = and(Y, X)).
cnf(g, negated_conjecture, and(a, b) != and(b, a)).", "Status"]
```
<!-- => "Proved" -->

[Importing TPTP Benchmarks](paclet:WolframInstitute/THVMLink/tutorial/TPTPImport) covers the format in more detail.

## Choosing a function

| Problem | Function |
| --- | --- |
| ground equalities and disequalities | <code>[TSatEUF]()[*equalities*, *disequalities*]</code> |
| a Boolean combination of ground equations | <code>[TSmtDecide]()[*formula*]</code> |
| a ground goal and ground hypotheses | <code>[TFindProof]()[*goal*, *hypotheses*, [Method]() -> "SMT"]</code> |
| a refuting model for a ground goal | <code>[TFindProof]()[*goal*, *hypotheses*, "Counterexample"]</code> |
| equations with variables | <code>[TFindProof]()[*goal*, *axioms*]</code> |
| a ground TPTP problem | <code>[TFindProof]()["*text*", [Method]() -> "SMT"]</code> |
| a TPTP problem with variables | <code>[TFindProof]()["*text*"]</code> |

## Scope

Congruence closure and DPLL(T) decide the quantifier-free theory of equality with uninterpreted functions, and Boolean combinations over it, completely: every input gets a SAT or an UNSAT verdict. Quantified formulas are outside that theory; for equations with variables, [TFindProof]() uses its completion engine, described in [Theorem Proving with THVMLink](paclet:WolframInstitute/THVMLink/tutorial/ATP). Other theories, such as linear arithmetic, arrays and bit vectors, are not decided. DPLL(T) checks one truth assignment at a time, so a formula whose Boolean structure admits very many assignments can take correspondingly long. Refuting models are the subject of [Disproving Conjectures with Countermodels](paclet:WolframInstitute/THVMLink/tutorial/Disproof).
