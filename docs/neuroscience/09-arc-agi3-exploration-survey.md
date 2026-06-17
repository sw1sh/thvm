# 09 - ARC-AGI-3 and the exploration / goal-inference literature: a survey

This page is a standalone survey, written May 2026, of the literature
that bears on the project's north star -- **ARC-AGI-3**, the agentic
rung of the Abstraction and Reasoning Corpus (ARC) ladder
(arXiv:2603.24621, launched March 25, 2026). [Page 8](08-reinforcement-learning.md)
told the history of reinforcement learning (RL) as a field, and
[page 5](05-toy-problems-with-thvm.md)'s "North star" section named
ARC-AGI-3 as the target. But page 8's literature -- DreamerV3, MuZero,
the Implicitly Quantized world model IRIS, the multi-game generalist
agents -- shares two assumptions that ARC-AGI-3 violates outright. It
assumes **a reward to maximise**, and it assumes the agent **trains on
the games it is tested on**. ARC-AGI-3 gives no stated goal, no reward
until a rare level completion, novel mechanics per game, and demands
few-shot transfer to held-out environments. This page surveys the
literature for *that* difficulty: exploration under sparse or absent
reward, inferring an objective when none is stated, and fast
adaptation to genuinely novel rules.

A caveat that frames the whole page, and is itself the central
finding: **the literature on ARC-AGI-3 specifically is thin.** The
benchmark is two months old. There is the challenge paper, one prize
technical report that predates it, one preview competition, and at
the time of writing a single arXiv preprint describing a method aimed
at it. Everything else surveyed here is *adjacent* work -- exploration
and meta-RL papers built for other settings -- that the project would
have to port. Treat the ARC-AGI-3-specific section as a near-empty
map and the rest as the toolbox that has to fill it.

Sections 1-2 cover ARC-AGI-3 itself and what has actually been tried.
Sections 3-6 walk the four adjacent literatures: exploration under
sparse reward, goal and objective inference, controllability and
empowerment, and few-shot transfer to novel mechanics. The last
section, "What the thvm arc should harvest", is the concrete and
deliberately unoptimistic assessment.

## 1. ARC-AGI-3: the structure of the benchmark

