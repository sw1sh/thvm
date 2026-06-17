---
Template: Symbol
Name: TAtpGraphDataset
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpGraphDataset
Keywords: [ENIGMA, GNN, ATP, graph dataset, critical pair, labelled, proof relevance, graph neural network]
SeeAlso: [TAtpCpGraph, TAtpTrainGnn, TAtpCpDataset, TFindProof, AxiomaticTheory]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpGraphDataset]()[*conjectures*, *axioms*]</code> proves each conjecture against the shared *axioms* and turns the verified `ProofObject`'s lemmas into a labelled graph dataset, where each graph is a [TAtpCpGraph]() Association, label `1` marks a proof-essential lemma and `0` a saturated-but-unused rule.

<code>[TAtpGraphDataset]()[*theory*]</code> runs every <code>[AxiomaticTheory]()[*theory*, "NotableTheorems"]</code> against the theory's axioms.

<code>[TAtpGraphDataset]()[*proofObject*]</code> (or a list of them) yields the proof-essential positives only; <code>[TAtpGraphDataset]()[*proofObject*, *lemmas*]</code> adds negatives from a supplied saturated set (the `"Lemmas"` output of [TFindProof]()).

The result is `<|"Graphs", "Labels", "NPos", "NNeg", "NProofs"|>`.

## Details & Options

- Positives are the `CriticalPairLemma` / `SubstitutionLemma` equations of the proof chain; negatives are the saturated rule set minus any rule structurally equal to a positive (a canonical-key match that anonymizes variables and treats each equation as an unordered pair). Only proved runs contribute graphs.
- Unlike [TAtpCpDataset]()'s per-critical-pair feature rows, this sources clean positives straight from the verified proof object; feed `"Graphs"` / `"Labels"` to a GNN ([TAtpTrainGnn]()).
- Options:
  - `Method` - the proof Method.
  - `TimeConstraint` - per-proof wall-clock bound.
  - `MaxSteps` - the saturation step cap.

## Basic Examples

Build a labelled graph dataset from a theory's notable theorems:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
ds = TAtpGraphDataset["GroupAxioms", TimeConstraint -> 10];
Length @ ds["Graphs"]
```
<!-- => an integer graph count (positives + negatives across proved runs) -->

## Scope

Read the positive / negative / proof counts:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
ds = TAtpGraphDataset["GroupAxioms", TimeConstraint -> 10];
{ds["NPos"], ds["NNeg"], ds["NProofs"]}
```
<!-- => {<NPos>, <NNeg>, <NProofs>} integer counts -->

## Properties & Relations

- [TAtpCpGraph]() is the per-equation encoder this applies to every lemma; [TAtpTrainGnn]() trains a graph convolutional network on `"Graphs"` / `"Labels"`.
- [TAtpCpDataset]() is the Tier-1 feature-vector sibling: per-critical-pair rows labelled by trace reachability rather than clean per-lemma graphs.

## Possible Issues

- Generating a dataset from a corpus means proving the corpus; bound each proof with `TimeConstraint` and chunk large `NotableTheorems` sweeps to keep memory in check.
