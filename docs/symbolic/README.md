# Symbolic and neuro-symbolic AI

(AI = artificial intelligence. NeSy = neuro-symbolic.)

A research orientation on the symbolist tradition, the ways people
combine symbolists with connectionists, and the geometry/algebra
threads that try to unify both. Written May 2026, motivated directly
by a concrete problem in this repo: the `brain/` experiment arc found
that a handful of ARC-AGI-3 games (re86, wa30, tr87, lf52) are stuck
not for lack of compute or a better prior, but because the toolkit
has no mechanism for **rule induction** -- inferring the symbolic
rule that governs a game's dynamics from a few observed transitions.
That is a symbolist problem, and this doc set surveys the field that
owns it.

Like [docs/neuroscience/](../neuroscience/README.md), this is a
*reading and orientation* document: opinionated about what is solid
versus vapor, every claim linked to a source in
[references.md](references.md). Unlike the neuroscience set, it has a
live tie-in to running code -- the closing page connects the survey
back to the `brain/experiments/` arc (experiment 200 onward) and to
the [waldmeister](../../waldmeister) term-rewriting engine, which is a
program-synthesis substrate already in the tree.

## Pages

1. [The symbolist/connectionist divide, and the Symbolica case
   study](01-the-divide.md) -- what each tradition buys you, why ARC
   exists, and a cautionary tale: the highest-profile "category
   theory will replace transformers" startup quietly abandoned that
   thesis and now ships an LLM-orchestration SDK.
2. [Neuro-symbolic integration that actually works](02-integration.md)
   -- Henry Kautz's taxonomy, the concrete systems (Logic Tensor
   Networks, DeepProbLog, AlphaGeometry, AlphaProof, Scallop,
   abductive learning), program synthesis (DreamCoder), and the ARC
   benchmark record. The pattern that wins: neural-proposes /
   symbolic-verifies.
3. [The geometry of learning](03-geometry.md) -- Geometric Deep
   Learning, categorical deep learning, the honest "equivariance at
   scale" verdict, and the quasimetric-geometry-for-planning thread
   that connects directly to this repo's own value functions
   (including the IQE upgrade to our metric head).
4. [Implications for thvm](04-implications-for-thvm.md) -- the
   actionable through-line: rule induction as the missing ingredient,
   program synthesis (not an LLM oracle) as the in-house answer,
   waldmeister as the engine, and a free win (MRN -> IQE) sitting in
   `qm_harness.py`.

## Conventions

- **Expand abbreviations on first use** per page (each page reads
  standalone); the directory-wide cheat sheet is below as a backstop.
- **Every quantitative claim is dated and sourced.** Company
  trajectories and benchmark numbers move fast; items that could not
  be verified against a primary source are flagged inline as
  `[unverified]`.
- **Follow-up questions get folded in.** When the reader asks a
  clarifying question about something here, the answer is written
  back into the relevant page as an attributed note (date + the
  question), not just answered in chat.
- No em dashes (use `--`), no Unicode box-drawing (use ASCII).

## Cheat sheet

| Abbreviation | Expansion |
|---|---|
| AI | artificial intelligence |
| NeSy | neuro-symbolic |
| GOFAI | good old-fashioned AI (the classical symbolic tradition) |
| FOL | first-order logic |
| DSL | domain-specific language |
| LTN | Logic Tensor Networks |
| NTP | Neural Theorem Prover |
| ABL | abductive learning |
| GDL | geometric deep learning |
| CDL | categorical deep learning |
| ACT | applied category theory |
| GNN | graph neural network |
| CNN | convolutional neural network |
| RL | reinforcement learning |
| MPC | model-predictive control |
| ARC | Abstraction and Reasoning Corpus (ARC-AGI) |
| IMO | International Mathematical Olympiad |
| MDL | minimum description length |
| TTT | test-time training |
| IQE | Interval Quasimetric Embedding |
| MRN | Metric Residual Network |
| QRL | Quasimetric Reinforcement Learning |
| LLM | large language model |
