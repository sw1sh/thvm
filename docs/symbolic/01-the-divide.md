# 1. The symbolist/connectionist divide, and the Symbolica case study

(AI = artificial intelligence. GOFAI = good old-fashioned AI, the
classical symbolic tradition. CDL = categorical deep learning.
ACT = applied category theory. LLM = large language model.
ARC = Abstraction and Reasoning Corpus.)

## The two traditions

The **symbolist** tradition (GOFAI: logic, search, programs, explicit
rules) gives you compositionality, sample-efficiency, verifiability,
and out-of-distribution generalization, at the cost of brittleness
and combinatorial search. The **connectionist** tradition (neural
networks) gives you perception, noise-robustness, and gradient
learning from data, at the cost of being data-hungry and
generalizing poorly to genuinely novel structure.

Neuro-symbolic AI is the (roughly 50-year-old) project of getting
both at once. The reason it keeps mattering: there are problems where
the symbolist virtues are not optional. **ARC** (Chollet, *On the
Measure of Intelligence*, arXiv:1911.01547, 2019) is the cleanest
such benchmark -- it was built explicitly to show that scaling neural
nets does not buy skill-acquisition efficiency. Each task supplies a
few input/output examples of a novel transformation and asks you to
infer the rule and apply it to a held-out input. You cannot memorize
your way through it; you have to induce a rule from a handful of
examples. That is a symbolist task wearing a perception costume.

This repo hit the same wall from the other direction. The `brain/`
arc built a learned-world-model planner for ARC-AGI-3 (the
interactive successor benchmark) and found that the games it cannot
solve are precisely the ones whose dynamics it cannot *induce a rule
for* -- the convolutional world model fits transitions to 99%
per-cell accuracy while encoding no rule at all (it predicts the
modal "nothing changed"). See
[04-implications-for-thvm.md](04-implications-for-thvm.md).

## The Symbolica case study: a cautionary tale about betting on theory

If you are tempted to reach for an exotic symbolic-or-categorical
architecture, the most instructive recent data point is **Symbolica
AI**, the highest-profile bet that a principled structured approach
would beat transformer scaling.

The facts, as of May 2026 (sources in
[references.md](references.md#symbolica-and-categorical-deep-learning)):

- **Launch.** Founder George Morgan (ex-Tesla Autopilot engineer)
  raised roughly **$31M led by Khosla Ventures**, emerging from
  stealth 2024-04-09 (TechCrunch, Fortune). The pitch: small,
  *category-theoretic* structured models giving "greater accuracy
  with lower data, lower training time, lower cost, and provably
  correct structured outputs," targeting code synthesis and theorem
  proving.
- **The intellectual engine** was the position paper *"Categorical
  Deep Learning is an Algebraic Theory of All Architectures"*
  (Gavranovic, Lessard, Dudzik, von Glehn, Araujo, Velickovic;
  arXiv:2402.15332; ICML 2024). Its thesis: the universal algebra of
  monads valued in a 2-category of parametric maps is a single
  language for both architectural *constraints* and *implementations*,
  subsuming geometric deep learning's symmetry view and extending to
  stateful/automata computation that symmetry groups cannot express.
  Discussed in [03-geometry.md](03-geometry.md).
- **What happened by early 2026.** Symbolica **abandoned the thesis in
  product**. It now ships **Agentica**, a type-safe agent SDK that
  orchestrates *third-party* frontier LLMs (Claude Opus 4.6, GPT 5.4);
  the current docs and blog mention category theory nowhere. Its
  headline ARC-AGI results -- 85.28% on ARC-AGI-2, "0% to 36% on day
  1 of ARC-AGI-3" (2026-03-25) -- come from **Agentica orchestrating
  Opus 4.6 with a recursive sub-agent + persistent Python REPL**, not
  from any categorically-trained model. (Symbolica flags these as
  unverified public-set scores; treat accordingly.)
- **The originators left.** Per John Baez's well-sourced account
  (2025-02-08), Morgan "was never fully sold on category theory" and
  preferred his original hypergraph approach; internal conflict
  followed; Bruno Gavranovic's tenure was 2024-03 to 2024-12 and he is
  now back at Google DeepMind. The category theorists whose paper
  raised the $31M had largely exited within a year.

The lesson is **not** that category theory is worthless -- it is a
genuine and beautiful unifying language (see
[03-geometry.md](03-geometry.md)). The lesson is that, as of 2026, the
categorical-deep-learning program has produced **descriptive theory,
not benchmark-winning models**, and the most credible team to try it
defaulted to LLM-orchestration on ARC-AGI-3 -- the exact benchmark
this repo works on. That data point argues *against* building
anything exotic and *for* the boring-but-working pattern in
[02-integration.md](02-integration.md): neural proposes, symbolic
verifies, with the symbolic side being a concrete program search, not
a grand unifying formalism.
