# Neuroscience for toy AI

(AI = artificial intelligence.)

A small, self-contained primer: enough neuroscience to read the
modern "brain-inspired AI" literature with a critical eye, organised
around one pedagogical spine -- Max Bennett's *A Brief History of
Intelligence* (2023), which tells brain evolution as a sequence of
five computational breakthroughs, each with a clean AI analogy
(dopamine as temporal-difference error, the basal ganglia as an
actor-critic, the neocortex as a generative / Helmholtz-style world
model, the prefrontal cortex as a meta-learner and theory-of-mind
engine, language as a generative sequence model).

From there it expands to where the field is now: Yann LeCun's
Joint Embedding Predictive Architecture (JEPA) line, the 2024-2026
"world model" wave (V-JEPA 2, LeJEPA, LeWorldModel, Genie, Dreamer,
Cosmos), and the broader NeuroAI (neuroscience-informed AI) programme
-- predictive coding / active inference, hippocampus-as-transformer,
distributional reinforcement learning (RL) and dopamine, prefrontal
meta-RL. The last page sketches concrete toy experiments to run
inside thvm.

This is a *reading and orientation* document. It is opinionated about
what is solid versus speculative, and every claim links to a source.
It is not a thvm architecture doc -- nothing here is implemented in
the repo yet; [05-toy-problems-with-thvm.md](05-toy-problems-with-thvm.md)
is the bridge.

## Conventions

- **Expand abbreviations on first use.** Every page expands an
  abbreviation the first time it appears *in that page* (so each page
  reads standalone). The table below is the directory-wide cheat
  sheet -- a backstop, not a substitute for the inline expansions.
