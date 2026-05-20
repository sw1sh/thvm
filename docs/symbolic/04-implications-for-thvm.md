# 4. Implications for thvm

(ARC = Abstraction and Reasoning Corpus. CNN = convolutional neural
network. MPC = model-predictive control. DSL = domain-specific
language. LLM = large language model. IQE = Interval Quasimetric
Embedding. MRN = Metric Residual Network.)

This page connects the survey back to the running `brain/experiments/`
arc. It is the reason the doc set exists.

## The problem, stated precisely

The `brain/` arc built a learned-world-model planner for ARC-AGI-3:
an encoder, a forward model `f(frame, action) -> next_frame` (a small
CNN), a quasimetric `d(s, g)`, a Go-Explore archive, and a goal-MPC
planner. It reaches ~12-13 of 24 games at level >=1 given enough
compute (experiment 197). The residual stuck set -- re86, wa30, tr87,
lf52 (plus near-static ka59 and the fully-static games) -- resisted
*every* cheap lever:

- different encoder, including a transformer (experiment 199A) -- null
- more random / deeper / structured exploration (195, 199D, 199E) --
  null
- planning-side curiosity bonus in goal-MPC (199G) -- null

The diagnostic chain (198-199G) and this survey converged on one
diagnosis: **the toolkit has no mechanism for rule induction.** The
CNN forward model fits these games' transitions to ~0.99 per-cell
accuracy yet encodes no rule -- per-cell cross-entropy rewards
predicting the modal "nothing changed," so the model is a
pattern-fitter that never represents "the agent moves left when you
press a2." Without a faithful world model, goal-MPC plans over a
blind imagination and never navigates to the level-up trigger.

## What the survey says to do (and not do)

From [01-the-divide.md](01-the-divide.md): do **not** reach for an
exotic categorical architecture. The flagship bet on that (Symbolica)
abandoned it and defaulted to LLM-orchestration on this very
benchmark.

From [02-integration.md](02-integration.md): the winning pattern is
**neural-proposes / symbolic-verifies**, and for ARC specifically the
symbolic side is **program synthesis over a DSL** (DreamCoder-style),
not differentiable logic. An LLM is one proposal distribution -- the
laziest one. The capability actually missing is rule induction, and
program synthesis is the in-house, ARC-appropriate substrate.
Critically, [waldmeister](../../waldmeister) is a Knuth-Bendix
completion / equational term-rewriting engine: a program-synthesis
substrate already in the tree, the natural engine for a larger DSL
search.

## Experiment 200 confirmed the diagnosis (2026-05-20)

A no-neural, no-LLM probe
([brain/experiments/200_symbolic_induction](../../brain/experiments/200_symbolic_induction/README.md))
characterized the cardinal-only stuck games' transitions and induced
a minimal move-object DSL. Findings:

- **All three games are rule-governed, not random.** Each cell-rewrite
  histogram is dominated by **symmetric swap pairs** (wa30 0<->1 /
  1<->14; tr87 0<->3 / 5<->7; re86 5<->9 / 5<->11) -- the signature of
  a moving object that leaves background behind (vacated cell X->Y,
  entered cell Y->X, in equal counts).
- **The DSL recovered wa30's cardinal action mapping exactly from data
  alone**: a0=up, a1=down, a2=left, a3=right, a4=stay. A single-cell
  token (color 0) on background 1, plus a second token (color 14).
- A 3-line, zero-parameter symbolic model beat the CNN's changed-cell
  accuracy (0.374 / 0.286 / 0 versus the CNN's 0 / 0 / 0). The CNN, at
  0.99 overall accuracy, is blind to all of it.

This is the empirical validation of the survey's thesis: the stuck
games' dynamics are symbolic and simple; the blindness was the
absence of an induction mechanism. Chapter B is a **program-synthesis
dynamics model**, not an LLM oracle.

## The chapter-B plan (experiment 201 onward)

1. Grow the DSL: multiple movable tokens, wall/boundary collision
   (blocked move -> stay), explicit move-leaves-background semantics.
   Target: wa30 changed-cell accuracy 0.37 -> >0.9.
2. Plug the induced **symbolic** transition model into goal-MPC as the
   world model, replacing the CNN `f`, and run the stacked agent on
   wa30 at the 15k budget. Decisive question: with a faithful world
   model, does goal-MPC navigate to wa30's level-up trigger where the
   CNN-based planner (192/195) stayed at level 0? Zero LLM, zero new
   neural training.
3. If wa30 unlocks, generalize the DSL search (waldmeister as the
   engine for the larger search) to tr87 and re86 (the latter needs a
   richer rule -- a 3-color interaction, not a single translation).

## A free, independent win: MRN -> IQE

Separate from the chapter-B question, [03-geometry.md](03-geometry.md)
surfaced that this repo's quasimetric head (`quasi_d` in
`brain/qm_harness.py`, introduced in experiment 155-159) is
**MRN-class**, one rung below the state of the art. **IQE -- Interval
Quasimetric Embedding** (Wang & Isola, arXiv:2211.15120) is a
drop-in replacement: provably correct (identity, nonnegativity,
triangle inequality, positive homogeneity), ~1 parameter in the
distance head versus ~12,500 for MRN, with reported large accuracy
gains. It is low-risk and independent of the chapter-B decision; the
cheap validation is to swap it in and re-run held-out quasimetric rank
accuracy on a few games against the MRN baseline.

## Reader Q&A

(Follow-up questions about this doc set get folded in here, dated.)

- **2026-05-20 -- "Why do you need an LLM really? Just for its
  language prior?"** Largely yes, in the naive framing -- and that is
  the weakness. ARC-AGI is *designed* to defeat priors, so swapping a
  proxy prior for an LLM's pretraining prior is trying a different
  prior on games that are stuck *because* they are prior-resistant.
  The only defensible reason to use an LLM is **in-context rule
  induction** (reason over observed transitions, infer the rule), a
  capability the feedforward models genuinely lack -- but if that is
  what is missing, **program synthesis** is the more appropriate and
  self-contained tool, and the term-rewriting engine in this repo is a
  natural substrate. Experiment 200 then confirmed the stuck games are
  rule-governed and symbolically capturable without any LLM. This
  exchange is what motivated the whole doc set.
