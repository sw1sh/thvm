# 8. The other neuro-symbolic methods

(Abbreviations are page-local; expanded on first use here. NeSy =
neuro-symbolic. KG = knowledge graph. FOL = first-order logic. MRF =
Markov random field. MLN = Markov logic network. SAT = boolean
satisfiability. MAXSAT = maximum satisfiability. SDP = semidefinite
program. QP = quadratic program. DSL = domain-specific language. NAR =
neural algorithmic reasoning. GNN = graph neural network. VSA = vector
symbolic architecture. HDC = hyperdimensional computing. HRR =
holographic reduced representation. LLM = large language model. PAL =
program-aided language model. PoT = program of thoughts. API =
application programming interface. IMO = International Mathematical
Olympiad. ARC = Abstraction and Reasoning Corpus.)

This page is the catch-all: the neuro-symbolic families that the
earlier pages did not cover, surveyed at the same standard (dated,
sourced, opinionated about works-versus-curiosity). Pages
[02](02-integration.md) (the integration patterns that win),
[03](03-geometry.md) (geometry/category theory), and
[06](06-classical-foundations.md) (classical foundations) own their
material; this page does not re-derive it.

## 1. Knowledge-graph embeddings: symbolic relations as geometry

A knowledge graph (KG) is a set of `(head, relation, tail)` triples,
e.g. `(Paris, capital_of, France)`. KG-embedding methods learn a
vector per entity and a transformation per relation so that *true*
triples score high, then use the learned space for **link prediction**
(score a missing triple). The interesting bit for this doc set: each
method picks a different *geometric* realization of "relation."

