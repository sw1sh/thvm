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

The completion engine of [TFindProof]() keeps a queue of critical pairs, the equations that arise where two rewrite rules overlap, and the order in which it takes them from the queue decides how soon a proof is found. A hand-tuned weight, such as the size of the equation, stands in for the real question: will this equation be used in the proof? Learned guidance answers that question with a model trained on earlier proofs, the approach of the ENIGMA (Efficient learNing-based Inference Guiding MAchine) provers. THVMLink has two learned scorers. One rates fixed features of each critical pair and is used by <code>[Method]() -> "ENIGMA"</code>, as described in [Methods and Presets for TFindProof](paclet:WolframInstitute/THVMLink/tutorial/AtpMethods). The other, the subject of this tutorial, is a graph neural network (GNN), specifically a graph convolutional network (GCN), that reads the structure of an equation with the names of its symbols removed, so that a model trained on one theory applies to theories with other symbols. It is trained and run on the tensor runtime of THVMLink.

| | |
| --- | --- |
| <code>[TAtpCpGraph]()[*lhs* == *rhs*]</code> | encode an equation as a graph |
| <code>[TAtpCpGraphEquation]()[*graph*]</code> | recover the equation from its graph |
| <code>[TAtpGraphDataset]()[*conjectures*, *axioms*]</code> | prove *conjectures* and label the equations of the searches |
| <code>[TAtpTrainGnn]()[*dataset*]</code> | train a GCN on a dataset |
| <code>[TAtpGnnScore]()[*model*, *dataset*]</code> | score the graphs of a dataset with a model |
| <code>[TAtpSaveGnnScorer]()[*model*, *file*]</code> | save a model to a file |
| <code>[TAtpLoadGnnScorer]()[*file*]</code> | load a model from a file |
| <code>[TAtpGnnScorerAsset]()[]</code> | the file of the pretrained model that comes with the paclet |
| <code>[TAtpSetGnnScorer]()[*model*]</code> | make *model* the scorer of the prover |
| <code>[TFindProofGnnReranked]()[*conjecture*, *axioms*, *model*]</code> | prove *conjecture* with *model* ranking the critical pairs |

## An equation as a graph

[TAtpCpGraph]() encodes an equation as a graph with a node for the equation as a whole, a node for each occurrence of a subterm, and a node for each distinct symbol and variable, connected along the structure of the terms. Each node has a vector of features that describe its kind, its number of arguments and how often it occurs, but not its name:

```wl
cp = TAtpCpGraph[CircleTimes[a, OverBar[a]] == ident]
```
<!-- => <|"NodeTypes" -> {0, 1, 2, 1, 2, 1, 2, 1, 1, 2}, "NodeFeatures" -> {{0., 0., 0., 0., 1., 1., 0., 0., 0., 0., 1., 1.}, …}, "Edges" -> {{1, 2, 0}, {3, 4, 0}, {1, 3, 1}, {5, 6, 0}, {7, 4, 0}, {5, 7, 1}, {1, 5, 1}, {8, 9, 0}, {0, 1, 2}, {0, 8, 3}}, "NodeLabels" -> {0, 0, 3, 0, 4, 0, 5, 0, 0, 6}, "NNodes" -> 10, "NEdges" -> 10, "Symbols" -> {Missing["NotASymbol"], Missing["NotASymbol"], "CircleTimes", Missing["NotASymbol"], "a", Missing["NotASymbol"], "OverBar", Missing["NotASymbol"], Missing["NotASymbol"], "ident"}|> -->

The node types are `0` for the equation, `1` for a subterm, `2` for a symbol and `3` for a variable:

```wl
cp["NodeTypes"]
```
<!-- => {0, 1, 2, 1, 2, 1, 2, 1, 1, 2} -->

Since the features carry no names, an equation with its symbols renamed has the same features:

```wl
TAtpCpGraph[CircleTimes[b, OverBar[b]] == unit]["NodeFeatures"] == cp["NodeFeatures"]
```
<!-- => True -->

The names are kept separately, under `"Symbols"`, so the encoding loses nothing. [TAtpCpGraphEquation]() rebuilds the equation, in the inactive form the prover uses for its rules:

```wl
TAtpCpGraphEquation[cp]
```
<!-- => Inactive[Equal][a ⊗ OverBar[a], ident] -->

## A dataset from proofs

