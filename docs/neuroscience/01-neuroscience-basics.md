# 01 - Neuroscience basics

Just enough vocabulary to read the rest of these docs. If you know
the difference between Hebbian and gradient learning, and you can
place the basal ganglia, neocortex, hippocampus and prefrontal
cortex on a rough map, skip to
[02-five-breakthroughs.md](02-five-breakthroughs.md).

## The neuron

A neuron is a cell that integrates inputs and emits *spikes* (action
potentials). Three parts matter computationally:

- **Dendrites** -- the input tree. Synapses from upstream neurons
  land here. Dendrites are not passive wires; branches do local
  nonlinear integration, which is why "a cortical neuron is roughly a
  small multi-layer network" is a defensible slogan.
- **Soma** -- the cell body. Sums dendritic input; if the membrane
  voltage crosses threshold, it fires.
- **Axon** -- the output wire. One axon, but it branches to thousands
  of targets. Spikes propagate down it and trigger neurotransmitter
  release at the far end.

A spike is roughly all-or-none and ~1 ms long. Information is carried
by *which* neurons fire and *when* / *how often* (rate codes, timing
codes, population codes). Most AI (artificial intelligence) "neurons"
are rate abstractions: a scalar activation standing in for a firing
rate, no spikes.

There are ~86 billion neurons in a human brain, ~16 billion of them
in the neocortex, plus a comparable number of glia (support cells --
increasingly thought to do real computation too).

## The synapse and plasticity

A **synapse** is the junction where one neuron's axon meets another's
dendrite. The presynaptic spike releases neurotransmitter; the
postsynaptic side has receptors; the net effect is excitatory (push
toward firing) or inhibitory (push away). The **synaptic weight** is
how strong that push is -- and it changes with experience. That is
*learning*.

Forms of plasticity you will see referenced:

- **Hebbian learning** -- "cells that fire together wire together."
  If pre and post are active together, strengthen the synapse. This
  is local (uses only the two cells' activity), unsupervised, and is
  the substrate for *auto-association* / pattern completion: a partial
  cue re-activates the whole stored pattern (the smell of coffee
  evokes the cafe). Hopfield networks are the clean math model.
- **Spike-timing-dependent plasticity (STDP)** -- Hebbian with a
  temporal asymmetry: pre-before-post strengthens (causal), post-
  before-pre weakens. A biologically plausible learning rule.
- **Three-factor / neuromodulated plasticity** -- Hebbian *gated by a
  global signal* such as dopamine: "strengthen the synapses that were
  recently active, but only when the dopamine burst says it paid
  off." This is the bridge from local Hebbian rules to
  reward-driven learning -- and it is the biological reading of an
  actor-critic update (see breakthrough 2).
- **Backpropagation** is *not* a known biological mechanism: it needs
  symmetric weights and a separate error pathway. A whole subfield
  (predictive coding, feedback alignment, equilibrium propagation)
  asks whether the brain approximates backprop with local rules; see
  [04-brain-inspired-ai.md](04-brain-inspired-ai.md).

## Neuromodulators

Beyond fast point-to-point transmission, the brain has slow,
broadcast chemical signals that retune large regions:

- **Dopamine** -- the star of these docs. Released by midbrain nuclei
  (the ventral tegmental area [VTA] and substantia nigra pars
  compacta) onto the basal ganglia and frontal cortex. The dominant
  computational theory: dopamine encodes a **reward prediction error**
  (RPE) -- the temporal-difference (TD) error of reinforcement
  learning (RL) (Schultz, Dayan, Montague 1997). A burst means
  "better than expected", a dip "worse than expected", and the error
  gates plasticity. (Older folk story: dopamine = pleasure. The RPE
  account replaced it.)
- **Serotonin** -- roughly opponent / longer-horizon; linked to
  satiation, mood, behavioural inhibition, average-reward and patience
  signals. In Bennett's framing, the original "I am satisfied, stop
  searching" signal in early bilaterians.
- **Acetylcholine, noradrenaline** -- attention, arousal, the
  signal-to-noise / learning-rate knobs; "expected vs unexpected
  uncertainty" in some models.

## A pocket map of the brain

Front to back, inside to outside. Names you need:

- **Brainstem / hindbrain** -- breathing, heart rate, reflexes,
  arousal. Evolutionarily oldest. Houses the dopamine and serotonin
  source nuclei.
- **Cerebellum** -- supervised motor learning and timing; a huge,
  highly regular feedforward circuit. (Often modelled as a perceptron
  / liquid-state machine; mostly outside our story.)
- **Thalamus** -- the central relay. Almost all sensory input passes
  through it on the way to the cortex, and cortex talks back to it
  heavily. Increasingly modelled as part of a cortico-thalamic loop
  doing attention / gain control / predictive routing.
- **Basal ganglia** -- a set of subcortical nuclei (striatum,
  globus pallidus, subthalamic nucleus, substantia nigra) forming
  loops *cortex -> basal ganglia -> thalamus -> cortex*. Function:
  action selection and reinforcement learning. The leading theory:
  parallel basal-ganglia circuits implement an **actor-critic**
  (Sutton-Barto) -- one circuit (the "actor") learns which actions to
  release, another (the "critic") learns to predict future reward and
  drive the dopamine signal. This is breakthrough 2.
- **Hippocampus** -- a curled strip of older "allocortex" in the
  medial temporal lobe. Episodic memory (the time you...), spatial
  navigation (**place cells**, and **grid cells** in the neighbouring
  entorhinal cortex), and *replay* -- offline re-running of past and
  hypothetical trajectories during rest and sleep. Modelled as a
  fast-binding associative memory / a predictive map (successor
  representation) / a "Tolman-Eichenbaum Machine"; recent work shows
  a transformer with the right positional code reproduces its
  representations. This feeds breakthroughs 2 and 3.
- **Neocortex** -- the wrinkled outer sheet, six layers, the bulk of
  human brain volume, primate-and-mammal-defining at this size. It is
  built from a *repeating microcircuit* (the cortical column /
  canonical microcircuit) tiled across regions -- visual, auditory,
  motor, association -- which is why the same patch of cortex can
  learn very different things. Function in Bennett's frame: a
  **generative model of the world** that supports *simulation*
  (imagining sensory consequences of actions before acting). The
  Helmholtz-machine reading: cortex runs a generative model
  top-down and a recognition model bottom-up, trained to agree. This
  is breakthrough 3, and the direct ancestor of JEPA-style world
  models (page 3).
- **Prefrontal cortex (PFC)** -- the front of the neocortex.
  Bennett (following Wise, Passingham and others) splits it:
  - **Agranular PFC (aPFC)** -- the part mammals have. Lacks the
    granular input layer 4. Models the animal's *own* intentions and
    upcoming actions; selects *what to simulate* given the current
    goal; anchors and steers the cortical simulation. Roughly: the
    controller of the imagination engine.
  - **Granular PFC (gPFC)** -- the part primates added. Builds models
    *of the aPFC's models* -- models of minds, including other
    agents' and one's own future self. **Theory of mind**,
    metacognition, multi-step planning. This is breakthrough 4.
  PFC is also where the "prefrontal cortex as a meta-reinforcement
  learning system" idea lives (Wang et al. 2018): slow dopaminergic
  RL trains the PFC recurrent network, and the trained dynamics then
  *are* a fast, in-context learner.

## How "brain learning" differs from typical deep learning

Keep these contrasts in mind when you read an analogy:

| Brain | Mainstream deep learning |
|---|---|
| Local, mostly Hebbian / three-factor rules | Global backprop |
| Online, never i.i.d. (independent and identically distributed), catastrophic-forgetting-prone yet somehow not | Mini-batches, shuffled, offline epochs |
| Sparse spikes, ~20 W, asynchronous | Dense floats, kW-scale, synchronous |
| Reward / surprise / intrinsic signals, no labels | Mostly supervised or next-token labels |
| Architecture is largely innate (evolved priors); learning fills it in | Architecture hand-designed, weights all learned |
| Recurrent, with constant top-down feedback | Mostly feedforward at inference |
| One model, many tasks, lifelong | Often one model per task (changing fast) |

The interesting AI work tends to be where someone takes one row of
the right column and moves it toward the left: local learning rules,
continual learning, intrinsic motivation, innate inductive biases,
predictive feedback.

## Next

[02-five-breakthroughs.md](02-five-breakthroughs.md) -- the
evolutionary spine.

References for this page: Kandel et al., *Principles of Neural
Science*; Dayan & Abbott, *Theoretical Neuroscience*; Schultz, Dayan
& Montague (1997), "A neural substrate of prediction and reward",
*Science*. Full links in [references.md](references.md).
