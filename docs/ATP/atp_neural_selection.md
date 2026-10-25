# Neural clause selection, second attempt: the Suda 2025 design

Scoping plan (2026-09-28). Not a landed change; go / no-go gates per phase.

Superseded 2026-10-01 for all ENIGMA work by docs/plans/enigma_portfolio.md (ENIGMA versus the CASC portfolio); the phase results below stay as the record.

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

## Phase 0 result (2026-09-29): FAIL

Infrastructure, verified on a pod: the opt-in history export
(`THVM_ATP_HISTORY_EXPORT`, `src/atp/history.c`) is observation-only (twee
lane, identical selections, proofs and stats with it on), and rebuilding the
queue from it puts the engine's actual pick at rank 0 in all 353,143
selections checked. Harvest (twee lane, 60s, `--proof`): 485 of 664 train
and 134 of 171 held-out problems solved, 12.6M critical pairs, about 25 proof
pairs per problem. The trainer (`tools/atp_enigma/history_net.py`) runs on
the tinygrad API.

On the validation slice carved from train (64 problems), the engine's own
priority already places the pending proof pairs at a median percentile of
0.108, and its top 10 holds a proof pair at 60% of sampled steps. Every
trained model was worse; the pre-registered rule chose the dimension-32
history net (validation median ratio 1.13). Its one held-out run (132
problems): median percentile 0.111 against the engine's 0.113 (ratio 0.99,
95% bootstrap interval over problems 0.72 to 1.20), top-10 hit rate 0.646
against 0.633 (ratio 1.02, interval 0.95 to 1.10). The pointwise history
net ties the engine; the gate needed ratios of 0.5 and 2.

Two lessons. The pre-registered top-10 criterion (twice the engine's hit
rate) was unattainable once the engine exceeds 0.5, a flaw in the gate, not
the model. And the objective does not match the gate: class-balanced
cross-entropy scores each pair in isolation, while selection ranks it against
the rest of the queue at that step.

## Phase 0b result (2026-09-29): FAIL, but the first model that beats the engine

Pre-registered before any 0b training or held-out number
(`~/.cache/thvm/casc/J13/neural_p0/gate_spec_0b.txt`): same pool, split,
steps and queue reconstruction; PASS iff the model's median-percentile ratio
to the engine is at most 0.5 AND its top-10 miss rate (1 - hit) at most half
the engine's. Trainer: `tools/atp_enigma/listwise_net.py`. The loss is
listwise: at each sampled selection step, -log of the softmax mass of the
pending proof pairs within the rebuilt queue (positives weigh 1, sampled
negatives 1/rate), the supervised form of the [Sud25] policy gradient. The
score takes the engine's own signal at the step (its rank, rank over queue
size and log priority, computed without labels) and starts as exactly the
engine's ranking: s = -kappa log1p(engine rank) + an MLP correction whose
output layer starts at zero.

| variant (validation, 64 problems) | median ratio | miss ratio |
|---|---|---|
| features + engine signal head, no history (H32) | 0.636 | 0.782 |
| history net, dim 16 (N16) | 0.635 | 0.819 |
| history net, dim 32 (N32) | 0.596 | 0.792 |

