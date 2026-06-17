---
Template: Symbol
Name: TFindProofGnnReranked
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindProofGnnReranked
Keywords: [ENIGMA, GNN, GCN, ATP, critical pair, re-rank, learned guidance, theorem proving, clause selection]
SeeAlso: [TFindProofReranked, TAtpTrainGnn, TAtpSetGnnScorer, TAtpGnnScore, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFindProofGnnReranked]()[*conjecture*, *axioms*, *model*]</code> proves the *conjecture* with the trained GCN *model* (the `"Model"` from [TAtpTrainGnn]()) guiding critical-pair selection entirely in C: it pushes the GCN weights into the engine ([TAtpSetGnnScorer]()), then drives one saturation in which the engine re-ranks the live queue every `"RerankPeriod"` selections on `thvm`'s own tensor runtime. Returns the status string (`"PROVED"`, `"TIMEOUT"`, `"QUEUE_EMPTY"`, ...).

## Details & Options

- Unlike [TFindProofReranked]() there is no Wolfram round-trip in the proof loop, so it is far faster. Completeness is preserved: re-ranking only permutes selection order, and the periodic first-in-first-out pick still fires.
- Options:
  - `"RerankPeriod"` - how many selections between re-rankings.
  - `MaxSteps` - the saturation step cap.
  - `"CriticalPairWeight"`, `"Ordering"`, `"AutoPrecedence"` - the base heuristic the re-rank rides on.

## Basic Examples

Prove a group-theory goal with the bundled pretrained model guiding selection:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
model = TAtpLoadGnnScorer[TAtpGnnScorerAsset[]];
axioms = {
    ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]],
    ForAll[{x}, CircleTimes[x, ident] == x],
    ForAll[{x}, CircleTimes[x, inv[x]] == ident]};
TFindProofGnnReranked[ForAll[{x}, CircleTimes[ident, x] == x], axioms, model,
    "RerankPeriod" -> 50, MaxSteps -> 8000]
```
<!-- => "PROVED" -->

## Properties & Relations

- [TFindProofReranked]() does the same re-ranking from Wolfram (each chunk scored with [TAtpGnnScore]() over the persistent-handle bridge); this symbol keeps the whole loop in C.
- [TAtpTrainGnn]() produces the `"Model"`; [TAtpSetGnnScorer]() is the explicit push this calls internally.

## Possible Issues

- Whether learned guidance speeds up a proof depends on the scorer's quality and how aggressively it coops with the primary heuristic; a cold or over-fit model only slows a proof, never loses one (the periodic FIFO pick preserves completeness).
