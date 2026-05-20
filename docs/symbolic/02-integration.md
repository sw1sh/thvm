# 2. Neuro-symbolic integration that actually works

(NeSy = neuro-symbolic. FOL = first-order logic. DSL = domain-specific
language. LTN = Logic Tensor Networks. NTP = Neural Theorem Prover.
ABL = abductive learning. RL = reinforcement learning. IMO =
International Mathematical Olympiad. TTT = test-time training. MDL =
minimum description length. ARC = Abstraction and Reasoning Corpus.)

## Kautz's taxonomy

Henry Kautz's six-way taxonomy (AAAI 2020 lecture; popularized in
Garcez & Lamb, *Neurosymbolic AI: The 3rd Wave*, arXiv:2012.05876) is
the standard map of integration patterns, in roughly increasing order
of entanglement:

1. **`Symbolic-Neuro-Symbolic`** -- symbolic input/output, neural
   processing. This is where LLMs sit.
2. **`Symbolic[Neuro]`** -- a symbolic solver as the outer loop calls
   a neural subroutine. Example: AlphaGo (Monte Carlo tree search +
   neural position/value evaluation).
3. **`Neuro | Symbolic`** -- a pipeline that hands off between
   distinct neural and symbolic components. The generate-and-verify
   pattern lives here.
4. **`Neuro: Symbolic -> Neuro`** -- symbolic rules compiled into a
   network's structure or weights, then trained.
5. **`NeuroSymbolic`** -- FOL "tensorized," neural methods reason over
   the tensorized representation. Example: Logic Tensor Networks.
6. **`Neuro[Symbolic]`** -- a neural model performing symbolic
   reasoning internally. Example: graph neural networks.

## The concrete systems (and their honest limits)

- **Logic Tensor Networks** (Badreddine, Garcez, Serafini, Spranger;
  *Artificial Intelligence* 303, 2022; orig. arXiv:1606.04422). "Real
  Logic": predicates are neural nets, truth values live in [0,1],
  training maximizes satisfiability of a knowledge base (logic as a
  loss). *Limit:* soft satisfaction is not sound deduction; scales
  badly with quantifier nesting.
- **DeepProbLog** (Manhaeve, Dumancic, Kimmig, Demeester, De Raedt;
  NeurIPS 2018, arXiv:1805.10872). Neural predicates inside the
  probabilistic-logic language ProbLog; exact inference via knowledge
  compilation gives gradients. *Limit:* exact compilation does not
  scale -- combinatorial grounding blow-up is the central bottleneck.
- **Neural Theorem Provers** (Rocktaschel & Riedel, NeurIPS 2017,
  arXiv:1705.11040). Differentiable backward chaining with soft
  unification over learned symbol embeddings; induces interpretable
  rules. *Limit:* the proof tree explodes combinatorially.
- **Scallop** (Li, Huang, Naik; PLDI 2023, arXiv:2304.04812).
  Differentiable Datalog via provenance semirings; choosing the
  semiring (e.g. top-k proofs) trades exactness for tractability. The
  most practical of the differentiable-logic line.
- **Abductive Learning** (Dai, Xu, Yu, Zhou; NeurIPS 2019). A
  perception net proposes symbolic facts; a logic engine *abduces* a
  consistent revision; consistency optimization yields pseudo-labels
  fed back to the net. Couples learning and reasoning without
  differentiating the logic. *Limit:* the inner abductive search is
  itself combinatorial.

## The pattern that actually wins: neural-proposes / symbolic-verifies

The systems that topped *hard* reasoning in 2024-2025 are all the
`Neuro | Symbolic` generate-and-verify pattern, not unified
differentiable logic:

- **AlphaGeometry / AlphaGeometry2** (Trinh et al., *Nature*
  2024-01; v2 arXiv 2025-02). A symbolic deduction engine closes
  proofs; a from-scratch language model proposes auxiliary
  constructions when the engine stalls. 25/30 then ~84% of IMO
  geometry.
- **AlphaProof** (DeepMind, *Nature* 2025-11). AlphaZero-style RL
  searching for **formal Lean** proofs against a sound verifier;
  with AlphaGeometry2, **28/42 = IMO silver, 2024**. Lean is the
  symbolic verifier -- proofs are machine-checked.
- **LLM-Modulo** (Kambhampati et al., ICML 2024, arXiv:2402.01817).
  The explicit position: LLMs cannot plan, but can *help* planning
  when wrapped by external sound verifiers in a generate-and-check
  loop.

