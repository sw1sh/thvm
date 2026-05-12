# 04 - Brain-inspired AI: the NeuroAI programme and open directions

(AI = artificial intelligence; NeuroAI = neuroscience-informed AI.)

Page 3 was one bet: scale a latent-predictive world model. This page
is the other -- **NeuroAI**: the claim that the next leap comes from
reverse-engineering the *embodied, evolved competence* of animal
brains (Bennett's breakthroughs 1-4), not from scaling the language
layer. It surveys the recent literature you'd want to know, then
distils a set of open directions sized for experiments.

## The NeuroAI agenda (2022-2024)

- **Zador et al., "Catalyzing next-generation AI through NeuroAI"**,
  *Nature Communications* 14:1597 (2023) (preprint arXiv:2210.08340).
  A who's-who of computational neuroscience (Zador, Bengio, LeCun,
  Botvinick, Hassabis-adjacent, DiCarlo, Ganguli, Hawkins, Kording,
  Olshausen, Sejnowski, Tsao, ...). Thesis: the impressive things
  humans do that AI can't aren't language and Go -- they're the
  *sensorimotor* basics every animal has (a mouse navigating, a
  squirrel jumping branch to branch), built by ~500M years of
  evolution. Proposes the **embodied Turing test**: an AI "animal"
  that interacts with the physical world at the skill level of its
  biological counterpart. This is Bennett's "we built layer 5 first"
  diagnosis, turned into a research call.
- **Doerig et al., "The neuroconnectionist research programme"**,
  *Nature Reviews Neuroscience* 24:431-450 (2023). Frames
  "use biologically-inspired artificial neural networks (ANNs) as
  falsifiable models of brain computation" as a Lakatosian research
  programme: a hard core
  (distributed representations, learning from data, hierarchical
  processing) plus an adjustable belt of modelling choices. Useful
  for *epistemics* -- it tells you how to take "this ANN matches V4
  recordings" claims seriously without overclaiming.

## Predictive coding and the free energy principle

This is the theoretical bridge between LeCun-style predictive world
models (page 3) and neuroscience -- and the formal version of
Bennett's "cortex is a generative model".

- **Predictive coding (PC).** Cortex as a hierarchy that sends
  predictions top-down and prediction errors bottom-up; perception is
  the configuration that minimises total prediction error. Canonical
  circuit story: Bastos et al., "Canonical microcircuits for
  predictive coding", *Neuron* 2012 (and 2024-2025 extensions adding
  the thalamus and laminar detail, e.g. *Science Advances* 2024;
  *Neural Computation* 2025).
- **PC as a backpropagation ("backprop") alternative.** Millidge,
  Song, Salvatori, Lukasiewicz, Bogacz and colleagues:
  predictive-coding networks, using only *local* Hebbian-style updates
  plus an inference relaxation, can approximate exact backprop
  gradients on *arbitrary* computation graphs -- biologically
  plausible, parallelisable, neuromorphic-friendly. See Millidge et
  al., "Predictive Coding: Towards a Future of Deep Learning beyond
  Backpropagation?" (IJCAI 2022, arXiv:2202.09467) and the 2025
  machine-learning-audience tutorial "Introduction to Predictive
  Coding Networks for Machine Learning" (arXiv:2506.06332). Also in
  this neighbourhood: feedback alignment,
  equilibrium propagation, target propagation, forward-forward
  (Hinton 2022) -- the "how could the brain learn deep
  representations without backprop" cottage industry.
- **The free energy principle (FEP) / active inference (Friston).**
  The grand-unification claim: perception, learning, *and* action all
  minimise variational free energy (an upper bound on "surprise").
  Predictive coding is the perceptual half; **active inference** is
  the action half -- act so that your predictions come true (move so
  the world matches your model). Notable recent items: experimental
  support -- "Experimental validation of the free-energy principle
  with in vitro neural networks", *Nature Communications* 2023 (rat
  cortical cultures self-organise the way FEP predicts); and the
  AI-facing argument -- Pezzulo, Parr & Friston, "Generating meaning:
  active inference and the scope and limits of passive AI", *Trends in
  Cognitive Sciences* 2024 (large language models [LLMs] are *passive*
  predictors; meaning /
  understanding needs embodied, action-coupled inference -- same
  punchline as Bennett and Zador). FEP is grand, polarising, and
  hard to falsify; treat it as a *unifying lens* rather than a
  theorem you can build on directly.

## Hippocampus, transformers, and predictive maps

A genuinely clean AI<->brain correspondence -- worth knowing in
detail because it's a model of what a *good* analogy looks like.

