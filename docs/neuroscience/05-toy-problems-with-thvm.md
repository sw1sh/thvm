# 05 - Toy problems with thvm

The bridge from the reading (pages 1-4) to the codebase. None of this
is implemented yet; this page is a *menu of experiments*, ordered to
match Bennett's breakthroughs and sized to what thvm can do today.

## What thvm gives you

- **Reverse-mode autodiff** (automatic differentiation) -- dup-like
  grad cells, chain-rule adjoints, materialization. See
  [../grad.md](../grad.md), [../grad-roadmap.md](../grad-roadmap.md).
  `TGradMany` for multi-output gradients; `TProfile` to catch
  allocation leaks.
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
- **CPU and Metal backends**, just-in-time (JIT) compilation, autotune
  -- [../cpu.md](../cpu.md), [../metal.md](../metal.md). MNIST-scale
  is comfortable; anything bigger, watch host RAM (random-access
  memory) (see the memory note in the repo's auto-memory).

Reuse the existing NN primitives -- don't redefine `TGlorot`/`TZeros`
inline in a `.wls`; add to `NN.wl` if something is missing (this is a
standing project convention).

## Breakthrough 1 - Steering: a 2-D valence-arousal controller

The simplest possible "brain": a creature in a 1-D or 2-D world with
one chemical-gradient sensor. Hard-wire a controller that maps sensor
reading -> (turn rate, speed) through a tiny `TLinear` + `TReLU`, with
behaviour modulated by two scalars (valence = is the gradient
increasing?, arousal = how steep?). Then add **Pavlovian
conditioning**: a neutral cue channel that, via a Hebbian update
(outer product of cue activity and valence, applied by hand -- no
autodiff needed), comes to predict the gradient and pre-trigger
approach.

- **Point:** intelligence as categorise-and-steer; the origin of
  affect; associative learning as the first plasticity. Mostly a
  *visualisation* exercise -- plot the trajectory, plot the weight
  growing.
- **thvm:** trivial; a few `TLinear`s and a manual Hebbian step. Good
  warm-up for the WL surface.

## Breakthrough 2 - Reinforcing: actor-critic / TD-learning on a gridworld

The flagship buildable experiment, and the one with the clearest
neuroscience payoff.

1. **Tabular TD(0)** (temporal-difference learning) **and tabular
   actor-critic** on a small gridworld (a few obstacles, one goal).
   Critic learns V(s); the TD error delta = r + gamma V(s') - V(s) is
   your "dopamine". Actor adjusts action preferences by delta. No
   autodiff -- just arrays. Plot the value map filling in; plot delta
   over time and watch it migrate from reward-time to cue-time (the
   dopamine-shift result).
2. **Function-approximated** version: replace the tables with tiny
   `TLinear`/`TReLU` nets, train with `TAdam` through `TGradMany`.
   Now it's a (tiny) deep actor-critic / A2C (advantage actor-critic).
3. **Add curiosity:** an intrinsic reward proportional to a
   prediction-error / novelty signal (Bennett's "surprise becomes
   reinforcing"; intrinsic-curiosity-module [ICM] style, after Pathak
   et al.). Watch it explore a no-extrinsic-reward maze.
4. **Distributional twist:** make the critic output a few quantiles
   instead of a scalar mean (quantile-regression style). Cheap change;
   connects to the Dabney-et-al dopamine result.
5. **Successor representation (SR):** learn M(s, s') = expected
   discounted future occupancy, derive values as M @ r. Then *change
   the reward* and show it re-plans without re-learning M (the SR's
   selling point), and visualise the place-cell-like rows / grid-like
   eigenvectors of M.

- **Point:** RL (reinforcement learning) = the basal ganglia; dopamine
  = TD error; actor-critic structure; curiosity; distributional value;
  SR as the model-free/model-based middle ground.
- **thvm:** (1) is pure-array, (2)-(5) exercise autodiff + `TAdam`
  lightly. All MNIST-scale or smaller.

## Breakthrough 3 - Simulating: generative / predictive world models

### 3a. A Helmholtz / wake-sleep autoencoder on toy images

Two nets: a *recognition* net (image -> latent) and a *generation*
net (latent -> image), trained wake-sleep style (or just train a
variational autoencoder, VAE -- the modern, autodiff-friendly
version) on MNIST or a synthetic shapes dataset. Then *sample from the
generator with no input* --
that's "imagination". Show that recognition and generation are
approximate inverses.

- **Point:** Bennett's breakthrough-3 mechanism, literally:
  perception as inference in a generative model; hallucination as the
  generative model unclamped.
- **thvm:** straightforward -- it's an autoencoder; reuse the MNIST
  data path from `wl/Examples/mlp-mnist` / `beautiful-mnist`.

### 3b. JEPA-in-miniature

(JEPA = Joint Embedding Predictive Architecture; see page 3.) Mask a
patch of a small image (or a few frames of a toy video). Two encoders
(`TConv` + `TLinear`; the target encoder an exponential-moving-average
[EMA] copy), one predictor (`TLinear` / a multi-layer perceptron)
mapping context embedding -> target embedding; loss = distance in
embedding space; add an anti-collapse regulariser -- the easy one is
**VICReg-style** (variance-invariance-covariance regularisation)
variance + covariance terms, the principled one is **SIGReg**
(Sketched Isotropic Gaussian Regularization, from LeJEPA: random 1-D
projections, match each to a standard Gaussian). Probe the learned
embedding with a linear classifier.

- **Point:** "predict a learned latent of the missing part, not the
  pixels"; collapse as the central failure mode; SIGReg as the fix
  (page 3).
- **thvm:** moderate -- the architecture is small, but you'll want to
  implement the EMA-encoder update and the regulariser carefully.
  Watch for collapse (embeddings -> constant).

### 3c. LeWorldModel-in-miniature: latent world model + planning

The integrative one. On a tiny environment (gridworld, or a
pong/cart-pole-like toy):

- **Encoder** `phi`: observation -> low-dim latent `z`.
- **Predictor** `f`: `(z, action) -> z_next` -- the world model.
- Train end-to-end on `(o, a, o')` triples with **two losses**:
  next-latent prediction `||f(phi(o), a) - sg(phi(o'))||` (or without
  stop-grad if you use SIGReg) + a Gaussian-latent regulariser
  (SIGReg). One trade-off hyperparameter -- the LeWorldModel recipe.
- **Plan** with model-predictive control (MPC): from the current `z`,
  roll `f` forward over candidate action sequences, score each by a
  cost
  (distance to a goal latent + the breakthrough-2 critic), pick the
  first action of the best sequence, replan next step. Optimise the
  action sequence by gradient descent *through* `f` -- this is exactly
  what thvm's autodiff is for.
- **Diagnostics:** does the latent recover the true state (probe it)?
  Does prediction error spike on physically-impossible transitions
  you inject?

- **Point:** the page-3 takeaway in one experiment -- small,
  end-to-end, two losses, planning by differentiating through a
  learned latent dynamics model; the complete (toy) LeCun
  perception -> world-model -> actor loop, with the breakthrough-2
  critic supplying the cost.
- **thvm:** the most ambitious item here but still small (LeWorldModel
  proper is ~15M params on one GPU). This is the natural "capstone"
  and a real stress test of the autodiff path through a recurrent
  rollout (`TProfile` will matter).

## Breakthrough 4 - Mentalizing: (stretch goal, low priority)

Two agents in a gridworld; agent A keeps a model of agent B's policy
and plans against it (opponent modelling); a ToMnet-style network (a
theory-of-mind network, after Rabinowitz et al.) predicts B's next
action from a few observed episodes. Or: a meta-RL (meta-reinforcement
learning) setup (RL^2-style -- "RL squared": the recurrent state
itself learns to do RL) where the recurrent state adapts to a new maze
within an episode -- the "PFC (prefrontal cortex) as meta-RL" idea.

- **Point:** recursive world models; meta-learning; theory of mind.
- **thvm:** doable but only worth it once breakthroughs 2-3 are
  solid; the neuroscience here is the most speculative (page 2,
  breakthrough 4) and what AI (artificial intelligence) offers here is
  the thinnest (page 4).

## Breakthrough 5 - Speaking

thvm already has a `gpt2` example -- that *is* the layer-5 experiment.
The brain-inspired angle isn't "train a bigger language model (LM)";
it's "condition a layer-3 world model on language" (a configurator
that reads a text goal and sets the planner's cost) -- which is
really a breakthrough-3
experiment with a text input, and is far down the line.

## Suggested order

1. Breakthrough 2, step 1 (tabular actor-critic gridworld) -- learn
   the WL surface, no autodiff, fast feedback, real neuroscience
   payoff (the dopamine-shift plot).
2. Breakthrough 2, steps 2-3 (deep A2C + curiosity) -- first real use
   of `TGradMany` + `TAdam` in an RL loop.
3. Breakthrough 3a (Helmholtz/VAE on MNIST) -- the generative-model
   half of "simulating".
4. Breakthrough 3b (JEPA-in-miniature) -- the latent-predictive half;
   meet collapse and SIGReg.
5. Breakthrough 3c (LeWorldModel-in-miniature + planning) -- the
   capstone; integrates 2 + 3.
6. Optional: 2.4/2.5 (distributional critic, SR), then 4.

Profile each piece individually before chaining them (a standing
project convention -- time each layer/op before an end-to-end run).

## Pointers back

- The science behind each: [02-five-breakthroughs.md](02-five-breakthroughs.md).
- JEPA / LeJEPA / LeWorldModel details:
  [03-jepa-and-world-models.md](03-jepa-and-world-models.md).
- Predictive coding / SR / meta-RL background and the open directions
  these experiments probe: [04-brain-inspired-ai.md](04-brain-inspired-ai.md).
- thvm mechanics: [../grad.md](../grad.md), [../wl.md](../wl.md),
  [../cpu.md](../cpu.md); `wl/Examples/`;
  `docs/plans/beautiful_mnist_parity.md`.
