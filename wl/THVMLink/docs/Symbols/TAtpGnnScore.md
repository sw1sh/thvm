---
Template: Symbol
Name: TAtpGnnScore
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpGnnScore
Keywords: [ENIGMA, GNN, GCN, ATP, scoring, proof relevance, graph neural network, learned guidance]
SeeAlso: [TAtpTrainGnn, TAtpGraphDataset, TAtpCpGraph, TFindProofGnnReranked]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpGnnScore]()[*model*, *dataset*]</code> scores a graph *dataset* (a [TAtpGraphDataset]() output, or any `<|"Graphs"|>`) with a trained GNN *model* (the `"Model"` from [TAtpTrainGnn]()), returning the per-graph proof-relevance score as a list, one entry per graph.

The score is the readout's `logit_pos` minus `logit_neg`.

## Details & Options

- It runs the same forward as training with the model's weights held constant. The graph convolutional network is node-count agnostic, so a model trained on one corpus scores graphs of any size; this is what held-out evaluation and the engine's critical-pair re-rank use.

## Basic Examples

Score two equation graphs with the bundled pretrained model - the group inverse law outranks bare commutativity:

```wl
model = TAtpLoadGnnScorer[TAtpGnnScorerAsset[]];
TAtpGnnScore[model, <|"Graphs" -> {
    TAtpCpGraph[CircleTimes[x, y] == CircleTimes[y, x]],
    TAtpCpGraph[CircleTimes[x, OverBar[x]] == ident]}|>]
```
<!-- => {-3.88736, 9.42424} - one proof-relevance score per graph -->

## Properties & Relations

- [TAtpTrainGnn]() produces the `"Model"`; [TAtpGraphDataset]() / [TAtpCpGraph]() produce the `"Graphs"`.
- [TFindProofReranked]() uses this from Wolfram to re-rank the live critical-pair queue; [TFindProofGnnReranked]() does the equivalent re-rank entirely in C.

## Possible Issues

- The scores are raw logit differences, not probabilities; higher means more proof-relevant, but the absolute scale depends on the trained model.