- **The successor representation (SR)** (Dayan 1993; Stachenfeld,
  Botvinick & Gershman, "The hippocampus as a predictive map",
  *Nature Neuroscience* 2017). Represent each state by its *expected
  discounted future occupancy of every other state* -- a middle
  ground between model-free (cache only values) and model-based (cache
  the full transition model) reinforcement learning (RL). Prediction:
  hippocampal **place
  fields** are rows of the SR (they skew toward upcoming locations,
  warp around obstacles), and **grid cells** are a low-dimensional
  (eigen-)basis for it. Both borne out. Also used to read prefrontal
  / basal-ganglia contributions to goal-directed behaviour.
- **The Tolman-Eichenbaum Machine (TEM)** (Whittington et al.,
  *Cell* 2020): entorhinal cortex learns an abstract *structural*
  code (transition statistics -> grid-like cells); hippocampus binds
  it to sensory observations (-> place-like cells); spatial navigation
  and non-spatial relational memory are the *same* operation
  (generalising over graph structure).
- **TEM = a transformer** (Whittington, Warren & Behrens, "Relating
  transformers to models and neural representations of the
  hippocampal formation", ICLR 2022, arXiv:2112.04035): a transformer
  with the right (recurrent) positional encoding is mathematically
  TEM -- self-attention is the Hopfield-style associative memory TEM
  uses -- and such a transformer reproduces precisely-tuned place and
  grid cells. So "the hippocampal formation runs a transformer-like
  computation" is a *derived* claim, not a vibe.
- Practical upshot: SR is a cheap, biologically-motivated way to get
  some of model-based RL's flexibility (re-plan when the *reward*
  changes without re-learning dynamics) at near-model-free cost --
  and it's a small, tractable thvm experiment.

## Dopamine, distributional RL, and meta-RL (the two-way street)