- **Follow-up questions get folded in.** When the reader asks a
  clarifying question about something here, the answer is written
  back into the relevant page as an attributed note (date + the
  question), not just answered in chat -- so the docs accumulate the
  Q&A (questions and answers). Instances so far:
  - the Helmholtz-machine / encoder-decoder / sleep aside in
    [02-five-breakthroughs.md](02-five-breakthroughs.md#aside-the-helmholtz-machine-in-one-picture-and-where-sleep-fits)
    (2026-05-11);
  - the whole of [06-classical-ml-and-rlhf.md](06-classical-ml-and-rlhf.md)
    (2026-05-12) -- classical ML's five tribes, plus modern RLHF /
    DPO / GRPO.
  - the "Cortical columns as the unit of intelligence" section in
    [04-brain-inspired-ai.md](04-brain-inspired-ai.md#cortical-columns-as-the-unit-of-intelligence-numenta-htm-and-the-thousand-brains-project)
    (2026-05-12) -- Numenta, HTM, the Thousand Brains Theory and
    Project, plus the cousin programmes (sparse coding, HDC/VSA,
    neuromorphic hardware, capsule networks).
  - the "Sakana AI: evolution + collective intelligence" section in
    [04-brain-inspired-ai.md](04-brain-inspired-ai.md#sakana-ai-evolution--collective-intelligence-as-a-working-brain-inspired-bet)
    (2026-05-12) -- CTM, NAMM, ShinkaEvolve / DGM / DiscoPOP,
    AB-MCTS / Trinity / Conductor, and the AI CUDA Engineer
    cautionary tale.
  - "Part 1.5: Tensors as the primitive" in
    [06-classical-ml-and-rlhf.md](06-classical-ml-and-rlhf.md#part-15-tensors-as-the-primitive--domingoss-tensor-logic-sequel--the-tensor-network-line)
    (2026-05-12) -- Domingos's 2025 *Tensor Logic* paper (the
    master-algorithm sequel: tensor equation = logical rule via
    einsum), tensor networks in ML (Stoudenmire-Schwab, MPS/PEPS/
    MERA), and einsum-as-primitive (einops / einx / opt_einsum /
    JAX / thvm).

### Abbreviation cheat sheet

| Abbrev | Expansion | Abbrev | Expansion |
|---|---|---|---|
| AI | artificial intelligence | ML | machine learning |
| RL | reinforcement learning | TD | temporal difference |
| RPE | reward prediction error | SR | successor representation |
| ANN | artificial neural network | NN | neural network |
| MLP | multi-layer perceptron | ViT | vision transformer |
| LLM | large language model | LM | language model |
| VAE | variational autoencoder | ELBO | evidence lower bound |
| EMA | exponential moving average | A2C | advantage actor-critic |
| ICM | intrinsic curiosity module | MPC | model-predictive control |
| DQN | deep Q-network | QR-DQN | quantile-regression deep Q-network |
| JEPA | Joint Embedding Predictive Architecture | I/V-JEPA | image / video JEPA |
| SIGReg | Sketched Isotropic Gaussian Regularization | VICReg | Variance-Invariance-Covariance Regularization |
| PFC | prefrontal cortex | aPFC / gPFC | agranular / granular PFC |
| VTA | ventral tegmental area | STDP | spike-timing-dependent plasticity |
| REM | rapid eye movement (sleep) | fMRI | functional magnetic resonance imaging |
| PC | predictive coding | FEP | free energy principle |
| TEM | Tolman-Eichenbaum Machine | SPA | Semantic Pointer Architecture |
| NeuroAI | neuroscience-informed AI | NPC | non-player character |
| MCTS | Monte-Carlo tree search | SOTA | state of the art |
| CPU / GPU | central / graphics processing unit | RAM | random-access memory |
| JIT | just-in-time (compilation) | FAIR | (Meta) Fundamental AI Research |
| WL | Wolfram Language | MNIST | the handwritten-digit dataset |
| SVM | support vector machine | VC dim | Vapnik-Chervonenkis dimension |
| MCMC | Markov-chain Monte Carlo | NTK | neural tangent kernel |
| SFT | supervised fine-tuning | RLHF | RL from human feedback |
| RLAIF | RL from AI feedback | RLVR | RL from verifiable rewards |
| PPO | proximal policy optimization | GRPO | group relative policy optimization |
| DPO | direct preference optimization | KTO | Kahneman-Tversky optimization |
| ORPO | odds-ratio preference optimization | SimPO | simple preference optimization |
| RLOO | REINFORCE leave-one-out | PRM | process reward model |
| KL | Kullback-Leibler (divergence) | CoT | chain of thought |
| HTM | Hierarchical Temporal Memory | SDR | sparse distributed representation |
| TBP | Thousand Brains Project | NuPIC | Numenta Platform for Intelligent Computing |
| HDC | hyperdimensional computing | VSA | Vector Symbolic Architecture |
| SNN | spiking neural network | SLT | statistical learning theory |
| MPS | Matrix Product State | PEPS | Projected Entangled Pair State |
| MERA | Multi-scale Entanglement Renorm. Ansatz | DSL | domain-specific language |
| IR | intermediate representation | GEMM | general matrix multiply |

(Common venue abbreviations -- *Nature*, NeurIPS, ICML, ICLR, CVPR,
IJCAI, etc. -- are left as is.)

## Reading order

1. [01-neuroscience-basics.md](01-neuroscience-basics.md) -- neurons,
   synapses, plasticity, and a map of the brain structures the rest
   of the docs name (basal ganglia, neocortex, hippocampus,
   prefrontal cortex, thalamus, dopamine system).
2. [02-five-breakthroughs.md](02-five-breakthroughs.md) -- Bennett's
   five breakthroughs (steering, reinforcing, simulating, mentalizing,
   speaking) and the AI analogy for each, with the strong/weak
   evidence split.
3. [03-jepa-and-world-models.md](03-jepa-and-world-models.md) -- JEPA,
   LeCun's autonomous-agent architecture, why "predict in latent
   space" beats "generate pixels", LeJEPA / SIGReg, LeWorldModel, and
   the wider world-model field (Genie, DreamerV3, Cosmos, the Sora
   debate).
4. [04-brain-inspired-ai.md](04-brain-inspired-ai.md) -- the NeuroAI
   research programme, predictive coding and the free energy
   principle, the hippocampus-transformer correspondence, successor
   representations, distributional RL and dopamine, prefrontal
   meta-RL, and a synthesis of *open directions* for novel
   brain-inspired AI.
5. [05-toy-problems-with-thvm.md](05-toy-problems-with-thvm.md) --
   concrete small experiments mapped to the breakthroughs, sized to
   thvm's current capabilities (autodiff, the WL surface, MNIST-scale
   training).
6. [06-classical-ml-and-rlhf.md](06-classical-ml-and-rlhf.md) --
   widens the lens: Pedro Domingos's five tribes of machine learning
   (symbolists, connectionists, evolutionaries, Bayesians, analogizers
   -- with the Vapnik / statistical-learning-theory backbone of the
   last one), and the modern alignment / reasoning-model wave on top
   of LLMs (RLHF, DPO, GRPO, RLVR, KTO/ORPO/SimPO/RLOO, RLAIF,
   process reward models, DeepSeek-R1).
7. [references.md](references.md) -- consolidated bibliography with
   links.

## How to use this with thvm

thvm already has the pieces you need for breakthroughs 1-3 at toy
scale: reverse-mode autodiff ([../grad.md](../grad.md)), a tensor /
NN surface in Wolfram ([../wl.md](../wl.md), `wl/NN.wl`), and
MNIST-scale training (`docs/plans/beautiful_mnist_parity.md`). The
natural progression is: TD-learning / actor-critic on a gridworld
(breakthrough 2), then a tiny generative / predictive model -- a
Helmholtz-style wake-sleep autoencoder or a JEPA on toy images
(breakthrough 3) -- then a latent world model with planning
(LeWorldModel-in-miniature). Page 5 spells these out.

## A note on the genre

"X in the brain is just Y from AI" is a productive heuristic and a
recurring overclaim. The pattern that has actually paid off
historically runs in *both* directions: temporal-difference learning
predicted the dopamine signal (Schultz, Montague, Dayan 1997);
distributional RL then predicted the *spread* of dopamine responses
and it was found in mice (Dabney et al. 2020). Treat every analogy in
these docs as a hypothesis with a track record, not a fact -- the
evidence column matters.
