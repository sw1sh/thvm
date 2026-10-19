---
Template: Symbol
Name: TAtpGraphDataset
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpGraphDataset
Keywords: [ENIGMA, graph neural network, GNN, theorem proving, graph dataset, lemmas, learned guidance]
SeeAlso: [TAtpCpGraph, TAtpTrainGnn, TAtpGnnScore, TAtpCpDataset, TFindProof, AxiomaticTheory]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Learned Guidance: a GNN for Critical-Pair Selection](GnnGuidance)"]
---

## Usage

<code>[TAtpGraphDataset]()[*conjectures*, *axioms*]</code> proves each of the *conjectures* from *axioms*, and gives the graphs of the derived lemmas, labeled by whether the proof uses them.

<code>[TAtpGraphDataset]()["*theory*"]</code> uses the notable theorems of the named [AxiomaticTheory]() theory as the conjectures.

<code>[TAtpGraphDataset]()[*proof*]</code> gives the graphs of the lemmas of the [ProofObject]() *proof*, all labeled `1`.

<code>[TAtpGraphDataset]()[*proof*, *lemmas*]</code> adds the rules *lemmas* that the proof does not use, labeled `0`.

## Details & Options

- The result has the keys `"Graphs"` (one [TAtpCpGraph]() Association for each lemma), `"Labels"` (`1` for a lemma of the proof and `0` for a derived rule the proof does not use), `"NPos"`, `"NNeg"` and `"NProofs"`.
- The lemmas of a proof are its critical-pair and substitution lemmas. The other lemmas are the rules of the completion, without those that equal a lemma of the proof up to a renaming of variables and an exchange of sides.
- *proof* can be a list of [ProofObject]() expressions, and *lemmas* the `"Lemmas"` spec of [TFindProof]().
- Only the conjectures that are proved contribute graphs.
- [TAtpTrainGnn]() trains a graph neural network on the dataset, and [TAtpGnnScore]() scores graphs with it.
- The following options can be given:

| | | |
| --- | --- | --- |
| [Method]() | `{"Completion"}` | proof search method |
| [TimeConstraint]() | `30` | time limit in seconds for each proof |
| [MaxSteps]() | [Automatic]() | maximum number of critical pairs for each proof |

## Basic Examples

The dataset of a one-step proof, which uses no lemma; its one derived rule is labeled `0`:

```wl
TAtpGraphDataset[{f[f[f[a]]] == f[a]}, {f[f[x_]] == x_}]
```

<!-- => <|"Graphs" -> {<|"NodeTypes" -> {0, 1, 2, 1, 1, 2, 1}, ..., "Symbols" -> {Missing["NotASymbol"], Missing["NotASymbol"], "f", Missing["NotASymbol"], Missing["NotASymbol"], "x", Missing["NotASymbol"]}|>}, "Labels" -> {0}, "NPos" -> 0, "NNeg" -> 1, "NProofs" -> 1|> -->

## Scope

The proof of the double-inverse theorem of abelian groups has five lemmas, and the completion five further rules:

```wl
ds = TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]]
```

<!-- => <|"Graphs" -> {...ten graphs...}, "Labels" -> {...}, "NPos" -> 5, "NNeg" -> 5, "NProofs" -> 1|> -->

The labels:

```wl
ds["Labels"]
```

<!-- => {1, 1, 1, 1, 1, 0, 0, 0, 0, 0} -->

The equations of the graphs:

```wl
TAtpCpGraphEquation /@ ds["Graphs"]
```

<!-- => the ten lemmas and rules as inactive equations -->

---

The notable theorems of the group axioms:

```wl
TAtpGraphDataset["GroupAxioms", TimeConstraint -> 5]["NProofs"]
```

<!-- => 5 -->

---

The lemmas of a proof object, all labeled `1`:

```wl
TAtpGraphDataset[TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms"]]["Labels"]
```

<!-- => {1, 1, 1, 1, 1} -->

---

The rules of the completion that the proof does not use are added with the label `0`:

```wl
TAtpGraphDataset[TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms"], TFindProof[OverBar[OverBar[x]] == x, "AbelianGroupAxioms", "Lemmas"]]["Labels"]
```

<!-- => {1, 1, 1, 1, 1, 0, 0, 0, 0, 0} -->

## Options

### Method

Prove with a different critical-pair weight:

```wl
TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"], Method -> {"Completion", "CriticalPairWeight" -> "Add"}]["NPos"]
```

<!-- => 5 -->

### TimeConstraint

A conjecture that is not proved within the time limit contributes no graphs:

```wl
TAtpGraphDataset[{x ** y == y ** x}, AxiomaticTheory["AbelianGroupAxioms"], TimeConstraint -> 1]
```

<!-- => <|"Graphs" -> {}, "Labels" -> {}, "NPos" -> 0, "NNeg" -> 0, "NProofs" -> 0|> -->

### MaxSteps

A step budget bounds each proof:

```wl
TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"], MaxSteps -> 1000]["NProofs"]
```

<!-- => 1 -->

## Properties and Relations

[TAtpGnnScore]() scores the graphs with a graph neural network, here a small network written by hand:

```wl
TAtpGnnScore[<|"Kind" -> "GNN", "W1" -> {N[IdentityMatrix[{12, 2}]]}, "Ws" -> {N[IdentityMatrix[{12, 2}]]}, "Bh" -> {{0., 0.}}, "Wout" -> {{1., 0.}, {0., 1.}}, "Bout" -> {0., 0.}, "Rounds" -> 1, "Hidden" -> 2, "NMax" -> 64|>, TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]]]
```

<!-- => {-0.582353, -0.356667, -0.597222, -0.460256, -0.630952, -0.356667, -0.345833, -0.817778, -0.68, -0.555555} -->

---

[TAtpCpDataset]() gives a dataset of critical-pair features instead:

```wl
TAtpCpDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]]["NRows"]
```

<!-- => 10 -->
