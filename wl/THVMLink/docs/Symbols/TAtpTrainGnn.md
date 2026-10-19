---
Template: Symbol
Name: TAtpTrainGnn
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpTrainGnn
Keywords: [ENIGMA, graph neural network, GNN, GCN, theorem proving, training, message passing, learned guidance]
SeeAlso: [TAtpGraphDataset, TAtpGnnScore, TAtpSetGnnScorer, TAtpSaveGnnScorer, TFindProofGnnReranked, TAtpTrainScorer]
RelatedGuides: [THVMLink]
RelatedTutorials: ["[Learned Guidance: a GNN for Critical-Pair Selection](GnnGuidance)"]
---

## Usage

<code>[TAtpTrainGnn]()[*dataset*]</code> trains a graph neural network that scores equations by their relevance to a proof, on a *dataset* of equation graphs from [TAtpGraphDataset]().

<code>[TAtpTrainGnn]()["*theory*"]</code> trains on the dataset of the notable theorems of the named [AxiomaticTheory]() theory.

<code>[TAtpTrainGnn]()[*conjectures*, *axioms*]</code> trains on the dataset of the proofs of *conjectures* from *axioms*.

<!-- #| annotation: 26.10.19: The training cells are kept unevaluated: TAtpTrainGnn aborts the kernel with "thvm fatal: fired kernel kid=..., which the kernel GC stripped (its inputs were freed)", in a fresh kernel too, and TAtpTrainGnn builds its first layer for 6 node features while TAtpCpGraph now gives 12, which TAtpSetGnnScorer rejects. The hints follow the result shape in the source; evaluate the cells again once training runs. -->

## Details & Options

- *dataset* is an Association with the keys `"Graphs"`, a list of [TAtpCpGraph]() graphs, and `"Labels"`, `1` for a lemma of a proof and `0` otherwise, as [TAtpGraphDataset]() gives.
- [TAtpTrainGnn]() trains a graph convolutional network (GCN) with THVMLink's own tensor runtime, and gives an Association with the keys `"Model"`, `"TrainAUC"`, `"LossStart"`, `"LossEnd"`, `"NPos"` and `"NNeg"`.
- The network passes messages between neighboring nodes for `"Rounds"` rounds, averages over the nodes of each graph, and reads out two outputs, for relevant and not relevant. It is trained on the labels with the cross-entropy loss and the Adam optimizer.
- `"Model"` is the form that [TAtpGnnScore]() and [TAtpSetGnnScorer]() take, and that [TAtpSaveGnnScorer]() saves.
- `"TrainAUC"` is the area under the receiver operating characteristic (ROC) curve of the scores on the training graphs, and `"LossStart"` and `"LossEnd"` are the loss before and after training.
- With the forms that build the dataset, the options of [TAtpGraphDataset]() are used for the proofs.
- [TAtpTrainScorer]() trains a model on critical-pair features instead of graphs.
- The following options can be given:

| | | |
| --- | --- | --- |
| `"Hidden"` | `32` | number of hidden units |
| `"Rounds"` | `3` | number of rounds of message passing |
| [MaxTrainingRounds]() | `300` | number of training steps |
| `"LearningRate"` | `0.01` | learning rate of the optimizer |
| `"BatchSize"` | `128` | number of graphs in each training step |
| `"NodeCap"` | `64` | maximum number of nodes of a graph |

## Basic Examples

Train a network on the lemmas of a proof of the double-inverse theorem of abelian groups:

```wl
#| eval: false
TAtpTrainGnn[TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]], "Hidden" -> 8, "Rounds" -> 2]
```

<!-- => <|"Model" -> <|"Kind" -> "GNN", "W1" -> {...}, "Ws" -> {...}, "Bh" -> {...}, "Wout" -> {...}, "Bout" -> {...}, "Rounds" -> 2, "Hidden" -> 8, "NMax" -> ...|>, "TrainAUC" -> ..., "LossStart" -> ..., "LossEnd" -> ..., "NPos" -> 5, "NNeg" -> 5|> -->

## Scope

The dataset the network is trained on:

```wl
TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]]["Labels"]
```

<!-- => {1, 1, 1, 1, 1, 0, 0, 0, 0, 0} -->

---

Train on the notable theorems of a theory:

```wl
#| eval: false
TAtpTrainGnn["GroupAxioms", TimeConstraint -> 5]
```

<!-- => an Association with "Model", "TrainAUC", "LossStart", "LossEnd", "NPos" -> 61 and "NNeg" -> 37 -->

## Options

### Hidden

A network with four hidden units:

```wl
#| eval: false
TAtpTrainGnn[TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]], "Hidden" -> 4]["Model"]["Hidden"]
```

<!-- => 4 -->

### Rounds

A network with one round of message passing:

```wl
#| eval: false
TAtpTrainGnn[TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]], "Rounds" -> 1]["Model"]["Rounds"]
```

<!-- => 1 -->

### MaxTrainingRounds

Fewer training steps leave the loss higher:

```wl
#| eval: false
TAtpTrainGnn[TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]], MaxTrainingRounds -> 10]["LossEnd"]
```

<!-- => the loss after ten steps -->

### LearningRate

A smaller learning rate takes smaller steps:

```wl
#| eval: false
TAtpTrainGnn[TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]], "LearningRate" -> 0.001]["LossEnd"]
```

<!-- => the loss after training with the smaller learning rate -->

### BatchSize

Train on four graphs at a time:

```wl
#| eval: false
TAtpTrainGnn[TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]], "BatchSize" -> 4]["TrainAUC"]
```

<!-- => the area under the curve on the training graphs -->

### NodeCap

Cap the graphs at 16 nodes:

```wl
#| eval: false
TAtpTrainGnn[TAtpGraphDataset[{OverBar[OverBar[x]] == x}, AxiomaticTheory["AbelianGroupAxioms"]], "NodeCap" -> 16]["Model"]["NMax"]
```

<!-- => 16 -->

## Properties and Relations

The bundled pretrained network has the form of a trained model, with three rounds:

```wl
TAtpLoadGnnScorer[TAtpGnnScorerAsset[]]["Rounds"]
```

<!-- => 3 -->

---

[TAtpGnnScore]() scores graphs with a model:

```wl
TAtpGnnScore[TAtpLoadGnnScorer[TAtpGnnScorerAsset[]], <|"Graphs" -> {TAtpCpGraph[CircleTimes[x, OverBar[x]] == ident]}|>]
```

<!-- => {0.3535} -->