**ARC-AGI-3** (Chollet, Knoop et al., "ARC-AGI-3: A New Challenge for
Frontier Agentic Intelligence", arXiv:2603.24621) is the interactive
rung of the ARC-AGI ladder. Where ARC-AGI-1 and ARC-AGI-2 are static
input/output grid puzzles (page 5), ARC-AGI-3 is a set of hand-crafted
**turn-based games**. The defining design rule, repeated through the
paper: **no instructions, no stated rules, no stated goal.** The agent
is dropped into an environment and must explore, infer the dynamics,
work out what "winning" is, and -- the part the paper stresses most --
**compose mechanics learned on earlier levels to solve later ones it
has never seen.**

The concrete interface is small and worth stating exactly, because it
constrains every method below:

- **Observation.** A single "frame" is a 64x64 grid; each cell is one
  of 16 colours. This is the entire observation -- no text, no
  numbers, no score readout. It is the same Core Knowledge prior
  space as ARC-AGI-1/2, made dynamic.
- **Action space.** A small fixed set: five simple key actions, an
  Undo action that reverts to the previous frame, and one coordinate
  action that selects a cell of the 64x64 grid by `(x, y)`. Different
  environments expose different subsets. The coordinate action means
  the *effective* action space is ~4096-wide, and which coordinates
  do anything is environment-specific and unstated.
- **Levels and games.** Each environment ("game") is a series of
  **at least six levels**. Levels share mechanics and escalate;
  crucially, the paper notes a game does not reset to level 1 on
  failure, so progress is path-dependent.
- **The benchmark composition.** At launch: 25 public demonstration
  environments, 55 semi-private, 55 fully-private -- 135 total. The
  private split is the honest-evaluation guarantee, exactly as on the
  earlier rungs.
- **Scoring.** Not pass/fail but **Relative Human Action Efficiency
  (RHAE)**: per level `e`, score `S = min(1, h/a)^2` where `h` is the
  second-best human's action count and `a` is the agent's. Squaring
  punishes brute force hard -- an agent that solves a level but takes
  10x the human action count scores 0.01, not 1. Efficiency *is* the
  metric, not a tiebreak.

The human/AI gap at launch is the headline: **humans solve 100%** of
the environments (median successful attempt 8.1 minutes), while
frontier models on the semi-private leaderboard score **below 1%** --
Gemini 3.1 Pro Preview 0.37%, GPT-5.4 (High) 0.26%, Opus 4.6 (Max)
0.25%, Grok-4.20 0.00%. For comparison the static rungs are partly
cracked: the ARC Prize 2025 competition (next section) reached 24% on
ARC-AGI-2. The interactive rung is a different regime.

Why it is hard maps cleanly onto the four breakthroughs of
[page 2](02-five-breakthroughs.md): exploration under no signal
(steering), inferring what is rewarding (reinforcing), building a
model of unstated dynamics (simulating), and -- because difficulty
"comes from the composition of multiple mechanics learned across
levels" -- compositional transfer. The benchmark is engineered so
that the two things that crack the static rungs are *structurally
excluded*: there is no train set to overfit, and per-task test-time
search has nothing to search against without first inferring the
goal.

## 2. What has actually been tried on ARC-AGI-3

This section is short because the field is. There are three data
points.

**The ARC Prize 2025 Technical Report** (arXiv:2601.10904) is the
prior-art baseline, and it is about the *static* rung -- it predates
ARC-AGI-3's launch. Its defining theme is the **refinement loop**: a
per-task iterative program-optimisation loop guided by a feedback
signal. The 2025 Kaggle competition (1,455 teams, on ARC-AGI-2)
topped out at **24.03%** (NVARC, test-time training plus synthetic
data); the ARChitects (16.53%) used a 2D-aware masked-diffusion
language model with recursive self-refinement; MindsAI (12.64%) used
test-time fine-tuning and augmentation ensembles. A separate strand is
zero-pretraining deep learning -- the Tiny Recursive Model reached 45%
on ARC-AGI-1 with 7M parameters, CompressARC 20% with 76K parameters.
The report is candid that all of this is "fundamentally constrained by
knowledge coverage" and that ARC-AGI-3 will require "exploration,
planning, memory, goal acquisition" -- capabilities the static-rung
methods do not have. **None of the 2025 winning methods transfers to
the interactive setting**: a refinement loop needs a checkable output,
and ARC-AGI-3 gives no goal to check against.

**The ARC-AGI-3 Preview Agent Competition** (30 days, July-August
2025; three public environments, three held-out) is the only public
agent bake-off so far. The result that matters:

- **StochasticGoose** (Dries Smit, Tufa Labs) won with **12.58%** on
  the hidden preview set, completing 18 levels. The method:
  *no language model* (the author judged a per-frame image too
  expensive in tokens). A four-layer convolutional neural network
  (channels 32-64-128-256) takes the 16-channel one-hot 64x64 frame
  and predicts, per action, the probability that the action will
  **change the frame** -- an *action-affordance* or "what is
  clickable" predictor. For the coordinate action, a convolutional
  head keeps spatial structure and predicts a 64x64 click map. It is
  trained by **binary cross-entropy on (frame, action) -> did-the-
  frame-change**, with light entropy regularisation -- supervised
  prediction of affordance, not reward maximisation. Exploration is
  stochastic sampling biased toward actions predicted to do
  something. An experience buffer of up to 200K hash-deduplicated
  state-action pairs feeds off-policy retraining every few steps; the
  buffer is reset between levels.
- **Blind Squirrel** came second (6.71%, 13 levels) with a
  graph-based method: build a state graph from observed frames, prune
  non-productive actions with a ResNet-18 value model.
- Most other entries were "smart random" -- rules, small CNNs, or
  frame graphs. **Pure language-model agents crashed frequently and
  performed poorly.**

The single most important number on this page is the next one.
**When StochasticGoose was run on the full official benchmark at
launch, it scored 0.25%** -- the same band as the frontier models.
The 12.58% was specific to the six preview environments; the method
did not transfer to 135. The ARC Prize team's own framing is that
"future games will be hardened to reduce brute-force solutions". The
honest reading: the preview headline number is a *within-distribution*
result, and the one method that beat the frontier models did so by
fitting the preview games, not by acquiring transferable agentic
skill. The buffer-reset-between-levels detail confirms it -- the
agent does not even carry learning across levels of one game, let
alone across games.

**Graph-Based Exploration for ARC-AGI-3** (Rudakov, Shock, Cowley,
arXiv:2512.24156) is, at the time of writing, the only arXiv preprint
describing a method targeted at ARC-AGI-3. It builds a graph of
observed frame states and traverses it to structure exploration
rather than enumerate blindly, pruning unproductive transformations.
It is the same family as Blind Squirrel's approach. The preprint
demonstrates that structured graph exploration beats unguided search,
but does not report a benchmark-wide score that changes the picture
above.

That is the entire ARC-AGI-3-specific literature as of May 2026: one
challenge paper, one report on the static predecessor, one preview
competition whose winner does not transfer, one method preprint.
**There is no published method that does well on ARC-AGI-3.** Stating
that plainly is the most useful thing this section can do.

## 3. Exploration under sparse or absent reward

If there is no reward signal until a rare level completion, the agent
must generate its own drive to act. This is the **hard-exploration**
literature, and unlike the ARC-AGI-3-specific section it is deep,
mature, and mostly pre-2022. Its canonical proving ground is the
Atari game Montezuma's Revenge, where reward is so sparse that
reward-driven RL never starts.

**Prediction-error curiosity.** The **Intrinsic Curiosity Module
(ICM)** (Pathak et al., "Curiosity-driven Exploration by
Self-supervised Prediction", ICML 2017, arXiv:1705.05363) gives the
agent an intrinsic reward equal to the error of a learned forward
model in a learned feature space -- the agent is paid to go where its
own predictions are wrong. The key trick is the *inverse model*: the
features are trained to predict the action between two states, so the
feature space keeps only what the agent can *control* and ignores
unpredictable-but-irrelevant noise. This last point matters for
ARC-AGI-3, whose frames have no distractor noise but plenty of
agent-irrelevant structure.

**Random Network Distillation (RND)** (Burda et al., "Exploration by
Random Network Distillation", 2018, arXiv:1810.12894) is the cleaner,
more robust descendant: a fixed randomly-initialised target network
maps states to features; a predictor network is trained to match it;
the prediction error is the novelty bonus. Error is high on rarely
seen states and decays as they are revisited. RND was the first
method to beat the human average on Montezuma's Revenge. It is simple
to implement and has no forward-model instability -- the default
novelty signal to reach for.

*Follow-up (2026-06-15): "RND is just prediction-error novelty -- why not use
JEPA's latent prediction error instead, since it already has an anti-collapse
regulariser (VICReg / SIGReg)?"* Good instinct, with one important boundary.
RND's target is a FROZEN RANDOM net (already spread out), so its failure mode is
the PREDICTOR over-generalising on low-variety observations -- it learns to match
the target even on unseen states, novelty -> 0 everywhere, signal dies (observed
directly when a naive small-MLP RND was bolted onto DreamerV3 over downsampled
Atari frames: novelty collapsed within ~30k steps on Montezuma's plain first
room). JEPA's target is instead a LEARNED embedding, which can collapse to a
near-constant -> the predictor matches it trivially -> novelty dies by a different
route; this is exactly the collapse VICReg and SIGReg (LeJEPA's sketched
isotropic-Gaussian regulariser, one statistical test replacing VICReg's three
terms) prevent by forcing the embedding to stay high-variance / isotropic, keeping
the prediction-error signal informative. So a latent-world-model prediction-error
curiosity with a SIGReg-kept-rich embedding is the robust, non-collapsing form --
and `brain/experiments/319`'s LatentWM (a JEPA with VICReg) plus Plan2Explore-style
latent disagreement is most of that scaffolding already. TWO caveats keep it from
being a silver bullet: (1) it fixes FRAGILITY, not ALIGNMENT -- a non-collapsing
latent curiosity (Plan2Explore) still recovered ~0% of Pong's hidden reward
(`brain/experiments/335`), because novelty, however robust, is not a skill-task's
reward; (2) latent novelty has its own non-collapse failure -- in 319 the
VICReg-regularised latent novelty FLATLINED (washed out once the world model
trained), and 320 had to fix that STRUCTURALLY (Go-Explore frontier-return +
count-based latent-cell novelty), not with a better regulariser. SIGReg is the
right tool to keep the signal alive; it does not make novelty task-aligned.

**Count-based and pseudo-count exploration.** The classical
tabular-RL exploration bonus is `1/sqrt(N(s))` -- favour rarely
visited states. **Pseudo-counts** (Bellemare et al., "Unifying
Count-Based Exploration and Intrinsic Motivation", NeurIPS 2016,
arXiv:1606.01868) generalise the count to high-dimensional states by
deriving it from the change in a density model's probability when a
state is observed. This is the bridge from tabular counts to
deep-RL-scale novelty, and RND is in practice its most usable form.

**Go-Explore** (Ecoffet, Huizinga, Lehman, Stanley, Clune, "First
return, then explore", *Nature* 590, 2021; arXiv:1901.10995) diagnoses
*why* curiosity bonuses stall and fixes it structurally. Two failure
modes: **detachment** -- the agent forgets how to get back to a
promising frontier it left -- and **derailment** -- exploratory noise
knocks it off course before it ever reaches the frontier. Go-Explore's
fix is an explicit **archive of visited states**: to explore, first
*deterministically return* to a remembered promising state, *then*
explore from it. It solved every previously unsolved Atari game and
beat the state of the art on every hard-exploration game by orders of
magnitude. The principle -- separate "get back to the frontier" from
"explore the frontier", and keep an explicit archive -- is the
strongest single idea in this section and the most transferable to a
turn-based, Undo-equipped, replayable benchmark.

**Never-Give-Up (NGU)** (Badia et al., "Never Give Up: Learning
Directed Exploration Strategies", ICLR 2020, arXiv:2002.06038)
combines two novelty timescales: an **episodic** novelty (a memory of
states seen *this episode*, so revisiting is discouraged within an
episode) multiplied by a **life-long** novelty from RND (so
exploration does not stop once a state is globally familiar). It
trains a family of policies from highly exploratory to highly
exploitative at once. **Agent57** (Badia et al. 2020,
arXiv:2003.13350, already cited on page 8) puts a meta-controller on
top of NGU that learns, per game, how to weight exploration against
exploitation and what discount to use -- and was the first agent
above the human baseline on *all 57* Atari games. NGU's episodic /
life-long split is directly relevant: ARC-AGI-3's per-level structure
is exactly an episodic boundary.

The mitigation hierarchy for "the agent has no reason to act", weakest
to strongest: a forward-model prediction-error bonus (ICM); a
random-network novelty bonus that does not destabilise (RND); a
two-timescale episodic-plus-lifelong novelty (NGU/Agent57); and an
explicit archive with return-then-explore (Go-Explore). The
through-line: an intrinsic drive plus *memory of where you have
been*.

**Learning progress, not raw surprise (the missing named lever).**
Every novelty bonus above rewards *surprise* (prediction error, low
count). The known failure of that family is the **noisy-TV problem**:
an agent paid for surprise is captured by an unlearnable
high-entropy source forever. The biological fix is to reward
*reducible* uncertainty -- **learning progress** -- rather than
surprise itself: engage what is at the edge of, but inside, current
competence. This is the **Goldilocks effect** (Kidd, Piantadosi &
Aslin 2012: infants disengage from both too-predictable and
too-complex stimuli), formalised as intrinsic motivation by learning
progress (Oudeyer & Kaplan) and compression progress (Schmidhuber),
and it is Vygotsky's *zone of proximal development* in
reinforcement-learning dress. RND's error-decays-on-revisit is a
crude proxy for it; ICM's inverse-model feature space is another (it
strips the agent-irrelevant noise that would otherwise be the noisy
TV). The same anti-collapse logic drives the **automatic-curriculum**
literature: a task *proposer* that maximises solver-difficulty
collapses onto degenerate, artificially-hard tasks (asymmetric
self-play's adversarial-proposer instability; the Conjecturer
collapse that **Self-Guided Self-Play**, Bailey, Wen, Dong,
Hashimoto & Ma, arXiv:2604.20209, fixes with a third "Guide" role
scoring proposals for frontier-relevance and naturalness). The neuro
analogy is direct: the Guide is functionally a learning-progress
regulator, and its "relevance to unsolved targets" is the
expected-value-of-the-backup that gates **hippocampal replay**
(Mattar & Daw 2018) and the expected-value-of-control that
prefrontal cortex computes when allocating effort (Shenhav, Botvinick
& Cohen 2013). Where the analogy breaks: SGS's Guide judges
*linguistic* naturalness with a language model, which has no neural
correlate beyond "a prior over natural tasks", and the brain has no
single explicit Guide -- the regulation is distributed across novelty
habituation, learning-progress reward, and value-gated replay. The
transferable, LLM-free lever for this arc: gate any intrinsic bonus
(or any self-proposed goal/curriculum) by an estimate of *learnable*
progress, not by surprise -- the regulator the brain-arc affordance
explorers (`brain/experiments/326`-`328`) lacked when they collapsed
onto a proxy signal, which Go-Explore's frontier-return only
partially rescued.

*Follow-up (2026-06-11): "is there a neuro analogy of Self-Guided
Self-Play (arXiv:2604.20209)?" -- yes; this paragraph is the answer.
The Guide that prevents proposer collapse is the engineering of
learning-progress curiosity (reward learnable difficulty, not raw
difficulty), with replay-prioritisation and expected-value-of-control
as the closest neural mechanisms.*

## 4. Goal and objective inference: acting when nothing states the goal

Exploration gets the agent to act; it does not tell it what *winning*
is. ARC-AGI-3 gives no goal, so the agent must infer one. Three
literatures bear on this.

*Follow-up (2026-06-15): "what makes novelty task-aligned in the brain --
is the solution better task inference?"* In the brain, novelty does not
*become* aligned; it is SUBORDINATED to value and goals. Novelty is the
fallback run when there is no goal/value signal yet; once there is one,
goal-directed value reweights exploration. Three layers (Bennett's
breakthrough order): (2) dopamine codes reward-prediction error for both
extrinsic reward AND novelty (the novelty bonus of the SN/VTA-hippocampal
loop, Lisman & Grace 2005), but the novelty bonus HABITUATES and incentive
salience (Berridge) makes reward-predictive cues attractive -- so exploration
is pulled toward reward-predictive structure (the Pong-curiosity 0% recovery,
`brain/experiments/335`, is an agent stuck with novelty and no value system to
correct it); (3) a goal is SET (homeostatic drive, or inferred) and exploration
is CONDITIONED on it -- the same state is interesting or not depending on the
active goal, the variable pure novelty lacks; and model-based simulation toward
the goal (hippocampal vicarious trial-and-error; Redish; Pfeiffer & Foster) makes
exploration goal-DIRECTED, with curiosity/learning-progress relegated to building
the model on the learnable frontier so simulation-toward-goal is possible at all.
So "better task inference" is NECESSARY (ARC-AGI-3 states no goal, unlike an
animal with innate drives) but NOT SUFFICIENT: the inferred goal must then
CONDITION exploration (goal-conditioned value/policy + simulation). Task
inference without goal-conditioning is a label never used. Concrete arc lever:
robust curiosity builds the latent WM; the instant a success appears (the rare
331 level-up), infer a goal from it (hindsight relabelling, below) and condition
exploration on it (320 frontier-return, but goal-DIRECTED not novelty-directed).
The open part (as in RL): how the brain weights curiosity against value -- the
explore-for-the-model vs exploit-toward-goal balance -- is unsettled.

**Inverse reinforcement learning (IRL)** infers a reward function
from observed expert behaviour: Ng and Russell ("Algorithms for
Inverse Reinforcement Learning", ICML 2000) and Ziebart et al.'s
Maximum-Entropy IRL (AAAI 2008) are the classical statements (both
already cited for page 4). IRL is the textbook answer to "recover the
objective", **but it needs demonstrations**, and ARC-AGI-3 provides
none -- no expert trajectory, only the agent's own experience. IRL is
therefore relevant as a *concept* (the goal is a latent to be
inferred) but not as a drop-in method. The closest usable variant is
inferring a goal from the agent's *own* successful trajectories once
it stumbles into a level completion -- which is structurally
**hindsight relabelling** (Hindsight Experience Replay, page 8): the
state that *was* reached at a win becomes the goal, retroactively.

**Skill discovery as objective-free learning.** If no goal is given,
one option is to learn a *space of goals* the agent can reach.
**Variational Intrinsic Control (VIC)** (Gregor, Rezende, Wierstra,
2016, arXiv:1611.07507) and **Diversity Is All You Need (DIAYN)**
(Eysenbach et al., "Diversity is All You Need: Learning Skills without
a Reward Function", 2018, arXiv:1802.06070) learn a set of distinct
skills by maximising the mutual information between a latent skill
code and the states it reaches -- the agent is rewarded for skills
that are *distinguishable*, with no task reward at all. The result is
a repertoire of behaviours that can later be composed once a goal
appears. Recent work (Contrastive Intrinsic Control; DUSDi,
"Disentangled Unsupervised Skill Discovery", arXiv:2410.11251) makes
the discovered skills more disentangled and composable. For ARC-AGI-3
the relevance is conceptual: the agent could pre-discover "what
distinct things can I make happen here" before it knows which of them
is the goal.

**Goal-conditioned value as the objective.** Page 8's section 10
already covered goal-conditioned RL and quasimetrics -- learning
`V(s, g)` for *any* goal `g`. This is the cleanest fit: if the agent
learns a quasimetric over its own latent state, then *any* observed
frame can be posited as a goal and the metric says how far it is. The
goal-inference problem reduces to *choosing which reachable state to
treat as the goal* -- and the natural choice is "the state that
looked like progress", detected by novelty, by a frame-change cascade,
or, once a win happens, by hindsight. This is the bridge from the
goal-inference literature back into the thvm controller arc, and it is
developed in the final section.

**Compression / MDL as the goal prior (the structure intuition).**
*Follow-up (2026-06-15): "infer sub-tasks not from novelty but from achieving
COMPRESSIBLE state representations -- fill a pattern, make things match, achieve
symmetry -- which correlates with the goal for most games."* This is sharper than
novelty for pattern/puzzle games (which ARC-AGI-1/2 are, and many ARC-AGI-3): the
goal state is the LOW-DESCRIPTION-LENGTH state (symmetric/tiled/sorted/matched), so
"make the state more compressible" CORRELATES with progress where novelty is
orthogonal to it. This is Gestalt **Pragnanz** (perception organises toward the
simplest interpretation) and **predictive coding** (cortex minimises
prediction error = compresses its input); note the distinction from Schmidhuber's
compression-PROGRESS (the MODEL getting better at compressing = the learning-
progress thread) -- here it is the STATE itself becoming compressible, the more
task-aligned of the two. Techniques, cheapest first: direct compression length
(gzip/PNG/RLE of the grid, run-length of the object array); a battery of structure
metrics as intrinsic rewards (reflective/rotational/translational symmetry,
periodicity/autocorrelation, object alignment, count regularity -- the ARC priors
made measurable); generative-prior likelihood (a small AR/VAE trained on observed
states, reward typical/structured configs); and active inference / expected free
energy (Friston) -- act to make observations match a prior preference for
low-complexity states. THE CRUCIAL GOTCHA: naive compression collapses to the
degenerate optimum -- a blank/uniform grid is maximally compressible, so
minimise-description-length alone teaches the agent to ERASE structure (the same
collapse family as novelty/RND). The fix is the rate-distortion / information-
bottleneck framing: maximise compressibility SUBJECT TO preserving the conserved
content (the objects/quantities the game keeps), i.e. reward compressible
ARRANGEMENTS of conserved pieces, not erasure. From scalar to SUB-TASKS: decompose
the structure signal into axes (symmetry-type, object-relation, region); use
empowerment-over-structure to find which axes are CONTROLLABLE (`brain/experiments/330`
object-factoring gives the handles); and HINDSIGHT-ground them -- when the rare win
fires, the structure axis that jumped most is the inferred sub-goal, pursued by a
goal-conditioned controller (320 frontier-return aimed at the structure target).
Honest caveats: it is a PRIOR not a law (navigation/collection/avoidance games have
no more-compressible goal state), so it must be a LEARNED-and-VALIDATED prior (does
rising structure actually track the sparse reward in THIS game?), not hard-coded
(else it overfits puzzle games and breaks the rest -- the simulator-exploitation
trap this arc forbids); and "compressible" (order) vs "controllable"
(options/empowerment) pull opposite ways, so which intrinsic
objective a game wants is itself something to infer per-game. This is the MDL/
rate-distortion objective the arc has favoured, turned into a goal signal -- the
most promising task-grounded self-guidance lever surfaced so far.

*Follow-up (2026-06-15): "do this in LATENT space with SIGReg-family techniques."*
The cleaner form: measure/drive low complexity in the JEPA LATENT, where SIGReg has
shaped the embedding into a clean isotropic Gaussian -- which dissolves the
blank-grid collapse. Formalisation: the rate-distortion / beta-VAE
`KL(q(z|x) || prior)` term IS the latent description length (the "rate" = bits to
encode the state); minimise rate while the encoder preserves content
(reconstruction / JEPA prediction) = a low-complexity latent that keeps the objects.
And under a SIGReg-enforced isotropic Gaussian, surprise = -log p(z) ~ ||z||^2, so
the state's complexity ~ its LATENT NORM and the intrinsic signal is just "reduce
||E(s)||^2" = move toward the latent mode = active inference (reach the prior's
high-density region); SIGReg is what makes ||z||^2 a calibrated complexity rather
than junk. Why no blank-collapse: the encoder PRESERVES content (a blank board and a
structured board have different latents) and SIGReg keeps the latent full-rank, so
there is no information-destroying attractor -- "low complexity in latent" = conserved
content arranged typically, the rate-distortion constraint enforced by the
representation instead of a hand-built rule. THE SUBTLETY: SIGReg gives TYPICALITY,
not task-structure -- the mode is the centroid of VISITED states, so low ||z|| = a
common state, which equals a structured/goal state only if structured configs embed
near the mode. Make that true either by baking structure into the encoder
(object-factored / symmetry-aware, exp 330) or by NOT assuming it and HINDSIGHT-
grounding (let the rare win certify that "latent got simpler" tracks "closer to goal"
this game). Coupling to watch: a policy seeking low-complexity states concentrates the
data near the mode, which SIGReg re-spreads -- train the encoder on a diverse,
exploration-inclusive buffer or the metric drifts. Full coherent stack: JEPA latent
(space) + SIGReg/VICReg (clean, non-collapsing) + rate-distortion/beta-VAE (complexity
= rate) + active-inference move-to-the-mode (signal) + 330 object-factoring
(typicality = structure) + 320 goal-conditioning + hindsight (validate mode ~ goal).
Pure-RL, CPU-runnable.

## 5. Controllability and empowerment: an intrinsic objective with no reward

There is one intrinsic objective that needs neither a reward nor a
demonstration nor a stated goal, and it deserves its own section
because it is the best-matched single idea to ARC-AGI-3's "figure out
what you can do".

**Empowerment** (Klyubin, Polani, Nehaniv, "Empowerment: a universal
agent-centric measure of control", 2005) is the channel capacity from
the agent's actions to its future observations -- formally the
maximum mutual information `I(A; S')` between an action sequence and
the resulting future state. An agent that maximises empowerment seeks
states from which it has *the most control* -- the most distinct
reachable futures. It is fully intrinsic: it is defined from the
agent's own action-observation channel with no reference to any task.
**Variational Intrinsic Control** (section 4) is in fact the
empowerment objective made trainable with a variational bound.
Recent work folds empowerment into model-based RL -- "A Unified
Bellman Optimality Principle Combining Reward Maximization and
Empowerment" (arXiv:1907.12392) and "Towards Empowerment Gain through
Causal Structure Learning in Model-Based RL" (arXiv:2502.10077) --
and shows it solves sparse-reward manipulation where reward-only RL
does not.

The reason empowerment matters here is that it *is* what
StochasticGoose approximated by hand. An action-affordance predictor
that asks "which actions change the frame" is a crude, one-step,
binary empowerment estimate -- it favours acting where the agent has
control. The winning preview method was, in effect, a
hand-rolled empowerment heuristic. The literature has the principled
version. Affordance learning -- learning *what is controllable* and
*what each control does* -- is the same idea seen from the
psychology side, and it is precisely the "steering" breakthrough of
page 2.

## 6. Few-shot transfer to novel mechanics

The last difficulty: ARC-AGI-3 grades on held-out games with mechanics
the agent has never seen, and demands that mechanics learned on early
levels compose on later ones. This is **meta-RL** and **in-context
RL**.

**Meta-RL** trains an agent across a *distribution* of tasks so it
learns *how to adapt* fast to a new draw. The two classical statements
are **RL^2** (Wang et al., "Learning to reinforcement learn", 2016,
arXiv:1611.05763 -- already cited for page 4; and Duan et al.'s
concurrent "RL^2"), where a recurrent network's hidden state *is* the
adaptation -- the network is trained across tasks so that running it
forward on a new task implements a learning algorithm in its
activations -- and **Model-Agnostic Meta-Learning (MAML)** (Finn,
Abbeel, Levine, 2017, arXiv:1703.03400), which meta-learns an initial
parameter set from which a few gradient steps solve a new task. Page 8
and page 4 both touch the neuroscience reading: Wang et al.'s
"Prefrontal cortex as a meta-reinforcement learning system" (*Nature
Neuroscience* 2018) is the same loop in the brain.

**In-context RL (ICRL)** is the modern, Transformer-era form. The
agent does not update weights to adapt; it adapts *in its context
window*, the way a language model does in-context learning.
**Algorithm Distillation** (Laskin et al., "In-context Reinforcement
Learning with Algorithm Distillation", 2022, arXiv:2210.14215) trains
a Transformer on the *learning histories* of an RL algorithm -- whole
across-episode improvement curves -- so the model learns to *continue
the improvement curve* in context on a new task, with frozen weights.
The **Decision-Pretrained Transformer** (Lee et al., 2023,
arXiv:2306.14892) reaches in-context RL from a supervised pretraining
objective. More recent work pushes scale and planning:
**"Towards Large-Scale In-Context RL by Meta-Training in Randomized
Worlds"** (arXiv:2502.02869) meta-trains across procedurally
randomised worlds for broad in-context adaptation, and
**"Distilling RL Algorithms for In-Context Model-Based Planning"**
(DICP, arXiv:2502.19009) distils a *dynamics model* the agent can plan
with in context, rather than distilling a reactive policy. The
in-context-RL direction is the natural one for ARC-AGI-3's few-shot
demand -- adaptation should happen *within* an environment's episodes
without a separate training run.

There is also a structural lesson from page 8's open-endedness
section. Meta-RL only produces transfer if it is meta-trained on a
*diverse enough distribution* of mechanics. DeepMind's XLand
("Open-Ended Learning Leads to Generally Capable Agents", 2021,
arXiv:2107.12808) is the existence proof: train across a vast
auto-generated task space and zero-shot transfer to held-out games
emerges. The auto-curriculum literature page 5 already inventories
(POET, PLR, ACCEL) is therefore not a separate concern from
ARC-AGI-3 -- it is the *supply side* of the transfer the benchmark
grades.

**Why Montezuma-solvers are not enough (a follow-up worth stating plainly).**
*Follow-up (2026-06-14): "why aren't the methods that solve Montezuma's Revenge enough for ARC-AGI-3?"*
Montezuma-class methods (RND, Go-Explore, NGU/Agent57) are exploration engines for ONE fixed world
with an effectively UNLIMITED budget; ARC-AGI-3 is many NOVEL worlds with a TINY per-game budget where
the mechanics and the goal must be INFERRED. Four mismatches: (1) **memorization vs few-shot** --
Go-Explore save-states the emulator and deterministically returns to archived positions, brute-forcing
a resettable map over billions of frames; ARC-AGI-3 gives a few thousand steps then a different game,
so there is nothing durable to memorize. (2) **known vs inferred task** -- in Montezuma the physics and
the goal (score) are fixed and given, so the ONLY hard part is reaching a far-away but well-defined
reward; ARC-AGI-3 makes the controls, objects, and win-condition unknown -- a rule-induction problem
exploration methods do not address. (3) **novelty != task** -- Montezuma is the game where exploration
shines BECAUSE exploration is the task (new room = progress = reward); the Atari reward-hiding probe
(`brain/experiments/335`) measured curiosity recovering ~0% of Pong's reward, because Pong's reward is
a low-novelty skill, and most ARC-AGI-3 games are skill/structure-shaped, not coverage-shaped. (4)
**generalization is the target** -- ARC grades skill-ACQUISITION efficiency on held-out games; a
Montezuma-solver is a Montezuma expert and Agent57 is 57 separate experts, the opposite of what the
benchmark rewards. The brain-arc evidence matches: the one Montezuma idea that transferred --
Go-Explore's return-then-explore (experiment 320) -- was also the only mechanism that unlocked a
residual seed, yet it hit a hard ceiling (experiment 331's ~8%/path lottery, the credit-assignment
wall) precisely because the other three gaps are not exploration problems.

## What the thvm arc should harvest

The honest assessment, the point of the page. The brain arc has just
built a universal world model on gridworlds (experiments 160-167) --
a shared encoder, a latent dynamics model, a quasimetric value,
planned over by Monte Carlo Tree Search (MCTS). The pivot to
ARC-AGI-3 is real, and this survey says what to take and what to
leave.

**The single most important finding is a negative one. There is no
published method that solves ARC-AGI-3.** The one agent that beat the
frontier models in the preview (StochasticGoose, 12.58%) scored 0.25%
on the full benchmark -- it fit six games, not the skill. The frontier
models are at 0.25-0.37%. The 2025 prize-winning static-rung methods
(refinement loops, test-time training) **do not transfer**, because
they need a checkable goal and ARC-AGI-3 supplies none. The arc is
not "behind" a known recipe; there is no recipe. That is good news for
a research project and it should be stated to anyone who frames
ARC-AGI-3 progress as catching up. The deliverable to aim for is
page 5's second target -- the cleanest *traceable account of why a
bootstrap arc helps at all* -- not a leaderboard number.

The concrete recommendations, in priority order:

**1. Make controllability / empowerment the stage-2 intrinsic drive,
and recognise the affordance predictor as its crude form.** The one
method that beat the frontier models did so with a hand-rolled,
one-step, binary empowerment estimate -- "which actions change the
frame". The arc should build the principled version: an
action-affordance head `phi(o) -> P(action changes z)` trained by the
same supervised did-it-change signal, sitting alongside the page-5
stage-2 curiosity head. This is cheap, it is the highest-confidence
transfer in the survey, and it *is* the "steering" breakthrough the
bootstrap arc is supposed to build first. Empowerment (the multi-step
mutual-information objective, VIC-style) is the upgrade once the
one-step head works. Do not wait for an extrinsic reward; ARC-AGI-3
will not give one.

**2. Adopt Go-Explore's archive-and-return as the exploration
backbone, not a curiosity bonus alone.** ARC-AGI-3 is turn-based, has
an explicit Undo action, and is replayable -- the most Go-Explore-
friendly setting imaginable. An explicit archive of visited latent
frames, plus "deterministically return to a promising archived state,
then explore", directly attacks the detachment and derailment that
will otherwise stall any curiosity-only agent. The arc already keeps
enumerable graphs in its quasimetric controller and in
`brain.qm_harness`; a state archive is a small extension of machinery
it has. Pair it with an RND-style novelty signal (robust, no
forward-model instability) and NGU's episodic/life-long split, where
"episodic" is reset at each ARC-AGI-3 level boundary.

**3. Reuse the quasimetric controller as the goal-inference
mechanism, with hindsight.** The arc's central asset is a learned
quasimetric `d(s, g)` over a latent state (page 8, section 10). This
is exactly what "no stated goal" needs: with a quasimetric, *any*
observed frame can be posited as a goal and the metric scores
distance to it. Goal inference reduces to *choosing which state to
treat as the goal* -- pick novel / high-empowerment states to aim at
during exploration, and the moment a level completion is hit, use
**hindsight relabelling** (the achieved state becomes the goal,
retroactively) to convert that one rare success into dense
quasimetric training signal. Inverse RL proper is not usable
(no demonstrations); hindsight over the agent's own wins is the
usable shadow of it.

**4. Make compositional transfer the explicit objective of the
multi-environment curriculum, and meta-train for in-context
adaptation.** ARC-AGI-3's stated difficulty is *composing mechanics
across levels and transferring to held-out games*. The arc's
multi-environment universal world model (165-167) is the right
substrate, but it must be trained as meta-RL: the OneWayGrid /
Sokoban / gridworld curriculum should be treated as a *task
distribution*, the world model and quasimetric meta-trained across it,
and adaptation pushed *in-context* (Algorithm-Distillation style --
let a recurrent or attention state carry the per-game adaptation)
rather than by a fresh training run per game. The auto-curriculum
work page 5 already inventories (PLR, ACCEL) is the supply side: it
generates the mechanic diversity without which meta-RL produces no
transfer. This is the highest-effort recommendation and the one most
likely to be where the real result is.

**5. Do not build a language-model agent for the loop, and do not
chase a preview-style score.** Pure language-model agents "crashed
frequently and performed poorly" in the only public bake-off, and the
64x64x16-colour frame is a poor fit for token-based perception -- the
winning entry dropped language models for exactly this reason. The
arc's 10-100M-parameter convolutional-plus-latent stack is the right
size and the right modality; this is a case where being small is an
advantage. Equally, do not optimise against a handful of public
ARC-AGI-3 environments: StochasticGoose's 12.58%-to-0.25% collapse is
the cautionary tale, and it is the same Goodhart failure page 5's
auto-curriculum section already warns about. Measure transfer to
held-out environments from the first checkpoint, exactly as page 5
says to treat ARC-AGI itself as evaluated-never-trained-on.

The shape of the bet, in one sentence: **ARC-AGI-3 is not a reward-
maximisation problem, so do not bring a reward-maximisation agent --
bring an empowerment-driven explorer with an explicit state archive, a
quasimetric that turns any frame into a goal, and a world model
meta-trained across a diverse mechanic distribution for in-context
adaptation.** Every piece of that exists in the arc already or is a
small port; none of it is a leaderboard guarantee; the value is the
map.

## References

ARC-AGI-3 and the ARC Prize:

- Francois Chollet, Mike Knoop et al., "ARC-AGI-3: A New Challenge for
  Frontier Agentic Intelligence", arXiv:2603.24621 (2026).
- ARC Prize Foundation, "ARC Prize 2025: Technical Report",
  arXiv:2601.10904 (2026).
- ARC Prize Foundation, "ARC-AGI-3 Preview: 30-Day Learnings"
  (blog, 2025).
- Dries Smit, "StochasticGoose" -- 1st place, ARC-AGI-3 Preview Agent
  Competition (write-up + code, 2025).
- Evgenii Rudakov, Jonathan Shock & Benjamin Ultan Cowley,
  "Graph-Based Exploration for ARC-AGI-3 Interactive Reasoning Tasks",
  arXiv:2512.24156 (2025).

Exploration under sparse reward:

- Deepak Pathak, Pulkit Agrawal, Alexei A. Efros & Trevor Darrell,
  "Curiosity-driven Exploration by Self-supervised Prediction" (ICM),
  ICML 2017; arXiv:1705.05363.
- Yuri Burda, Harrison Edwards, Amos Storkey & Oleg Klimov,
  "Exploration by Random Network Distillation" (RND),
  arXiv:1810.12894 (2018).
- Marc G. Bellemare et al., "Unifying Count-Based Exploration and
  Intrinsic Motivation", NeurIPS 2016; arXiv:1606.01868.
- Adrien Ecoffet, Joost Huizinga, Joel Lehman, Kenneth O. Stanley &
  Jeff Clune, "First return, then explore" (Go-Explore), *Nature*
  590:580-586 (2021); arXiv:1901.10995.
- Adria Puigdomenech Badia et al., "Never Give Up: Learning Directed
  Exploration Strategies", ICLR 2020; arXiv:2002.06038.
- Adria Puigdomenech Badia et al., "Agent57: Outperforming the Atari
  Human Benchmark", arXiv:2003.13350 (2020).

Goal inference, skill discovery, empowerment:

- Andrew Y. Ng & Stuart Russell, "Algorithms for Inverse
  Reinforcement Learning", ICML 2000.
- Brian D. Ziebart, Andrew Maas, J. Andrew Bagnell & Anind K. Dey,
  "Maximum Entropy Inverse Reinforcement Learning", AAAI 2008.
- Karl Gregor, Danilo Jimenez Rezende & Daan Wierstra, "Variational
  Intrinsic Control", arXiv:1611.07507 (2016).
- Benjamin Eysenbach, Abhishek Gupta, Julian Ibarz & Sergey Levine,
  "Diversity is All You Need: Learning Skills without a Reward
  Function" (DIAYN), arXiv:1802.06070 (2018).
- Jiaheng Hu et al., "Disentangled Unsupervised Skill Discovery for
  Efficient Hierarchical Reinforcement Learning" (DUSDi),
  arXiv:2410.11251 (2024); NeurIPS 2024.
- Alexander S. Klyubin, Daniel Polani & Chrystopher L. Nehaniv,
  "Empowerment: A Universal Agent-Centric Measure of Control", IEEE
  Congress on Evolutionary Computation, 2005.
- "A Unified Bellman Optimality Principle Combining Reward
  Maximization and Empowerment", arXiv:1907.12392 (2019).
- "Towards Empowerment Gain through Causal Structure Learning in
  Model-Based RL", arXiv:2502.10077 (2025).

Few-shot transfer, meta-RL, in-context RL:

- Jane X. Wang et al., "Learning to Reinforcement Learn",
  arXiv:1611.05763 (2016); Yan Duan et al., "RL^2: Fast Reinforcement
  Learning via Slow Reinforcement Learning", arXiv:1611.02779 (2016).
- Chelsea Finn, Pieter Abbeel & Sergey Levine, "Model-Agnostic
  Meta-Learning for Fast Adaptation of Deep Networks" (MAML),
  arXiv:1703.03400 (2017); ICML 2017.
- Michael Laskin et al., "In-context Reinforcement Learning with
  Algorithm Distillation", arXiv:2210.14215 (2022).
- Jonathan N. Lee et al., "Supervised Pretraining Can Learn
  In-Context Reinforcement Learning" (Decision-Pretrained
  Transformer), arXiv:2306.14892 (2023).
- "Towards Large-Scale In-Context Reinforcement Learning by
  Meta-Training in Randomized Worlds", arXiv:2502.02869 (2025).
- "Distilling Reinforcement Learning Algorithms for In-Context
  Model-Based Planning" (DICP), arXiv:2502.19009 (2025).
- Open-Ended Learning Team (DeepMind), "Open-Ended Learning Leads to
  Generally Capable Agents" (XLand), arXiv:2107.12808 (2021).

## Pointers

- The north star and the fixed curriculum:
  [05-toy-problems-with-thvm.md](05-toy-problems-with-thvm.md).
- RL history, MuZero, world models, goal-conditioned RL and
  quasimetrics: [08-reinforcement-learning.md](08-reinforcement-learning.md).
- The engineering of world-model rollout and drift:
  [07-video-world-models-survey.md](07-video-world-models-survey.md).
- The arc's experiments this survey informs:
  `brain/experiments/160`-`167` and the planned ARC-AGI-3 pivot.
- Full citations: see the "ARC-AGI-3 and the exploration / goal-
  inference literature (page 9)" block in [references.md](references.md).
