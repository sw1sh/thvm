---
Template: Symbol
Name: TAtpTrainGnn
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpTrainGnn
Keywords: [ENIGMA, GNN, GCN, ATP, training, message passing, graph neural network, proof relevance, learned guidance]
SeeAlso: [TAtpGraphDataset, TAtpGnnScore, TAtpSetGnnScorer, TAtpSaveGnnScorer, TFindProofGnnReranked]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpTrainGnn]()[*dataset*]</code> trains a graph convolutional network (GCN) on a [TAtpGraphDataset]() result (or any `<|"Graphs", "Labels"|>`) in `thvm`'s own tensor stack and returns `<|"Model", "TrainAUC", "LossStart", "LossEnd", "NPos", "NNeg"|>`, where `"Model"` is the `"GNN"`-kind weight Association the scorers consume.

<code>[TAtpTrainGnn]()[*theory*]</code> and <code>[TAtpTrainGnn]()[*conjectures*, *axioms*]</code> prepare the dataset via [TAtpGraphDataset]() and train in one call.

## Details & Options

- The forward batches every graph to a common padded node count, runs `"Rounds"` rounds of row-normalized-adjacency message passing (*H' = relu(A.H.W1 + H.Ws + b)*), masked-mean-pools to a graph embedding, and reads out a two-class proof-relevance head trained with categorical cross-entropy and Adam. The reported `"TrainAUC"` is the rank area-under-the-ROC-curve on the training graphs.
- This is the ENIGMA (Efficient learNing-based Inference Guiding MAchine) Tier-2 deliverable: a symbol-independent network learning proof relevance from clause structure, complementing the Tier-1 hand-feature scorer ([TAtpTrainScorer]()).
- Options:
  - `"Hidden"` - the message-passing width.
  - `"Rounds"` - the number of message-passing rounds.
  - `MaxTrainingRounds` - the Adam round cap.
  - `"LearningRate"` - the Adam step size.

## Basic Examples

Train a GCN on a theory's notable theorems and read the loss curve endpoints:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
SeedRandom[1234];
ds = TAtpGraphDataset["GroupAxioms", TimeConstraint -> 10];
r = TAtpTrainGnn[ds, "Hidden" -> 16, "Rounds" -> 2, MaxTrainingRounds -> 120];
{r["LossStart"], r["LossEnd"]}
```
<!-- => {<lossStart>, <lossEnd>} - LossEnd below LossStart as the network learns -->

## Scope

Read the training-set rank AUC:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
SeedRandom[1234];
TAtpTrainGnn["GroupAxioms", TimeConstraint -> 10]["TrainAUC"]
```
<!-- => a Real in [0, 1] (training-set rank AUC) -->

## Properties & Relations

- [TAtpGraphDataset]() produces the `"Graphs"` / `"Labels"` this trains on; [TAtpCpGraph]() is the per-equation encoder underneath.
- [TAtpGnnScore]() scores graphs with the resulting `"Model"`; [TAtpSetGnnScorer]() pushes it into the C engine; [TAtpSaveGnnScorer]() / [TAtpLoadGnnScorer]() serialize it.
- [TFindProofGnnReranked]() drives a proof guided by the trained model.

## Possible Issues

- A `(theory)` or `(conjectures, axioms)` call proves the corpus first; bound it with `TimeConstraint` and chunk large sweeps.
