---
Template: TechNote
Name: GnnGuidance
Title: "Learned Guidance: a GNN for Critical-Pair Selection"
Context: WolframInstitute`THVMLink`ATP`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/GnnGuidance
Keywords: [ENIGMA, GNN, graph neural network, machine learning, critical pair, clause selection, learned guidance, GCN, safetensors, proof relevance, theorem proving]
RelatedGuides: [THVMLink]
RelatedTutorials: [ATP, AtpMethods, Train]
---

## What learned guidance does

The [ATP](paclet:WolframInstitute/THVMLink/tutorial/ATP) engine proves an
equational theorem by Knuth-Bendix completion: it repeatedly picks a
*critical pair* (an overlap of two rules whose joinability must be
checked), normalizes it, and keeps the survivors as new rules. Which
critical pair to pick next is the central heuristic, and a hand-tuned
weight is only a proxy for the real question: *will this pair end up in the
proof?* Learned guidance answers that question with a model trained on past
proofs, the idea behind the ENIGMA line of work (Efficient learNing-based
Inference Guiding MAchine).

thvm ships two learned scorers. The [AtpMethods](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods)
tutorial covers the Tier-1 hand-feature scorer behind
<code>[TFindProof]()[*goal*, *theory*, Method -> "ENIGMA"]</code>. This page
covers the Tier-2 graph neural network (GNN): a graph convolutional network
(GCN) that reads the *structure* of a critical pair, with no symbol
identities, so one model generalizes across theories with different
signatures. The pipeline is four steps, all on thvm's own tensor runtime:
encode each pair as a graph, build a labelled dataset, train the GCN, and
push it into the engine to guide selection.

## A critical pair as a graph

[TAtpCpGraph]() encodes one equation as an anonymized hypergraph: a node for
the critical-pair super-node, one per function-symbol occurrence, and one
per variable occurrence, wired by the term structure. The node *features* are
purely structural (node kind, arity, occurrence count, side), never the
symbol name, so the network sees shape, not `nand` versus `CircleTimes`.

```wl
Keys @ TAtpCpGraph[Inactive[Equal][CircleTimes[a, OverBar[a]], ident]]
```
<!-- => {NodeTypes, NodeFeatures, Edges, NodeLabels, NNodes, NEdges, Symbols} -->

The node-type vector marks each node as the super-node (`0`), a term (`1`),
a symbol (`2`), or a variable (`3`):

```wl
TAtpCpGraph[Inactive[Equal][CircleTimes[a, OverBar[a]], ident]]["NodeTypes"]
```
<!-- => {0, 1, 2, 1, 2, 1, 2, 1, 1, 2} -->

The features are anonymized, but the graph *also* stores the concrete
identity of each node in `"Symbols"` (and the raw label in `"NodeLabels"`),
so the encoding is lossless: [TAtpCpGraphEquation]() is its exact inverse and
rebuilds the original equation from the stored symbols plus the term
structure.

```wl
TAtpCpGraphEquation @ TAtpCpGraph[Inactive[Equal][CircleTimes[a, OverBar[a]], ident]]
```
<!-- => Inactive[Equal][a \[CircleTimes] OverBar[a], ident] -->

## Building a dataset

[TAtpGraphDataset]() turns proofs into supervision. Given a theory, it
proves every `NotableTheorem` against the theory's axioms and labels the
resulting equations: `1` for a lemma the proof actually used
(proof-essential), `0` for a rule that saturation generated but the proof
did not. Only proved goals contribute, and structurally identical rules are
de-duplicated.

```wl
ds = TAtpGraphDataset["GroupAxioms", TimeConstraint -> 10];
Length @ ds["Graphs"]
```
<!-- => 103 -->

```wl
{ds["NPos"], ds["NNeg"], ds["NProofs"]}
```
<!-- => {66, 37, 5} -->

The same builder takes an explicit conjecture set against shared axioms, or
a list of `ProofObject`s straight from [TFindProof]() (positives only; pass
the saturated `"Lemmas"` set as a second argument to add negatives):

```wl
#| eval: false
ds = TAtpGraphDataset[conjectures, axioms]
ds = TAtpGraphDataset[proofObject, TFindProof[goal, axioms, "Lemmas"]]
```

A raw TPTP (Thousands of Problems for Theorem Provers) file works too:
<code>[TFindProof]()[[File]()["x.p"]]</code> parses and proves the unit-equality
fragment, and the returned `ProofObject` feeds the same builder. Generating
a dataset from a whole corpus means proving the corpus, so bound each proof
with `TimeConstraint` and chunk large sweeps; see the engineering notes in
`docs/atp/gnn_pipeline.md`.

## Training the network

[TAtpTrainGnn]() trains the GCN on a dataset entirely in thvm's tensor
stack: `"Rounds"` rounds of row-normalized-adjacency message passing
(*H' = relu(A.H.W1 + H.Ws + b)*), masked-mean pooling to a graph embedding,
and a two-class proof-relevance head trained with categorical
cross-entropy and Adam. It returns a report whose `"Model"` is the trained
network.

```wl
SeedRandom[1234];
ds = TAtpGraphDataset["GroupAxioms", TimeConstraint -> 10];
r = TAtpTrainGnn[ds, "Hidden" -> 16, "Rounds" -> 2, MaxTrainingRounds -> 120];
r["LossStart"]
```
<!-- => 1.0197 -->

```wl
r["LossEnd"]
```
<!-- => 0.6245 -->

The loss falls as the network learns proof relevance. With the default
config (`"Hidden" -> 32`, `"Rounds" -> 3`, 300 Adam steps) and a by-problem
held-out split on this theory, the proof-relevance score reaches a test
[area-under-the-ROC-curve](https://reference.wolfram.com/language/ref/ROCCurve.html)
of about 0.89, generalizing to theorems it never trained on:

```wl
SeedRandom[1234];
TAtpTrainGnn["GroupAxioms", TimeConstraint -> 10]["TrainAUC"]
```
<!-- => 0.7600 (train); ~0.89 held-out by-problem -->

## Saving and loading

A trained model serializes to a `.safetensors` file with [TAtpSaveGnnScorer]()
and reloads with [TAtpLoadGnnScorer](). The format is the standard
safetensors layout (each weight a named tensor, the scalar config in the
file metadata), so the file is also readable by Python's `safetensors`, and
a pretrained scorer ships as a paclet asset.

```wl
FileExistsQ @ TAtpGnnScorerAsset[]
```
<!-- => True -->

```wl
model = TAtpLoadGnnScorer[TAtpGnnScorerAsset[]];
{model["Rounds"], model["Hidden"]}
```
<!-- => {2, 8} -->

The weights round-trip bit-for-bit through a file:

```wl
path = TAtpSaveGnnScorer[model, FileNameJoin[{$TemporaryDirectory, "gcn.safetensors"}]];
TAtpLoadGnnScorer[path]["W1"] === model["W1"]
```
<!-- => True -->

## Guiding a proof

[TAtpSetGnnScorer]() pushes a model into the C engine. The GNN then drives
the *secondary* selection dimension: every few selections the engine picks
the pair the network scores highest, while the primary heuristic still owns
the rest of the queue. Completeness is untouched because the engine takes a
periodic first-in-first-out pick regardless of the score, so a cold or
over-fit model only slows a proof, never loses one.

```wl
model = TAtpLoadGnnScorer[TAtpGnnScorerAsset[]];
TAtpSetGnnScorer[model]
```
<!-- => True -->

[TFindProofGnnReranked]() proves a goal with the model guiding selection,
re-scoring the live queue every `"RerankPeriod"` selections, all in C with
no Wolfram round-trip in the proof loop:

```wl
model = TAtpLoadGnnScorer[TAtpGnnScorerAsset[]];
axioms = {
    ForAll[{x, y, z}, CircleTimes[CircleTimes[x, y], z] == CircleTimes[x, CircleTimes[y, z]]],
    ForAll[{x}, CircleTimes[x, ident] == x],
    ForAll[{x}, CircleTimes[x, inv[x]] == ident]};
