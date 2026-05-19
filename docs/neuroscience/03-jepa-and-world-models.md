# 03 - JEPA, world models, and "predict in latent space"

[Breakthrough 3](02-five-breakthroughs.md#breakthrough-3---simulating-the-first-mammals-150-250-mya)
said the cortex is a generative model that supports simulation. This
page is the modern, concrete version of that bet -- Yann LeCun's
Joint Embedding Predictive Architecture (JEPA) line and the broader
2024-2026 "world model" wave -- plus the argument LeCun makes about
*why generating pixels is the wrong way to build a brain*.

## LeCun's blueprint: "A Path Towards Autonomous Machine Intelligence" (2022)

Position paper, OpenReview, June 2022 (v0.9.2):
<https://openreview.net/pdf?id=BZ5a1r-kVsf>. The architecture LeCun
proposes for an autonomous agent has six modules -- and you should
read them against Bennett's breakthroughs:

- **Perception** -- estimate current world state from sensors.
  (Bennett: cortical recognition.)
- **World model** -- the centerpiece. Predict plausible *future*
  states given actions; fill in unobserved state; handle uncertainty
  by predicting at an *abstract* level, throwing away unpredictable
  detail. (Bennett: cortical simulation, breakthrough 3.)
- **Cost module** -- intrinsic (hard-wired) costs + a trainable
  "critic"; the scalar the agent minimises. (Bennett: valence /
  steering, breakthrough 1; the critic, breakthrough 2.)
- **Actor** -- propose action sequences; optimise them by gradient
  descent *through the world model* against the cost (model-predictive
  control / planning-as-inference). (Bennett: basal-ganglia action
  selection + cortical vicarious trial-and-error.)
- **Short-term memory** -- working memory for states/costs.
  (Bennett: PFC working memory.)
- **Configurator** -- an executive that, given a task, reconfigures
  all the other modules (sets the cost, primes perception, etc.) --
  "the conductor". (Bennett: aPFC/gPFC control of simulation,
  breakthroughs 3-4.)

The technical heart is the world model, and the claim is: **it must
predict in a learned representation space, not in input space.**

## What a JEPA is

A Joint Embedding Predictive Architecture has three parts:

- an **encoder** that maps a context `x` to an embedding `s_x`;
- an **encoder** (often an exponential-moving-average [EMA] copy of
  the first) that maps a target `y` to an embedding `s_y`;
- a **predictor** that maps `s_x` (plus optional latent variable `z`
  and/or an action `a`) to a *prediction of `s_y`*.

Train it to make the predicted embedding match the target embedding.
Contrast with the two alternatives:

- **Generative / reconstructive** (autoencoders, masked
  autoencoders, diffusion, autoregressive pixel models): predict the
  *raw target* `y`. Problem: you are forced to spend capacity
  predicting inherently unpredictable detail (every leaf, every
  texture pixel, the exact splash), and the loss punishes you for not
  doing the impossible. LeCun: "predicting pixels is wasteful and
  doomed."
- **Contrastive** (SimCLR, etc.): pull positive pairs together, push
  negatives apart. Works, but needs many negatives, careful
  augmentations, and is finicky at scale.

A JEPA sidesteps both: the *target encoder* can discard
unpredictable detail, so the predictor only has to get the
*predictable abstract structure* right. The price is the **collapse**
failure mode -- both encoders can cheat by mapping everything to a
constant (loss zero, representation useless). Everything hard about
training JEPAs is preventing collapse. JEPAs are also presentable as
a special case of *latent-variable energy-based models* (Dawid &
LeCun 2023, arXiv:2306.02572).

### The line of work

- **I-JEPA** (image JEPA) -- Assran et al., CVPR 2023,
  arXiv:2301.08243. From one context block, a vision-transformer (ViT)
  predictor predicts the *representations* (from an exponential-moving-
  average target encoder) of several large target blocks elsewhere in
  the image. No augmentations, no negatives, no pixel loss. Big-block
  masking is what makes the features semantic. ViT-Huge trains on
  ImageNet in <72 h on 16 A100 graphics cards.
- **V-JEPA** (video JEPA) -- Bardes et al., 2024, arXiv:2404.08471.
  Predict masked spatio-temporal feature regions from visible ones;
  pure feature prediction on ~2M public videos. Frozen-backbone:
  ~82% Kinetics-400, ~72% Something-Something-v2 -- motion-aware
  features for free.
- **V-JEPA 2** -- Meta FAIR (Fundamental AI Research), June 2025,
  arXiv:2506.09985. Scaled to >1M hours of video, ViT-g (1B+
  parameters). Three results: (1) strong perception/anticipation;
  (2) glued to a large language model (LLM), state-of-the-art (SOTA)
  video question-answering (QA) at the 8B scale; (3) **V-JEPA 2-AC**
  (action-conditioned) -- a latent world model post-trained on <62 h
  of unlabeled robot video, then used **zero-shot on a real Franka
  arm** for pick-and-place by *planning* (model-predictive control:
  roll out the action-conditioned predictor in latent space, optimise
  the action sequence toward an image goal). This is the clearest
  realisation so far of the perception -> world-model -> actor loop
  above.
- **Brain-JEPA** -- Dong et al., NeurIPS 2024 (spotlight),
  arXiv:2409.19407. JEPA *applied to brains*: a foundation model of
  functional magnetic resonance imaging (fMRI) brain dynamics with
  "brain gradient positioning" and spatiotemporal masking; SOTA on
  demographic / disease / trait prediction. (Nice mirror: a
  brain-inspired architecture used to
  model brain data.)

## LeJEPA: making it provable (Balestriero & LeCun, Nov 2025)

arXiv:2511.08544. The problem statement: real JEPA training is held
together by *heuristics* -- stop-gradient, EMA teacher/student,
asymmetric predictors, output normalisation, special LR/momentum
schedulers -- all there to dodge collapse, with no principled answer
to *what distribution the embeddings should have*. LeJEPA's
contributions:

1. **Theory:** the **isotropic Gaussian** is the optimal embedding
   distribution -- it minimises worst-case downstream prediction risk
   over unknown tasks (right bias-variance / Lipschitz behaviour).
2. **SIGReg (Sketched Isotropic Gaussian Regularization):** push the
   embedding distribution toward isotropic Gaussian by taking many
   random 1-D projections ("slices") and forcing each univariate
   marginal to match a standard Gaussian via a characteristic-function
   goodness-of-fit test. Unbiased, low-variance, *linear* time and
   memory.
3. **LeJEPA = JEPA prediction loss + SIGReg.** One trade-off
   hyperparameter. No stop-gradient, no teacher/student, no schedulers.
   ~50 lines of distributed code. Stable across ResNets / ViTs /
   ConvNeXts and across domains; stable up to 1.8B-param ViT-g; ~79%
   ImageNet-1K linear probe with ViT-H/14. Bonus: training loss
   *correlates with downstream linear-probe accuracy* -- model
   selection without labels.

Why it matters here: it turns "the cortex is a generative model" from
a vibe into an objective with a uniqueness theorem behind the latent
geometry. (It has already spawned follow-ups -- KerJEPA, Var-JEPA,
etc. -- treat those lightly.)

## LeWorldModel: a stable world model from raw pixels (Mar 2026)

arXiv:2603.19312; Maes, Le Lidec, Scieur, LeCun, Balestriero
(Mila / New York University / Samsung SAIL / Brown). Project page:
<https://le-wm.github.io/>; code: <https://github.com/lucas-maes/le-wm>.

This is the one the user asked about, and it is the direct payoff of
LeJEPA. Earlier action-conditioned JEPAs (V-JEPA 2-AC) post-trained
*on top of* a giant pretrained video encoder; the prior attempt to
train a JEPA world model **end-to-end from pixels** needed six tuned
loss terms and was unstable. LeWorldModel:

- two components -- an **encoder** (frame -> compact low-dim latent)
  and a **predictor** (latent + action -> next latent);
- exactly **two loss terms** -- a next-embedding prediction loss + a
  Gaussian-latent regulariser (the SIGReg idea) -- so **one** tunable
  hyperparameter instead of six;
- **~15M parameters**, trainable on a single GPU in a few hours;
- **plans up to ~48x faster** than world models built on foundation
  encoders, while staying competitive on 2D/3D control;
- the latent space probes reveal physical structure, and "surprise"
  (high prediction error) reliably flags physically implausible
  events -- i.e. it learned something model-like, not just predictive
  texture.

For thvm this is the most copyable target: it is small, end-to-end,
two losses, single-GPU. A "LeWorldModel-in-miniature" on a tiny
gridworld or pong-like environment is a realistic thvm experiment;
see [05-toy-problems-with-thvm.md](05-toy-problems-with-thvm.md).

## The rest of the world-model field (2024-2026), briefly

- **Genie / Genie 2 / Genie 3** (DeepMind): "foundation world models"
  -- Genie (ICML 2024, arXiv:2402.15391) learns a *latent action
  space* unsupervised from internet platformer videos so one image
  becomes a playable 2D game; Genie 2 (blog, Dec 2024) does 3D,
  ~1-minute-consistent worlds; Genie 3 (blog, Aug 2025) is real-time
  (24 fps, 720p), multi-minute-consistent, with object permanence --
  pitched as an *environment generator* for training embodied agents.
  (Genie 2/3 are blog/tech-report releases, not papers.)
- **DreamerV3** (Hafner et al., arXiv:2301.04104; *Nature* 2025):
  model-based reinforcement learning (RL) done right -- learn a
  recurrent latent world model, improve the policy by "imagining"
  rollouts in latent space; one hyperparameter config masters 150+
  tasks; first to mine diamonds in Minecraft from scratch. This is
  "vicarious trial-and-error" as an algorithm.
- **NVIDIA Cosmos** (arXiv:2501.03575): an open-weight *platform* of
  world foundation models (diffusion-latent and autoregressive-token
  variants) plus tokenizers and post-training recipes, aimed at
  robotics / autonomous vehicles -- "digital twins" of the physical
  world.
- **SANA-WM** (NVIDIA, arXiv:2605.15178, May 2026): a 2.6B-parameter
  diffusion transformer that generates minute-scale 720p video with
  6-DoF camera control on a single GPU. A pixel-space generator (the
  camp this page argues against), but two engineering ideas are
  paradigm-independent: *hybrid linear attention* -- a frame-wise
  Gated DeltaNet linear recurrence carries the compressed long
  history cheaply, softmax attention does the sharp local work -- and
  a coarse-then-refiner two-stage pipeline. ~36x the throughput of
  comparable baselines.
- **Warp-as-History** (Wang & He, arXiv:2605.15182, May 2026):
  camera-controlled video from *one* training video. Adds no camera
  encoder and no control branch -- it renders the frames it already
  has under the target camera path (a geometric *warp*) and feeds
  those as "pseudo-history" through the model's *existing*
  history-conditioning pathway, dropping disoccluded tokens so the
  model inpaints them. Two transferable principles: route a new
  capability through an interface the model already has rather than
  bolting on a module, and hand the model the cheap deterministic
  part (the warp) for free so it only learns the hard residual (the
  disocclusions).
- **The Sora "world simulator" debate** (2024): OpenAI introduced
  Sora claiming scaled video generation is "a promising path toward
  general-purpose simulators of the physical world". LeCun rejected
  this publicly -- generating realistic video does *not* entail
  understanding physics; "generation is very different from causal
  prediction from a world model"; modelling the world by predicting
  pixels is "wasteful and doomed" -- and pointed to V-JEPA as the
  alternative. Surveys took the question up (e.g. "Is Sora a World
  Simulator?", arXiv:2405.03520). This generative-pixels-vs-predictive-
  latents axis is the organising fault line of the whole 2024-2026
  world-model discourse.

## The takeaway for thvm

Bennett's breakthrough 3 said "build a generative model and simulate
with it". The 2024-2026 refinement is: **don't generate the input --
predict a learned latent of the future, conditioned on action, and
plan in that latent.** Keep it small (LeWorldModel: 15M params, two
losses), regularise the latent toward an isotropic Gaussian (LeJEPA),
and you have a planning agent. Combined with breakthrough 2's
actor-critic for the *cost/critic* side, that is a complete (toy)
autonomous agent -- and every piece of it is differentiable, which is
exactly what thvm is good at.

**Note (2026-05-18), reader's question -- what the `brain/`
quasimetric arc can take from SANA-WM and Warp-as-History.** Both are
pixel-space generators, so the *paradigm* is the wrong one for us;
the *architecture* lessons are not. (1) SANA-WM's hybrid linear +
softmax attention is the arc's own recurring finding made structural
-- "a reliable local map, depth eats it" (experiments 144-155) is
answered by giving the long horizon its own cheap linear-recurrent
state instead of one attention doing both jobs; the learned dynamics
model of stage 3 should carry a compact recurrent state, not attend
over all history. (2) Warp-as-History's "route it through the
existing interface, do not add a module" is something the arc proved
three times independently: experiment 153's privileged passive
channel specialised while the structureless agency heads smeared;
156/157 bolted expert-iteration and hierarchical planning *on top*
and both failed; 158's win was reshaping the *existing* loss. (3) Its
"deterministic prior for free, learn only the residual" matches the
quasimetric's `sym + asym` split and resolves the
no-hand-coded-simulator tension correctly -- a cheap deterministic
*prior* the learned model only corrects is legitimate, a hand-coded
*simulator* as the deliverable is not.

For the deep companion survey of the *generative* video and game
world-model field -- the SANA-WM neighbourhood, its compression
lineages, long-context backbones, autoregressive-rollout drift fixes,
interactive game engines, camera control and memory mechanisms -- see
[07-video-world-models-survey.md](07-video-world-models-survey.md).

Next: [04-brain-inspired-ai.md](04-brain-inspired-ai.md) -- the
other research bet (reverse-engineer the brain's learning rules and
inductive biases, not just the architecture).

References: see [references.md](references.md) for full links to all
arXiv IDs above.