The fixed rule (lowest max of the two ratios) chose H32. Its one held-out
run (132 problems): median percentile 0.078 against the engine's 0.113
(ratio 0.69, interval 0.55 to 0.87; better on 86 problems, worse on 44), top-10
miss rate 0.282 against 0.368 (ratio 0.77, interval 0.66 to 0.87). FAIL
against the 0.5 thresholds, but both intervals exclude 1: the listwise
objective with the engine's signal as input is the first model in this arc
that ranks proof pairs better than the engine's own priority on unseen
problems. The derivation-history embedding adds nothing measurable over
the features-only head (N32's median edge comes with a worse miss rate).

Caveat for any follow-up gate: the positives are the proof the engine found
along its own selection order, so an offline comparison on that trajectory
favors the engine. The rank feature is also step-dependent; an online lane
(phase 1) would score with the static columns (priority, features).

tinygrad 0.11 trap met on the way: `Tensor.zeros(shape).contiguous()` is
hash-consed, so a parameter initialized that way shares its buffer with
Adam's same-shaped `m`, `v`, `b1_t` and `b2_t` (built the same way) once a
forward pass realizes it, and the first step writes NaN. Initialize
constant parameters from numpy. Phase 0's parameters were all random, so
its Adam was unaffected.

## Phase 0c result (2026-09-29): FAIL, the static head ties the engine

Pre-registered before the harvest and any training
(`~/.cache/thvm/casc/J13/neural_p1/gate_spec_0c.txt`): 0b's pool, split,
steps, queue reconstruction, loss and selection rule, but the model sees
only what a selector has at push: s(c) = -kappa log1p(cp_pri at push) + an
MLP over the standardized 14 engine CP features (`thvm_atp_cp_features`)
and log1p(cp_pri at push), output layer at zero so it starts as the
engine. PASS iff the upper ends of both bootstrap intervals (median-
percentile ratio, top-10 miss ratio) are below 1. Trainer:
`tools/atp_enigma/static_net.py`.

Seven of the 14 features (GT and MIX2 weights, goal weight, top-symbol
labels, orientability, unification measure) cannot be derived from the
phase-0 export, so the harvest was re-run with the export's new `F` record
(`THVM_ATP_HISTORY_CPFEAT=1`: the queued pair decoded from the slot,
`thvm_atp_cp_features` with age = seq). On phase 0's 7 observation problems
selections, proofs and stats match with the export off and on (GRP417-1
differed once on the off side, an 11 s run straddling the lane's 10 s
CPU-time interreduction trigger; three repeats of each side then matched
phase 0). The `F` columns that the `P` record also defines agree exactly on
6.7M rows. The decode roughly doubles the export's cost (median 1.8x wall on
common solves), so the 60 s harvest lost four wall-edge solves (train 482,
held-out 133, against 485 and 134). FindProof runs without a heap
collector, so the decoded cells are never reclaimed; one long unsolved run
(BOO078-1) exhausted the heap.

| variant (validation, 64 problems) | median ratio | miss ratio |
|---|---|---|
| S16 (hidden 16) | 0.950 [0.66, 1.73] | 0.980 [0.86, 1.11] |
| S32 (hidden 32) | 0.954 [0.65, 1.74] | 0.961 [0.84, 1.09] |

The rule chose S32. Its one held-out run (131 problems): median percentile
0.124 against the engine's 0.112 (ratio 1.10, interval 0.92 to 1.34;
better on 67 problems, worse on 60), top-10 miss rate 0.345 against 0.362
(ratio 0.95, interval 0.87 to 1.04). FAIL: a tie. The head helps on small
problems (under 100 selections: median 0.125 against 0.167) and loses on
large ones (over 1000 selections: 0.135 against 0.061). No gated problem
re-scored a queued CP in place, so the engine ranked by exactly the push
priority the head starts from.

So 0b's edge came from the step-dependent signal (a CP's rank in the live
queue), not from anything a CP carries at push. A push-time scorer over
these features has no offline support, and phase 1 as planned (score once
at push) is not built. Using the queue-relative position online means
re-scoring the queue head periodically, the Tier-2 re-rank design whose
per-candidate cost was its named bottleneck.

## Phase 0d result (2026-09-29): the selection-time re-ranker beats the engine offline, ties it online

Pre-registered before any code or number
(`~/.cache/thvm/casc/J13/neural_p2/gate_spec_0d.txt`). The deployable form
of 0b: at every weight (non-FIFO) selection, take the engine's top K by its
own key and let a head pick among them. Inputs are what a selector has at
that moment: the candidate's rank in the top K, the queue size, its
priority, the 14 push features and its age in selections. The head is 0b's
listwise head trained within the top K, starting as the engine's pick.
Trainer and gate: `tools/atp_enigma/topk_net.py`, on the 0c harvest, exact
(unsubsampled) problems only.

Step A, offline. Six variants (K 8 / 16 / 32, hidden 16 / 32); every one
beat the engine on validation, and the rule chose K 32, hidden 32. On the
held-out split (109 problems, 4,667 selections with a proof CP pending) the
engine's own pick is a proof CP 20.4% of the time and the re-ranker's 30.9%:
ratio 1.51, 95% interval 1.39 to 1.66, better on 82 problems and worse on
17. PASS. The top 32 holds a proof CP at 76% of those selections, so any
re-ranker is capped at 3.7 times the engine's rate.

