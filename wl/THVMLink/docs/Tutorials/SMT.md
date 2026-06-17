---
Template: TechNote
Name: SMT
Title: Congruence Closure and Lazy DPLL(T)
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/SMT
Keywords: [SMT, congruence closure, QF_UF, DPLL(T), Downey-Sethi-Tarjan, TPTP, equational, decision procedure]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, Overview, Disproof]
---

## What the SMT surface covers

[TFindProof](paclet:WolframInstitute/THVMLink/ref/TFindProof) (covered in the [ATP](paclet:WolframInstitute/THVMLink/tutorial/ATP) tech note) is an unfailing Knuth-Bendix completion engine: it handles universally-quantified equational axioms by saturating critical pairs. That is the right tool when axioms have free variables, e.g. <code>and[x_,y_] == and[y_,x_]</code>.

When the conjecture and every axiom are *ground* - no variables - the problem collapses to QF_UF (quantifier-free first-order equality with uninterpreted function symbols). Congruence closure decides QF_UF in near-linear time, whereas the saturator may spin on irrelevant critical pairs until the wall budget runs out. <code>THVMLink\`ATP\`</code> exposes the fast path through three entries:

- [TSatEUF]() - raw decision procedure over a list of equalities and disequalities.
- [TSmtDecide]() - DPLL(T) lift to arbitrary Boolean combinations of equality atoms.
- [TFindProof]() with `Method -> "SMT"` - the entailment surface that reduces `H |= G` to satisfiability of `H && !G`, deciding it by congruence closure.

TPTP problem files drop in directly: `File["...p"]` or a `cnf/fof` string at [TFindProof]() (with `Method -> "SMT"` for the ground SMT path) triggers the shared TPTP parser before dispatch.

## Setting up

The SMT entries live alongside the equational engine. One `Needs` brings them all in:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
```

All examples below assume this entry has run. Equality atoms are written with `==`, disequalities with `!=`. The engines accept any combination of undefined symbols (`a`, `b`, ...) and compound function applications (`f[a, b]`) - everything in sight is treated as an uninterpreted term.

## Pure congruence closure

[TSatEUF]() takes two lists - equalities and disequalities - and returns either a counter-example witness or the inferred equivalence classes:

```wl
TSatEUF[{a == b, b == c}, {a != c}]
```
<!-- => <|"Status" -> "UNSAT", "Witness" -> a != c|> -->

```wl
TSatEUF[{a == b}, {f[a] != f[b]}]
```
<!-- => <|"Status" -> "UNSAT", "Witness" -> f[a] != f[b]|> -->

The UNSAT verdict on the second call is the standard congruence-closure outcome: the union-find sees `a` and `b` collapse, then walks the use-lists and notices that `f[a]` and `f[b]` are now congruent. A satisfiable input returns the inferred classes for every subterm seen:

```wl
TSatEUF[{a == b, c == d}, {}]
```
<!-- => <|"Status" -> "SAT", "Classes" -> {{a, b}, {c, d}}|> -->

The implementation is Downey-Sethi-Tarjan: each subterm starts in its own union-find class with a use-list of compound parents whose arguments mention it; each merge propagates congruence; path-compressed find plus union-by-rank give the near-linear bound.

## Boolean combinations

[TSmtDecide]() lifts the same theory solver to Boolean combinations of equality atoms via `And`, `Or`, `Not`, `Implies`, `Equivalent`, `Xor`:

```wl
TSmtDecide[(a == b || b == c) && a != b && b != c]
```
<!-- => <|"Status" -> "UNSAT"|> -->

```wl
TSmtDecide[((a == b && c == d) || x == y) && f[a, c] != f[b, d] && x != y]
```
<!-- => <|"Status" -> "UNSAT"|> -->

The second example exercises the theory-conflict path: the SAT shell picks the `(a==b && c==d)` disjunct as the candidate model, congruence closure fires inside it and discovers `f[a, c] == f[b, d]`, which contradicts the stated disequality; the alternative `x == y` branch is already excluded; UNSAT.

A SAT verdict comes with a satisfying assignment:

```wl
TSmtDecide[(a == b || c == d) && c == e]
```
<!-- => <|"Status" -> "SAT", "Model" -> <|...|>|> -->

The algorithm is the textbook lazy DPLL(T) shell: each equality/disequality atom is abstracted as a fresh propositional variable; the Boolean abstraction is handed to Wolfram's <code>[SatisfiabilityInstances]()</code> as the propositional kernel; every candidate model is theory-checked via [TSatEUF](); on T-conflict the exact assignment is forbidden by a blocking clause and the kernel is re-queried. Loop terminates - there are <code>2^|atoms|</code> assignments and each iteration prunes one. Sound and complete for QF_UF.

## Entailment surface

The ground entailment surface is reached through [TFindProof](): `Method -> "SMT"` reduces `H1, ..., Hn |= G` to a satisfiability query on `H1 && ... && Hn && !G` and decides it by congruence closure (or DPLL(T) for a Boolean goal). It returns a `"Proved"` decision Association when the entailment holds:

```wl
TFindProof[a == c, {a == b, b == c}, Method -> "SMT"]
```
<!-- => <|"Status" -> "Proved", "Method" -> "CongruenceClosure", ...|> -->

```wl
TFindProof[Implies[a == b && b == c, a == c], {}, Method -> "SMT"]
```
<!-- => <|"Status" -> "Proved", "Method" -> "DPLL(T)+CongruenceClosure", ...|> -->

A non-entailment is refuted with a [CounterexampleObject]() whose `"Model"` is the congruence-closure quotient as a finite algebra in [FindFiniteModels]() structure ({a, b} merged to element 0, c apart as element 1). The `"Counterexample"` output kind asks for it directly:

```wl
TFindProof[a == c, {a == b}, "Counterexample"]
```
<!-- => CounterexampleObject[<|"Domain" -> 2, "Model" -> <|a -> 0, b -> 0, c -> 1|>, ...|>] -->

The [Disproof](paclet:WolframInstitute/THVMLink/tutorial/Disproof) tech note covers the `CounterexampleObject` and the quantified case. The `"Method"` field of the decision Association discloses which engine handled the call: pure congruence closure for equality-literal goals, DPLL(T) + congruence closure for Boolean-combination goals.

## TPTP overloads

`TFindProof[File["...p"], Method -> "SMT"]` and `TFindProof["...cnf/fof string...", Method -> "SMT"]` route through the same TPTP parser the equational engine uses, then dispatch through congruence closure:

```wl
TFindProof[
    "cnf(a1, axiom, a = b).
     cnf(a2, axiom, b = c).
     cnf(g, negated_conjecture, a != c).", Method -> "SMT"]
```
<!-- => <|"Status" -> "Proved", ...|> -->

```wl
TFindProof[
    "fof(a1, axiom, a = b).
     fof(g, negated_conjecture, f(a) != f(b)).", Method -> "SMT"]
```
<!-- => <|"Status" -> "Proved", ...|> -->

A ground-gate rejects inputs with universally-quantified variables (`Pattern` / `Blank` heads) - congruence closure is a quantifier-free decision procedure:

```wl
TFindProof[
    "cnf(a1, axiom, and(X, Y) = and(Y, X)).
     cnf(g, negated_conjecture, and(a, b) != and(b, a)).", Method -> "SMT"]
```
<!-- => TFindProof::nonground; reach for the default completion engine -->

The parser succeeded; only the SMT dispatch refused. For variable-bearing axioms drop `Method -> "SMT"` and use the default completion engine.

## `Method -> "SMT"` in `TFindProof`

A single suboption turns the standard equational entry into an SMT dispatch:

```wl
TFindProof[a == c, {a == b, b == c}, Method -> "SMT"]
```
<!-- => <|"Status" -> "Proved", "Method" -> "CongruenceClosure", ...|> -->

```wl
TFindProof[Implies[a == b && b == c, a == c], {}, Method -> "SMT"]
```
<!-- => <|"Status" -> "Proved", "Method" -> "DPLL(T)+CongruenceClosure", ...|> -->

Default `TFindProof` (no `Method`) still goes to the saturator and returns a Wolfram `ProofObject`. `Method -> "SMT"` returns the SMT Association directly, not a `ProofObject` - the result shape is different on purpose, since the SMT-style witness (a disequality whose two sides collapsed) is not a proof tree.

## Choosing the right entry

| Input shape                                    | Use                                              |
|------------------------------------------------|--------------------------------------------------|
| Ground equational, single goal                 | `TFindProof[g, h, Method -> "SMT"]`              |
| Ground, Boolean combination of (in)equalities  | [TSmtDecide]() or `TFindProof[g, h, Method -> "SMT"]` |
| Variable-bearing equational axioms             | [TFindProof]() (default)                         |
| TPTP UEQ benchmark with universal vars         | `TFindProof[File["...p"]]`                       |
| TPTP ground problem                            | `TFindProof[File["...p"], Method -> "SMT"]`      |

## Scope and limits

Decided here:

- QF_UF (ground first-order equality plus uninterpreted functions). Full decision procedure - returns SAT or UNSAT in finite time.
- Boolean combinations of equality atoms via lazy DPLL(T). Same guarantee.

Not decided:

- Quantified formulas. Reach for [TFindProof]() for the equational fragment.
- Linear arithmetic, arrays, bit-vectors. Separate theory solvers, not yet ported; could plug behind the same DPLL(T) shell.
- Anything where the propositional structure has exponentially many candidate models: lazy DPLL(T) pays for each one. An eager T-propagation pass that pushes congruence facts into the SAT kernel as additional clauses is the natural extension.

For how this single-theory core compares to the broader field - general SMT solvers (Z3, CVC5), first-order ATPs (Vampire, E), and unit-equational provers (Waldmeister, Twee) - see the positioning survey in `src/atp/survey.md`, which lays out the feature matrices honestly: thvm decides the EUF theory completely, and routes everything quantified to the completion engine rather than to SMT.
