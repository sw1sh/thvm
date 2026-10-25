# Tier 2: symbol-independent GNN critical-pair scorer

Design for the ENIGMA-Anonymous tier of the ML-in-ATP roadmap
(`docs/plans/atp_ml_roadmap.md`; field context in
`docs/atp/ml_guidance.md`). Tier 1 (the 14 hand-features + linear/MLP
scorer + coop) is landed. Its ceiling is the two symbol-name features
(`top_symbol_l/r`): a raw functor label means nothing across problems
with different signatures, so a Tier 1 model cannot transfer what it
learns about "the operator that behaves like a group product" from one
theory to another. The graph neural network (GNN) with **anonymisation**
removes that ceiling -- it scores a critical pair (CP) from its
*structure*, invariant to renaming the symbols and variables.

This is the tier that, in the literature (ENIGMA Anonymous, IJCAR 2020;
Olsak/Kaliszyk/Urban property-invariant embeddings, ECAI 2020), cracks the
hold-outs hand-features cannot.

## 1. Representation: the anonymised CP hypergraph

A CP is a single equation `lhs = rhs` (no clausal structure beyond the
pair). We encode it as a typed graph with three node kinds, following the
property-invariant scheme:

- **Term nodes** -- one per distinct subterm occurrence of `lhs` and
  `rhs`, plus a CP super-node. (Sharing identical subterms into one node
  is an option; the v0 scaffold keeps occurrences distinct for simplicity
  and lets message passing discover sharing.)
- **Symbol nodes** -- one per distinct function/constant *label* in the
  CP. Critically, a symbol node carries **no label-derived feature**; its
  identity is purely its connectivity. Initial embedding is a function of
  structural facts only: arity bucket, is-constant, occurrence count.
- **Variable nodes** -- one per distinct variable id in the CP, again
  anonymised: initial embedding depends only on "is a variable" (+ later,
  occurrence multiplicity), never the raw id.

Typed edges (each edge type gets its own message function):

- `term -> top-symbol` (or `term -> variable`): a term node links to the
  symbol/variable node at its root.
- `term -> child-term`, tagged with argument position (position handled by
  a small positional embedding added to the child message, so the GNN is
  argument-order aware without being symbol-name aware).
- `cp-super -> lhs-root`, `cp-super -> rhs-root` (a distinct edge type for
  the two sides so the model can tell them apart but treats the equation
  symmetrically if it learns to).

Because no node embedding is seeded from a label or a var id, two CPs that
differ only by a consistent renaming of symbols and/or variables produce
**identical anonymised node-feature arrays and isomorphic edge sets**.
That invariance is the whole point and is the property the scaffold test
must assert.

## 2. The GNN

Message passing over the typed edges, R rounds (R = 2..4):

```
for r in 1..R:
  sym'  = ReLU(Wsym  . sym  + aggregate_over(term->sym  edges)(term))
  term' = ReLU(Wterm . term + aggregate_over(term->child edges)(child + posemb)
                            + aggregate_over(term->sym   edges)(sym))
  (sym, term) <- (sym', term')
score = w . pool(cp-super embedding) + b      # a raw logit
```

thvm primitives: the aggregation is gather/scatter over the COO edge index
(`TGather` / scatter-add) + `TMatMul` + `Ramp`; `pool` is the cp-super
node's final embedding (or a mean over term nodes). All differentiable, so
`TNetTrain` trains it on the same labelled dataset Tier 1 uses
(`TAtpCpDataset` already emits the labels; only the per-CP *input* changes
from a 14-vector to a graph). This stays entirely on thvm's tensor stack.

## 3. The latency wall (the hard part)

A saturation prover scores thousands of CPs/sec; per-CP GNN inference in
the hot loop is the bottleneck ENIGMA solved with an amortised evaluation
server + a cheap parental-guidance pre-filter. thvm's batched-tensor + JIT
design is the right lever, so design for batching from day one:

1. **Cheap pre-filter in C.** The Tier 1 linear scorer (already in the
   engine, near-free) gates which CPs are even worth a GNN score -- only
   the top fraction by the linear logit get graph-scored.
2. **Batch, do not loop.** Score a *batch* of queued CPs in one thvm
   forward (the heap already holds them); never one-at-a-time.
3. **Compile the forward.** `TJit` the GNN forward to a fused kernel so a
   batch eval is a single dispatch.
4. **Periodic, not per-selection.** Re-rank the queue with the GNN every K
   selections (between, the cheap Tier 1 / GT order holds), amortising the
   batch cost. Coop and the FIFO fairness keep completeness regardless.

Integration shape: unlike Tier 1's in-C per-CP scorer, the GNN runs
**batched in WL** over exported CP graphs and pushes per-CP priorities
back. That needs a new engine seam: "dump the queued CPs' graphs" +
"accept a priority vector." This is more than Tier 1's single setter, so
it is the second implementation step, after the extractor.

## 4. Implementation steps

1. **C hypergraph extractor (the scaffold, first).**
   `thvm_atp_cp_graph(s, lhs, rhs, out)` -> a fixed-schema graph:
   node-type vector, anonymised initial node-feature matrix, and a typed
   COO edge index. Pure read of the terms (mirrors `atp_symbol_count` /
   `atp_term_depth` recursion). Caps node/edge counts (overflow -> signal,
   caller falls back to Tier 1). Test asserts: well-formedness, node/edge
   counts on a hand term, and **renaming invariance** (relabel every
   symbol + renumber every variable -> identical feature arrays + same
   edge multiset). This is independently testable with zero ML.
2. **WL graph export + batched GNN.** A LibraryLink that dumps the queued
   CPs' graphs as packed tensors; a `TFromNet`/hand-built thvm GNN; a
   `TAtpTrainScorer`-analog that trains it (reusing `TAtpCpDataset`
   labels). Model "Kind" -> "GNN".
3. **Periodic batched re-rank hook.** The engine seam to accept a
   WL-computed priority vector every K selections, gated behind the
   pre-filter; coop/FIFO keep completeness.
4. **Measure** on the held-out corpus vs Tier 1 + GT, same harness as
   `docs/atp/ml_guidance.md` section 5.

## 5. Open questions

- Subterm sharing (DAG vs tree) -- start with tree (occurrences distinct),
  revisit if message passing under-shares.
- Where the pre-filter cutoff sits (top-k vs threshold on the linear
  logit) -- tune empirically.
- In-C GNN evaluator vs WL-batched -- start WL-batched (reuses the stack,
  no C GNN code); move hot pieces to C only if the WL round-trip dominates.