- **Distributional RL <-> dopamine.** Dabney, Kurth-Nelson, Uchida,
  Starkweather, Hassabis, Munos, Botvinick, "A distributional code
  for value in dopamine-based reinforcement learning", *Nature* 577
  (2020). Distributional RL agents (e.g. the quantile-regression deep
  Q-network, QR-DQN) learn the *whole distribution* of returns, with
  different "channels" tuned to different optimism levels (quantiles).
  The paper predicted -- and found in mouse ventral-tegmental-area
  (VTA) recordings -- that dopamine neurons are
  similarly diverse: different cells have different "reversal points",
  collectively coding a *distribution* over future reward, not a
  scalar. AI algorithm -> neuroscience prediction -> confirmation.
  (Follow-up: "Distributional reinforcement learning in prefrontal
  cortex", *Nature Neuroscience* 2023.)
- **Prefrontal cortex as meta-RL.** Wang, Kurth-Nelson, Kumaran,
  Tirumala, Soyer, Leibo, Hassabis, Botvinick, "Prefrontal cortex as
  a meta-reinforcement learning system", *Nature Neuroscience* 21
  (2018). Slow dopaminergic RL trains the *weights* of the recurrent
  PFC network; the trained recurrent *dynamics* then implement a
  second, faster RL algorithm in *activations* -- the PFC is a
  meta-learned learner, and in-context adaptation = the network's
  recurrent state. (Broader synthesis: Botvinick et al., "Deep
  reinforcement learning and its neuroscientific implications",
  *Neuron* 2020. Newer: meta-RL stories for orbitofrontal cortex,
  *Nature Neuroscience* 2023; multi-timescale RL in the brain, 2025;
  meta-RL accounts of anterior cingulate, *PLOS Comp Biol* 2025.)
- **Learning the learning rule.** Oh et al., "Discovering
  state-of-the-art reinforcement learning algorithms", *Nature* 2025
  ("DiscoRL"): meta-learn the *update rule itself* (a neural net, not
  an equation) over a population of agents across many environments;
  the discovered rule beats hand-designed RL on Atari/ProcGen and
  invents prediction "semantics" distinct from value functions.
  Resonant with the meta-RL theme -- and with the idea that evolution
  "discovered" the brain's learning rules.

## Whole-brain functional models (the Spaun line)

Eliasmith's group: **Spaun** (Eliasmith et al., "A large-scale model
of the functioning brain", *Science* 338, 2012) -- ~2.5M spiking
neurons organised into brain-region-like subsystems (visual cortex,
PFC, basal ganglia, thalamus, motor cortex) that, with *fixed*
parameters, performs 8 different cognitive tasks (digit recognition,
working memory, serial recall, pattern completion/induction,
reasoning) and writes answers with a simulated arm. Built on the
**Nengo** simulator + the **Neural Engineering Framework** (compute
with populations of spiking neurons) + the **Semantic Pointer
Architecture (SPA)** (compositional, partly-semantic neural vectors
for symbol-like cognition). Less central to the 2023-2026 frontier,
but
it's the canonical "the whole pipeline, end to end, in spikes" model
-- and the SPA's compressed composable vectors are an interesting
contrast to JEPA's learned latents and to LLM token embeddings.

## Cortical columns as the unit of intelligence: Numenta, HTM, and the Thousand Brains Project

*(Added in response to a reader question, 2026-05-12 -- "the Numenta
and related neuro-inspired ML?")*

A 20-year, somewhat-outside-the-mainstream research programme that
deserves its own section: Jeff Hawkins's company **Numenta** (founded
2005) and the line of theory that starts with the 2004 popular book
*On Intelligence* and lands at *A Thousand Brains: A New Theory of
Intelligence* (Basic Books, 2021). One unifying conjecture, very
different from the LeCun / Bennett / Friston lines: **every cortical
column runs the same algorithm, and each column independently learns
a complete model of objects and concepts.** Intelligence is not a
hierarchical pipeline; it is *thousands of voting models*.

The line of work, with the names:

- **HTM (Hierarchical Temporal Memory)** -- the original Numenta
  framework. Two pillars: **sparse distributed representations (SDRs)**
  (binary vectors with very low active fraction -- ~2% sparsity --
  used as the universal currency of cortical computation; their
  *high-dimensional binary* structure gives robustness, capacity,
  union-as-superposition, and overlap-as-similarity), and **sequence
  memory** in a multi-layer column model with Hebbian / activity-gated
  synaptic learning. Hawkins, Ahmad & Cui, "A theory of how columns in
  the neocortex enable learning the structure of the world",
  *Frontiers in Neural Circuits* 2017.
- **Cortical grid cells (2018-2019).** The conceptual move that
  motivates the Thousand Brains story. Hawkins, Lewis, Klukas, Purdy
  & Ahmad, "A Framework for Intelligence and Cortical Function Based
  on Grid Cells in the Neocortex", *Frontiers in Neural Circuits* 12
  (2019); and Lewis, Purdy, Ahmad & Hawkins, "Locations in the
  Neocortex: A Theory of Sensorimotor Object Recognition Using
  Cortical Grid Cells", *Frontiers in Neural Circuits* 13 (2019). The
  bet: the grid-cell circuitry that the *entorhinal cortex* uses to
  navigate physical space (page 4's TEM / hippocampus story) is the
  *same* circuitry that *every* cortical column uses to attach
  features to *locations in a learned reference frame* of an object,
  a concept, even an abstract idea. So every column has its own
  little map.
- **The Thousand Brains Theory (2021).** Synthesis: ~150,000
  cortical columns in a human neocortex, each one a complete
  sensorimotor learner that builds models in its own reference
  frames; columns share answers via long-range cortico-cortical
  connections and **vote** on the identity of what's being perceived.
  No central executive, no clean hierarchy -- a parallel democracy of
  fragmentary models. The book is popular; the technical version is
  Hawkins, Ahmad, Lewis, "A Framework for Intelligence and Cortical
  Function...", *Frontiers* 2019 (above).
- **NuPIC and the sparsity-for-LLMs pivot.** Numenta's commercial
  side: their **NuPIC** (Numenta Platform for Intelligent Computing)
  applies cortically-motivated sparsity, pruning, and quantisation to
  transformer / LLM inference, claiming 10-100x throughput gains on
  standard CPUs (the "Sparsity Enables 100x Performance Acceleration
  in Deep Learning Networks" whitepaper, 2023). This is the
  practically-impactful Numenta output of the past few years:
  *transformer inference, accelerated by neuroscience-motivated
  sparsity*. Less radical than HTM, but useful and shippable.
- **The Thousand Brains Project (TBP, Dec 2024 / Jan 2025).** Numenta
  spun the foundational research mission into an independent
  nonprofit (the **Thousand Brains Project, TBP**), with a first
  system called
  **Monty** -- a software implementation built from repeating
  **learning modules** modelled on cortical columns, each a
  semi-independent sensorimotor object-modeller that votes with its
  neighbours. Leadholm, Lewis, Clay, Lee, Grewal, Purdy, Long,
  Stocker, Hoffmann, Ahmad & Hawkins, "The Thousand Brains Project: A
  New Paradigm for Sensorimotor Intelligence", arXiv:2412.18354
  (Dec 2024). Where HTM emphasised
  *low-level* mechanisms (SDRs, sequence memory, biologically
  plausible learning rules), TBP emphasises *high-level* ones
  (sensorimotor learning, the cortical column as a universal
  modelling unit, reference frames as the structure of knowledge).

**How it lands against Bennett.** Numenta's claim cuts across the
breakthrough framing in a specific way: their model of the cortex is
*much more uniform* than Bennett's (no aPFC/gPFC distinction, no
breakthrough-3/4 split), and *much less hierarchical* (no top-down
generative model, no Helmholtz machine; instead, lots of little
column-local generative models voting). It is also more *sensorimotor*
than the standard predictive-coding story -- every column does
*active* sensing, integrating self-movement with sensation via grid
cells. If you find the Friston / LeCun / Bennett picture too
hierarchical and not embodied enough, Numenta is the loudest
alternative; if you find it too uniform / not enough room for the
prefrontal-cortex meta-control machinery, Bennett is.

**How seriously to take it.** Mixed. The theoretical contributions are
genuine and the grid-cells-everywhere prediction is testable (and
partially supported by recent fMRI evidence of grid-like coding in
non-spatial cortical regions). The implementation track record is
modest -- HTM never displaced deep nets on any standard benchmark,
and the Thousand Brains Project / Monty is an early-stage
demonstration, not a state-of-the-art (SOTA) system. But the
*architectural insight* --
**a repeating, sensorimotor, reference-frame-anchored column as the
fundamental unit, with voting instead of a stack** -- is exactly the
kind of structural prior the brain-inspired-AI literature keeps
gesturing toward and rarely commits to. Worth watching.

### Related neuro-inspired programmes (one short paragraph each)

The Numenta line has a small constellation of cousins worth knowing.

- **Sparse coding** -- Olshausen & Field, "Emergence of simple-cell
  receptive field properties by learning a sparse code for natural
  images", *Nature* 381:607-609 (1996). Train a generative model to
  reconstruct images with a *sparsity* penalty on its hidden code;
  the learned basis functions look exactly like V1 simple-cell
  receptive fields. The ancestor of the "the cortex computes with
  sparse codes" tradition that Numenta's SDRs descend from. Also
  closely related to compressed sensing and to L1-regularised dictionary
  learning.
- **Hyperdimensional computing (HDC) / Vector Symbolic Architectures
  (VSAs)** -- Pentti Kanerva, "Hyperdimensional computing: an
  introduction to computing in distributed representation with
  high-dimensional random vectors", *Cognitive Computation* 1:139-159
  (2009); review: Kleyko, Rachkovskij, Osipov & Rahimi, "Vector
  Symbolic Architectures as a Computing Framework for Emerging
  Hardware", *Proc. IEEE* 110(10) (2022, arXiv:2106.05268). Compute
  with very-high-dimensional (often binary) vectors using a small
  algebra: **bundling** (superposition), **binding** (variable-value
  pairing), **permutation** (sequencing); concepts are represented
  by their atomic vectors, structures by algebraic combinations.
  Cousin to Eliasmith's Semantic Pointer Architecture (the SPA *is*
  a VSA), to Plate's Holographic Reduced Representations, and to
  Smolensky's tensor-product representations. The VSA community
  is a natural partner for neuromorphic hardware (Intel's
  **Loihi** / **Loihi 2** chips were explicitly co-designed with HDC
  workloads in mind) and shows up in low-power on-device learning
  and one-shot recognition tasks. Conceptually it gives you a
  *symbolic*-flavoured algebra over *distributed* representations --
  a bridge between Domingos's symbolists and connectionists (page 6).
- **Neuromorphic hardware.** IBM's **TrueNorth** (Merolla et al.,
  *Science* 2014, 1M spiking neurons on chip), Intel's **Loihi** and
  **Loihi 2** (Davies et al., *IEEE Micro* 2018; 2021 update),
  Manchester's **SpiNNaker** / **SpiNNaker 2** (Furber et al.), and
  Heidelberg's **BrainScaleS** are the main players. Spiking neural
  networks (SNNs) get their own benchmark suite (NeuroBench, 2024).
  This is a substrate story -- if learning rules really are local
  and spikes really are sparse, the right silicon is asynchronous,
  event-driven, and ~100-1000x more energy-efficient than a GPU. The
  problem space is small but real: edge devices, sensor-near
  inference, robotics, brain-computer interfaces.