[TAtpGraphDataset]() proves each conjecture and turns the equations of the search into labeled graphs: `1` for a lemma the proof uses, and `0` for a rule the completion derived that the proof does not use. Proving `f[f[f[a]]] == f[a]` from `f[f[x_]] == x_` takes one rewrite with the axiom, so the one rule the completion derives is unused and labeled `0`:

```wl
TAtpGraphDataset[{f[f[f[a]]] == f[a]}, {f[f[x_]] == x_}]
```
<!-- => <|"Graphs" -> {<|"NodeTypes" -> {0, 1, 2, 1, 1, 2, 1}, …, "NNodes" -> 7, "NEdges" -> 8, "Symbols" -> {Missing["NotASymbol"], Missing["NotASymbol"], "f", Missing["NotASymbol"], Missing["NotASymbol"], "x", Missing["NotASymbol"]}|>}, "Labels" -> {0}, "NPos" -> 0, "NNeg" -> 1, "NProofs" -> 1|> -->

The axioms of an abelian group give a larger example:

```wl
abelian = {x_ \[CircleTimes] (y_ \[CircleTimes] z_) == (x_ \[CircleTimes] y_) \[CircleTimes] z_, x_ \[CircleTimes] y_ == y_ \[CircleTimes] x_, x_ \[CircleTimes] e == x_, x_ \[CircleTimes] inv[x_] == e}
```
<!-- => {x_ ⊗ (y_ ⊗ z_) == (x_ ⊗ y_) ⊗ z_, x_ ⊗ y_ == y_ ⊗ x_, x_ ⊗ e == x_, x_ ⊗ inv[x_] == e} -->

The proof that the inverse of the inverse of `a` is `a` uses five lemmas, and the completion derives six further rules:

```wl
TAtpGraphDataset[{inv[inv[a]] == a}, abelian, TimeConstraint -> 5]["Labels"]
```
<!-- => {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0} -->

The equations behind the labels, the five lemmas first:

```wl
TAtpCpGraphEquation /@ TAtpGraphDataset[{inv[inv[a]] == a}, abelian, TimeConstraint -> 5]["Graphs"]
```
<!-- => {Inactive[Equal][x ⊗ (inv[x] ⊗ y), e ⊗ y], Inactive[Equal][x, e ⊗ x], Inactive[Equal][x ⊗ (inv[x] ⊗ y), y], Inactive[Equal][inv[inv[x]], x ⊗ e], Inactive[Equal][inv[inv[x]], x], Inactive[Equal][x ⊗ e, x], Inactive[Equal][x ⊗ inv[x], e], Inactive[Equal][(x ⊗ y) ⊗ z, x ⊗ (y ⊗ z)], Inactive[Equal][x ⊗ y, y ⊗ x], Inactive[Equal][inv[e], e], Inactive[Equal][x ⊗ (y ⊗ inv[x ⊗ y]), e]} -->

Several conjectures give one dataset, and only proved conjectures contribute to it. Besides the graphs, the dataset counts the positive and negative examples and the proofs:

```wl
KeyDrop[TAtpGraphDataset[{inv[inv[a]] == a, inv[a \[CircleTimes] b] == inv[a] \[CircleTimes] inv[b]}, abelian, TimeConstraint -> 5], "Graphs"]
```
<!-- => <|"Labels" -> {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, "NPos" -> 17, "NNeg" -> 16, "NProofs" -> 2|> -->

[TAtpGraphDataset]() also takes the name of an [AxiomaticTheory]() theory, whose notable theorems it proves, and a [ProofObject]() from [TFindProof](), whose lemmas are the positive examples. A problem in the TPTP (Thousands of Problems for Theorem Provers) format can be proved with [TFindProof]() first. Each proof costs a search, so [TimeConstraint]() bounds each one.

## Training a network

[TAtpTrainGnn]() trains a GCN on a dataset. The network passes messages along the edges of each graph for a number of rounds, each round computing new node vectors *H'* = relu(*A*.*H*.*W1* + *H*.*Ws* + *b*) from the node vectors *H* and the normalized adjacency matrix *A*. It then averages the node vectors into one vector for the graph and reads off a score for the two classes, trained with categorical cross-entropy and the Adam optimizer. The result holds the trained `"Model"`, its area under the receiver operating characteristic (ROC) curve on the training graphs, and the loss at the start and at the end of training. Training compiles the tensor kernels of the network the first time it runs and can take minutes:

```wl
#| eval: false
TAtpTrainGnn[TAtpGraphDataset[{inv[inv[a]] == a, inv[a \[CircleTimes] b] == inv[a] \[CircleTimes] inv[b]}, abelian, TimeConstraint -> 5], "Hidden" -> 16, "Rounds" -> 2]
```
<!-- => <|"Model" -> <|"Kind" -> "GNN", "W1" -> {…}, "Ws" -> {…}, "Bh" -> {…}, "Wout" -> {…}, "Bout" -> {…}, "Rounds" -> 2, "Hidden" -> 16, "NMax" -> …|>, "TrainAUC" -> …, "LossStart" -> …, "LossEnd" -> …, "NPos" -> 17, "NNeg" -> 16|> -->

`"Hidden"` sets the length of the node vectors and `"Rounds"` the number of rounds of message passing. With the name of a theory, <code>[TAtpTrainGnn]()["*theory*"]</code> builds the dataset from the notable theorems of the theory and trains on it in one step.

## The pretrained model

The paclet comes with a pretrained model, stored in the safetensors format, the standard format for the weights of neural networks:

```wl
TAtpGnnScorerAsset[]
```
<!-- => the path of gcn_atp.safetensors in the Assets directory of the paclet -->

[TAtpLoadGnnScorer]() loads it as an [Association]() of weight matrices and settings. The settings of the pretrained model are three rounds of message passing on node vectors of length 64:

```wl
KeyTake[TAtpLoadGnnScorer[TAtpGnnScorerAsset[]], {"Kind", "Rounds", "Hidden", "NMax"}]
```
<!-- => <|"Kind" -> "GNN", "Rounds" -> 3, "Hidden" -> 64, "NMax" -> 64|> -->

[TAtpGnnScore]() scores each graph of a dataset; a higher score means the model rates the equation as more likely to be used in a proof. The scores for the eleven equations of the group example:

```wl
TAtpGnnScore[TAtpLoadGnnScorer[TAtpGnnScorerAsset[]], TAtpGraphDataset[{inv[inv[a]] == a}, abelian, TimeConstraint -> 5]]
```
<!-- => {-2.0863874554634094, -1.1306440234184265, -1.536804974079132, 0.22483105212450027, -1.3557806611061096, -1.1827852725982666, 0.3535003066062927, -5.955554485321045, -5.111791133880615, 0.05050660576671362, -1.3583860993385315} -->

[TAtpSaveGnnScorer]() writes a model to a file and gives the file:

```wl
file = TAtpSaveGnnScorer[TAtpLoadGnnScorer[TAtpGnnScorerAsset[]], FileNameJoin[{$TemporaryDirectory, "gcn.safetensors"}]]
```
<!-- => the path of gcn.safetensors in $TemporaryDirectory -->

Loading the file gives back the same weights:

```wl
TAtpLoadGnnScorer[file] === TAtpLoadGnnScorer[TAtpGnnScorerAsset[]]
```
<!-- => True -->

## Guiding a proof

[TAtpSetGnnScorer]() makes a model the scorer of the prover. It takes a model or the file of one:

```wl
TAtpSetGnnScorer[TAtpGnnScorerAsset[]]
```
<!-- => True -->

[TFindProofGnnReranked]() proves a conjecture with the model ranking the critical pairs: every `"RerankPeriod"` selections, the network scores the critical pairs in the queue and the queue is reordered by the scores. The scoring runs inside the prover, on the tensor runtime, with no evaluation in the Wolfram Language between selections. The result is the status of the search:

```wl
TFindProofGnnReranked[inv[inv[a]] == a, abelian, TAtpLoadGnnScorer[TAtpGnnScorerAsset[]], "RerankPeriod" -> 50, MaxSteps -> 8000]
```
<!-- => "PROVED" -->

The model only changes the order in which critical pairs are selected. The prover still selects the oldest pair in the queue at regular intervals, so every pair is eventually processed: a poor model can make a proof take longer, but it cannot make a provable conjecture unprovable. [TAtpSetGnnScorer]() with [Clear]() removes the model:

```wl
TAtpSetGnnScorer[Clear]
```
<!-- => True -->

How training works on the tensor runtime is the subject of [Training Neural Networks with THVMLink](paclet:WolframInstitute/THVMLink/tutorial/Train), and the prover itself of [Theorem Proving with THVMLink](paclet:WolframInstitute/THVMLink/tutorial/ATP).
