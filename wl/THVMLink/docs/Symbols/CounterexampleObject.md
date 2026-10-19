---
Template: Symbol
Name: CounterexampleObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/CounterexampleObject
Keywords: [counterexample, countermodel, disproof, refutation, finite model, congruence closure, saturation, equational logic]
SeeAlso: [TFindProof, TFindFiniteModels, TSatEUF, TSmtDecide, ProofObject]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Disproving Conjectures with Countermodels](Disproof)"]
---

## Usage

<code>[CounterexampleObject]()[*method*, *proposition*, *axioms*, *data*]</code> represents a counterexample, found by *method*, showing that *proposition* does not follow from *axioms*, with the refuting model in the Association *data*.

## Details & Options

- [CounterexampleObject]() is what <code>[TFindProof]()[*conjecture*, *axioms*, "Counterexample"]</code> gives for a conjecture that does not follow from the axioms. It is the counterpart of the [ProofObject]() that [TFindProof]() gives for a theorem.
- [CounterexampleObject]() displays as a summary box.
- The *method* is `"CongruenceClosure"` for a ground problem, whose model is a finite quotient of the ground terms, or `"SaturationNormalForm"` for a problem with variables, whose model comes from rewrite rules that separate the two sides of the conjecture.
- <code>*co*["*prop*"]</code> gives a property of the counterexample *co*. The properties are:

| | |
| --- | --- |
| `"Status"` | `"Refuted"` |
| `"Method"` | the method that found the counterexample |
| `"Proposition"`, `"Goal"` | the refuted proposition |
| `"Axioms"`, `"Hypotheses"` | the axioms |
| `"Model"`, `"Setup"` | the refuting model |
| `"Counterexample"`, `"Witness"` | the values of the two sides of the proposition in the model |
| `"Domain"` | the number of elements of the model |
| `"NormalForms"` | the normal forms of the two sides, for `"SaturationNormalForm"` |
| `"FalsificationFunction"` | a function of no arguments that evaluates the proposition in the model |
| `"VerificationFunction"` | a function of no arguments that evaluates the axioms in the model |
| `"Data"` | the Association *data* |
| `"Properties"` | the list of properties |

- The model gives a table of values for each operator and a value for each constant, over the elements `0`, `1`, ..., *k* - 1 of a domain of size *k*.
- The falsification function gives [False]() and the verification function gives [True](), so the counterexample checks itself.
- An unknown property gives a [Missing]() value.

## Basic Examples

A counterexample showing that `a == c` does not follow from `a == b`:

```wl
CounterexampleObject["CongruenceClosure", a == c, {a == b}, <|"Setup" -> <|a -> 0, b -> 0, c -> 1|>, "Domain" -> 2, "Counterexample" -> <|a -> 0, c -> 1|>|>]
```

<!-- => a CounterexampleObject summary box, method CongruenceClosure -->

---

[TFindProof]() finds the same counterexample:

```wl
TFindProof[a == c, {a == b}, "Counterexample"]
```

<!-- => a CounterexampleObject summary box, method CongruenceClosure -->

## Scope

Refute a conjecture over an involution `g`:

```wl
co = TFindProof[g[a] == a, {ForAll[x, g[g[x]] == x]}, "Counterexample"]
```

<!-- => a CounterexampleObject summary box, method SaturationNormalForm -->

The status of the conjecture:

```wl
co["Status"]
```

<!-- => "Refuted" -->

The method that found it:

```wl
co["Method"]
```

<!-- => "SaturationNormalForm" -->

The refuted proposition:

```wl
co["Proposition"]
```

<!-- => g[a] == a -->

The axioms:

```wl
co["Axioms"]
```

<!-- => {ForAll[x, g[g[x]] == x]} -->

The model, with a table for `g` and a value for `a` over two elements:

```wl
co["Model"]
```

<!-- => <|g -> {1, 0}, a -> 0|> -->

The size of the domain:

```wl
co["Domain"]
```

<!-- => 2 -->

The two sides of the proposition take different values in the model:

```wl
co["Counterexample"]
```

<!-- => <|g[a] -> 1, a -> 0|> -->

The normal forms of the two sides under the completed rules:

```wl
co["NormalForms"]
```

<!-- => {g[a], a} -->

The falsification function evaluates the proposition in the model:

```wl
co["FalsificationFunction"]
```

<!-- => Function[{}, g[a] == a /. {g -> ({2, 1}[[##1]] &), a -> 1}] -->

The verification function evaluates the axioms in the model:

```wl
co["VerificationFunction"]
```

<!-- => Function[{}, And @@ ({g[g[1]] == 1, g[g[2]] == 2} /. {g -> ({2, 1}[[##1]] &), a -> 1})] -->

The list of properties:

```wl
co["Properties"]
```

<!-- => {"Status", "Method", "Proposition", "Goal", "Axioms", "Hypotheses", "Setup", "Model", "Counterexample", "Witness", "NormalForms", "Domain", "FalsificationFunction", "VerificationFunction", "Data", "Properties"} -->

---

A ground problem with a binary operator gives a table of values for the operator:

```wl
TFindProof[f[a, b] == f[b, a], {f[a, c] == c}, "Counterexample"]["Model"]
```

<!-- => <|f -> {{0, 0, 0, 0, 0}, {0, 0, 0, 2, 0}, {0, 0, 0, 0, 0}, {0, 4, 0, 0, 0}, {0, 0, 0, 0, 0}}, c -> 0, a -> 1, b -> 3|> -->

## Properties and Relations

Find a counterexample:

```wl
co = TFindProof[g[a] == a, {ForAll[x, g[g[x]] == x]}, "Counterexample"]
```

<!-- => a CounterexampleObject summary box, method SaturationNormalForm -->

The falsification function gives [False]() in the model:

```wl
co["FalsificationFunction"][]
```

<!-- => False -->

The verification function gives [True](), since the axioms hold in the model:

```wl
co["VerificationFunction"][]
```

<!-- => True -->

---

The `"SMT"` method of [TFindProof]() gives a counterexample for a refuted ground problem:

```wl
TFindProof[f[a] == c, {a == b, b == c}, Method -> "SMT"]
```

<!-- => a CounterexampleObject summary box, method CongruenceClosure -->

## Possible Issues

Find a counterexample:

```wl
co = TFindProof[g[a] == a, {ForAll[x, g[g[x]] == x]}, "Counterexample"]
```

<!-- => a CounterexampleObject summary box, method SaturationNormalForm -->

The model numbers its elements from `0`:

```wl
co["Model"]
```

<!-- => <|g -> {1, 0}, a -> 0|> -->

The falsification function indexes the tables from `1`, as [Part]() does:

```wl
co["FalsificationFunction"]
```

<!-- => Function[{}, g[a] == a /. {g -> ({2, 1}[[##1]] &), a -> 1}] -->

---

A conjecture that does follow from the axioms has no counterexample, and gives `$Failed`:

```wl
TFindProof[f[a] == b, {f[f[x_]] == x_, f[b] == a}, "Counterexample"]
```

<!-- => $Failed -->

---

A property that a counterexample does not have gives a [Missing]() value:

```wl
TFindProof[a == c, {a == b}, "Counterexample"]["NormalForms"]
```

<!-- => Missing["NotAvailable"] -->
