# 05 - Toy problems with thvm: the bootstrap track

*(Restructured 2026-05-13 in response to a reader request -- "for
thvm, instead of engineering toy experiments for each rung of the
breakthrough ladder, I want some kind of bootstrap toy example
starting from the first one and then continuously growing it so that
instead of LLMs (large language models) engineering only the top
rung, we bootstrap the same model or its components and grow on top
of it -- so we have a learning system that learns from the ground up
in increasingly complex environments and datasets.")*

The previous version of this page was a menu of *independent*
experiments, one per breakthrough. The point of the redesign: that
menu is a *backwards* recipe -- it builds and discards each layer in
turn, exactly the failure mode Bennett's book diagnoses in current
AI (artificial intelligence) -- "we built layer 5 first and skimped
on 2-4". The fix is to follow
the *cumulative-not-replacing* property of biological evolution:
**one agent, one shared substrate, layers added on top, earlier
capabilities preserved.**

This page describes that bootstrap track. The five "stages" each add
a module (or two) to a single growing agent, training it in an
environment that gets one notch harder. The verification milestones
inside each stage are the same toy results that used to be the menu
items -- they survive, but now as *checkpoints* you must pass before
adding the next layer, not as standalone experiments.

## North star: ARC-AGI 1, 2, 3 -- the ultimate generalisation test

*(Added 2026-05-13 in response to a reader request -- "ground this
to ARC-AGI 1 to 3 somehow so that there is an ultimate goal".)*

The bootstrap is a *means*. The *end* is generalisation -- the
ability to handle a task you were not trained on, with very few
demonstrations. The clearest current public benchmark for that is
the **ARC-AGI** ladder, designed by Francois Chollet and Mike Knoop
to *measure intelligence rather than knowledge*: each task is novel,
each test set is held out, and the demonstrations are few-shot
("here are 3 input/output pairs; produce the output for this new
input").

The three rungs match where the bootstrap arc lands:

- **ARC-AGI-1** -- the original 2019 grid-puzzle benchmark
  (Chollet, "On the Measure of Intelligence", arXiv:1911.01547).
  Static input/output reasoning over 30x30 colour grids: rotation,
  symmetry, gravity, object counting, infill, etc. 800 tasks (400
  public / 400 private). Frontier LLMs (large language models) with
  test-time compute are now in the 50-90% range after years of
  effort; humans solve roughly 85% on the public set. **Hits stages
  1-3 of the bootstrap**: perception, abstraction, and offline
  reasoning, no agency.
- **ARC-AGI-2** -- the 2025 sequel (ARC Prize Foundation; ARC Prize
  2025 Technical Report, arXiv:2601.10904). Carefully recalibrated
  to be hard for current LLMs but tractable for humans; introduces
  *compositional reasoning over multiple abstractions per task* and
  filters out tasks current solvers ace. **Same rungs as ARC-AGI-1,
  one notch harder; pushes a real stage-3 world model**.
- **ARC-AGI-3** -- the **interactive / agentic** rung (Chollet,
  Knoop et al., "ARC-AGI-3: A New Challenge for Frontier Agentic
  Intelligence", arXiv:2603.24621, launched March 25, 2026). Each
  task is a hand-crafted **turn-based game** with **no rules, no
  instructions, no stated goals** -- the agent must *explore*,
  *infer the dynamics*, *figure out what winning looks like*, and
  *transfer what it learned to harder levels*. At launch: **humans
  100%, frontier AI ~0.5%** (best public scores were Gemini Pro
  0.37%, GPT-5.4 0.26%, Opus 4.6 0.25%, Grok-4.20 0%). The 2026
  prize pool is $2M. **Hits stages 1-5 of the bootstrap squarely**
  -- sensorimotor exploration (stage 1), reward inference (stage 2),
  world-model construction (stage 3), goal inference and possibly
  other-agent modelling (stage 4), and -- because the levels are
  named and described in language -- a configurator (stage 5).

Why ARC-AGI as the north star:

- **It is explicitly a generalisation test, not a memorisation
  test.** The training set never overlaps with the eval set, and
  every task is new to the solver. That is the property the
  bootstrap arc is designed to develop.
- **It scales with the bootstrap.** ARC-AGI-1 is reachable from
  stages 1-3 alone (no agency required); ARC-AGI-2 stresses world
  modelling; ARC-AGI-3 *requires* the full agentic stack. So each
  stage of the arc has a corresponding rung of the benchmark you
  can probe against.
- **Honest evaluation.** Humans saturate the benchmark; frontier
  LLMs do not. There is no risk of training-set contamination
  inflating scores (the private set is unseen), and the gap to
  human is large enough that incremental progress is measurable.

For thvm purposes the realistic target is *not* to win the ARC
Prize (frontier LLMs with $$ of test-time compute are the
competition). It is to **(a) match GPT-3.5-class performance on
ARC-AGI-1 from a 10-100M-parameter model**, and **(b) be the
cleanest publicly-traceable account of why the bootstrap arc helps
ARC-AGI-3 at all**, even at low absolute score. Either is a thesis-
sized result; the second is the closer fit for a learning project.

## The fixed curriculum: a diverse environment set spanning the rungs

ARC-AGI is the *test*; the agent never trains on it. The training
curriculum is a **fixed, diverse set of environments**, mapped to
the rungs so each stage of the bootstrap has somewhere to live and
older capabilities have somewhere to keep practising.

| Stage | Environment | Why it's in the curriculum |
|---|---|---|
| 1 -- Steering | a hand-crafted 1-D/2-D **gradient world** (built in stage 1); **Classic Control** (CartPole, Pendulum, MountainCar; OpenAI Gym / Gymnasium) | minimal sensorimotor loop; well-understood dynamics; cheap |
| 2 -- Reinforcing | **MiniGrid** (Chevalier-Boisvert et al.); **Procgen Benchmark** (Cobbe et al. 2020, procedural variation -- forces generalisation); a small subset of the **DeepMind Control Suite** | sparse rewards, gridworld primitives, procgen for the generalisation gradient |
| 3 -- Simulating | **Crafter** (Hafner 2022) -- the natural Dreamer / world-model benchmark; **MiniHack** / **NetHack Learning Environment** (Kuttler et al. 2020) for long-horizon, procedurally generated; an **Atari** subset for canonical pixel-RL | long-horizon, partial observability, world-model gain has to show up here |
| 4 -- Mentalizing | **Hanabi Learning Environment** (Bard et al. 2020) -- cooperative theory-of-mind (ToM) benchmark; **Melting Pot** (Agapiou et al. 2022, DeepMind) -- 50+ multi-agent social-dilemma scenarios; **Overcooked-AI** (Carroll et al. 2019) for paired coordination | the canonical ToM / coop / coordination test bed in RL |
| 5 -- Speaking | **BabyAI** (Chevalier-Boisvert et al. 2019) -- language-instructed gridworld, gold standard for grounded language; **ALFWorld** (Shridhar et al. 2021) -- text + embodied; **MineDojo** / **Voyager** scaffolding for Minecraft as a language-driven open-ended environment | grounded language with verifiable success conditions; long-horizon |
| Generalisation test | **ARC-AGI-1, 2, 3** | held out, never trained on -- the north star |

A few design rules for this fixed set:

- **Everything except ARC-AGI is in the training mixture, always.**
  When stage 5 trains, it samples episodes from stage 1's gradient
  world, stage 2's MiniGrid, stage 3's Crafter, stage 4's Hanabi,
  and stage 5's BabyAI, with weights that shift as the stages
  unlock.
- **ARC-AGI is *evaluated*, not trained on.** Run it as a probe at
  every checkpoint. The *training* curriculum's job is to grow
  abstractions; the *test* job is to see whether those abstractions
  transfer to never-seen tasks.
- **Wherever possible, reuse existing Python / Gym interfaces.**
  thvm's WL surface is fine for the agent and training loop; the
  environment can stay on the Python side via `py/thvm` ctypes (or
  a thin RPC). Don't reimplement MiniGrid; bind to the published
  one.
- **Crafter is the single most important environment in the
  curriculum**, because it is the only one in the list where the
  world-model gain (stage 3) shows up cleanly at small scale. If
  Crafter's stage-3 milestone fails, the bootstrap is broken.

## What thvm gives you

- **Reverse-mode autodiff** (automatic differentiation) -- dup-like
  grad cells, chain-rule adjoints, materialization. See
  [../grad.md](../grad.md).  `TGradMany` for multi-output gradients;
  `TProfile` to catch allocation leaks.
- **A tensor / NN (neural-network) surface in the Wolfram Language
  (WL)** -- `wl/THVMLink/Kernel/NN.wl` has `TLinear`, `TConv`,
  `TEmbedding`, `TBatchNorm`/`TBatchNormTrain`, `TLayerNorm`, `TReLU`,
  `TSoftmax`/`TSoftmaxAxis`, and `TGlorot` / `TZeros` / `TOnes`
  initialisers. `Optim.wl` has `TAdam`. See [../wl.md](../wl.md).
- **Worked training examples** -- `wl/Examples/`: `linear-train`,
  `mlp-mnist`, `lenet-mnist`, `beautiful-mnist` (MNIST -- the
  handwritten-digit benchmark -- parity is tracked in
  `docs/plans/beautiful_mnist_parity.md`), `gpt2`, `newton-1d`
  (gradient-based optimisation of a scalar). Copy one of these as a
  skeleton.
- **CPU (central processing unit) and Metal backends**, just-in-time
  (JIT) compilation, autotune -- [../cpu.md](../cpu.md),
  [../metal.md](../metal.md). MNIST-scale
  is comfortable; anything bigger, watch host RAM (random-access
  memory) (see the memory note in the repo's auto-memory).

Reuse the existing NN primitives -- don't redefine `TGlorot`/`TZeros`
inline in a `.wls`; add to `NN.wl` if something is missing (this is a
standing project convention).

## The shared substrate

Every stage operates over the same three first-class objects.
Adding a stage means attaching a new *head* / module to one of these,
not redesigning them:

- **`z`: the shared latent state.** A small vector (start with 32-64
  dimensions, grow with curriculum). Stage 1 builds the encoder
  `phi: obs -> z`; every later stage *reuses* `phi` and trains it
  with additional losses.
- **`a`: the shared action space.** Discrete or low-dimensional
  continuous (e.g. {left, right, forward, no-op} for the first
  environments). Stage 1 builds the actor `pi: z -> a`; later stages
  *reuse* `pi` as the warm-start policy and add planning on top.
- **`c`: the shared scalar cost / valence.** A scalar the agent
  *minimises* -- collects intrinsic terms (curiosity, novelty,
  surprise) and extrinsic terms (reward, goal-distance, language
  alignment) as the stages accrete. Stage 1 starts with a hand-coded
  `c`; stage 2 learns a value function `V` that *predicts* future
  cumulative `c`; stage 3 evaluates rollouts under `c`; stage 5
  adds language-conditioned `c`.

Concretely in thvm: `phi` is a tiny `TConv` + `TLinear` stack
projecting observation to `z`; `pi` is a `TLinear` head producing
action logits; `c` is a `TLinear` head producing a scalar.
Initialisers from `NN.wl` (`TGlorot` for weights, `TZeros` for
biases). All differentiable via `TGradMany`. Optimised with `TAdam`.

The standing rule for the whole track: **parameters from earlier
stages are kept, optionally fine-tuned with a small learning rate;
new parameters are added at the higher rate.** This is the
cumulative-not-replacing constraint, enforced.

## Stage 1 -- Steering: build `phi`, `pi`, `c`; teach Hebbian binding

(Breakthrough 1 on [page 2](02-five-breakthroughs.md#breakthrough-1---steering-the-first-bilaterians-550-600-mya).)

**What is added.** The three substrate modules themselves.
- `phi` (encoder): observation `[gradient_sensor, cue_channels]` ->
  `z`.
- `pi` (actor): `z -> (turn_rate, speed)`.
- `c` (valence): hand-coded as +1 when the gradient is increasing,
  -1 otherwise, modulated by an "arousal" scalar = slope magnitude.

**What is reused.** Nothing -- this is the seed.

**Environment.** 1-D or 2-D world with a chemical gradient (single
food source). A few extra "cue" channels that, by hand, correlate
with the gradient (e.g. a colour patch always appears just before
the gradient strengthens).

**Learning rule.** *Not* autodiff yet. A manual three-factor Hebbian
update: for each step, `dW[cue, valence_head] += eta * cue *
valence`, applied to the cue-channel inputs of `c`. After enough
exposure the cue alone drives `c` and the actor approaches before
the gradient gets steep -- **classical conditioning**.

**Verification milestones (the old "Breakthrough 1" experiment).**
- Plot the trajectory: the agent finds the gradient.
- Plot the cue-to-`c` weight over time: it grows and then plateaus.
- After conditioning, hide the gradient and present only the cue --
  the agent still approaches (the cue *is* the value now).

**thvm specifics.** All `TLinear` + `TReLU`. The Hebbian update is a
manual `TUOp`-style outer product applied to one weight matrix; no
loss function, no `TAdam` yet. Sit on the WL surface and the data
plumbing.

**Pitfall to avoid.** It is tempting to start with a value head and
TD learning here. Don't. The whole point of stage 1 is to *not* have
a critic yet -- to show that pure Hebbian binding does real work on
its own, and to leave a clean place for stage 2 to attach.

## Stage 2 -- Reinforcing: attach `V`, switch plasticity to TD-gated

(Breakthrough 2 on [page 2](02-five-breakthroughs.md#breakthrough-2---reinforcing-the-first-vertebrates-520-540-mya).)

**What is added.**
- `V: z -> R` (critic head). A second `TLinear` head off `phi(o)`.
- A **temporal-difference (TD) error** `delta = c_t + gamma * V(z_t+1) -
  V(z_t)`, the "dopamine".
- The actor `pi` keeps the stage-1 weights but is now updated by
  policy gradient with `delta` as the advantage estimate (the
  classical A2C, advantage actor-critic).
- *Optional add-ons that don't change the substrate*: a curiosity
  head (intrinsic reward proportional to `phi`-prediction error), a
  distributional critic (K quantile heads instead of one mean), an
  SR (successor representation) head `M(s, s')`.

**What is reused.** `phi` and `pi` from stage 1 (warm-started, then
fine-tuned end-to-end). The hand-coded extrinsic `c` from stage 1
becomes the reward signal; intrinsic curiosity adds to it.

**Environment.** A small gridworld (start 7x7 with one goal and a
few walls, scale up). The same gradient-world from stage 1 is *kept
in the curriculum* -- if stage 2 forgets how to do classical
conditioning, you regressed.

**Learning rule.** Now autodiff: `TGradMany` through (`pi`, `V`,
`phi`) against the standard A2C loss `-log pi(a|z) * stop_grad(delta)
+ 0.5 * delta^2 + entropy_bonus`. Train with `TAdam`. Three-factor
Hebbian from stage 1 generalises naturally: the *eligibility trace*
(activity that recently mattered) gets gated by `delta` (the global
dopamine signal). This is the same shape as biological three-factor
plasticity.

**Verification milestones.**
- The **dopamine-shift plot**: `delta` over training migrates from
  reward-time to cue-time (the Schultz-Dayan-Montague signature).
- The value map `V(z)` filled in across the gridworld.
- Curiosity-only run: in a no-extrinsic-reward maze, the agent still
  explores (the *novelty* drives `delta`).
- *Non-regression*: the stage-1 gradient-world still works.

**thvm specifics.** A `TGradMany` call returns gradients for all
three heads. `TAdam` updates them at a *higher* learning rate; if
you want to fine-tune `phi` more slowly, use a second `TAdam` with a
smaller `lr` for `phi`'s parameters. (Two-optimiser pattern -- a
standing thvm trick.)

**Pitfall to avoid.** The reward shaping is where everything breaks
(see the AI CUDA Engineer cautionary tale on
[page 4](04-brain-inspired-ai.md#the-cautionary-tale-ai-cuda-engineer-feb-2025)).
Define the reward *first*, in code, and treat it as ground truth --
don't let the agent's behaviour change your reward definition.

## Stage 3 -- Simulating: attach `f`; plan by rolling out `f` through `V`

(Breakthrough 3 on [page 2](02-five-breakthroughs.md#breakthrough-3---simulating-the-first-mammals-150-250-mya);
the LeWorldModel recipe from [page 3](03-jepa-and-world-models.md#leworldmodel-a-stable-world-model-from-raw-pixels-mar-2026).)

**What is added.**
- `f: (z, a) -> z_next` (the latent dynamics predictor, a small
  `TLinear` / multi-layer perceptron, MLP). This is the **world
  model**.
- An auxiliary **anti-collapse regulariser** on `phi`: either
  VICReg (Variance-Invariance-Covariance Regularisation) or, the
  principled choice from page 3, SIGReg (Sketched Isotropic
  Gaussian Regularisation).
- A **model-predictive control (MPC) planner**: from the current
  `z`, sample K action sequences, roll `f` forward for H steps,
  score each by `sum gamma^t * c(z_t) + V(z_H)`, pick the first
  action of the best. Optimise the action sequence by gradient
  descent *through* `f` -- this is exactly what thvm's autodiff is
  for.
- *(Optional, very useful)*: a generative head `g: z -> obs` if you
  want a Helmholtz / VAE (variational autoencoder) reading -- but
  the JEPA (Joint Embedding Predictive Architecture) twist is that
  you can *skip generation entirely* and just predict latents.

**What is reused.** `phi`, `pi`, `V` from stage 2. `pi` becomes the
*warm-start* for the planner's action sequence -- the planner
optimises it, but pi gives the initial guess and is itself trained
to imitate the planner's outputs (an inner-loop distillation that
keeps the model-free path responsive).

**Environment.** Same gridworld, then a cart-pole or pong-like toy
where rollouts genuinely help. The stage-1 and stage-2 environments
stay in the curriculum.

**Learning rule.** Two loss terms (the LeWorldModel recipe):
`L_pred = || f(phi(o), a) - sg(phi(o')) ||^2` and `L_reg = SIGReg
(phi(o))`. Trained jointly with the stage-2 A2C losses. One trade-off
hyperparameter between L_pred and L_reg; everything else is the
existing pipeline.

**Verification milestones.**
- A linear probe on `z` recovers the true gridworld state (the
  latent is *useful*, not just predictive).
- Inject a physically-impossible transition into the trajectory --
  prediction error on `f` should spike. This is the "surprise as a
  flag" property from page 3.
- The MPC agent outperforms the stage-2 model-free agent on the
  same task. If it doesn't, the world model isn't learning;
  diagnose before adding stage 4.
- *Non-regression*: stages 1 and 2 still pass.

**thvm specifics.** This is where `TProfile` matters -- rolling `f`
H steps inside a `TGradMany` call generates a long autodiff path,
and you want to know which UOps allocate. SIGReg's random 1-D
projections are cheap; budget for them.

**Pitfall to avoid.** *Representation collapse* -- without the
regulariser, `phi` is rewarded for going constant (zero prediction
error). Watch the variance of `z` across a batch; if it drops,
something is wrong with the regulariser weighting.

## Stage 4 -- Mentalizing: attach an agent-conditioned `f`; ToMnet head

(Breakthrough 4 on [page 2](02-five-breakthroughs.md#breakthrough-4---mentalizing-the-first-primates-30-65-mya);
ToMnet, Cicero, opponent modelling on
[page 4](04-brain-inspired-ai.md#mentalizing-ai-theory-of-mind-imitation-and-intent-inference-breakthrough-4).)

**What is added.**
- An **agent embedding** `e_ag` -- a vector that summarises another
  agent's policy. Computed by a small recurrent net (the "character
  net" of ToMnet) from a few observed episodes of that agent's
  behaviour.
- An **agent-conditioned world model** `f_ag: (z, a, e_ag) -> z'`
  -- predicts the *next state given the other agent's action
  choice*, which means the planner can roll forward in a multi-agent
  scene.
- *(Optional)*: a ToMnet-style **belief head** that predicts the
  *other* agent's beliefs / next-action distribution from current
  observations -- the false-belief tasks from page 4 become a
  measurable target.

**What is reused.** `phi`, `pi`, `V`, `f` from stage 3, all with
fine-tuning. Crucially, the agent's *own* trajectory becomes
training data for `f_ag` (the simplest possible "other": me-yesterday).
Imitation = stage-4 conditioning on a stored demonstrator embedding.

**Environment.** A multi-agent gridworld. Start with two agents,
shared reward (cooperation) or zero-sum (opponent modelling). The
single-agent stage-1-3 environments stay in the curriculum.

**Learning rule.** Stage-3 losses extended: predict the *combined*
next state (own + others) rather than just own; train the ToMnet
character net by next-action prediction on observed peers. **No new
optimisation primitive needed.**

**Verification milestones.**
- The ToMnet head predicts a held-out peer's next action above
  chance after a small number of observed episodes.
- The agent that knows its peer's embedding cooperates / out-plays
  one that doesn't, on the same task.
- A false-belief gridworld task (peer's view is blocked at a key
  moment): the agent's *belief head* tracks the peer's *belief*,
  not the ground truth. If you only ever match ground truth, your
  "ToM" is just policy prediction without belief representation
  (the Ullman critique from page 4).
- *Non-regression*: stages 1-3 still pass.

**thvm specifics.** The character net is a small recurrent network
-- you can use a `TLinear` over a concatenation of last-K states /
actions, or build a proper GRU (gated recurrent unit) if recurrence
is in NN.wl. Otherwise straightforward.

**Pitfall to avoid.** Surface-pattern shortcuts (the LLM-ToM debate
on page 4). The false-belief test is the discriminator: if the
agent does as well *without* the belief head as with, you're not
mentalizing.

## Stage 5 -- Speaking: language-conditioned configurator on top

(Breakthrough 5 on [page 2](02-five-breakthroughs.md#breakthrough-5---speaking-humans-100k-500k-years-ago); the
"configurator" idea is LeCun's, on
[page 3](03-jepa-and-world-models.md#lecuns-blueprint-a-path-towards-autonomous-machine-intelligence-2022).)

**What is added.**
- A **language encoder** `lambda: text -> e_text` (a small
  transformer head; the existing thvm `gpt2` example gives you a
  starting point). For a toy version use a tiny `TEmbedding` +
  averaged `TLinear` first; upgrade later.
- A **language-conditioned cost** `c_text(z, e_text)` -- adds a
  goal-alignment term to the planner's cost. Reading "go to the
  blue square" yields a `e_text` that the cost evaluates against
  states; states that match the description get lower cost.
- *(Optional, and the real test)*: a **language head** `mu: z ->
  text` that describes the current state in natural language. With
  both `lambda` and `mu` you can imitate / be taught by language,
  and you can report what you are doing.

**What is reused.** Everything from stages 1-4. The configurator
*shapes* `c` and the planner's cost; it does not replace the
underlying agent.

**Environment.** Language-instructed gridworld (BabyAI-style):
"pick up the red key", "go to the goal after touching the blue
square". The multi-agent and single-agent environments stay in the
curriculum.

**Learning rule.** Stages 1-4 losses unchanged. Add:
- A small supervised loss `L_text` on a tiny instruction dataset
  (text -> goal coordinates or goal embedding).
- *(Bigger lift, later)*: imitation learning from
  human-generated demonstration + caption pairs -- the imitation
  family from [page 4](04-brain-inspired-ai.md#mentalizing-ai-theory-of-mind-imitation-and-intent-inference-breakthrough-4)
  (behavioural cloning, Generative Adversarial Imitation Learning
  [GAIL], Diffusion Policy) plugs in here.

**Verification milestones.**
- Language-instructed task success.
- *Generalisation*: instructions composed from training primitives
  in new ways still work.
- *Non-regression*: stages 1-4 still pass without language input.

**thvm specifics.** The `gpt2` example shows the full text-side
pipeline; the trick is keeping the language module small enough that
it doesn't dwarf the agent.

**Pitfall to avoid.** *Skipping straight to here.* If stages 1-4 are
underbuilt, stage 5 will paper over them with linguistic fluency
that has nothing under it -- which is the diagnosis that motivated
the whole document set (page 2's "we built layer 5 first").

## Auto-curriculum: evolve the environment stream

The fixed curriculum above keeps the agent honest about what it
*should* know; but a fixed mix is not enough to make the bootstrap
*sample-efficient*. The reader's other request -- "a mechanism to
learn the curriculum itself so that bootstrapped learning is more
efficient, with evolutionary mechanisms" -- is exactly the
**open-ended / auto-curriculum** literature. The shape of the bet,
in one sentence: **let an evolutionary outer loop *generate*
environment variations that sit just past the agent's current
frontier, and let the agent's frontier *select* what gets
generated.**

The main published recipes, all directly transplantable:

- **POET (Paired Open-Ended Trailblazer)** -- Wang, Lehman, Clune
  & Stanley, "POET: Endlessly Generating Increasingly Complex and
  Diverse Learning Environments and Their Solutions",
  arXiv:1901.01753 (2019, Uber AI Labs). The seminal paper. **Co-
  evolve agents and environments**: each (env, agent) pair is a
  population member; environments mutate (the agent's body, the
  terrain, the obstacles); agents transfer between environments
  when they help; selection keeps pairs that are "not too easy, not
  too hard" for the current agent. Solves 2-D bipedal-walker tasks
  no direct curriculum can.
- **Enhanced POET** -- Wang et al., ICML 2020 -- adds a *novelty*
  criterion to environment acceptance (must differ enough from
  prior environments in behavioural space) and a stronger transfer
  test. The right version to copy if you implement only one.
- **PLR (Prioritised Level Replay)** -- Jiang, Grefenstette &
  Rocktaschel, "Prioritized Level Replay", ICML 2021,
  arXiv:2010.03934. Cheaper than POET: don't generate new levels,
  *re-weight existing ones* by their *learning potential* (an
  estimate of how much the agent's policy is currently improving on
  each level). Drops in to any procgen-style env (the fixed
  curriculum's Procgen + MiniGrid + ALFWorld slots).
- **ACCEL** -- Parker-Holder, Jiang, Dennis et al., "Evolving
  Curricula with Regret-Based Environment Design", ICML 2022,
  arXiv:2203.01302 (DeepMind). The bridge: PLR's replay scheme +
  evolutionary *editing* of high-regret levels. Faster than POET
  for the same generalisation gain.
- **OMNI (Open-endedness via Models of human Notions of
  Interestingness)** -- Zhang, Lehman, Stanley & Clune,
  arXiv:2306.01711 (NeurIPS 2024). An LLM scores candidate tasks by
  "interestingness" before
  the agent tries them; only interesting tasks enter the
  curriculum. The first principled bridge between *LLM-as-judge*
  and *auto-curriculum*.
- **Eureka** -- Ma, Liang, Wang et al., "Eureka: Human-Level Reward
  Design via Coding Large Language Models", arXiv:2310.12931
  (NVIDIA, 2023). LLM proposes the *reward function* in code; the
  RL agent trains against it; results feed back. Same shape as
  DiscoPOP / Sakana's evolutionaries (page 4) but for reward
  design specifically.
- **NEAT (NeuroEvolution of Augmenting Topologies)-flavoured
  environment mutation** -- straight evolutionary variation on
  environment parameters (terrain seed, obstacle density, action
  delay, sensor noise) with **MAP-Elites** (Multi-dimensional
  Archive of Phenotypic Elites; Mouret & Clune 2015) archives
  ([page 4](04-brain-inspired-ai.md#sakana-ai-evolution--collective-intelligence-as-a-working-brain-inspired-bet)
  context). The simplest baseline.

What to actually build, in priority order:

1. **PLR over the fixed curriculum** -- cheap, no environment
   generation needed, just re-weighting. The first auto-curriculum
   win.
2. **ACCEL on Crafter** -- editing tile-by-tile Crafter levels by
   evolutionary mutation, keeping the high-regret ones. Crafter's
   level generator is already procedural; ACCEL just edits the seed
   space.
3. **Enhanced POET on a small bipedal-walker variant** -- the
   canonical POET demonstrator; useful for validating the loop.
4. **OMNI / Eureka-style LLM-in-the-loop scoring** -- once the
   outer-loop LLM is wired up (next section), let it propose
   interestingness scores or reward shapes.

The evolutionary scaffold sits *above* the agent: each
"generation", a population of environments (or environment
parameters) is held in an **archive** -- ideally MAP-Elites style
indexed by *behavioural descriptors* (how the agent solves them).
At each step the outer loop **(a) selects** environments where
*learning potential* is high (PLR-style regret estimate, or the
"frontier" zone of POET), **(b) mutates** them (random edits,
LLM-proposed edits, or POET-style structural mutations), and
**(c) injects** the mutants back into the archive, dropping
solved-or-impossible ones. The agent runs on the resulting stream.

## The outer loop: LLM agents bootstrap the bootstrap

The reader's third request -- "the whole pipeline itself we will
bootstrap via LLM agents like you" -- is the meta-recipe: the
agent that runs the bootstrap arc is *itself* steered by an LLM-
driven outer loop. **Three published templates** to copy directly,
all from the Sakana programme on
[page 4](04-brain-inspired-ai.md#sakana-ai-evolution--collective-intelligence-as-a-working-brain-inspired-bet):

- **AI Scientist v2** (arXiv:2504.08066) -- end-to-end research
  loop: ideate -> code -> run -> analyse -> write up -> iterate.
  An ICLR-workshop paper from this loop scored 6.33, above the
  human acceptance threshold (with the documented caveat that the
  paper's substance was thin). The right template for **"each
  bootstrap iteration is a small research cycle"**.
- **DGM (Darwin-Godel Machine)** -- arXiv:2505.22954 -- an agent
  that **rewrites its own code**, maintains a lineage of agent
  variants, evaluates each by SWE-bench-style fitness, and selects.
  SWE-bench 20% -> 50% via self-modification. Direct template for
  **"the bootstrap's training loop / module-attaching code is
  itself evolved by the LLM"**.
- **ShinkaEvolve** -- arXiv:2509.19349 -- LLM-driven evolutionary
  program search with MAP-Elites novelty rejection, performance/
  novelty-weighted parent sampling, and a multi-armed bandit
  over which LLM to call. Sample-efficient (new state-of-the-art,
  SOTA, on circle
  packing in ~150 evaluations). Direct template for **the
  evolutionary outer loop over environments, modules, and
  hyperparameters**.

Adjacent templates worth knowing:

- **Voyager** -- Wang, Xie, Jiang et al., arXiv:2305.16291 (NVIDIA
  + Caltech, 2023). An LLM-driven Minecraft agent that maintains a
  *skill library* and proposes its next-task curriculum itself
  ("the auto-curriculum loop, GPT-4 inside"). The cleanest published
  example of *LLM-driven curriculum-self-generation in an
  embodied environment*.
- **The Ralph loop** (this repo's own
  [ralph-loop skill](../../../.claude/skills/ralph-loop/SKILL.md))
  -- iterate over a task list, mark items done, propose the next
  one. The minimum-viable LLM outer loop and the right thing to
  start with.

### How the LLM outer loop wraps the bootstrap

A concrete cycle, "Bootstrap iteration #N":

1. **State.** The current agent checkpoint, the auto-curriculum
   archive, the per-stage verification milestone results so far,
   and a short "lab notebook" markdown file (the iteration log).
2. **Diagnose.** The LLM (Claude or another agent) reads the lab
   notebook + the verification milestone deltas since iteration
   #N-1 and writes a one-page diagnosis: which milestones improved,
   which regressed, which are blocked, what the most likely cause
   is.
3. **Propose.** The LLM proposes **one** change. Allowed kinds:
   - *module add/swap*: e.g. attach a distributional critic head,
     switch from VICReg to SIGReg, replace the planner's MPC with
     CEM (cross-entropy method).
   - *hyperparam change*: e.g. raise gamma, lower the learning rate
     (LR), increase rollout horizon.
   - *environment generation seed*: a new mutation rule for the
     ACCEL archive, or a new OMNI-style interestingness prompt.
   - *bug fix*: a code edit driven by a failing milestone.
4. **Execute.** The proposal is a code diff. Apply it; run a short
   eval (one milestone per stage); update the lab notebook.
5. **Select.** If the change improves milestones (or improves
   novelty on the auto-curriculum archive), keep it; otherwise
   roll back. **This is the ShinkaEvolve / DGM evolutionary
   selector**, with LLM-proposed mutations and milestone-based
   fitness.

Two non-negotiable safeguards (the [AI CUDA Engineer cautionary
tale](04-brain-inspired-ai.md#the-cautionary-tale-ai-cuda-engineer-feb-2025)):

- **Harden the milestones before scaling the loop.** Each
  verification milestone (Pavlovian curve, dopamine-shift plot,
  linear-probe accuracy, ToM head, language-instructed task) must
  be defined *adversarially* -- the loop will Goodhart any softness.
  Use *symbolic, code-checked* success conditions wherever possible
  (e.g. for ARC-AGI rungs, the grid match is exact; for Crafter,
  the achievement is enumerable).
- **Lineage tracking and rollback.** Every iteration writes its
  diff and milestone deltas to a lineage log. If milestone-N starts
  silently degrading, you can git-bisect-style find the offending
  iteration.

### How this maps onto the agents the reader has access to

Given the user is already inside a Claude Code session and the repo
has a `ralph-loop` skill: the minimum-viable outer loop is **(a)
this conversation as the "AI Scientist v2 idea + write-up step",
(b) a `ralph-loop` invocation as the "run + check milestone +
iterate" step, and (c) a thvm-side `bootstrap-iteration.wls` script
that takes a stage number + a config and produces a milestone log**.
Sakana's ShinkaEvolve repo (Apache-2.0, on GitHub) is the right
external reference implementation to mine for the evolutionary
selection / MAP-Elites bookkeeping.

The bigger lift -- DGM-style code self-modification with sandboxed
verification -- is realistic once stages 1-3 are wired and the
milestones are tight. That is the milestone for the *outer* loop,
not just the agent.

## Curriculum and continual learning

The bootstrap pitch only works if **earlier capabilities persist**.
Three pragmatic moves:

1. **Keep older environments in the rotation.** Every training step,
   sample from a mixture: some episodes from the current stage,
   some from each prior stage. The mix can decay older stages
   slowly but never to zero.
2. **Replay buffer that spans stages.** Store transitions from every
   stage in a single buffer with stage tags; sample uniformly across
   tags. The hippocampal-replay analogue from
   [page 4](04-brain-inspired-ai.md#open-directions--where-novel-brain-inspired-ai-could-go),
   open direction #3.
3. **Two learning rates** (mentioned earlier): older heads + `phi`
   at a slow rate, newly-attached heads at a fast rate. Crude EWC
   (Elastic Weight Consolidation, Kirkpatrick et al. 2017,
   <https://www.pnas.org/doi/10.1073/pnas.1611835114>) if you find
   forgetting bites: penalise drift in older parameters weighted by
   their Fisher information.

Per-stage verification milestones (the lists above) double as the
*non-regression* test suite. If stage `N` breaks any milestone from
stages `<N`, stop and fix before adding `N+1`.

## Engineering notes (thvm-specific)

- **One growing config.** A single `agent.wl` script with a
  `stage` parameter that turns modules on. Adding a stage is a code
  change *plus* a config bump, never a full rewrite.
- **Profile each module separately first.** Standing project
  convention: time `phi`, `pi`, `V`, `f`, the MPC inner loop,
  before chaining them. `TProfile` will catch the worst leaks
  before they cost you a multi-hour run.
- **Parameters as `TTerms`, never `Normal`-roundtripped.** Standing
  thvm rule: keep all state on the backend; don't pull `Normal @
  p["W"]` into host loops.
- **Memory.** The stage-3 MPC planner is the first place where host
  RAM (random-access memory) bites at any scale. Cap the rollout
  horizon and batch; the bench-train memory note from the project
  auto-memory applies.

## What this is *not*

Honest scoping, because the framing is more ambitious than the
artefact:

- This is *not* a path to AGI (artificial general intelligence).
  The cumulative-not-replacing constraint is well-motivated but
  doesn't, by itself, solve any of the well-known hard problems
  (sample efficiency, abstraction, transfer, long-horizon planning,
  alignment).
- This is *not* a published research programme; it is a *learning
  curriculum* for someone who wants to internalise the breakthrough
  ladder by *building* it. The science is in [02](02-five-breakthroughs.md)
  - [04](04-brain-inspired-ai.md); the engineering exercise is
  here.
- The closest published precedents are open-ended-evolution lines
  (Schmidhuber's PowerPlay; the OpenAI Hide-and-Seek auto-curriculum;
  the Voyager Minecraft agent's skill library) and curriculum
  learning (Bengio et al. 2009). None of them follow Bennett's
  ladder explicitly. That gap is partly the point of the exercise.

## Pointers back

- The science of each stage: [02-five-breakthroughs.md](02-five-breakthroughs.md).
- JEPA / LeJEPA / LeWorldModel details: [03-jepa-and-world-models.md](03-jepa-and-world-models.md).
- NeuroAI / Sakana / Numenta / mentalizing AI: [04-brain-inspired-ai.md](04-brain-inspired-ai.md).
- Classical ML pillars / Tensor Logic / RLHF: [06-classical-ml-and-rlhf.md](06-classical-ml-and-rlhf.md).
- thvm mechanics: [../grad.md](../grad.md), [../wl.md](../wl.md),
  [../cpu.md](../cpu.md); `wl/Examples/`;
  `docs/plans/beautiful_mnist_parity.md`.
