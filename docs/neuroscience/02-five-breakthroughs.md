# 02 - The five breakthroughs (Bennett's spine)

This page follows the pedagogical structure of Max Bennett, *A Brief
History of Intelligence: Evolution, AI, and the Five Breakthroughs
That Made Our Brains* (2023). Bennett is a tech founder, not an
academic neuroscientist; the book's value (and the consensus of
reviewers) is *distillation* -- it stitches a large literature into
one clean story, and crucially it tells that story with the AI
(artificial intelligence) analogy made explicit at every step. That
is exactly the framing we want for thvm: each breakthrough is a
computational problem with a known algorithmic shape.

Health warning, repeated up front: breakthroughs **1-3 are
well-grounded**, **4 is more speculative**, and **5 is a hard,
contested problem dressed up as a tidy chapter**. Reviewers (Adnan
Masood; the *Holodoxa* and *scyy.fi* notes; Bennett's own caveats)
all flag this. Treat the AI mappings as productive hypotheses with
varying track records, not settled fact.

Throughout, "X mya" = million years ago; dates are order-of-magnitude.

---

## Breakthrough 1 - Steering (the first bilaterians, ~550-600 mya)

**The problem.** A blob in the ocean needs to move *toward* food and
*away* from harm. That requires a body axis (front/back, the
"bilaterian" body plan), sensors at the front, and a controller that
turns sensor readings into a steering command.

**The biology.** Tiny nerve nets condense into a head ganglion. The
controller is two-dimensional: **valence** (is this good or bad?) and
**arousal** (how much, how urgent?). Bennett's claim: this is the
origin of *affect / emotion* -- valence-arousal is still the
low-dimensional core of emotion models today. Early neuromodulators
do the steering: a "something good is near, keep going / turn toward
it" signal (proto-dopamine) and an "I'm satisfied, stop searching"
signal (proto-serotonin). And associative (Pavlovian) learning
appears: a neutral cue that reliably precedes food acquires valence
-- classical conditioning.

**AI analogy.** This is the *substrate*, not yet a rich algorithm:
**Hebbian / associative learning** (a cue gets bound to a value),
plus the idea that behaviour is the output of a small set of
continuous drive variables. The deep point Bennett wants you to take:
intelligence starts as *categorisation into good/bad* and *steering*,
not as reasoning. Modern echo: every reinforcement-learning (RL)
agent ultimately collapses the world onto a scalar value; "valence"
is that, minus the bookkeeping.

**Evidence:** solid. Comparative neuroanatomy of nerve nets and
early ganglia, conserved neuromodulator roles across phyla, and a
century of conditioning experiments.

---

## Breakthrough 2 - Reinforcing (the first vertebrates, ~520-540 mya)

**The problem.** Pavlovian steering only links a *fixed* response to
a cue. The leap: learn an *arbitrary new action sequence* because it
eventually pays off -- trial-and-error / instrumental learning. To do
that you must solve **temporal credit assignment** (the reward came
late; which earlier action gets the credit?) and **exploration vs
exploitation**.

**The biology.** Vertebrates add the **basal ganglia** -- the
cortex/striatum -> pallidum -> thalamus -> cortex loops. Dopamine
gets repurposed: from "good thing nearby" to a **reward prediction
error** (RPE) -- fires for *unexpected* reward, goes silent for
*expected* reward, dips below baseline for *omitted* reward (the fish
that learns disappointment and relief). The standard model: the basal
ganglia is literally a **temporal-difference (TD) learning
actor-critic**:

- a **critic** circuit learns a value function V(state) and computes
  the TD error delta = r + gamma V(s') - V(s); that delta *is* the
  dopamine signal;
- an **actor** circuit (the striatum) adjusts action propensities --
  three-factor plasticity: strengthen the cortico-striatal synapses
  that were active just before a positive delta.

Two side effects Bennett highlights: **surprise itself becomes
rewarding** (the origin of curiosity / intrinsic motivation), and
spatial **place cells** (hippocampus) give the agent a state space to
do RL over.

**AI analogy.** Reinforcement learning, full stop -- Sutton & Barto.
TD(lambda), actor-critic, Q-learning, the deadly triad, eligibility
traces (a biological-looking credit-assignment trick), intrinsic
motivation / curiosity (Schmidhuber; Pathak et al.). The
dopamine = TD-error correspondence is the field's flagship success
story: TD theory *predicted* the dopamine response (Schultz, Dayan &
Montague 1997), and the later refinement -- *distributional* RL --
predicted that different dopamine neurons would carry different
"optimism levels" (quantiles), confirmed in mice (Dabney et al.,
*Nature* 2020). That two-way traffic is the model for what
"brain-inspired AI" should look like.

**Evidence:** solid, with active debate at the edges (does dopamine
also signal salience / cause-credit, not just RPE? is the actor-critic
mapping too neat? are there multiple dopamine subsystems with
different jobs? -- yes, probably).

**thvm hook:** this is the most directly buildable breakthrough --
tabular and then function-approximated actor-critic / TD-learning on
a gridworld; see [05-toy-problems-with-thvm.md](05-toy-problems-with-thvm.md).

---

## Breakthrough 3 - Simulating (the first mammals, ~150-250 mya)

**The problem.** Model-free RL is slow (you must actually try things)
and dangerous (some mistakes kill you). The leap: build an internal
*model of the world* and run **vicarious trial-and-error** -- imagine
the consequences of an action before doing it. This buys planning,
counterfactual learning, episodic memory, and "what if".

**The biology.** Mammals add the **neocortex** -- six layers, built
from a repeating **cortical column / canonical microcircuit** tiled
everywhere. Bennett's reading (drawing on predictive-coding theory,
Mountcastle's uniform-cortex hypothesis, Hawkins, Friston): the
neocortex is a **hierarchical generative model**. Top-down
connections render a *prediction* of incoming sensation; bottom-up
connections carry the *prediction error*; perception is the
generative model's best explanation of the input, and *imagination*
is the same generative model run without sensory clamping (dreams,
daydreams, planning rollouts). The hippocampus does **replay** --
re-running real and hypothetical trajectories offline -- which is how
the cortical model gets trained on rare/important experience and how
plans get evaluated. Crucially, the **agranular prefrontal cortex
(aPFC)** is the *controller* of the simulator: it holds the current
goal and selects *what* to simulate -- without it, simulation has no
steering wheel.

**AI analogy.** Two big clusters:

- **Generative models and the Helmholtz machine.** The Helmholtz
  machine (Dayan, Hinton, Neal & Zemel 1995) is the canonical
  "cortex algorithm": a *generative* network (top-down: hidden ->
  data) and a *recognition* network (bottom-up: data -> hidden),
  trained by the **wake-sleep** algorithm to be approximate inverses
  -- recognition and generation as mirror processes. This is the
  conceptual ancestor of variational autoencoders, and (looser) of
  diffusion models and autoregressive generative models. Bennett uses
  it as the through-line: *perception is inference in a generative
  model* (Helmholtz's original 1860s "unconscious inference"),
  hallucination is the generative model running unconstrained, and
  predictive coding is the message-passing scheme that implements it.
- **Model-based RL and planning.** Building a transition/reward model
  and planning in it: Dyna (Sutton 1991), Monte-Carlo tree search,
  and the modern **world model** line -- World Models (Ha &
  Schmidhuber 2018), MuZero, Dreamer/DreamerV3 (imagine rollouts in a
  learned latent space and improve the policy on them), and the whole
  JEPA (Joint Embedding Predictive Architecture) / V-JEPA 2 /
  LeWorldModel family on page 3. "Vicarious trial-and-error" in a rat
  at a maze junction (hippocampal place cells sweep ahead down each
  arm) *is* a tree-search rollout.

### Aside: the Helmholtz machine, in one picture (and where sleep fits)

*(Added in response to a reader question, 2026-05-11 -- "isn't the
Helmholtz machine like an encoder-decoder embedded in a single network
depending on the direction? what about sleep and the balance between
recognition and simulation?" Yes, and yes; here is the longer
answer.)*

**Yes -- it is an encoder-decoder, but folded into one hierarchy of
latent variables rather than two separate stacks.** A Helmholtz
machine has *one* layered structure of stochastic units `x` (data),
`h1`, `h2`, ... (hidden), and *two* sets of weights over it:

- **recognition weights** -- run *bottom-up*, `x -> h1 -> h2 -> ...`,
  computing an approximate posterior `q(h | x)` (the "encoder"); this
  is *perception / recognition*.
- **generative weights** -- run *top-down*, `... -> h2 -> h1 -> x`,
  the model `p(h)` and `p(x | h)` (the "decoder"); this is
  *generation / simulation / imagination*.

So unlike a vanilla autoencoder (encoder stack, then a *separate*
decoder stack), here the two directions are *inverses over the same
ladder of latents* -- run the network one way and it recognises, run
it the other way and it dreams. Bennett's "recognition and generation
are mirror processes in the cortex" is exactly this picture (top-down
cortical connections = the generative model, bottom-up = recognition).
Predictive coding is the same idea implemented *continuously* instead
of as alternating passes: the top-down generative prediction and the
bottom-up prediction error meet in every layer all the time.

**Where sleep fits: it is how the recognition model gets trained.**
The Helmholtz machine is trained by the **wake-sleep** algorithm
(Hinton, Dayan, Frey & Neal, *Science* 1995), two alternating phases:

- **Wake.** Show real data `x`. Run the *recognition* weights
  bottom-up to sample hidden states `h ~ q(h|x)`. Then nudge the
  *generative* weights to make `(x, h)` more likely. Slogan: *awake,
  you perceive the world and tune your model-of-the-world to explain
  what you perceived.*
- **Sleep.** No data. Run the *generative* weights top-down from a
  prior sample to **dream** a fantasy `x~` together with the hidden
  states that produced it. Then nudge the *recognition* weights to
  better infer those hidden states from `x~`. Slogan: *asleep, you
  generate fantasies and train your recogniser to invert your own
  generator.*

Why does training the recogniser *need* sleep / self-generated data?
Because for real data you never see the true hidden causes -- only the
recogniser's own guesses, which is circular. For *dreamed* data you do
have them: you generated it, so you know exactly which `h` made which
`x~`. Sleep is the only regime where the recognition network has
ground-truth targets.

**That is the "balance".** Wake trains *generation* using
*recognition's* current guesses; sleep trains *recognition* using
*generation's* current dreams; the two bootstrap toward being mutual
inverses, and if they drift apart, offline/sleep time is when they
re-align. The biological resonance was the point of the name -- but it
is an *analogy*, not an established mechanism. The real neuroscience of
sleep -- hippocampal **replay** during slow-wave sleep, the role of
REM (rapid-eye-movement) sleep, "generative replay" theories of memory
consolidation (and the complementary-learning-systems framework:
hippocampus as a fast store that re-trains a slow cortical generative
model offline), even Crick & Mitchison's old "reverse learning /
unlearning" theory of REM sleep -- is *related in spirit* (offline
self-generated experience that regularises a cortical model) but is
not literally wake-sleep. Modern descendants make the trade-off
cleaner: a **variational autoencoder** (VAE; Kingma & Welling 2013)
*is* a Helmholtz machine trained by one objective (the evidence lower
bound, ELBO, via the reparameterisation trick) instead of two
alternating phases; **reweighted wake-sleep** (Bornschein & Bengio
2014) revived the original. The JEPA twist on page 3 keeps the "two
encoders + a predictor between them" skeleton but drops the demand to
*generate the data* at all -- it predicts a learned *latent* of the
missing/future part instead, which is why it does not need a sleep
phase.

**Evidence:** the *function* (mental simulation, episodic memory,
replay, planning) is solid -- behavioural and recording evidence
across mammals. The *mechanism* (cortex = one generic generative
algorithm, à la predictive coding) is a strong, well-motivated
hypothesis but still contested; cortex is more heterogeneous than the
slogan, and "the cortical algorithm" has not been pinned down.

**thvm hook:** a tiny Helmholtz / wake-sleep autoencoder on toy
images; or a JEPA-in-miniature (predict the embedding of a masked
patch); or a Dreamer-style latent world model + planner on a small
gridworld. Page 5.

---

## Breakthrough 4 - Mentalizing (the first primates, ~30-65 mya)

**The problem.** If you can simulate the world, can you simulate
*another mind* -- what it knows, wants, will do? And your *own* future
self as just another agent to be modelled and bargained with? That is
**theory of mind**, and it unlocks tactical deception, imitation
learning ("do what *they* did"), teaching, reputation, and social
strategy ("politics", in Bennett's word).

**The biology.** Primates add **granular prefrontal cortex (gPFC)** --
the part that has a layer 4. Bennett's claim: gPFC models *the aPFC's
models* -- it builds models of intentions/minds, recursively. It is
active in self-referential and theory-of-mind tasks. Mirror neurons
get a supporting role (simulating others' actions on your own motor
apparatus). Planning gets a horizon: not just "what happens next" but
"what will future-me want, and how do I commit present-me to it".
Evolved, the story goes, in fruit-eating primates -- foraging maps,
long juvenile periods, big social groups.

**AI analogy.** Recursive / nested world models; **opponent
modelling** and theory-of-mind agents in multi-agent RL (e.g.
DeepMind's "Machine Theory of Mind", ToMnet); **meta-learning** and
**meta-RL** ("learn to learn"; Wang et al. 2018's "prefrontal cortex
as a meta-RL system" lives partly here -- the network's recurrent
state implements a fast learner); imitation / inverse RL; hierarchical
RL with sub-goals; and -- a stretch Bennett makes -- the
self-modelling that lets an agent reason about its own future
behaviour. Honestly, this is where AI has the *least* to offer back:
large-language-model (LLM) agents have shallow, brittle theory of
mind, and multi-agent "social" RL is a niche. For a fuller survey
of the AI side of mentalizing (Bayesian theory of mind, ToMnet,
Cicero, imitation learning and IRL, opponent modelling, the LLM-ToM
benchmark debate, generative agents, RSA, action anticipation), see
[04-brain-inspired-ai.md](04-brain-inspired-ai.md#mentalizing-ai-theory-of-mind-imitation-and-intent-inference-breakthrough-4).

**Evidence:** weaker. The gPFC-as-mind-modeller story is a reasonable
synthesis but far less nailed down than the basal-ganglia or
hippocampal stories; theory-of-mind localisation is genuinely
debated; "primates added gPFC and that's where minds-modelling-minds
happens" papers over a lot.

---

## Breakthrough 5 - Speaking (humans, ~100k-500k years ago)

**The problem.** Other animals can imitate an action they *saw*.
Humans can learn from an action they only *imagined being told
about* -- transmit the *contents of one mind's simulation* into
another mind, and accumulate that across generations (cumulative
culture). That, not raw brainpower, is Bennett's answer to "why us".

**The biology.** No new structure -- humans have a scaled-up
chimpanzee brain with the same gross wiring. What changed, the story
goes, is *developmental instincts*: human infants do proto-conversation
(~4 months), joint attention (~9 months), pointing-to-share,
question-asking -- the social scaffolding language needs. There is no
"language organ"; language recruits a distributed circuit. (This is a
genuinely contested area -- Chomskyan universal grammar vs. usage-based
emergentism vs. Bennett's "it's about social instincts plus
generative simulation" -- and the chapter glides over the fights.)

**AI analogy.** Large language models, obviously: generative
sequence models, compositional / recursive structure, in-context
learning, the surprising amount of "world knowledge" that falls out
of next-token prediction at scale. Bennett's framing makes the
limitation legible too: an LLM is the *speaking* layer with weak
versions of *simulating* (shallow, inconsistent world models) and
*mentalizing* (brittle theory of mind) underneath it, and none of the
*reinforcing* layer's grounded, embodied trial-and-error. That
diagnosis -- "we built layer 5 first and skimped on 2-4" -- is the
book's punchline, and it lines up neatly with LeCun's critique on
page 3 and the NeuroAI agenda on page 4.

**Evidence:** the comparative-development observations are real; the
overall account of *why* language → cumulative culture → us is a
plausible synthesis among several; the neat "five layers" packaging
of human uniqueness is more pedagogy than settled science.

---

## The cumulative picture

Each breakthrough is a *layer*, not a replacement -- you still have
all of them:

| # | Breakthrough | ~When | Key structure | Computational problem | AI analogy |
|---|---|---|---|---|---|
| 1 | Steering | 550-600 mya | head ganglion; neuromodulators | categorise good/bad, approach/avoid | valence-arousal; Hebbian/associative learning |
| 2 | Reinforcing | 520-540 mya | basal ganglia; dopamine | temporal credit assignment; explore/exploit | TD learning; actor-critic; curiosity / intrinsic motivation |
| 3 | Simulating | 150-250 mya | neocortex (columns); hippocampus; aPFC | model-based planning; counterfactuals; episodic memory | generative models; Helmholtz machine / wake-sleep; predictive coding; world models / Dreamer / JEPA |
| 4 | Mentalizing | 30-65 mya | granular PFC | model other minds & future self | recursive/opponent world models; meta-learning / meta-RL; theory-of-mind agents |
| 5 | Speaking | 100-500 kya | scaled cortex + social instincts | transmit simulations; cumulative culture | large language models; compositional generative sequence models |

And the cross-cutting observation that motivates this whole doc set:
**today's frontier AI built layer 5 first.** LLMs are extraordinary
*speakers* sitting on a thin *simulator* with almost no *reinforcer*
(grounded, embodied trial-and-error) and a brittle *mentalizer*. The
next two pages are the two main research bets on fixing that:
[03-jepa-and-world-models.md](03-jepa-and-world-models.md) (LeCun's:
build a real layer-3 world model, predicting in latent space) and
[04-brain-inspired-ai.md](04-brain-inspired-ai.md) (the NeuroAI bet:
reverse-engineer layers 1-4 -- the embodied, evolved competence --
rather than scaling layer 5).

---

## Where to read more

- Max Bennett, *A Brief History of Intelligence*, Mariner Books,
  2023. Author site: <https://www.abriefhistoryofintelligence.com/>.
- Pedagogical summaries used for this page: Adnan Masood, "The Five
  Breakthroughs of Intelligence" (Medium review); the *Holodoxa*
  (Stetson) review; the *scyy.fi* reading notes; Bennett's own talks
  and the *Brain Inspired* podcast episode. Links in
  [references.md](references.md).
- Primary anchors: Sutton & Barto, *Reinforcement Learning: An
  Introduction* (2nd ed., 2018); Dayan, Hinton, Neal & Zemel (1995),
  "The Helmholtz machine", *Neural Computation*; Schultz, Dayan &
  Montague (1997), *Science*; Dabney et al. (2020), "A distributional
  code for value in dopamine-based reinforcement learning", *Nature*;
  Wang et al. (2018), "Prefrontal cortex as a meta-reinforcement
  learning system", *Nature Neuroscience*.