- **TransE** (Bordes, Usunier, Garcia-Duran, Weston, Yakhnenko,
  "Translating Embeddings for Modeling Multi-relational Data," NIPS
  2013,
  <https://papers.nips.cc/paper/5071-translating-embeddings-for-modeling-multi-relational-data>).
  A relation is a **translation**: `h + r approx t`. Dead simple, few
  parameters, and it set the template. *Limit:* a single translation
  vector cannot represent symmetric relations (if `h + r = t` then
  `t + r != h` unless `r = 0`), nor one-to-many.
- **ComplEx** (Trouillon, Welbl, Riedel, Gaussier, Bouchard, "Complex
  Embeddings for Simple Link Prediction," ICML 2016,
  arXiv:1606.06357, <https://arxiv.org/abs/1606.06357>). Embeddings
  live in **complex** space; the score is the real part of a Hermitian
  product. Because the Hermitian product is not symmetric, ComplEx
  captures **antisymmetric** relations (`born_in`) while staying linear
  in time and space.
- **RotatE** (Sun, Deng, Nie, Tang, "RotatE: Knowledge Graph Embedding
  by Relational Rotation in Complex Space," ICLR 2019,
  arXiv:1902.10197, <https://arxiv.org/abs/1902.10197>). A relation is
  a **rotation** in complex space (`t = h * r` with `|r| = 1`).
  Rotations compose, invert, and can be self-inverse, so RotatE models
  symmetry, antisymmetry, inversion, and composition in one geometry.

Honest verdict (2026): KG embeddings are mature, useful, and *narrow*.
They do link prediction on a fixed schema well; they do not do rule
induction or multi-hop deduction natively, and they are not where
frontier reasoning lives. They matter to this doc set as the cleanest
worked example of "a symbolic relation is a geometric operator on a
vector," which is exactly the move [03-geometry.md](03-geometry.md)
discusses and the substrate a tensor VM computes. The
translation/rotation parameterizations are also a direct cousin of the
binding operators in section 5.

## 2. Differentiable solvers as neural layers

Instead of relaxing logic into a fuzzy loss (the LTN/NTP line in
[02-integration.md](02-integration.md)), this family embeds an *exact*
combinatorial or convex solver as a layer and back-propagates through
its solution via implicit differentiation of the optimality
conditions.

- **OptNet** (Amos, Kolter, "OptNet: Differentiable Optimization as a
  Layer in Neural Networks," ICML 2017, arXiv:1703.00443,
  <https://arxiv.org/abs/1703.00443>). Solves a quadratic program (QP)
  in the forward pass; differentiates through the Karush-Kuhn-Tucker
  optimality conditions in the backward pass. The foundational result:
  an argmin over constraints can be a layer.
- **cvxpylayers** (Agrawal, Amos, Barratt, Boyd, Diamond, Kolter,
  "Differentiable Convex Optimization Layers," NeurIPS 2019,
  arXiv:1910.12430, <https://arxiv.org/abs/1910.12430>). Generalizes
  OptNet to any **disciplined convex program**, so you write the
  problem in CVXPY and get a differentiable PyTorch/JAX layer for free.
  This is the practical, broadly-used member of the family.
- **SATNet** (Wang, Donti, Wilder, Kolter, "SATNet: Bridging deep
  learning and logical reasoning using a differentiable satisfiability
  solver," ICML 2019, arXiv:1905.12149,
  <https://arxiv.org/abs/1905.12149>). Embeds a smoothed MAXSAT solver,
  formulated as a semidefinite program (SDP) solved by coordinate
  descent, as a layer. Famously learns to play 9x9 Sudoku and the
  parity function from examples, with the *logical structure itself*
  learned as the SDP's coefficients rather than hand-coded.

The motivating application is **decision-focused learning** (a.k.a.
"predict-then-optimize"): train the perception net on the *quality of
the downstream decision* the solver makes, not on an intermediate
prediction loss. Honest verdict: this works and is in production use
(portfolio optimization, control, scheduling), but it scales with the
solver, not the GPU. SATNet in particular drew a notable critique that
part of its Sudoku result leaked label information through the
input encoding ("Are wider nets better given the same number of
parameters?"-style scrutiny in follow-up work); treat the
"learns logic from scratch" framing with care. The architectural idea
(a sound solver as a differentiable module) is solid and is the convex
cousin of the neural-proposes/symbolic-verifies pattern.

## 3. Markov logic networks, and Domingos's Tensor Logic

**Markov logic networks** (Richardson, Domingos, "Markov logic
networks," *Machine Learning* 62:107-136, 2006,
doi:10.1007/s10994-006-5833-1,
<https://link.springer.com/article/10.1007/s10994-006-5833-1>) attach
a real **weight** to each first-order-logic (FOL) formula. Grounding
the formulas over a set of constants produces a Markov random field
(MRF) with one feature per grounding; a hard constraint is the limit
of an infinite weight. This was the canonical statistical-relational
learning framework for a decade. *Limit:* grounding blows up
combinatorially (the same wall DeepProbLog hits in
[02-integration.md](02-integration.md)), and weight learning /
inference are expensive.

The same author's 2025 sequel is the one that matters for **a tensor
VM**:

- **Tensor Logic** (Pedro Domingos, "Tensor Logic: The Language of
  AI," arXiv:2510.12269, October 2025,
  <https://arxiv.org/abs/2510.12269>). [Verified against arXiv.] The
  central claim: **a logical rule and an Einstein summation are the
  same operation.** A Horn clause `p(X,Z) :- q(X,Y), r(Y,Z)`, with
  predicates as indicator tensors, is exactly the contraction
  `P[i,k] = sum_j Q[i,j] * R[j,k]`, i.e. `einsum('ij,jk->ik', Q, R)`.
  The *same* arithmetic is matrix multiplication, a relational join, a
  Datalog rule firing, and an attention head. Domingos proposes the
  tensor equation as the **sole construct** of an AI language, with
  neural nets, formal reasoning, kernel machines, and graphical models
  all reducing to it. A follow-up claims an actual implementation
  ("Implementing Tensor Logic: Unifying Datalog and Neural Reasoning
  via Tensor Contraction," arXiv:2601.17188, January 2026,
  <https://arxiv.org/abs/2601.17188>).

This is the most on-the-nose item in the whole doc set for this repo:
thvm is a **tensor** virtual machine, an einsum/contraction engine
with autodiff. If Domingos is right that logic *is* contraction, then
the substrate a tensor VM already provides is the substrate for
symbolic reasoning, no separate logic engine required. The
[docs/neuroscience/](../neuroscience/README.md) track already worked
through this; see its
[Part 1.5 on Tensor Logic](../neuroscience/06-classical-ml-and-rlhf.md)
(2026-05-12), which also disentangles Tensor Logic from the older
"differentiable Datalog" line (Cohen-Yang's TensorLog, NeurASP) and
from physics-style tensor *networks*. Caveat: as of May 2026 the
forward direction (Datalog as contraction) is old news, the *unifying
language* claim is a position paper plus an early implementation, and
the empirical case that it beats specialized systems is unproven.

## 4. Neural algorithmic reasoning

NAR asks a neural network to **execute a classical algorithm** (sort,
shortest path, dynamic programming) so that it generalizes to inputs
far larger than training, the way the real algorithm does.

- **"Neural Algorithmic Reasoning"** (Velickovic, Blundell, *Patterns*
  2(7), 2021, arXiv:2105.02761, <https://arxiv.org/abs/2105.02761>)
  states the **blueprint**: train a GNN "processor" to imitate an
  algorithm in an abstract latent space, then reuse that frozen
  processor inside a larger pipeline by attaching task-specific
  encoders/decoders. The processor is the reusable symbolic competence;
  the encoders bridge messy real inputs to it.
- **The CLRS Algorithmic Reasoning Benchmark** (Velickovic, Badia,
  Budden, Pascanu, Banino, Dashevskiy, Hadsell, Blundell, ICML 2022,
  arXiv:2205.15659, <https://arxiv.org/abs/2205.15659>) operationalizes
  it with 30 algorithms from Cormen-Leiserson-Rivest-Stein
  *Introduction to Algorithms*, providing step-by-step "hints" as
  supervision and testing out-of-distribution length generalization.

(Velickovic also co-authored the categorical-deep-learning position
paper discussed in [03-geometry.md](03-geometry.md); the NAR and GNN
work and the algebraic-architectures work are the same research
program viewed from two angles.) Honest verdict: NAR is a clean, real
research line with steady progress, but length generalization remains
the hard, unsolved part, and a GNN executing Dijkstra is not yet a
practical substitute for running Dijkstra. Its value to this repo is
conceptual: the "frozen processor as reusable symbolic module" is the
same instinct as caching an induced rule and reusing it across games
(see [04-implications-for-thvm.md](04-implications-for-thvm.md)).

## 5. Vector symbolic architectures / hyperdimensional computing

VSA, equivalently HDC, represent symbols as high-dimensional (thousands
of components) random vectors and define an algebra over them:
**binding** (combine a variable with a value, typically invertible),
**unbinding** (recover one from the bound pair), and **superposition**
(bundle several items into one vector by addition). Because the vectors
are near-orthogonal at high dimension, you can pack a structured,
symbolic object into one fixed-width vector and query it
approximately.

- **Holographic Reduced Representations** (Plate, "Holographic Reduced
  Representations," *IEEE Transactions on Neural Networks* 6(3):623-641,
  1995, <https://ieeexplore.ieee.org/document/377968>; expanded in the
  2003 book *Holographic Reduced Representation: Distributed
  Representation for Cognitive Structures*). Binding is **circular
  convolution**; this is the founding VSA.
- **Hyperdimensional computing** (Kanerva, "Hyperdimensional Computing:
  An Introduction to Computing in Distributed Representation with
  High-Dimensional Random Vectors," *Cognitive Computation* 1:139-159,
  2009, <https://link.springer.com/article/10.1007/s12559-009-9009-8>)
  named and popularized the paradigm, emphasizing robustness and
  cheap, parallel, near-memory operations.
- **Gayler** ("Vector Symbolic Architectures answer Jackendoff's
  challenges for cognitive neuroscience," 2003, arXiv:cs/0412059,
  <https://arxiv.org/abs/cs/0412059>) coined the term VSA and
  contributed the multiply-add-permute (MAP) algebra.

Recent VSA-for-ARC attempts exist and are worth noting precisely
because ARC is this repo's problem: **LARS-VSA** (Mraz et al.,
"LARS-VSA: A Vector Symbolic Architecture For Learning with Abstract
Rules," 2024, arXiv:2405.14436, <https://arxiv.org/abs/2405.14436>)
builds a VSA attention mechanism for relational tasks, and **"Vector
Symbolic Algebras for the Abstraction and Reasoning Corpus"** (2025,
arXiv:2511.08747, <https://arxiv.org/abs/2511.08747>) uses VSAs for
object-centric program synthesis on ARC, reporting ~10.8% on
ARC-AGI-1-Train (strong on the easier Sort-of-ARC / 1D-ARC variants,
modest on full ARC). Honest verdict: VSA/HDC is elegant and
hardware-friendly, has a small devoted community, and is mostly a
**research curiosity** at frontier scale; the binding/superposition
algebra is the genuinely interesting export, and it is, again, just
tensor operations a tensor VM runs natively.

## 6. LLM-side neuro-symbolic: tool use and program offloading

The dominant 2022-2026 NeSy pattern in practice is the laziest one:
**the LLM is the neural component, and it calls a symbolic
tool/executor** for the parts neural nets are bad at (exact
arithmetic, search, constraint solving). This is the
`Symbolic-Neuro-Symbolic` / `Neuro | Symbolic` corner of Kautz's
taxonomy from [02-integration.md](02-integration.md).

- **PAL** (Gao, Madaan, Zhou, Alon, Liu, Yang, Callan, Neubig,
  "PAL: Program-aided Language Models," 2022, ICML 2023,
  arXiv:2211.10435, <https://arxiv.org/abs/2211.10435>) and **Program
  of Thoughts** (Chen, Ma, Wang, Cohen, "Program of Thoughts Prompting:
  Disentangling Computation from Reasoning for Numerical Reasoning
  Tasks," 2022, TMLR 2023, arXiv:2211.12588,
  <https://arxiv.org/abs/2211.12588>) make the LLM emit a **program**
  as its reasoning trace and offload execution to a Python interpreter,
  so the arithmetic is exact. Same idea, near-simultaneous.
- **ReAct** (Yao, Zhao, Yu, Du, Shafran, Narasimhan, Cao, "ReAct:
  Synergizing Reasoning and Acting in Language Models," 2022, ICLR
  2023, arXiv:2210.03629, <https://arxiv.org/abs/2210.03629>)
  interleaves reasoning traces with tool **actions** (search,
  environment steps), letting the model gather facts mid-reasoning.
  This is the template under most "agent" loops today.
- **Toolformer** (Schick, Dwivedi-Yu, Dessi, Raileanu, Lomeli, Zettle-
  moyer, Cancedda, Scialom, "Toolformer: Language Models Can Teach
  Themselves to Use Tools," 2023, arXiv:2302.04761,
  <https://arxiv.org/abs/2302.04761>) trains the model, self-
  supervised, to decide *when* to call an API (calculator, search,
  calendar) and how to splice the result back into generation.

These relate directly to the **LLM-Modulo** generate-and-verify idea in
[02-integration.md](02-integration.md): an LLM is a fast, fallible
proposal distribution, and the tool/executor is the sound symbolic
check. PAL/PoT verify by *executing*; ReAct/Toolformer verify by
*grounding in a tool's output*. Honest verdict: this is the only NeSy
family that is unambiguously deployed at scale in 2026, but the
"symbolic" half is a calculator or interpreter call, not learned
reasoning. It buys reliability on offloadable subtasks; it does not by
itself confer rule induction (the capability
[04-implications-for-thvm.md](04-implications-for-thvm.md) argues this
repo actually needs).

## Which of these is relevant to a tensor VM

Three, in order:

1. **Tensor Logic** is the bullseye. thvm is an einsum engine with
   autodiff; if logic really is contraction, then the symbolic-
   reasoning substrate is *already built* and the right move is to
   express Datalog-style rules as thvm tensor equations and measure
   whether the unified-language claim survives contact with the ARC
   dynamics in [04-implications-for-thvm.md](04-implications-for-thvm.md).
2. **Differentiable solvers** (OptNet/cvxpylayers/SATNet) are the
   closest thing to "drop a sound symbolic module into the autodiff
   graph" that this repo could actually wire up, and the implicit-
   differentiation machinery is squarely a tensor-VM concern.
3. **KG embeddings** are the cleanest demonstration that a symbolic
   relation is a geometric operator on vectors, which is the
   [03-geometry.md](03-geometry.md) thesis and what every op above
   ultimately compiles down to.

NAR, VSA/HDC, and LLM tool use are worth knowing but are either
research curiosities at frontier scale (NAR length generalization,
VSA-for-ARC) or orthogonal to the compiler (the LLM-tool-use loop
lives above the runtime, not inside it). The center of gravity for a
tensor VM is the Tensor Logic / differentiable-solver / KG-embedding
trio, all of which are, in the end, just contractions.
