---
Template: Symbol
Name: TFindProofReranked
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TFindProofReranked
Keywords: [ENIGMA, GNN, GCN, ATP, critical pair, re-rank, learned guidance, persistent handle, theorem proving]
SeeAlso: [TFindProofGnnReranked, TAtpGnnScore, TAtpTrainGnn, TAtpSetGnnScorer, TFindProof]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TFindProofReranked]()[*conjecture*, *axioms*, *model*]</code> proves the *conjecture* while re-ranking the critical-pair queue with a trained GNN *model* (the `"Model"` from [TAtpTrainGnn]()): it drives the C saturation in `"RerankPeriod"`-step chunks and, between chunks, pulls the live queued critical pairs, scores each with [TAtpGnnScore](), and pushes the priorities back into the engine's selection heap. Returns the status string (`"PROVED"`, `"TIMEOUT"`, `"QUEUE_EMPTY"`, ...).

## Details & Options

- This is the ENIGMA (Efficient learNing-based Inference Guiding MAchine) inference loop driven from Wolfram over the persistent-handle bridge. Completeness is preserved: re-ranking only permutes selection order, and the periodic first-in-first-out pick still fires.
- For the same guidance with no Wolfram round-trip in the proof loop (far faster), use [TFindProofGnnReranked](), which runs the whole re-rank in C.
- Options:
  - `"RerankPeriod"` - how many steps per chunk between re-rankings.
  - `MaxSteps` - the saturation step cap.
  - `"CriticalPairWeight"`, `"Ordering"`, `"AutoPrecedence"`, `"QueueCap"` - the base heuristic the re-rank rides on.

## Basic Examples

Prove a group-theory goal, scoring the live queue from Wolfram between chunks:

```wl
#| eval: false
Needs["WolframInstitute`THVMLink`ATP`"];
model = TAtpLoadGnnScorer[TAtpGnnScorerAsset[]];
axioms = {
    ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]],
    ForAll[{x}, CircleTimes[x, ident] == x],
    ForAll[{x}, CircleTimes[x, inv[x]] == ident]};
TFindProofReranked[ForAll[{x}, CircleTimes[ident, x] == x], axioms, model,
    "RerankPeriod" -> 50, MaxSteps -> 8000]
```
<!-- => "PROVED" -->

## Properties & Relations

- [TFindProofGnnReranked]() keeps the same loop entirely in C (no per-chunk Wolfram round-trip); prefer it for speed and this one for inspecting the scoring step from Wolfram.
- [TAtpGnnScore]() is the scoring call each chunk performs; [TAtpTrainGnn]() produces the `"Model"`.

## Possible Issues

- The per-chunk Wolfram round-trip makes this slower than [TFindProofGnnReranked](); it exists for transparency and experimentation, not throughput.
