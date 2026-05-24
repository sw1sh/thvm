# 12 - Modern robotics research: a survey for the brain-arc

This page surveys what modern robotics research (2022-present) has
converged on, with the goal of importing mechanisms into the brain-arc
ARC-AGI-3 experiment thread (`brain/experiments/`) that closed its
skill chapter at experiment 238. The skill chapter (226-238) hit a
realistic 12/15 ceiling on the stuck-game set: explorer diversity (225)
gets ~10/15, budget extension via the 234 null check adds tr87 s0/s3
(catalyzed 3.5x by 237's online forward-model learning), and sb26
residual seeds s1/s3/s4 are the structural brick wall under every
mechanism tested. The question this page tries to answer: what
robotics-side ideas might break that brick wall, and which of them
are brain-inspired in a way that fits the project's direction?

Two scope notes up front. First, the project's standing constraint
rules out in-context-LLM / VLA approaches as a primary direction even
when they are state of the art in robotics, so vision-language-action
foundation models (RT-2, OpenVLA, pi0, RT-X) get a brief landscape
note and not a lead. Second, robotics works under very different
conditions from ARC-AGI-3 (continuous control, multi-modal sensors,
hours of demonstration data) so the import is by mechanism, not by
benchmark.

## Why look at robotics now

Robotics is the field where learned-agent research at scale has
actually paid off in the last three years. World models that train and
deploy (Dreamer V3 solves Minecraft from scratch), diffusion-based
action distributions that genuinely fix the deterministic-policy
pathology that kept biting our skill chapter, action chunking that
empirically beats one-step behavior cloning by a wide margin, latent
action pretraining that learns an action embedding from action-less
video data, online distillation that improves agents during deployment
-- each of these is a mechanism a sufficiently small experiment on the
shared cached-substrate harness ([../../brain/brain/concrete/arc_common.py](../../brain/brain/concrete/arc_common.py))
could test against the 12/15 ceiling. None of them require an LLM.

## The landscape (since 2022, the four converging surges)

The robotics surge has four threads that interleave. World models
that learn from data and deploy. Generative (diffusion / flow) action
policies. Pretraining at scale and cross-embodiment transfer. Online
adaptation and self-improvement. Each got its mature 2023-2024
landmark and each maps to a brain-arc mechanism the skill chapter
either did not test or tested in a narrower form.

### A. World models that LEARN AND DEPLOY

The Dreamer line (V1 2019, V2 2020, V3 2023) is the cleanest
demonstration of "learn a latent dynamics model from offline rollouts,
train a policy in imagination on the model, deploy in the real
environment." Dreamer V3 ([Hafner et al. 2023](https://arxiv.org/abs/2301.04104))
solves Minecraft diamond mining from scratch, sweeps the BSuite /
Atari / Crafter benchmarks at a single hyperparameter set, and works
on continuous control. The recipe is a recurrent state-space model
(RSSM) trained to predict next latent + reward + termination, a value
function and an actor trained on rollouts in the model, and a return-
normalization trick that makes hyperparameters stable across domains.
DayDreamer ([Wu et al. 2022](https://arxiv.org/abs/2206.14176))
applies the same pipeline to physical robots; the world model is
trained ONLINE during deployment, which is the closest existing
validation of experiment 237's online forward-model learning, and at
the right scale (an A1 quadruped learning to walk in an hour).
Transformer / latent-tokenization world models (IRIS, TransDreamer,
DreamerV3-Token, [Genie](https://arxiv.org/abs/2402.15391)) replace
the RSSM with attention; IRIS ([Micheli et al. 2023, ICLR](https://arxiv.org/abs/2209.00588))
already gets cited in page 11.

What this validates and what it extends. The chapter's frozen ForwardCNN
+ MPC is a tiny instance of the same idea, and 237's online forward-
model learning was directly motivated by the Dreamer / DayDreamer
recipe. What Dreamer V3 does that the chapter does not: (a) the model
predicts in a LATENT space (RSSM), not raw pixels, which avoids the
per-pixel-error-compounds problem 221 hit; (b) the policy trains in
imagination on the model, not on rollout data via hindsight (as in 227);
(c) the model + policy + value function are jointly trained on a replay
buffer that grows. Each is a concrete chapter-importable mechanism.

### B. Diffusion (and flow) ACTION policies

Diffusion Policy ([Chi et al. 2023](https://arxiv.org/abs/2303.04137))
is the most directly relevant single robotics paper for the brain-arc.
Instead of representing pi(a | s) as a classifier over actions
(softmax/argmax), it represents it as a learned DIFFUSION PROCESS that
denoises action-sequences from Gaussian noise conditioned on the state.
Three properties of the diffusion formulation are exactly the
properties 226 BC worker, 228 worker eval-policy sweep, 233 afford_only,
and 236 BC-from-s0 each needed and failed to have. The policy is
STOCHASTIC by construction, so a deterministic-argmax loop is
structurally impossible. The policy is MULTIMODAL: it represents many
plausible action sequences for the same state, not a single peak,
which matters for puzzles where multiple paths reach the goal. And it
predicts ACTION SEQUENCES (chunks), not single steps, which connects
to the next thread.

The chapter's recurring deterministic-argmax-undercovers pathology --
hindsight BC (226 worker 42/371 vs random 109/371), affordance-only
(233 stuck at 3 configs on sb26), BC-from-s0 (236 stuck at cov 138 on
its own training seed) -- is the exact failure mode diffusion policies
were designed to fix. Decision Diffuser ([Ajay et al. 2023](https://arxiv.org/abs/2211.15657))
generalizes this to trajectory-level planning: condition diffusion on
return-to-go, language, or constraints; the model proposes whole
trajectories.

For a brain-arc import the question is whether the action space (5
simple + 256 click cells = 261 discrete) is amenable to diffusion.
Yes: D3PM ([Austin et al. 2021](https://arxiv.org/abs/2107.03006))
and discrete-diffusion work straightforwardly on token sequences,
and a 256-cell click action can be tokenized as a (row, col) pair.

### C. Action chunking and temporal abstraction

ACT ([Action Chunking Transformer, Zhao et al. 2023, ALOHA](https://arxiv.org/abs/2304.13705))
showed empirically that predicting K future actions at once and
executing them open-loop (or with temporal ensembling) beats one-step
BC by a wide margin on dexterous manipulation. The chunk amortizes
prediction error, ignores irrelevant per-step jitter, and provides
implicit temporal abstraction without a separate manager. K = 100 on
mm-precision tasks; K matched to natural action units is the right
tuning.

This is the same chunking that page 11 cited as cortico-basal-ganglia
in vivo behavior: motor sequences become single chunks under repetition,
the basal ganglia gates which chunk to release. ACT is a concrete
connectionist instantiation. RoboCat ([Bousmalis et al. 2023](https://arxiv.org/abs/2306.11706))
extends action chunking to a self-improving multi-embodiment agent that
ingests its own success traces and re-trains, validating both chunking
AND online distillation.

For brain-arc: a K-step action-sequence prediction head on the worker
or on a chunk-conditioned MPC variant could be tested cheaply on the
cached harness. The skill chapter's 226-238 worker is a per-step
argmax; an ACT-style K-step worker is the closest under-explored
chunking probe.

### D. Latent action pretraining

LAPA ([Latent Action Pretraining for General Action Models, Ye et al.
2024](https://arxiv.org/abs/2410.11758)) and Genie ([Bruce et al. 2024,
ICML](https://arxiv.org/abs/2402.15391)) / Genie 2 learn a LATENT
ACTION CODE from video data with no action labels: the model factors a
video into latent transitions that "explain" the change between
frames, then conditions a controllable simulator on those latents.
Genie produces playable 2D worlds from internet video. LAPA condition
a small action decoder on the latent and shows transfer to real robot
benchmarks.

The brain-arc connection runs two ways. First, the skill chapter's
object_key novelty (the 222 winner) is a hand-crafted "what changed"
signature; a LAPA-style latent transition code is the LEARNED version
of the same object. Second, the user's earlier intuition about "world
model attending to multiple coupled pieces of the state" (folded into
page 11 as the attention follow-up) is essentially the same mechanism
as LAPA's latent transition encoder. A latent-action world model
trained on the 25-game ARC corpus would give a single skill substrate
across games and could turn into a cross-game pretraining lever.

### E. Cross-embodiment + foundation models for action (the LLM branch)

The vision-language-action (VLA) line -- RT-1 ([Brohan et al. 2022](https://arxiv.org/abs/2212.06817)),
RT-2 ([Brohan et al. 2023](https://arxiv.org/abs/2307.15818)), RT-X /
Open X-Embodiment ([Padalkar et al. 2024](https://arxiv.org/abs/2310.08864)),
OpenVLA ([Kim et al. 2024](https://arxiv.org/abs/2406.09246)), pi0
([Black et al. 2024](https://arxiv.org/abs/2410.24164)) -- co-trains a
pretrained vision-language transformer (PaLI / Llama / SigLIP) with
robot action heads on millions of demonstration episodes pooled across
20+ robot embodiments. RT-X showed positive cross-embodiment transfer
(a single model on 22 embodiments beats embodiment-specific models on
held-out tasks); pi0 added a flow-matching action head for continuous
control. These models are the state of the art in robotics generalist
capability.

This branch is explicitly OUT OF SCOPE for the brain-arc per the
project constraint ("stop suggesting LLM, we're not going in that
direction", 2026-05-23). Mentioned here for landscape completeness;
the mechanisms a non-LLM brain-arc would import from this branch (data
scale, cross-task pretraining, action tokenization, return conditioning)
appear on other branches above.

### F. Online adaptation and self-improvement

DayDreamer is the cleanest world-model-side example (already covered).
RoboCat ingests its own successful trajectories. Mobile ALOHA
([Fu et al. 2024](https://arxiv.org/abs/2401.02117)) interleaves
demonstration collection with imitation training. The DeepMind robotics
group's online distillation work uses a slow-frozen teacher and a
fast-online student, both updated on the same stream.

The skill chapter's frozen-substrate-then-eval pattern (until 237)
runs directly counter to this; 237 was the chapter's first online
experiment and got the chapter's first consistent positive (+40-130%
coverage; tr87-s0 3.5x acceleration). The next steps along this axis
-- replay buffers, slow/fast student-teacher distillation, longer
episodes with cross-seed re-training -- are all importable.

### G. Visual representations for action

R3M ([Nair et al. 2023](https://arxiv.org/abs/2203.12601)), VC-1
([Majumdar et al. 2023, NeurIPS](https://arxiv.org/abs/2303.18240)),
MVP ([Xiao et al. 2022](https://arxiv.org/abs/2210.03109)) all
pretrain a visual encoder on large web data and transfer the
representation to downstream robot tasks. VC-1 trained on 5500 hours
of egocentric / interaction video; R3M used Ego4D + time-contrastive +
language alignment objectives.

For the brain-arc the encoder is currently a 3-layer 48-channel CNN
trained per-(game, seed) on a few thousand random transitions. A
single encoder pretrained on the full 25-game ARC corpus could ride
the same transfer logic. The cached substrate harness already keys on
(game, seed, cfg_hash), so a cross-game pretrained encoder is a
straightforward extension.

### H. Skill / option pretraining (the closest cousin to page 11)

PARROT ([Singh et al. 2021](https://arxiv.org/abs/2011.10024))
pretrains a goal-conditioned action prior on multi-task data and
fine-tunes for new tasks. BC-Z ([Jang et al. 2022](https://arxiv.org/abs/2202.02005))
trains zero-shot task-conditional policies from demonstrations. PARROT's
prior compresses "what useful agents tend to do" into a normalizing-
flow distribution that downstream tasks can reuse; downstream
fine-tuning then learns small deviations from the prior, which is much
faster than training from scratch.

This is the closest robotics cousin to the page-11 skill-learning
chapter, and the most under-tested mechanism in the brain-arc: a goal-
conditioned action prior pretrained on the 25-game ARC corpus (or
the 6-game subset 222 already used) would change the worker training
problem from "BC random data with hindsight" to "fine-tune a useful
prior on small task-specific data." That fix is structural, not a
tweak.

## What this means for the brain-arc

Five concrete leads, ranked by signal-per-effort on the cached
substrate harness.

1. **Diffusion worker policy.** Replace 227/228's softmax / argmax
   worker with a discrete-diffusion policy pi(a | s, g). Stochastic
   and multimodal by construction; structurally cannot collapse to the
   228 deterministic-argmax-undercoverage loop that has killed 226 BC,
   233 afford_only, and 236 BC-from-s0. The single most directly-
   applicable robotics lesson. Cost: ~150 LOC on the harness; needs a
   small discrete-diffusion CNN. The 228 worker_eval_policy_sweep
   already showed the Q net carries informative signal on tr87, just
   exploited badly by argmax; diffusion is the right exploiter.

2. **Action chunking on the MPC / worker.** Predict K future actions
   at once. ACT's empirical lesson is that K = small-tens (10-50) beats
   K = 1 on dexterous tasks; the brain-arc analog is a K-step click
   sequence. On sb26 (256-click action space, residual brick wall) a
   chunked action prior could amortize the 261-action sampling problem
   that 233 click_affordance failed to fix at the per-step level.

3. **In-imagination worker training (Dreamer-style).** Currently the
   worker (227) is trained by hindsight on RANDOM rollouts, which 226
   showed mostly imitates the random marginal. Train the worker
   instead by rolling it in the cached forward model (imagination) on
   self-proposed goals, with goal-reach reward via object_key match.
   This is HER + Dreamer + the cached substrate; it gives the worker
   on-policy-like data without environment cost. Most concrete fix to
   the 226-228 worker quality ceiling.

4. **Online distillation / replay extension.** Extend 237's online
   forward-model learning with a replay buffer (slow updates from
   replayed transitions in addition to fresh ones, to control
   catastrophic forgetting) and longer episodes that sometimes re-run
   from previously-discovered novel configs. Closest extension of
   the chapter's first consistent positive.

5. **Cross-game pretraining of the encoder + forward model.** The
   single 25-game ARC corpus is the "web data" of this benchmark.
   Pretrain one forward model on all 25 games' random rollouts, then
   fine-tune per-(game, seed) for the residual stuck games. R3M / VC-1
   logic at small scale.

Diffusion + chunking + in-imagination training together would
constitute the closest robotics-style refit of the worker, and each
can be tested independently first. Cross-game pretraining is the
representation lever.

## Neuroscience grounding

Each robotics mechanism above has a brain analog worth naming.
Dreamer-style world models map to cortical predictive coding (Friston;
Bastos et al. 2012, predictive-coding microcircuits). Action chunking
maps to cortico-basal-ganglia chunking (Graybiel; the page 11 cites).
Diffusion policies have no direct brain analog but the multimodal-
stochastic property maps to motor cortex population codes that
represent ACTION DISTRIBUTIONS, not single intended movements
(Churchland's dynamical-systems view). Latent action pretraining maps
to motor primitives / muscle synergies (Bizzi, d'Avella). Online
adaptation maps to context-dependent dopamine and cerebellar
adaptation (Wolpert). Cross-task pretraining maps to the general-
purpose-cortex hypothesis (Mountcastle's columnar uniformity).

The take-home is that none of these robotics threads is a foreign
import for a brain-inspired project: each has a corresponding brain
mechanism, and each underspecified mechanism in the skill chapter
(per-step worker, frozen model, deterministic argmax, single-task
training, fresh encoder per seed) has a robotics-validated fix with
a brain story behind it. The 12/15 ceiling on the stuck games is not
the end of the chapter; it is the end of the FROZEN-SUBSTRATE chapter.
The next chapter is the one where the substrate learns during the run.
