# 06 - Classical ML's five tribes, and modern alignment (RLHF, DPO, GRPO)

*(Added in response to a reader question, 2026-05-12 -- "what about
classical machine learning, there was a book about ~5 pillars of it,
deep learning + rules, SVM, genetic, statistics and Bayesian, and
something else maybe (Vapnik stat invariants?). And modern approaches
to RLHF? GRPO?")*

The earlier pages picked one path (deep learning, breakthroughs 1-5).
This page widens the lens in two directions you'd want for context:

1. **The five tribes of machine learning** -- Pedro Domingos's
   pedagogical taxonomy from *The Master Algorithm* (2015). Useful for
   orientation: deep learning is one of five historically-distinct ML
   (machine learning) research programmes, and each has a brain story
   you can match against Bennett's spine.
2. **Modern post-training of LLMs (large language models): RLHF,
   DPO, GRPO, and the alphabet soup beyond.** Where reinforcement
   learning (breakthrough 2) is currently being applied to language
   models (breakthrough 5).

(AI = artificial intelligence; LLM = large language model; SFT =
supervised fine-tuning; RLHF = reinforcement learning from human
feedback; DPO = direct preference optimization; PPO = proximal policy
optimization; GRPO = group relative policy optimization. Plus a few
more, all glossed below.)

---

## Part 1: The five tribes of machine learning

Pedro Domingos, *The Master Algorithm: How the Quest for the Ultimate
Learning Machine Will Remake Our World* (Basic Books, 2015). Each
tribe descends from a different intellectual root, has a different
"master algorithm" it would happily reduce all of intelligence to, and
fights different fights. The book's punchline -- "a master algorithm
would unify all five" -- is more rallying cry than result, but the
*taxonomy* is genuinely useful.

### 1. Symbolists -- "intelligence is manipulating symbols"

- **Roots:** logic, philosophy, computer science (LISP, Prolog, expert
  systems).
- **Master algorithm:** **inverse deduction** -- given facts and
  desired conclusions, fill in the missing rules. Concretely: decision
  trees (Quinlan's ID3 / C4.5 / Random Forests), rule learners,
  inductive logic programming.
- **Brain analogy / Bennett mapping:** thinnest. The brain doesn't
  store rules; this tribe was always *aspirational neuroscience*. The
  honest connection is to the *output* of breakthroughs 4-5 -- humans
  *talk* in rules, even if they don't compute that way.
- **Status 2026:** the engine room of "good old-fashioned AI" was
  largely outcompeted by statistical / deep methods in the 2010s, but
  symbolists were *right* about some things that deep learning still
  struggles with: explicit compositionality, systematic generalisation,
  out-of-distribution reasoning, verifiable correctness. The current
  revival is **neurosymbolic AI** (concept bottlenecks, program
  synthesis, formal verification of LLM outputs, theorem-prover-in-the-
  loop reasoning). thvm's own waldmeister / ATP (automated theorem
  proving) layer (see `docs/research/atp_ic_native.md`) is firmly in
  this tribe.

### 2. Connectionists -- "intelligence emerges from networks of simple units"

- **Roots:** neuroscience, cybernetics.
- **Master algorithm:** **backpropagation** (Rumelhart, Hinton,
  Williams 1986) and its descendants -- gradient descent through a
  differentiable computation graph.
- **Brain analogy / Bennett mapping:** the whole spine of these docs.
  Hebbian plasticity (breakthrough 1), three-factor dopamine-gated
  plasticity (breakthrough 2), cortical generative models
  (breakthrough 3), and language models (breakthrough 5) all sit
  here. Caveat: backprop itself is *not* a known biological mechanism
  -- predictive coding networks, forward-forward, etc., are the
  ongoing search for a brain-plausible substitute (page 4).
- **Status 2026:** dominant. This is what "AI" colloquially means.
  thvm is in this tribe (autodiff, NN.wl, gradient-based training).

### 3. Evolutionaries -- "intelligence is the product of selection"

- **Roots:** evolutionary biology, John Holland's *Adaptation in
  Natural and Artificial Systems* (1975).
- **Master algorithm:** **genetic programming / genetic algorithms**
  (Koza). Maintain a population of candidate solutions, mutate and
  recombine them, select by fitness, iterate.
- **Brain analogy / Bennett mapping:** evolution *built* the rest.
  Bennett's whole book is the story of *biological* evolution
  producing the five breakthroughs over 600M years -- and Domingos's
  evolutionaries claim the *same algorithm* can do that in silico.
  Modern echoes: **neuroevolution** (e.g. NEAT, NeuroEvolution of
  Augmenting Topologies, Stanley & Miikkulainen 2002); **DiscoRL** (Oh
  et al. 2025, *Nature*; see page 4) *meta-learns* the
  reinforcement-learning (RL) rule itself across a population of
  agents -- evolution discovering RL, in software. Also: AutoML
  (automated machine learning) and neural-architecture search (mostly
  gradient- and RL-based these days, but evolutionary methods remain
  competitive).
- **Status 2026:** niche but not dead. Where the loss landscape is
  non-differentiable, discrete, or you need black-box optimisation
  (hardware design, program synthesis, scientific discovery),
  evolutionary methods still earn their keep. They are also the right
  *frame* for thinking about evolution-of-architectures vs. learning-of-
  weights -- a distinction Bennett's "innate priors vs lifelong
  learning" leans on.

### 4. Bayesians -- "intelligence is probabilistic inference"

- **Roots:** statistics, Reverend Bayes, Laplace, Cox; modern formulation
  by Pearl (*Probabilistic Reasoning in Intelligent Systems*, 1988) and
  Jordan / Ghahramani / MacKay.
- **Master algorithm:** **probabilistic inference** -- Bayes' rule and
  its tractable approximations: Bayes nets / graphical models, MCMC
  (Markov-chain Monte Carlo), variational inference, probabilistic
  programming (Stan, Pyro). Causal inference (Pearl's *do*-calculus)
  is a Bayesian descendant that broke off as its own movement.
- **Brain analogy / Bennett mapping:** the **Bayesian brain**
  hypothesis -- perception, action, and learning are all approximate
  Bayesian inference under a generative model. This *is* the cortex
  story of breakthrough 3 (and the Helmholtz machine, predictive
  coding, and Friston's free energy principle from page 4 are its
  formal expressions). VAEs (variational autoencoders) and JEPAs
  (joint-embedding predictive architectures) inherit from this tribe
  as much as from the connectionists.
- **Status 2026:** quietly everywhere. Most of what *looks* like deep
  learning under the hood is Bayesian: VAEs, diffusion models (as a
  score-matching / hierarchical Bayes), Gaussian processes, dropout-
  as-Bayesian-approximation, RL-as-control-as-inference (the
  Levine 2018 view). The pure tribe is smaller than it was; the *ideas*
  won.

### 5. Analogizers -- "intelligence is recognising similarity"

- **Roots:** psychology, statistical learning theory.
- **Master algorithm:** **kernel methods / nearest neighbours /
  support vector machines (SVMs)** (Cortes & Vapnik 1995). Define a
  similarity (kernel) between examples; classify a new example by what
  its neighbours look like.
- **Brain analogy / Bennett mapping:** Hebbian auto-association -- the
  hippocampus's pattern-completion / nearest-recall, and to some extent
  the cerebellum's perceptron-like input expansion. Closer to
  breakthrough 1 than to anything later.
- **Statistical learning theory (the "Vapnik stat invariants" you were
  remembering).** This tribe's *theoretical* contribution is huge and
  half-orthogonal to the algorithms: **Vapnik-Chervonenkis (VC)
  dimension** as a measure of model capacity; **structural risk
  minimization** (trade off training error against capacity); the
  margin-maximising story behind SVMs; PAC (probably-approximately-
  correct) learning. The slogan: *generalization is bounded by capacity,
  not by parameter count.* Modern deep learning's failure to predict
  generalization from naive capacity bounds (a 20-billion-parameter
  network that doesn't overfit?) is exactly the puzzle Vapnik's
  framework set up; "double descent", "neural tangent kernel" (NTK),
  the "lottery ticket hypothesis", and the Belkin-Hsu-Ma-Mandal
  "reconciling modern ML practice with classical statistical theory"
  work (2019) are all attempts to extend Vapnik to overparameterised
  regimes.
- **Status 2026:** SVMs lost on most production tasks circa 2012-2015.
  But the *theory* underpins everything that calls itself "learning
  theory" today, and the *kernel view* of neural networks (NTK; "deep
  learning is a particular kernel method in the infinite-width limit")
  is one of the cleanest tools we have for understanding why deep nets
  work.

### How the tribes hybridise in 2026

Domingos's hope was for one master algorithm; what actually happened
is convergence-by-mixing. The dominant systems are *connectionist
foundations with bits of every other tribe bolted on*:

- **Neurosymbolic AI** = connectionist + symbolist. Concept
  bottlenecks, program synthesis from LLMs, formal-verifier-in-the-
  loop, retrieval-augmented generation (the retrieval index is an
  analogiser; the generator is a connectionist).
- **Latent-variable / generative models** (VAEs, diffusion, JEPAs) =
  connectionist + Bayesian. Page 3's world models live here.
- **Meta-learning and architecture search** = connectionist +
  evolutionary. DiscoRL (page 4) is the clean example.
- **Kernel-view theory** = connectionist + analogiser. NTK,
  representation-learning analyses, generalisation bounds for
  overparameterised nets.
- **Causal RL / model-based RL with causal world models** = Bayesian
  + connectionist + a side of symbolist.

The reason this matters for thvm: when you pick an experiment from
[page 5](05-toy-problems-with-thvm.md), be honest about which tribes
you're standing on -- a JEPA world model + planner is connectionist +
Bayesian + a kernel/analogiser slice (the latent-space distance is a
similarity), and the moment you bolt on a verifier (e.g. for a math
or code toy problem) you've added a symbolist.

---

## Part 2: Modern alignment / preference learning -- RLHF, DPO, GRPO

The five tribes are the *training-of-models-from-data* story. Modern
LLM **post-training** -- turning a next-token predictor into something
useful and aligned -- is a second story, sitting on top, and it is the
direct descendant of breakthrough 2 (reinforcement learning) applied
to breakthrough 5 (language). The current pipeline, and the variants
proliferating around it, are worth knowing.

### The baseline pipeline (InstructGPT / ChatGPT, 2022)

Stiennon et al. (2020, summarisation) and Ouyang et al., "Training
language models to follow instructions with human feedback"
(InstructGPT, 2022, arXiv:2203.02155) -- the canonical three stages:

1. **SFT (supervised fine-tuning).** Take a pretrained base LLM,
   fine-tune it on human-written demonstrations of the desired
   behaviour (instructions paired with good answers).
2. **Reward model.** Collect pairwise preferences -- humans rank two
   model outputs against each other; fit a scalar **reward model**
   `r(prompt, response)` to those preferences (a Bradley-Terry logistic
   regression, in practice).
3. **RL fine-tuning** with **PPO (proximal policy optimization)**
   (Schulman et al. 2017, arXiv:1707.06347). Treat the LLM as a
   policy; reward each generated response by the reward model; update
   the policy to maximise reward *minus* a Kullback-Leibler (KL)
   penalty back to the SFT model (so it doesn't drift into
   gibberish). PPO is a clipped-objective actor-critic -- it needs a
   separate **value (critic)** network of comparable size to the
   policy.

The breakthrough-2 reading: this is a classical *actor-critic* loop
where the *environment* is "respond to a prompt" and the *reward
signal* is human preference, distilled into a learned `r`. Or in
Bennett's vocabulary: layer-5 outputs scored by layer-2 valence, with
humans supplying the dopamine. Many of the pathologies of RLHF
(reward hacking, sycophancy, mode collapse) are textbook RL failure
modes in this framing.

### Reading the variants as "skip a piece of the pipeline"

The 2023-2026 literature is mostly about *removing* parts of the
classical pipeline while keeping the same overall behaviour. Each
variant is a step further down that path:

- **DPO (Direct Preference Optimization)** -- Rafailov, Sharma,
  Mitchell, Ermon, Manning & Finn, NeurIPS 2023; arXiv:2305.18290,
  "Your Language Model is Secretly a Reward Model". The clean
  observation: under the standard RLHF objective (reward + KL),
  there is a *closed-form* mapping between the optimal policy and the
  reward function. So you can re-parameterise the reward in terms of
  the policy itself and optimise the policy *directly* on the
  preference pairs with a simple binary cross-entropy loss -- no
  reward model, no PPO loop, no rollouts during training. Matches or
  beats PPO-RLHF on sentiment / summarisation / single-turn dialogue
  with a fraction of the engineering. Default choice in the open-
  weight community by 2024.
- **RLAIF (RL from AI Feedback)** -- Bai et al., "Constitutional AI:
  Harmlessness from AI Feedback" (Anthropic, arXiv:2212.08073, 2022)
  and Lee et al., "RLAIF vs RLHF: Scaling Reinforcement Learning from
  Human Feedback with AI Feedback" (Google, arXiv:2309.00267, 2023).
  Replace the *human* annotator with a strong LLM judging responses
  against a written rubric ("constitution"). Same RLHF pipeline
  otherwise. The cost line goes down; the alignment-of-the-judge
  question gets thornier.
- **KTO, ORPO, SimPO, RLOO** -- the alphabet soup. **KTO**
  (Kahneman-Tversky Optimization; Ethayarajh et al. 2024,
  arXiv:2402.01306) needs only *thumbs-up/down* per response, not
  pairwise preferences. **ORPO** (Odds Ratio Preference Optimization;
  Hong et al. 2024, arXiv:2403.07691) folds the preference loss into
  SFT, dropping the reference policy. **SimPO** (Simple Preference
  Optimization; Meng et al. 2024, arXiv:2405.14734) drops the
  reference policy too, with a length-normalised reward. **RLOO**
  (REINFORCE Leave-One-Out; Ahmadian et al. 2024, arXiv:2402.14740)
  brings classical REINFORCE back with a group-baseline trick,
  beating PPO on chat tasks at a fraction of the cost. All "cousins,
  doing the same job different ways" -- the shape of the problem is
  more important than which one you pick.

### GRPO and the reasoning-model wave (2024-2025)

**GRPO (Group Relative Policy Optimization)** -- Shao, Wang, Zhu, Xu,
Song et al., "DeepSeekMath: Pushing the Limits of Mathematical
Reasoning in Open Language Models", arXiv:2402.03300 (Feb 2024).
What it does:

- **Drops the value/critic network.** PPO's critic is a separately-
  trained network typically as large as the policy itself, doubling
  the training memory footprint. GRPO replaces it with a *group
  baseline*: for each prompt, sample `K` responses, score each, and
  use the *mean and standard deviation of the K rewards* as the
  baseline / advantage normalisation. The "advantage" is now relative
  to the prompt's own group.
- **Keeps the PPO clipped surrogate objective** otherwise (same
  trust-region story).
- **Memory and compute savings** of roughly 50% vs. PPO-RLHF -- the
  main practical reason it took off.

Then **DeepSeek-R1** (DeepSeek-AI, "DeepSeek-R1: Incentivizing
Reasoning Capability in LLMs via Reinforcement Learning",
arXiv:2501.12948, Jan 2025; published in *Nature* 645:633-638, Sep
2025 -- the first major open-weight LLM published after independent
peer review) used GRPO on **verifiable rewards** (math answers,
unit-test pass/fail) instead of a learned reward model. The result was
**R1-Zero**, trained with *no human reasoning demonstrations at all*,
which spontaneously developed self-reflection, verification, and long
chains of thought. R1 (the released model) adds a small "cold-start"
SFT before the same RL, and matches OpenAI o1 on math/code reasoning.

This regime has a name now: **RLVR (Reinforcement Learning from
Verifiable Rewards).** Where the answer is checkable (math, code,
formal logic), you don't need a preference reward model at all -- just
a verifier. RLVR + GRPO is the recipe behind essentially all
"reasoning models" of 2025-2026.

### Adjacent ideas to be aware of

- **Process reward models (PRMs)** vs. outcome rewards. Lightman et
  al., "Let's Verify Step by Step" (OpenAI, arXiv:2305.20050, 2023):
  reward *each step* of a chain of thought, not just the final answer.
  Costs more to train (you need step-level labels) but improves
  reasoning. Competing recipe with outcome-only RLVR.
- **Self-play / iterated AI feedback** -- self-rewarding language
  models (Yuan et al. 2024, arXiv:2401.10020), self-play preference
  optimisation; the agent generates its own training data via
  self-judgment.
- **Test-time / inference-time scaling** -- the OpenAI-o1 / DeepSeek-R1
  / Anthropic-extended-thinking line: spend more compute *at inference*
  on chain-of-thought rollouts, scored by a verifier or self-critique.
  Architectural-wise this is *vicarious trial-and-error*
  (Bennett, breakthrough 3) inside a single forward pass: the model
  "imagines" candidate solutions, then commits.

## How this maps onto Bennett's spine

The deepest part of the analogy. Modern alignment / reasoning training
is breakthroughs 2 and 3 being added on top of breakthrough 5 -- and
each variant is making one of those layers thicker:

- **RLHF / DPO / KTO / ORPO / SimPO / RLOO** -- breakthrough 2:
  reinforcement learning with a *learned* reward from preferences.
  Where the dopamine comes from a human (or a constitution-following
  judge LLM), but the credit-assignment shape is the same as
  basal-ganglia RL. Failure modes (reward hacking, sycophancy) are
  classical RL failures.
- **RLVR + GRPO (DeepSeek-R1, o1, etc.)** -- still breakthrough 2 (RL
  with a reward), but the reward is *verifiable* and the reasoning
  chain is breakthrough-3 *simulation* (an explicit, written-out
  rollout of candidate answers, scored, committed). This is closer to
  Bennett's "vicarious trial-and-error" than anything else current.
  GRPO's group-mean baseline is, charmingly, almost exactly the
  textbook **REINFORCE with baseline** -- a step *toward* a simpler,
  more brainlike algorithm than PPO's full actor-critic.
- **Process reward models** -- a *dense* prediction-error signal at
  every step, instead of a sparse one at the end. Predictive-coding-
  shaped: every reasoning step is "predicted" by the PRM, every step
  generates an error, and the error gates learning.
- **Constitutional AI / RLAIF** -- the *internalised* critic. The
  judge is a model trained on a written value statement; the agent
  learns to anticipate that judge. Two-system meta-cognition in a
  toy form -- gestures at Bennett's breakthrough 4 (mentalizing: an
  internal model of how *someone else* will judge me) without
  actually getting there.

The honest summary: contemporary RLHF / DPO / GRPO is the most
serious application of *breakthrough 2* (basal-ganglia RL) to LLMs to
date -- and it's already producing failure modes (sycophancy, reward
hacking, mode collapse, distribution drift) that the RL literature
has been documenting in animals for decades. Bennett's framing helps
you read those failure modes as *predictable consequences of welding
layer 2 onto layer 5 without rebuilding layer 3 underneath* (no
grounded world model, no embodied feedback). The fix that LeCun, V-
JEPA-2-AC, and LeWorldModel are pushing (page 3) -- a real predictive
world model the actor can plan inside -- is the same diagnosis from a
different direction.

## thvm-sized experiments this opens up

Mostly forward-looking; sketch only, see [page 5](05-toy-problems-with-thvm.md)
for fully-fleshed experiments.

- **Tabular DPO.** On a tiny preference-pair dataset (handcrafted: e.g.
  responses to a few prompts ranked by hand), implement the closed-form
  DPO update directly on a small transformer. Compare to PPO with a
  reward model on the same data. Both are autodiff-friendly; thvm has
  `gpt2` as the substrate already.
- **GRPO on a verifiable toy.** A two-digit-arithmetic task: have a
  small model emit `<scratchpad> ... </scratchpad> <answer>X</answer>`,
  verify the answer, run GRPO (sample `K` responses per prompt, use
  group mean/std as baseline). This is the smallest possible
  DeepSeek-R1-Zero, and it stresses the same autodiff + sampling +
  reward loop you'd want for any RL-on-LLM experiment.
- **Symbolist-connectionist hybrid.** Pair thvm's autodiff with the
  waldmeister / ATP layer (`docs/research/atp_ic_native.md`) so a
  small neural net proposes rewrites and the ATP layer verifies them
  -- a tiny neurosymbolic verifier-in-the-loop loop. This is the
  experimental shape that RLVR is generalising at LLM scale.

These are stretch goals next to the [page 5](05-toy-problems-with-thvm.md)
menu; the actor-critic gridworld is still the right first move.

---

## Further reading

References for everything cited here are in
[references.md](references.md) (new entries: Domingos's book; the
RLHF / DPO / GRPO / DeepSeek-R1 papers; Vapnik's *Statistical Learning
Theory*; the InstructGPT and PPO papers; surveys of post-2023 RLHF
alternatives).