- **Capsule networks** -- Sabour, Frosst & Hinton, "Dynamic Routing
  Between Capsules", NeurIPS 2017; arXiv:1710.09829. Geoff Hinton's
  attempt to put structure-like "objects with a pose" into a neural
  network: replace scalar activations with small *vectors* (capsules)
  that explicitly encode "presence + pose parameters", and route
  agreements between capsules across layers. The shared spirit with
  Numenta's cortical columns is real -- every capsule is a small
  parts-and-poses model -- but the lineage runs through Hinton's GLOM
  paper ("How to represent part-whole hierarchies in a neural
  network", arXiv:2102.12627, 2021) and capsule research has stayed
  niche relative to standard transformers.
- **HTM / Numenta in modern deep learning.** Numenta's own line is the
  loudest example, but the *general* idea -- repeating, locally-
  modelling units that vote -- shows up in mixture-of-experts (a
  voting layer of specialists), graph-neural-network message passing
  (locally updating nodes that "agree"), and the recent "modular
  neural networks" literature. If you squint, transformer attention
  heads are voting columns over a shared latent.

The throughline: **structured priors -- columns, reference frames,
sparse codes, capsules, hyperdimensional vectors -- are the part of
"brain-inspired AI" that the deep-learning mainstream has *not*
absorbed yet.** That is where the next surprise probably lives.

## Open directions -- where novel brain-inspired AI could go

Synthesising the above with Bennett's framing. None of these is
solved; each is a research bet, and several are thvm-sized at toy
scale.

1. **Stack the layers, don't pick one.** Frontier AI has a strong
   layer 5 (language), a weak layer 3 (world model), a thin layer 2
   (grounded RL), a brittle layer 4 (theory of mind). The
   LeCun/V-JEPA-2-AC/LeWorldModel agenda is "build a real layer 3".
   The complement: glue a layer-2 actor-critic *critic* onto a
   layer-3 latent world model onto, eventually, a layer-5 language
   conditioner -- a small *modular* agent, not a monolith. The
   "configurator" in LeCun's blueprint is the missing executive that
   binds them; almost nobody has built a real one.

2. **Local learning rules at scale.** Backprop works but isn't how
   brains learn. Predictive coding can approximate backprop with
   local updates; forward-forward, equilibrium prop, feedback
   alignment are other attempts. Open question: can any of them match
   backprop on a non-toy task while staying local, online, and
   continual? A negative result is informative; a positive one is
   huge (continual learning, neuromorphic hardware).

3. **Replay and offline model-improvement.** The hippocampus replays
   real and hypothetical trajectories during rest; Dyna and
   experience-replay are the AI echo, but biological replay is
   *prioritised*, *generative* (it replays things that never
   happened), and serves both *learning* and *planning*. A
   replay-buffer that *generates* useful counterfactual experience
   from a learned world model (LeWorldModel-style), prioritised by
   surprise / value-of-information, is underexplored.

4. **Successor-representation-style predictive state.** SR sits
   between model-free and model-based and matches hippocampal/grid
   data. Deep SR (and "successor features") exist but are niche;
   combining an SR-like predictive state with a JEPA latent (predict
   *future occupancy in latent space*) is a clean idea that, as far
   as I know, nobody has pushed hard.

5. **Intrinsic motivation as a first-class cost.** Bennett's "surprise
   becomes rewarding" (breakthrough 2) and LeCun's "intrinsic cost
   module" are the same thing. Curiosity / empowerment / free-energy-
   reduction as the *primary* driver (no extrinsic reward) in a world
   model that's good enough to make the intrinsic signal meaningful
   -- the pieces (LeWorldModel + an intrinsic cost) now exist at small
   scale.

6. **Distributional / risk-sensitive value everywhere.** Dopamine is
   distributional; most agents still cache scalar values. Carrying a
   distribution through the *critic of a world-model-based planner*
   (not just a deep Q-network) is natural and underused -- and it's a
   small change to make.

7. **The mentalizing layer.** Honest assessment: this is the least
   developed and the place neuroscience offers the least concrete
   guidance. Recursive/opponent world models, ToMnet-style theory-of-
   mind nets, meta-RL self-models -- all exist, none is convincing.
   High risk, high reward, probably premature until layer 3 is solid.

8. **Evaluate like a biologist.** The embodied Turing test (Zador et
   al.): stop ranking models only on benchmark accuracy; rank them on
   whether an agent behaves like the animal -- the *failure modes*,
   the sample efficiency, the transfer, the lesion effects. For thvm,
   the toy version: compare an actor-critic / world-model agent's
   learning curve and error pattern against the qualitative
   signatures (blocking, overshadowing, devaluation insensitivity of
   habits, vicarious-trial-and-error at choice points) the
   neuroscience literature reports.

## Next

[05-toy-problems-with-thvm.md](05-toy-problems-with-thvm.md) turns the
buildable subset of these into concrete experiments.

References: full links in [references.md](references.md).