The recurring recipe for making symbolic search *trainable*: a
learned/amortized **neural proposal distribution** drives a **sound
symbolic engine** (Lean, Datalog, a planner, a DSL search), optionally
improved with RL, with a test-time refinement loop. Make the proposer
fast and learned; keep the verifier sound and symbolic.

## Program synthesis: the symbolic-learning paradigm closest to "rule induction"

**DreamCoder** (Ellis et al., PLDI 2021; *Phil. Trans. R. Soc. A*
2023) is the canonical system. It learns to synthesize programs in a
DSL via a **wake-sleep** loop: *wake* searches for programs solving
tasks, guided by a learned neural recognition model (the amortized
proposal distribution); *sleep-abstraction* grows the DSL library by
refactoring recurring sub-programs into reusable abstractions (via an
E-graph / version-space refactor); *sleep-dreaming* trains the
recognizer on replayed and sampled programs. The library and the
search policy bootstrap each other -- it rediscovers map/filter/fold
and physical laws. This is the cleanest existing demonstration of
neural-guided symbolic search **plus** symbolic library learning
feeding back into the net.

E-graphs and term rewriting are the substrate underneath the
abstraction step. That is the direct hook to this repo:
[waldmeister](../../waldmeister) is a Knuth-Bendix completion /
equational term-rewriting engine, i.e. a program-synthesis substrate
already in the tree. See
[04-implications-for-thvm.md](04-implications-for-thvm.md).

## The ARC record

(Sources in [references.md](references.md#arc-and-the-benchmark-record).)

- **ARC-AGI-1.** Near-flat for four years (GPT-3/4 ~0%, GPT-4o ~5%).
  OpenAI **o3** broke through 2024-12-20: **75.7%** at $26/task,
  **87.5%** at ~$4,560/task -- first near-human result, at extreme
  cost.
- **ARC Prize 2024** (arXiv:2412.04604). Private-eval SOTA rose 33% ->
  55.5% over the year. The winning techniques clustered as
  **deep-learning-guided program synthesis** and **test-time
  training** (gradient updates on the task's own demo pairs at
  inference; Akyurek et al., arXiv:2411.07279, 47.5% via TTT).
- **ARC-AGI-2** (Chollet et al., arXiv:2505.11831). Built to defeat
  brute-force program search (multi-rule compositionality, in-context
  symbol definition). Human ~75%; frontier near floor at release.
- **ARC Prize 2025** (arXiv:2601.10904). Theme: the **per-task
  refinement loop**. Top compute-constrained ARC-AGI-2 score NVARC
  24.03%; verified frontier Gemini 3 Pro 54%, Claude Opus 4.5 37.6%.
  Notable: Tiny Recursive Model, CompressARC (MDL/compression, no
  pretraining), evolutionary program synthesis.
- **ARC-AGI-3.** The interactive/agentic successor, the format this
  repo's `brain/` arc targets.

The standing debate: ARC was built as a counter-argument to scaling,
and the strongest *open* results have come from program synthesis +
test-time adaptation (the symbolist camp); yet massive test-time-
compute neural reasoning (o3, Gemini 3) also climbs the original
benchmark, at costs orders of magnitude above humans. ARC-AGI-2's
near-floor AI scores against ~75% human are the field's current
evidence that scaled pattern-fitting alone has not closed the
abstraction gap.

## Making symbolic search differentiable -- the five strategies

1. **Continuous relaxation / fuzzy logic** (LTN, NTP, Semantic Loss).
   Biased; the recovered discrete program may not match the relaxed
   optimum.
2. **Exact differentiable inference via algebraic structure**
   (DeepProbLog knowledge compilation, Scallop semirings). Scales
   poorly; truncation reintroduces approximation.
3. **REINFORCE / RL over programs** (AlphaProof). High-variance
   gradients, sparse reward.
4. **Amortized inference / neural proposal distributions**
   (DreamCoder's recognition model). Keeps the search sound, makes it
   fast.
5. **Abduction / consistency-driven pseudo-labeling** (ABL). Avoids
   differentiating the logic; the inner search is combinatorial.

The strongest 2024-2026 systems combine a neural proposal (amortized)
with a sound symbolic verifier, plus RL and a test-time refinement
loop. That is the center of gravity, and it is what the closing page
recommends for this repo's stuck games.
