---
Template: Symbol
Name: TProofObject
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TProofObject
Keywords: [proof object, equational proof, ATP, ProofObject, proof steps]
SeeAlso: [TToProofObject, TFindProof, TFindEquationalPath, ProofObject, FindEquationalProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TProofObject]()[*data*]</code> represents a proof whose type, theorem, axioms, variables and steps are given in the Association *data*.

## Details & Options

- <code>[TFindProof]()[*conjecture*, *axioms*, "ProofForm" -> "TProofObject"]</code> gives a [TProofObject]() instead of a [ProofObject]().
- [TProofObject]() displays as a summary box with the type, the status and the number of steps of the proof.
- *data* has the keys `"ProofType"` (`"Equational"`), `"Theorem"`, `"Axioms"`, `"Variables"`, `"Constants"`, `"Steps"` and `"Status"`.
- `"Steps"` is an Association from step keys such as `{"Axiom", 1}`, `{"Hypothesis", 1}`, `{"CriticalPairLemma", 1}`, `{"SubstitutionLemma", 1}` and `{"Conclusion", 1}` to the statement of each step and how it was derived, as in the `"ProofDataset"` of a [ProofObject]().
- <code>*proof*["*prop*"]</code> gives a property of *proof*. The properties are:

| | |
| --- | --- |
| `"ProofType"` | the type of the proof |
| `"Theorem"`, `"Theorems"` | the proved theorem |
| `"Axioms"` | the axioms |
| `"Variables"` | the variables of the axioms |
| `"Constants"` | the constants |
| `"Status"` | the status of the proof |
| `"Statistics"` | statistics of the proof search |
| `"ProofAssociation"` | the steps as an Association |
| `"ProofDataset"` | the steps as a [Dataset]() |
| `"ProofLength"` | the number of derived steps |
| `"ProofGraph"` | the graph of the steps |
| `"ProofFunction"` | a function that checks the proof |
| `"ProofObject"` | the equivalent [ProofObject]() |
| `"Properties"` | the list of properties |

- An unknown property gives a [Missing]() value.
- [TToProofObject]() converts a [TProofObject]() to a [ProofObject]().

## Basic Examples

A proof that `f[f[f[a]]] == f[a]` follows from `f[f[x_]] == x_` in one step:

```wl
TProofObject[<|"ProofType" -> "Equational", "Theorem" -> f[f[f[a]]] == f[a], "Axioms" -> {f[f[x_]] == x_}, "Variables" -> {x}, "Constants" -> {}, "Steps" -> <|{"Axiom", 1} -> <|"Statement" -> HoldForm[f[f[x]] == x], "Proof" -> <||>|>, {"Hypothesis", 1} -> <|"Statement" -> HoldForm[f[f[f[a]]] == f[a]], "Proof" -> <||>|>, {"Conclusion", 1} -> <|"Statement" -> HoldForm[f[a] == f[a]], "Proof" -> <|"Input" -> {"Hypothesis", 1}, "Construct" -> {"Axiom", 1}, "Position" -> {}, "Rule" -> f[f[x_]] -> x, "Orientation" -> 1, "ConstructSide" -> 1, "InputOrientation" -> 1, "Side" -> 1, "OutputExpression" -> HoldForm[f[a] == f[a]]|>|>|>, "Status" -> "Proved"|>]
```

<!-- => a TProofObject summary box: Type Equational, Status Proved, Steps 1 -->

---

[TFindProof]() gives a [TProofObject]() with the option `"ProofForm" -> "TProofObject"`:

```wl
TFindProof[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "ProofForm" -> "TProofObject"]
```

<!-- => a TProofObject summary box: Type Equational, Status Proved, Steps 2 -->

## Scope

Find a proof:

```wl
p = TFindProof[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "ProofForm" -> "TProofObject"]
```

<!-- => a TProofObject summary box: Type Equational, Status Proved, Steps 2 -->

The type of the proof:

```wl
p["ProofType"]
```

<!-- => "Equational" -->

The status of the proof:

```wl
p["Status"]
```

<!-- => "Proved" -->

The theorem:

```wl
p["Theorem"]
```

<!-- => ((a ⊗ b) ⊗ c) ⊗ d == a ⊗ (b ⊗ (c ⊗ d)) -->

The axioms:

```wl
p["Axioms"]
```

<!-- => {((x_) ⊗ (y_)) ⊗ (z_) -> (x_) ⊗ ((y_) ⊗ (z_))} -->

The variables of the axioms:

```wl
p["Variables"]
```

<!-- => {x, y, z} -->

The constants:

```wl
p["Constants"]
```

<!-- => {} -->

The number of derived steps:

```wl
p["ProofLength"]
```

<!-- => 2 -->

The steps as an Association:

```wl
p["ProofAssociation"]
```

<!-- => <|{"Axiom", 1} -> <|...|>, {"Hypothesis", 1} -> <|...|>, {"SubstitutionLemma", 1} -> <|...|>, {"Conclusion", 1} -> <|...|>|> -->

The steps as a [Dataset]():

```wl
p["ProofDataset"]
```

<!-- => a Dataset with one row for each step -->

The graph of the steps, laid out from left to right:

```wl
Graph[p["ProofGraph"], GraphLayout -> {"LayeredDigraphEmbedding", "Orientation" -> Left}]
```

<!-- => a Graph from the axiom and the hypothesis to the conclusion -->

The statistics of the proof search:

```wl
p["Statistics"]
```

<!-- => <||> -->

The list of properties:

```wl
p["Properties"]
```

<!-- => {"ProofType", "Theorem", "Theorems", "Axioms", "Variables", "Constants", "Status", "Statistics", "ProofAssociation", "ProofDataset", "ProofLength", "ProofGraph", "ProofFunction", "ProofObject", "Properties"} -->

## Properties and Relations

Find a proof:

```wl
p = TFindProof[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "ProofForm" -> "TProofObject"]
```

<!-- => a TProofObject summary box: Type Equational, Status Proved, Steps 2 -->

The `"ProofFunction"` checks the proof:

```wl
p["ProofFunction"][p["Theorems"]]
```

<!-- => Success["ProofFunction", <|"MessageTemplate" -> "Success!"|>] -->

The `"ProofObject"` property gives the equivalent [ProofObject]():

```wl
p["ProofObject"]
```

<!-- => a ProofObject summary box, logic EquationalLogic -->

[TToProofObject]() gives the same [ProofObject]():

```wl
TToProofObject[p] === p["ProofObject"]
```

<!-- => True -->

## Possible Issues

An unknown property gives a [Missing]() value:

```wl
TFindProof[((a \[CircleTimes] b) \[CircleTimes] c) \[CircleTimes] d == a \[CircleTimes] (b \[CircleTimes] (c \[CircleTimes] d)), {(x_ \[CircleTimes] y_) \[CircleTimes] z_ -> x_ \[CircleTimes] (y_ \[CircleTimes] z_)}, "ProofForm" -> "TProofObject"]["Foo"]
```

<!-- => Missing["UnknownProperty", "Foo"] -->

---

When no proof is found, [TFindProof]() gives `$Failed` rather than a [TProofObject]():

```wl
TFindProof[a == c, {a == b}, "ProofForm" -> "TProofObject"]
```

<!-- => $Failed -->
