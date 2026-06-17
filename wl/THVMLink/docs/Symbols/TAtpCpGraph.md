---
Template: Symbol
Name: TAtpCpGraph
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpCpGraph
Keywords: [ENIGMA, GNN, ATP, critical pair, hypergraph, encoder, anonymized, graph neural network, message passing]
SeeAlso: [TAtpCpGraphEquation, TAtpGraphDataset, TAtpTrainGnn, TAtpGnnScore, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpCpGraph]()[*lhs* == *rhs*]</code> encodes one equation or critical pair into the anonymized typed hypergraph the ENIGMA (Efficient learNing-based Inference Guiding MAchine) Tier-2 graph neural network message-passes over. Also accepts `Inactive[Equal][*lhs*, *rhs*]` and a `HoldForm` of either.

The result is an Association keyed `"NodeTypes"`, `"NodeFeatures"` (an `NNodes` by 6 matrix), `"Edges"` (a list of `{src, dst, type}`), `"NNodes"`, `"NEdges"`, `"NodeLabels"`, and `"Symbols"`.

## Details & Options

- Node 0 is the critical-pair super-node; the rest are term occurrences (a preorder walk of *lhs* then *rhs*), symbol nodes (one per distinct operator or constant), and var nodes (one per distinct variable). Node types code `0` CPSuper, `1` Term, `2` Symbol, `3` Var; edge types code the term-to-symbol, term-to-child, and cp-to-side-root links. *lhs* and *rhs* share one encoder state, so a shared symbol or variable is a single deduped node.
- The six node-feature columns are purely structural (`is_term`, `is_symbol`, `is_var`, `arity`, `occurrence_count`, `is_cpsuper`) and never encode the concrete label, id, or value, so equations equal up to a consistent symbol/variable renaming produce bit-identical graphs. `"NodeLabels"` and `"Symbols"` keep the concrete per-node identity, so [TAtpCpGraphEquation]() can reconstruct the original equation.
- This is the per-equation encoder [TAtpGraphDataset]() emits; pair it with a GNN trained on the dataset ([TAtpTrainGnn]()).

## Basic Examples

Encode a group-theory equation into its anonymized graph:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
Keys @ TAtpCpGraph[Inactive[Equal][CircleTimes[a, OverBar[a]], ident]]
```
<!-- => {NodeTypes, NodeFeatures, Edges, NodeLabels, NNodes, NEdges, Symbols} -->

## Scope

The node-type vector marks each node's kind:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TAtpCpGraph[Inactive[Equal][CircleTimes[a, OverBar[a]], ident]]["NodeTypes"]
```
<!-- => {0, 1, 2, 1, 2, 1, 2, 1, 1, 2} -->

## Properties & Relations

- [TAtpCpGraphEquation]() is the exact inverse: it rebuilds `Inactive[Equal][lhs, rhs]` from the stored `"Symbols"` and `"Edges"`.
- [TAtpGraphDataset]() applies this encoder to every proof lemma to build a labelled dataset; [TAtpTrainGnn]() trains a graph convolutional network on the result.

## Possible Issues

- The structural features are deliberately symbol-blind, so two structurally identical equations over different signatures encode to the same graph; the concrete identities live only in `"Symbols"` / `"NodeLabels"`.
