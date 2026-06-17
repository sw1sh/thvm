---
Template: Symbol
Name: TAtpCpGraphEquation
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TAtpCpGraphEquation
Keywords: [ENIGMA, GNN, ATP, critical pair, hypergraph, decoder, inverse, graph neural network]
SeeAlso: [TAtpCpGraph, TAtpGraphDataset, TAtpTrainGnn, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TAtpCpGraphEquation]()[*graph*]</code> reconstructs the original equation from a [TAtpCpGraph]() result, returning `Inactive[Equal][lhs, rhs]`.

## Details & Options

- It is the exact inverse of [TAtpCpGraph](), reading the per-node `"Symbols"` identities and the `"Edges"` term structure (each term node's head, ordered children, and the two side roots).
- Returns `$Failed` if the *graph* carries no `"Symbols"` (for example a graph decoded without the live encoder state).

## Basic Examples

Round-trip an equation through the encoder and back:

```wl
Needs["WolframInstitute`THVMLink`ATP`"];
TAtpCpGraphEquation @ TAtpCpGraph[Inactive[Equal][CircleTimes[a, OverBar[a]], ident]]
```
<!-- => Inactive[Equal][a \[CircleTimes] OverBar[a], ident] -->

## Properties & Relations

- [TAtpCpGraph]() is the forward encoder; the anonymized structural features are symbol-blind, but `"Symbols"` and `"NodeLabels"` preserve the concrete identities this decoder reads to invert the encoding losslessly.

## Possible Issues

- A graph stripped of `"Symbols"` cannot be inverted; [TAtpCpGraphEquation]() returns `$Failed` rather than guessing the operator names.
