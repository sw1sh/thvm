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
  - `"BatchSize"` - the minibatch size (default 128).
  - `"NodeCap"` - graphs padded/capped to this node count per batch (default 64).
- Training JIT-compiles the whole batched forward + backward + Adam step chain through the CPU backend, so even a ten-graph dataset costs minutes of wall clock; a typical call is <code>[TAtpTrainGnn]()[[TAtpGraphDataset]()[*thms*, *axioms*], "Hidden" -> 8, "Rounds" -> 2]</code>, whose result reports `"LossStart"` above `"LossEnd"` as the network learns. Train once and persist the `"Model"` with [TAtpSaveGnnScorer]() rather than retraining per session.

## Basic Examples

The bundled pretrained scorer ([TAtpGnnScorerAsset]()) is exactly the `"Model"` Association a `TAtpTrainGnn` run returns (saved with [TAtpSaveGnnScorer]()); its keys are the GCN weight surface plus the scalar config:

```wl
Keys @ TAtpLoadGnnScorer[TAtpGnnScorerAsset[]]
```
<!-- => {Kind, W1, Ws, Bh, Wout, Bout, Rounds, Hidden, NMax} -->

The config records the message-passing depth and width it was trained with:

```wl
Lookup[TAtpLoadGnnScorer[TAtpGnnScorerAsset[]], {"Rounds", "Hidden"}]
```
<!-- => {3, 64} -->

## Properties & Relations

- [TAtpGraphDataset]() produces the `"Graphs"` / `"Labels"` this trains on; [TAtpCpGraph]() is the per-equation encoder underneath.
- [TAtpGnnScore]() scores graphs with the resulting `"Model"`; [TAtpSetGnnScorer]() pushes it into the C engine; [TAtpSaveGnnScorer]() / [TAtpLoadGnnScorer]() serialize it.
- [TFindProofGnnReranked]() drives a proof guided by the trained model.

## Possible Issues

- A `(theory)` or `(conjectures, axioms)` call proves the corpus first; bound it with `TimeConstraint` and chunk large sweeps.
