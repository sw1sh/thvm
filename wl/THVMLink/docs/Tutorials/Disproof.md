---
Template: TechNote
Name: Disproof
Title: Disproving Conjectures with Countermodels
Context: WolframInstitute`THVMLink`ATP`
ContextPath: [Global`]
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Disproof
Keywords: [counterexample, countermodel, disproof, refutation, congruence closure, saturation, term rewriting, finite model, equational]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, SMT, TPTPImport]
---

A goal follows from a set of axioms exactly when no structure satisfies the axioms and violates the goal. A proof settles the question one way; a countermodel, a structure in which every axiom holds and the goal fails, settles it the other way. [TFindProof]() looks for both. Its default result is a [ProofObject](), and the `"Counterexample"` spec asks instead for a [CounterexampleObject](), the record of a countermodel:

| | |
| --- | --- |
| <code>[TFindProof]()[*goal*, *axioms*, "Counterexample"]</code> | a [CounterexampleObject]() refuting *goal*, or `$Failed` |
| <code>[TFindProof]()[*goal*, *axioms*, {"Status", "Counterexample"}]</code> | the outcome of the search together with the counterexample |
| <code>*co*["Model"]</code> | the refuting model of the counterexample *co* |
| <code>*co*["FalsificationFunction"]</code> | a function that evaluates *goal* in the model |
| <code>*co*["VerificationFunction"]</code> | a function that evaluates *axioms* in the model |

[TFindProof]() chooses the method by the shape of the problem: congruence closure for a goal and axioms without variables, and the completion engine for axioms with universally quantified variables.

## A refutation without a model

The built-in [FindEquationalProof]() already distinguishes a goal it refutes from one it cannot prove. Commutativity does not follow from associativity alone, and the result is a [Failure]() whose tag is `"PropositionFalse"`:

```wl
FindEquationalProof[ForAll[{x, y}, f[x, y] == f[y, x]], {ForAll[{x, y, z}, f[f[x, y], z] == f[x, f[y, z]]]}]
```

The [Failure]() says that the goal is false but does not show where it fails. A [CounterexampleObject]() carries the structure that makes it fail. Its model follows the form of the Wolfram Function Repository's <code>[ResourceFunction]()["FindFiniteModels"]</code>, whose port in THVMLink is [TFindFiniteModels](): an [Association]() from each operator to its table of values and from each constant to its value, over a domain whose elements are numbered from 0.

## A countermodel from completion

The examples below use the theory of an involution, a function `g` that undoes itself:

```wl
involution = {ForAll[x, g[g[x]] == x]}
```
<!-- => {ForAll[x, g[g[x]] == x]} -->

The goal `g[a] == a` does not follow, and [TFindProof]() gives `$Failed` for it:

```wl
TFindProof[g[a] == a, involution]
```
<!-- => $Failed -->

The status records how the search ended. The completion engine turned the axiom into the single rewrite rule `g[g[x]] -> x`, derived nothing further and stopped with the goal unproved:

```wl
TFindProof[g[a] == a, involution, "Status"]
```
<!-- => "Saturated" -->

A saturated rule set rewrites every term to a unique normal form, and two terms are equal in the theory exactly when their normal forms agree. The normal forms of `g[a]` and `a` differ, so the goal is refuted, and the `"Counterexample"` spec gives the refutation:

```wl
co = TFindProof[g[a] == a, involution, "Counterexample"]
```

The method records where the counterexample comes from:

```wl
co["Method"]
```
<!-- => "SaturationNormalForm" -->

The two normal forms that differ:

```wl
co["NormalForms"]
```
<!-- => {g[a], a} -->

The terms built from `a` with `g` have just two normal forms, `a` and `g[a]`, and they form a finite model with two elements: `a` is element 0, and the table `{1, 0}` of `g` sends 0 to 1 and 1 to 0:

```wl
co["Model"]
```
<!-- => <|g -> {1, 0}, a -> 0|> -->

The two sides of the goal take different values in the model:

```wl
co["Counterexample"]
```
<!-- => <|g[a] -> 1, a -> 0|> -->

## Checking a countermodel

A [CounterexampleObject]() checks itself. The falsification function is the goal with the model substituted in; the table of `g` becomes a [Part]() lookup, so its elements are numbered from 1 rather than 0:

```wl
co["FalsificationFunction"]
```
<!-- => Function[{}, g[a] == a /. {g -> ({2, 1}[[##1]] &), a -> 1}] -->

The goal is [False]() in the model:

```wl
co["FalsificationFunction"][]
```
<!-- => False -->

The verification function instantiates the axioms at every element of the domain:

```wl
co["VerificationFunction"]
```
<!-- => Function[{}, And @@ ({g[g[1]] == 1, g[g[2]] == 2} /. {g -> ({2, 1}[[##1]] &), a -> 1})] -->

The axioms are [True]() in the model:

```wl
co["VerificationFunction"][]
```
<!-- => True -->

## A theorem has no countermodel

Applying `g` four times gives back `a`, so `g[g[g[g[a]]]] == a` is a theorem, and the `"Counterexample"` spec gives `$Failed`:

```wl
TFindProof[g[g[g[g[a]]]] == a, involution, "Counterexample"]
```
<!-- => $Failed -->

The proof goes through:

```wl
TFindProof[g[g[g[g[a]]]] == a, involution]
```

A list of specs gives both outcomes of one search. For the refuted goal, the status is `"Saturated"` and the counterexample is present:

```wl
TFindProof[g[a] == a, involution, {"Status", "Counterexample"}]
```
<!-- => <|"Status" -> "Saturated", "Counterexample" -> CounterexampleObject["SaturationNormalForm", g[a] == a, {ForAll[x, g[g[x]] == x]}, <|...|>]|> -->

[All]() includes both the `"ProofObject"` and the `"Counterexample"` specs, and for a goal that the search decides, one of them is `$Failed`.

## Larger and infinite models

The size of the model is the number of distinct normal forms of the terms built from the constants of the goal. An idempotent `f`, with `f[f[x]] == f[x]`, does not force `f[a]` to equal another constant `b`, and the model has four elements, `a`, `b`, `f[a]` and `f[b]`:

```wl
TFindProof[f[a] == b, {ForAll[x, f[f[x]] == f[x]]}, "Counterexample"]
```

The table of `f` sends `a` (element 0) to `f[a]` (element 2) and fixes 2 and 3, which is what idempotence requires:

```wl
TFindProof[f[a] == b, {ForAll[x, f[f[x]] == f[x]]}, "Counterexample"]["Model"]
```
<!-- => <|f -> {2, 3, 2, 3}, a -> 0, b -> 1|> -->

When the normal forms are infinitely many, there is no finite table. A successor `s` with a left inverse `p`, `p[s[x]] == x`, does not make `s[s[z]]` equal to `z`; the terms `z`, `s[z]`, `s[s[z]]`, ... are all distinct normal forms. The counterexample is still found:

```wl
TFindProof[s[s[z]] == z, {ForAll[x, p[s[x]] == x]}, "Counterexample"]
```

Its model is then the completed rule set itself, under which the two sides have different normal forms:

```wl
TFindProof[s[s[z]] == z, {ForAll[x, p[s[x]] == x]}, "Counterexample"]["NormalForms"]
```
<!-- => {s[s[z]], z} -->

## Declining an unorientable equation

Commutativity cannot be oriented into a terminating rewrite rule: neither side of `x ⊗ y == y ⊗ x` is larger than the other. Completion can still saturate, by using such an equation only in the direction that decreases a given instance, but then distinct normal forms no longer prove that two terms differ. The counterexample is extracted only when every rule of the completed set is strictly size-reducing, so for a commutative theory the `"Counterexample"` spec gives `$Failed`, even though the goal is not a theorem:

```wl
TFindProof[a \[CircleTimes] b == b, {ForAll[{x, y}, x \[CircleTimes] y == y \[CircleTimes] x]}, "Counterexample"]
```
<!-- => $Failed -->

The search itself ends saturated, without a proof:

```wl
TFindProof[a \[CircleTimes] b == b, {ForAll[{x, y}, x \[CircleTimes] y == y \[CircleTimes] x]}, "Status"]
```
<!-- => "Saturated" -->

For theories like this one, [TFindFiniteModels]() enumerates finite models directly.

## Ground goals

A goal and axioms without variables are a ground problem, decided by congruence closure (described in [Congruence Closure and Lazy DPLL(T)](paclet:WolframInstitute/THVMLink/tutorial/SMT)). The decision is complete: every ground goal is either proved or refuted. That `a == c` does not follow from `a == b` alone:

```wl
TFindProof[a == c, {a == b}, "Counterexample"]
```

The model is the quotient of the terms by the hypotheses: `a` and `b` are one element and `c` is another:

```wl
TFindProof[a == c, {a == b}, "Counterexample"]["Model"]
```
<!-- => <|a -> 0, b -> 0, c -> 1|> -->

A goal that combines equations with [Implies](), [And](), [Or]() or [Not]() is decided by [TSmtDecide](), and its counterexample is a truth assignment to the equations:

```wl
TFindProof[Implies[a == b, a == c], {}, "Counterexample"]
```

The assignment makes `a == b` true and `a == c` false:

```wl
TFindProof[Implies[a == b, a == c], {}, "Counterexample"]["Model"]
```
<!-- => <|a == b -> True, a == c -> False|> -->

## Which countermodel to expect

| Problem | `"Method"` of the counterexample | Model |
| --- | --- | --- |
| ground equations | `"CongruenceClosure"` | a finite table over the equivalence classes |
| a ground Boolean combination of equations | `"DPLL(T)+CongruenceClosure"` | a truth assignment to the equations |
| axioms with variables, finitely many normal forms | `"SaturationNormalForm"` | a finite table over the normal forms |
| axioms with variables, infinitely many normal forms | `"SaturationNormalForm"` | the completed rewrite rules |
| axioms with an unorientable equation | none, `$Failed` | use [TFindFiniteModels]() |

For ground problems the `"Counterexample"` spec is a complete decision: a goal that is not proved is refuted. For axioms with variables it is a sound refutation whenever completion saturates into a terminating rewrite system, and it gives `$Failed` when completion relies on an unorientable equation. The completion engine itself is the subject of [Theorem Proving with THVMLink](paclet:WolframInstitute/THVMLink/tutorial/ATP).
