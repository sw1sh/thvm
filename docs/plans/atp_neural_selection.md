# Neural clause selection, second attempt: the Suda 2025 design

Scoping plan (2026-09-28). Not a landed change; go / no-go gates per phase.

## Why now

At CASC-J13, Vampire 5.0.1 scored 370/400 in UEQ against Vampire 5.0's 322,
both entered the same year. The 5.0.1 system description lists one
search-relevant competition change: clause-selection guidance based on graph
and recursive neural networks [Sud25] (the rest is higher-order merging,
proof output and portability). Of the 55 UEQ problems our current build
still misses (July pod re-sweep, 343/400, measured at 240s without `--proof`,
so an upper bound), 14 are solved only by Vampire 5.0.1, including 12 `-10`
encoded problems that Vampire 5.0 also misses
(LCL263/278/282/304/331/339/351/395-10, PLA008/014/015-10, LAT065-10).
Vampire 5.0.1's margin over 5.0 is largest on the J13 problems that were in
neither CASC-30 nor CASC-28 (96 vs 77 of 109), the subset where our own
tuning transfers worst (52 of 109).

Our own learned-selection arc (ENIGMA Tier 1 and Tier 2, see memory
project_atp_enigma_ml) reached good offline ranking (AUC 0.904, precision@100
up 56x after DAgger) but was negative online: armed runs solved 73-75 against
136 unguided, 0 of 35 targeted misses flipped. The named bottleneck was
per-candidate scoring cost starving the step budget. Tier 2 ran a GNN over a
per-critical-pair hypergraph and re-ranked the queue periodically.

## The reference design ([Sud25] = Suda, "Efficient Neural Clause-Selection Reinforcement", arXiv 2503.07792)

- A GNN runs ONCE over the input CNF: 8 message-passing rounds, embedding 32
  (expanded 256), five node types (sort, symbol, variable, subterm, clause),
  GraphSAGE convolutions, 16 edge kinds. It yields embeddings for input
  clauses and signature symbols.
- Each derived clause gets a "generalized age" embedding from a recursive
  network over its derivation history: a trainable embedding for the
  inference rule (205 rules) concatenated with exactly two parent-embedding
  slots. A clause's embedding never changes once set.
- A second recursive network over the term structure ("generalized weight")
  runs over the clause's term DAG with shared, cached subterm embeddings;
  literal embeddings are summed.
- Both embeddings plus 12 simple features (age, weight, literal counts,
  variable occurrences, ...) feed a one-hidden-layer MLP that outputs a logit.
- Selection uses a SINGLE queue ordered by that logit (optional Gumbel noise
  for exploration). Newly derived clauses wait in a delayed insertion buffer
  and are scored in batches. LRS is disabled.
- Training is policy gradient: reward 1 for clauses that end up in the found
  proof, averaged over the passive set at each selection step; 24
  iterations, adaptive problem weighting, early stopping on a 20% validation
  split.
- Cost: about one third of a 30000 Mi instruction budget (model load 470 Mi,
  GNN 1740 Mi, clause evaluation 9420 Mi). Result on TPTP 9.0.0 (15000 train
  / ~4500 test): 46.3% to 54.5% solved on unseen problems (+20%), with 5.5x
  fewer clause activations on commonly solved problems.

## Mapping onto a completion engine

The engine's "clauses" are equations and critical pairs (CPs). Every CP
already carries its parents and its rule in the trace (TRACE_CP with parent
trace ids, TRACE_ORIENT, interreduction and rewrite records), so the
derivation-history network maps directly: embedding(CP) = f(rule, emb(parent
a), emb(parent b)). The GNN over the input problem runs once per slice, on
the existing CPU tensor runtime (the Tier 2 GCN forward already runs there).

The hard difference is volume. Completion queues hold millions of CPs (the
Sheffer c3 proof holds 28M). The history embedding is one small MLP per CP,
roughly a few thousand multiply-adds, affordable. The term-structure network
is the expensive part without hash-consed terms; it is phase 2, not phase 1.

## Phases and gates

Phase 0 (offline). Export per-CP derivation history (parent rules, rule
kind, push / selection / deletion step, the engine's priority, simple
features) as an opt-in, observation-only trace; label proof CPs from the
emitted proof's citation set (`thvm_atp_cp_label` still seeds from the
single-normal-form extractor and labels nothing on multi-normal-form
closures); train the history network and the simple-feature head offline.
Gate: on held-out problems, at sampled selection steps, the model ranks the
pending proof CPs (the ones the run selects later) at least twice as high as
the engine's own priority does (median percentile rank in the reconstructed
queue), and its top 10 contains a proof CP at least twice as often. Queue
membership per step is rebuilt from the push / selection / deletion steps,
with non-proof CPs subsampled at a recorded rate.

Phase 1 (online, one lane). Score CPs at push time from the parents'
cached embeddings, order a single queue by score, keep the age FIFO as a
fairness fraction. Gate on a pod A/B over the 400 J13 UEQ problems: the lane
must add solves that no current lane gets, without losing more than it adds
in the schedule composition.

Phase 2 (policy gradient + term network). Replace supervised labels with the
[Sud25] objective and add the term-structure network if hash-consing makes
it cheap enough. Gate as in phase 1.

Train and validate on problems disjoint from the evaluation set, by name:
J13 reused earlier CASC problems (232 of its 400 UEQ problems were in
CASC-30's 300), so the local CASC-28 and CASC-30 copies leave only 144
problems outside J13. The training pool is the TPTP library's unit-equality
problems minus the J13 400. The J13 lesson: we tied Twee on the problems we
had tuned against and trailed it by 18 on the rest.

## Risks

- Volume: history embeddings for 10M+ CPs cost memory (32 floats per CP is
  1.3 GB at 10M). Store embeddings only for rules and selected CPs, and
  compute a CP's score at push time without storing its embedding.
- Cross-domain transfer is weak ("Teaching Vampire New Tricks",
  arXiv 2609.16228: models help within a benchmark domain, underperform the
  default across domains). CASC draws from TPTP, so training on TPTP is
  in-domain.
- Our Tier 2 negative is a real prior. Phase 0's offline gate is cheap and
  decides whether phase 1 is worth building.