TFindProofGnnReranked[ForAll[{x}, CircleTimes[ident, x] == x], axioms, model,
    "RerankPeriod" -> 50, MaxSteps -> 8000]
```
<!-- => PROVED -->

Whether learned guidance *speeds up* a proof depends on the scorer's quality
and how aggressively it coops with the primary heuristic. The C benchmark
(`bin/test_atp_wolfram_bench`) runs this with no kernel, so a corpus sweep
cannot stall the front end: set `THVM_ATP_GNN_ASSET`,
`THVM_ATP_GNN_RERANK_PERIOD`, and `THVM_ATP_GNN_COOP_RATIO` (the coop
ratio is the safety lever, gentler is safer) on top of a baseline preset
like `THVM_ATP_WALDMEISTER`.

## Where to go next

- [AtpMethods](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods): the
  Tier-1 hand-feature scorer and the `Method -> "ENIGMA"` learn loop.
- [ATP](paclet:WolframInstitute/THVMLink/tutorial/ATP): the proof engine,
  presets, portfolios, and TPTP problem files.
- [Train](paclet:WolframInstitute/THVMLink/tutorial/Train): how training
  works on the tensor surface that the GCN is built on.
- `docs/atp/gnn_pipeline.md` and `docs/atp/ml_guidance.md` in the
  repository: the end-to-end design, the open problems (corpus-scale
  dataset generation, the `Method -> {"ENIGMA", "Model" -> ...}` fold), and
  the ML-in-ATP landscape.