Step B, online. `src/atp/topk.c` (FindProof `--rerank-topk=32
--rerank-model=tools/atp_enigma/models/topk_rerank_R32h32.safetensors`)
walks the CP heap for the root's 32 best at each weight pick and extracts
the head's choice. It reproduces the offline inputs exactly: the goal
columns are evaluated against the goal normal form as it stood at each
CP's push, and the candidates' pairs are decoded into heap scratch popped
after the scoring. On eight problems the engine's candidate sets, choices
and inputs equal the queue rebuilt from the same run's history export, and
its scores equal the Python head's to 1e-14. Unarmed, selection is
byte-identical (test_atp, the OA and DN pins, twee-lane proofs).

The online gate (171 held-out pool problems, 60 s, twee against
twee-rerank): both lanes solve the same 134 problems, none gained, none
lost. FAIL. The re-ranker finds most of those proofs in fewer selections
(fewer on 80 of 132, more on 26, geometric mean 0.85; LAT177-1 9,569 to
1,865), at 51 microseconds per pick, but the 37 problems twee misses stay
missed. The offline metric rewards picking the engine's own proof CPs
sooner; the misses need a different proof or far fewer steps, which a
one-step top-K re-ranking did not provide. The lane stays out of the
schedule and J13 was not run.

## Phase 0e result (2026-09-29): why better ranking does not convert; online FAIL

Pre-registered in two sections, each before the runs it governs
(`~/.cache/thvm/casc/J13/neural_p3/gate_spec_0e.txt`). Three hypotheses
for 0d's tie.

H3, overhead. On 0d's 134 common solves the re-ranked lane's wall is
0.87 of twee's (geometric mean, interval 0.79 to 0.95; slower on 42,
faster on 53), tracking its selections (0.85); the re-ranker itself takes
3.4% of the lane's wall. Not the reason.

H1, a longer wall. At 300 s neither twee nor the re-ranked lane solves
any of the 37 problems both miss at 60 s (FAIL; the gate needed +3).
Over a third of those runs end on the 3.5 GB memory guard before 300 s,
most of the rest run the full 300 s, and four exit at once (the lane
cannot search them).

H2, distribution shift, tested with one DAgger round. A harvest with the
0d head armed (history export on, the re-ranker's own proofs as labels)
gives the states the re-ranker walks; `topk_net.py prep ...
rerank:32:<model>` rebuilds them and replays the head on the rebuilt
inputs (it reproduces 96,618 of 96,675 stored picks; the rest are FIFO
picks). On those states 0d's head picks a proof CP at 20% of weight
selections against 27% on the engine's states (held-out 24% against
31%), although the labels there favor its own picks: the shift is real. A
head trained on both trajectories to convergence (`train2`, early
stopping; 0d had stopped at a 30-epoch cap with the loss still falling)
fixes the offline number:

| head (validation) | re-ranker-visited states | engine-visited states |
|---|---|---|
| engine pick | 0.158 | 0.169 |
| 0d, R32h32 | 0.202 (rho 1.27) | 0.268 (rho 1.58) |
| DAgger, D32h64 (chosen) | 0.314 (rho 1.98) | 0.303 (rho 1.79) |

Held-out: 0.359 against R32h32's 0.244 on re-ranker-visited states. A K
of 64 lifts the ceiling (a proof CP among the candidates) from 0.72 to
0.86, but the K 64 heads rank worse than K 32. The chosen head is
`tools/atp_enigma/models/topk_rerank_D32h64_dagger1.safetensors`; engine
and Python agree on it (parity and featparity on 8 problems).

Online it shortens proofs further (selections 0.78 of twee's, geometric
mean) and still solves exactly twee's 134 of the 171 held-out problems at
60 s (FAIL). At 300 s on the 37 it gains one, GRP441-1 (twee 0,
DAgger head 1; FAIL, needed +3): its proof is VerifiedGood under strict
CheckProof and repeats 3 of 3 times at 19,612 selections, while twee
reaches the memory guard at 20,480 every time.

Why it does not convert: the misses are far from the budget, not near
it. Twee's slowest held-out solve takes 18 s on one pod and 37 s on
another, nothing is solved between 38 and 60 s, and nothing more by
300 s. A 0.78x step factor moves a problem across 60 s only from the
(60, 77] s band, which is empty. Many misses are bounded by queue growth
(memory), and the one conversion is a proof found just under that
ceiling. Picking the eventual proof's CPs earlier at the head of the
queue shortens proofs the engine already finds; the misses need a search
orders of magnitude cheaper in selections or memory, or a different
proof. The lane stays out of the schedule; J13 was not run.

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
