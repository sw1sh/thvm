# 08 - Reinforcement learning: from optimal control to world models

*A detailed introduction to reinforcement learning (RL) and its
history, from the 1950s optimal-control roots through the deep-RL
revolution to the modern world-model and generalist-agent papers the
thvm experiment arc (155-167) builds on. Page 2's "breakthrough 2 --
Reinforcing" tells the story of RL in the brain; this page tells the
story of RL as a field. They are the same idea seen from two sides.*

## 0. What reinforcement learning is

Reinforcement learning is the problem of an agent learning to act, by
trial and error, to maximise a cumulative numerical reward. It is
usually formalised as a **Markov decision process** (MDP): a set of
states `S`, a set of actions `A`, a transition function
`P(s' | s, a)`, a reward function `R(s, a)`, and a discount factor
`gamma` in `[0, 1)`. The agent follows a **policy** `pi(a | s)`; its
goal is to maximise the expected **return**, the discounted sum of
future rewards `G_t = sum_k gamma^k r_{t+k}`.

Two functions organise almost everything that follows. The **value
function** `V^pi(s)` is the expected return from state `s` under
policy `pi`; the **action-value function** `Q^pi(s, a)` is the
expected return from taking action `a` in `s` and then following
`pi`. Both satisfy a recursive consistency condition -- the **Bellman
equation** -- which says a state's value equals the immediate reward
plus the discounted value of where you land. Every classical RL
algorithm is, at heart, a way to make an estimate satisfy that
equation.

RL sits between two parents: **optimal control** (how to act well in
a known dynamical system) and **animal learning psychology** (how
animals learn from reward). Its history is the story of those two
threads braiding together, then meeting deep learning.

## 1. Roots: optimal control and the law of effect (1900s-1980s)

The psychology thread is older. Edward Thorndike's **law of effect**
(1911) stated that actions followed by satisfaction are more likely
to recur -- trial-and-error learning as a mechanism, not a metaphor.
B. F. Skinner's operant conditioning (1930s-1950s) made it
quantitative. This is the behavioural substrate page 2 traces into
the vertebrate basal ganglia.

The control thread gave RL its mathematics. Richard Bellman's
*Dynamic Programming* (1957) introduced the Bellman equation and
**value iteration** -- compute the optimal value function for a known
MDP by repeatedly applying the Bellman update until it converges.
Ronald Howard's **policy iteration** (1960) alternated policy
evaluation and policy improvement. These solve an MDP *exactly* when
the model `P` and `R` are known and the state space is small enough
to enumerate -- the same regime the thvm arc's MCTS-over-the-real-
graph controller (experiments 155-159) operates in.

Arthur Samuel's checkers program (1959) is the first recognisable RL
system: it learned an evaluation function by playing itself and
adjusting weights toward the value of later positions -- temporal-
difference learning, two decades before the name. The synthesis of
the control and psychology threads into one field is largely the
work of **Richard Sutton and Andrew Barto**, whose textbook
*Reinforcement Learning: An Introduction* (1st edition 1998, 2nd
2018) is the field's canonical reference.

## 2. The classical era: temporal-difference learning (1988-2000)

The defining algorithmic idea of RL is **temporal-difference (TD)
learning**, introduced in Sutton's 1988 paper "Learning to predict by
the methods of temporal differences". TD learning updates a value
estimate toward a *bootstrapped* target -- the immediate reward plus
the current estimate of the next state's value -- rather than waiting
for the true return. The **TD error**

    delta = r + gamma * V(s') - V(s)

is the gap between prediction and bootstrapped reality. Learning is
"move the estimate to shrink delta". `TD(lambda)` generalises this
with **eligibility traces**, blending one-step and Monte-Carlo
targets along a trajectory (thvm experiment 19 reproduces the
`lambda` knob; experiment 09 is the one-step special case).

TD learning has a celebrated neuroscience echo. Schultz, Dayan and
Montague ("A neural substrate of prediction and reward", *Science*
1997) showed that the phasic firing of midbrain **dopamine** neurons
matches the TD error: dopamine spikes at unexpected reward, then,
after learning, migrates to the predictive cue. The brain appears to
run a TD algorithm. This is the bridge between this page and page 2.

Two control algorithms completed the classical toolkit. **Q-learning**
(Chris Watkins, 1989 thesis; Watkins and Dayan 1992) learns `Q(s, a)`
*off-policy* -- it learns the optimal action-value while behaving
under any exploratory policy, by bootstrapping from `max_a Q(s', a)`.
**SARSA** (Rummery and Niranjan 1994) is its on-policy sibling,
bootstrapping from the action actually taken. The thvm arc's
tabular Q-learner (experiments 12-27) is direct Watkins Q-learning.

The other branch is **policy-gradient** methods, which optimise a
parameterised policy directly rather than deriving it from a value
function. Ronald Williams's **REINFORCE** (1992) is the seminal
algorithm: scale the gradient of `log pi(a | s)` by the observed
return. The **policy gradient theorem** (Sutton, McAllester, Singh
and Mansour 2000) made this rigorous and gave the **actor-critic**
architecture its modern form -- an actor `pi` improved by policy
gradient, a critic `V` supplying a low-variance baseline. thvm
experiment 02 is a textbook actor-critic.

The era's hard lesson came from **TD-Gammon** (Gerald Tesauro 1995),
a backgammon program that reached near-expert play with a neural-
network value function trained by `TD(lambda)` self-play. It was a
triumph, but it did not generalise -- attempts to repeat it elsewhere
failed, because combining TD bootstrapping, function approximation,
and off-policy learning can diverge. Sutton later named this the
**deadly triad**. Stable deep RL would take fifteen more years.

## 3. The deep-RL revolution: DQN and Atari (2013-2016)

The breakthrough was the **Deep Q-Network** (DQN), from Mnih et al.
at DeepMind -- a 2013 workshop paper ("Playing Atari with Deep
Reinforcement Learning") and the 2015 *Nature* paper ("Human-level
control through deep reinforcement learning"). DQN learned to play
49 Atari 2600 games from raw pixels with one architecture and one set
of hyperparameters, reaching human-level on many.

DQN tamed the deadly triad with two ideas. **Experience replay**
stores transitions in a buffer and samples them in random minibatches,
breaking the temporal correlations that destabilise gradient descent
and reusing data. The **target network** is a periodically-frozen
copy of the Q-network used to compute the bootstrap target, so the
target does not move under the learner's feet. thvm experiment 24
reproduces both; experiment 25 adds **hindsight experience replay**
(Andrychowicz et al. 2017), which relabels failed trajectories with
the goal that *was* achieved -- turning every episode into a success
for some goal.

The **Arcade Learning Environment** (Bellemare et al. 2013) -- the
57-game Atari benchmark -- became the field's proving ground for the
next decade, and it is the benchmark the modern multi-game world
models of section 9 are still measured on.

DeepMind's other 2016 result, **AlphaGo** (Silver et al., *Nature*
2016), defeated Lee Sedol at Go by combining a policy network, a
value network, supervised learning from human games, policy-gradient
self-play, and **Monte Carlo tree search** (MCTS). MCTS -- build a
search tree by simulation, guided by a value/prior, back up the
results -- is the planning algorithm the thvm controller arc adopts
directly (experiments 151-167 all plan with MCTS).

## 4. Policy optimization matures (2015-2018)

DQN handles discrete actions; continuous control (robotics, physics)
needed policy methods that were stable at scale. A burst of
algorithms delivered that:

- **TRPO** (Trust Region Policy Optimization; Schulman et al. 2015,
  arXiv:1502.05477) constrained each policy update to a trust region
  in distribution space, guaranteeing monotone improvement.
- **A3C / A2C** (Asynchronous Advantage Actor-Critic; Mnih et al.
  2016, arXiv:1602.01783) ran many actors in parallel, stabilising
  the gradient without a replay buffer.
- **DDPG** (Lillicrap et al. 2015, arXiv:1509.02971) carried DQN's
  replay-and-target-network recipe into continuous actions with a
  deterministic actor.
- **PPO** (Proximal Policy Optimization; Schulman et al. 2017,
  arXiv:1707.06347) replaced TRPO's hard constraint with a clipped
  surrogate objective. Simple, robust, and still the default
  policy-gradient algorithm -- and, a decade later, the workhorse of
  reinforcement learning from human feedback (section 11).
- **SAC** (Soft Actor-Critic; Haarnoja et al. 2018, arXiv:1801.01290)
  added a maximum-entropy objective, rewarding the policy for staying
  stochastic, which improved exploration and sample efficiency.

In parallel, **distributional RL** reframed the value function: learn
the whole *distribution* of returns, not just its mean. **C51**
(Bellemare, Dabney and Munos 2017, arXiv:1707.06887) predicted a
categorical return distribution; **QR-DQN** used quantile regression.
**Rainbow** (Hessel et al. 2017, arXiv:1710.02298) combined six DQN
improvements -- double Q-learning, prioritised replay, dueling
networks, multi-step returns, distributional RL, noisy exploration --
into one agent, and the ablation showed every component carried
weight. thvm experiments 16-24 walk this same dueling / prioritised-
replay / distributional ladder in miniature.

## 5. Planning and self-play: AlphaZero and MuZero (2017-2020)

The model-based thread -- *plan* with a model of the world rather
than only react -- runs back to Sutton's **Dyna** (1990), which
interleaved real experience with simulated experience from a learned
model. It reached its peak in DeepMind's self-play line.

**AlphaGo Zero** (Silver et al., *Nature* 2017) removed the human
games entirely: starting from random play, MCTS self-play generated
its own training targets. **AlphaZero** (2017, arXiv:1712.01815;
*Science* 2018) generalised the same algorithm to chess and shogi
with no game-specific knowledge beyond the rules. The core loop --
MCTS produces an improved policy, the network is trained toward it,
the stronger network improves the next search -- is **expert
iteration**, and it is exactly the loop thvm experiment 156 tests on
the quasimetric (finding, instructively, that it is a no-op when the
value targets are already exact).

**MuZero** (Schrittwieser et al. 2019, arXiv:1911.08265; *Nature*
2020) took the decisive step: it learned the model too. MuZero plans
with MCTS over a **learned latent dynamics model** trained only to
predict reward, value and policy -- never to reconstruct the
observation. The model is whatever is useful for planning, nothing
more. This "value-equivalent" model is the conceptual ancestor of the
latent world models in section 8, and of thvm experiments 166-167.

## 6. The sample-efficiency problem (2019-2023)

Early deep RL was extravagantly data-hungry -- DQN needed hundreds of
millions of frames. Two research lines pushed back.

The **scaling** line chased peak performance with more data and
memory. **R2D2** (Kapturowski et al., ICLR 2019) added a recurrent
state to distributed replay; **Agent57** (Badia et al. 2020,
arXiv:2003.13350) was the first agent above the human baseline on all
57 Atari games, using a family of policies from exploratory to
exploitative.

The **sample-efficiency** line asked how well an agent could do with
*little* data -- the **Atari 100k** regime (100,000 frames, about two
hours of play). This is where representation learning entered RL.
**CURL** (Laskin et al. 2020) added a contrastive self-supervised
loss; **SPR** (Self-Predictive Representations; Schwarzer et al.
2020, arXiv:2007.05929) added a self-supervised multi-step latent-
prediction loss alongside the RL objective. **EfficientZero** (Ye et
al. 2021, arXiv:2111.00210) brought MuZero-style planning to Atari
100k and first crossed human-level there. **BBF** (Schwarzer et al.
2023, arXiv:2305.19452) pushed the regime further with network
scaling and periodic resets.

The lasting lesson: a good **representation** is the lever for
sample-efficient RL, and a self-supervised auxiliary loss is a
reliable way to get one. thvm experiments 28-33 explore exactly this
boundary, finding (experiment 33) that the regulariser, not the
predictor, is the load-bearing part of the self-supervised loss at
small scale.

## 7. World models: learning to plan in imagination (2018-2025)

A **world model** is a learned model of an environment's dynamics
that an agent can roll forward "in imagination" -- planning or
training on simulated experience instead of, or alongside, real
experience.

Ha and Schmidhuber's **"World Models"** (2018, arXiv:1803.10122) made
the idea concrete and memorable: a variational autoencoder compressed
each frame to a latent vector, a recurrent network predicted the next
latent, and a tiny controller was trained *entirely inside* the
model's dream. The agent could learn to drive without touching the
real environment.

DeepMind's **Dreamer** line industrialised it. **PlaNet** (Hafner et
al. 2018, arXiv:1811.04551) planned with a learned latent dynamics
model. **Dreamer** (2019, arXiv:1912.01603) learned the policy by
backpropagating value gradients through imagined latent rollouts.
**DreamerV2** (2020, arXiv:2010.02193) was the first agent to reach
human-level Atari purely by learning behaviour inside a world model,
and it introduced the trick that matters most for the thvm arc: a
**discrete, categorical latent** -- the world state is encoded as a
set of categorical variables, and the dynamics model predicts the
*distribution* over the next categories. **DreamerV3** (2023,
arXiv:2301.04104; *Nature* 2025) made one configuration work across
more than 150 tasks, from Atari to Minecraft, with fixed
hyperparameters.

A parallel line replaced the recurrent model with a Transformer.
**IRIS** (Micheli et al. 2022, arXiv:2209.00588) tokenised frames
with a discrete autoencoder and modelled the dynamics as autoregressive
next-token prediction over those tokens. **DIAMOND** (Alonso et al.
2024, arXiv:2405.12399) made the world model a *diffusion* model,
generating the next frame in pixel space.

The recurring design choice across this line -- DreamerV2's
categoricals, IRIS's discrete tokens -- is a **discrete latent**: the
world model predicts *which code* comes next, a classification, not a
continuous regression. thvm experiments 166 and 167 are a direct test
of that choice (continuous latent vs categorical latent), and
experiment 160's warp-residual model is a token-space world model in
the IRIS spirit.

## 8. RL as sequence modeling (2021-)

A different reframing arrived with the Transformer. The **Decision
Transformer** (Chen et al. 2021, arXiv:2106.01345) and the
**Trajectory Transformer** (Janner et al. 2021, arXiv:2106.02039)
proposed treating RL as a *supervised sequence-modelling* problem:
feed a Transformer the sequence of returns, states and actions, and
train it to predict the next action. Conditioned on a high desired
return, the model produces actions consistent with achieving it. No
Bellman backup, no TD error -- offline RL as autoregressive
prediction.

This recast made **offline RL** -- learning a policy purely from a
fixed dataset, with no environment interaction -- a first-class
setting, and it connected RL directly to the large-language-model
toolchain. It is also the conceptual hinge to the next section: if a
policy is just a sequence model, one sequence model can carry many
tasks.

## 9. Generalist agents and universal latents (2022-2026)

The modern frontier, and the lineage the current thvm arc is testing,
is the **generalist** agent: one model, one set of weights, many
environments.

- **Multi-Game Decision Transformer** (Lee et al. 2022,
  arXiv:2205.15241) trained a single return-conditioned Transformer
  across 41 Atari games at once, reaching 126% of human performance
  and showing that the single multi-game model transferred to new
  games far better than a from-scratch baseline.
- **Gato** (Reed et al. 2022, arXiv:2205.06175) pushed generality to
  the limit -- one Transformer, one weight set, across Atari, image
  captioning, dialogue, and real-robot control, every modality
  serialised into one token stream.
- **JOWA** (Jointly-Optimized World-Action model, arXiv:2410.00564)
  is a single offline model-based agent across Atari games, built on
  a vector-quantised discrete latent, jointly optimising the world-
  model and action losses so the shared latent encodes action-
  relevant structure.
- **Mixture-of-World Models** (arXiv:2602.01270, 2026) trains one
  agent over 26 Atari games with **modular latent dynamics** -- a
  shared Transformer backbone plus task-conditioned expert modules,
  with gradient-based task clustering -- matching a 26-model ensemble
  at half the parameters. It is the multi-task answer to the
  interference problem: let related tasks share, let dissimilar ones
  specialise.

These papers converge on a small set of tricks: a **discrete latent**
(so prediction is classification and cannot drift between states);
**joint optimization** of the world-model and value/action losses
over one shared representation; **balanced multi-task batches**; and,
for interference, **modular or mixture dynamics**. The thvm
experiments 165-167 are a deliberate, controlled re-derivation of
this line on small enumerable environments -- 165 builds the shared
encoder, 166 tests a continuous latent world model, 167 the
categorical one.

## 10. Goal-conditioned RL and quasimetrics

One sub-thread deserves its own section, because the thvm controller
arc lives in it. **Goal-conditioned RL** learns a policy or value
conditioned on a goal, `pi(a | s, g)` and `V(s, g)`, so one agent
reaches *any* goal. **Universal Value Function Approximators** (UVFA;
Schaul et al., ICML 2015) introduced the function-approximation form;
**hindsight experience replay** (2017) made it sample-efficient.

The value function of a goal-conditioned, shortest-path problem has
structure: the distance from `s` to `g`. It obeys the triangle
inequality, and -- when actions are not reversible -- it is
**asymmetric**: `d(s, g)` need not equal `d(g, s)`. That object is a
**quasimetric**. A line of work makes the value function an explicit
learned quasimetric: "On the Learning and Learnability of
Quasimetrics" (Wang and Isola 2022), **Quasimetric RL** ("Optimal
Goal-Reaching RL via Quasimetric Learning", Wang et al. 2023,
arXiv:2304.01203), and Metric Residual Networks. **Contrastive RL**
(Eysenbach et al. 2022, arXiv:2206.07568) reaches a related place
from a contrastive-learning direction.

This is precisely the thvm controller. Its encoder produces a latent
in which `d(s, g) = sym + asym` -- a symmetric Euclidean term plus an
asymmetric, sign-restricted residual -- and experiments 158-159
showed that training that quasimetric with a **ranking loss** on
successor order, rather than regressing absolute distance, is what
crosses the depth wall. The asymmetric term is what lets it represent
irreversibility -- the defining difficulty of the Sokoban
environments, and the subject of the OneWayGrid environment built for
the multi-environment curriculum.

## 11. Reinforcement learning meets large language models (2017-2026)

RL's most visible modern application is not games but language.
**Deep RL from human preferences** (Christiano et al. 2017,
arXiv:1706.03741) showed that a reward model could be learned from
human comparisons of trajectories, then optimised by RL.
**InstructGPT** (Ouyang et al. 2022, arXiv:2203.02155) applied this
to language models as **RLHF** -- reinforcement learning from human
feedback -- using PPO to fine-tune a model against a learned reward
model of human preference. RLHF is the alignment technique behind the
modern assistant models, and it is covered in depth on page 6.

The most recent turn is **RL from verifiable rewards** (RLVR): for
tasks with a checkable answer -- mathematics, code -- the reward is
the verifier's verdict, no human and no reward model needed. Large
reasoning models trained this way (the DeepSeek-R1 line,
arXiv:2501.12948, 2025) learn long chains of thought purely from the
RL signal. After three decades, the policy-gradient theorem and PPO
are training the frontier of artificial intelligence.

## 12. Open-endedness and the curriculum

A final thread: where do the *tasks* come from? **Open-ended** and
**auto-curriculum** research evolves the environment alongside the
agent. **POET** (Wang et al. 2019, arXiv:1901.01753) co-evolved
agents and environments; **Prioritised Level Replay** and **ACCEL**
made it cheaper by replaying and editing high-regret levels. This is
the subject of page 5's bootstrap track, and the reason the thvm
experiment arc keeps a fixed, diverse environment curriculum and is
building toward an evolving one.

## 13. Where the thvm experiment arc sits

The brain experiments (`brain/experiments/`) are a deliberate,
small-scale walk through this history:

- **Tabular and classical RL** (experiments 01-27): REINFORCE,
  actor-critic, Q-learning, SARSA, `TD(lambda)`, successor
  representations, DQN, hindsight replay, option-critic -- the
  classical toolkit of sections 2-4, each reproduced as a controlled
  study.
- **World models and imagination** (28-49): JEPA-style latent
  prediction, imagination planning, the wa30 / sp80 ARC-AGI-3
  attempts -- the section-6-7 representation-and-world-model line.
- **The quasimetric controller** (151-159): a goal-conditioned
  asymmetric metric (section 10), planned with MCTS over an
  enumerated graph (section 3-5), with the ranking-loss result of
  158-159 as the arc's central finding.
- **The universal world model** (160-167): the section-7-9 frontier
  -- a token-space warp-residual model (160), a shared multi-
  environment encoder (165), a continuous latent world model (166),
  a categorical one (167) -- a controlled re-derivation of the
  discrete-latent, multi-task lessons of DreamerV2, JOWA and
  Mixture-of-World Models.

The **north star** is page 5's: ARC-AGI 1, 2 and 3, a generalisation
test no amount of within-task RL can shortcut. The arc's wager is the
same one the generalist-agent papers of section 9 are making -- that
one shared representation, trained across diverse environments, is
the path to an agent that handles a task it was never trained on.

## References

Classical:

- R. Bellman, *Dynamic Programming*, Princeton University Press, 1957.
- A. Samuel, "Some Studies in Machine Learning Using the Game of
  Checkers", *IBM Journal*, 1959.
- R. Sutton, "Learning to Predict by the Methods of Temporal
  Differences", *Machine Learning*, 1988.
- C. Watkins and P. Dayan, "Q-learning", *Machine Learning*, 1992.
- R. Williams, "Simple Statistical Gradient-Following Algorithms for
  Connectionist Reinforcement Learning", *Machine Learning*, 1992.
- G. Tesauro, "Temporal Difference Learning and TD-Gammon",
  *Communications of the ACM*, 1995.
- W. Schultz, P. Dayan and P. R. Montague, "A Neural Substrate of
  Prediction and Reward", *Science* 275, 1997.
- R. Sutton, D. McAllester, S. Singh and Y. Mansour, "Policy Gradient
  Methods for Reinforcement Learning with Function Approximation",
  NeurIPS, 2000.
- R. Sutton and A. Barto, *Reinforcement Learning: An Introduction*,
  2nd edition, MIT Press, 2018.

Deep RL:

- V. Mnih et al., "Playing Atari with Deep Reinforcement Learning",
  arXiv:1312.5602 (2013); "Human-level control through deep
  reinforcement learning", *Nature* 518 (2015).
- M. Bellemare et al., "The Arcade Learning Environment",
  *JAIR*, 2013.
- J. Schulman et al., "Trust Region Policy Optimization",
  arXiv:1502.05477 (2015).
- T. Lillicrap et al., "Continuous Control with Deep Reinforcement
  Learning" (DDPG), arXiv:1509.02971 (2015).
- V. Mnih et al., "Asynchronous Methods for Deep Reinforcement
  Learning" (A3C), arXiv:1602.01783 (2016).
- J. Schulman et al., "Proximal Policy Optimization Algorithms",
  arXiv:1707.06347 (2017).
- T. Haarnoja et al., "Soft Actor-Critic", arXiv:1801.01290 (2018).
- M. Bellemare, W. Dabney and R. Munos, "A Distributional Perspective
  on Reinforcement Learning" (C51), arXiv:1707.06887 (2017).
- M. Hessel et al., "Rainbow: Combining Improvements in Deep
  Reinforcement Learning", arXiv:1710.02298 (2017).
- M. Andrychowicz et al., "Hindsight Experience Replay",
  arXiv:1707.01495 (2017).

Planning and self-play:

- D. Silver et al., "Mastering the Game of Go with Deep Neural
  Networks and Tree Search", *Nature* 529 (2016); "...without Human
  Knowledge" (AlphaGo Zero), *Nature* 550 (2017).
- D. Silver et al., "A General Reinforcement Learning Algorithm that
  Masters Chess, Shogi and Go" (AlphaZero), *Science* 362 (2018);
  arXiv:1712.01815.
- J. Schrittwieser et al., "Mastering Atari, Go, Chess and Shogi by
  Planning with a Learned Model" (MuZero), arXiv:1911.08265 (2019);
  *Nature* 588 (2020).

Sample efficiency:

- S. Kapturowski et al., "Recurrent Experience Replay in Distributed
  Reinforcement Learning" (R2D2), ICLR 2019.
- A. Badia et al., "Agent57: Outperforming the Atari Human Benchmark",
  arXiv:2003.13350 (2020).
- M. Schwarzer et al., "Data-Efficient Reinforcement Learning with
  Self-Predictive Representations" (SPR), arXiv:2007.05929 (2020);
  "Bigger, Better, Faster" (BBF), arXiv:2305.19452 (2023).
- W. Ye et al., "Mastering Atari Games with Limited Data"
  (EfficientZero), arXiv:2111.00210 (2021).

World models:

- D. Ha and J. Schmidhuber, "World Models", arXiv:1803.10122 (2018).
- D. Hafner et al., "Learning Latent Dynamics for Planning from
  Pixels" (PlaNet), arXiv:1811.04551 (2018); "Dream to Control"
  (Dreamer), arXiv:1912.01603 (2019); "Mastering Atari with Discrete
  World Models" (DreamerV2), arXiv:2010.02193 (2020); "Mastering
  Diverse Domains through World Models" (DreamerV3), arXiv:2301.04104
  (2023), *Nature* (2025).
- V. Micheli et al., "Transformers are Sample-Efficient World Models"
  (IRIS), arXiv:2209.00588 (2022).
- E. Alonso et al., "Diffusion for World Modeling" (DIAMOND),
  arXiv:2405.12399 (2024).

Sequence models and generalist agents:

- L. Chen et al., "Decision Transformer", arXiv:2106.01345 (2021).
- M. Janner et al., "Offline Reinforcement Learning as One Big
  Sequence Modeling Problem" (Trajectory Transformer),
  arXiv:2106.02039 (2021).
- K.-H. Lee et al., "Multi-Game Decision Transformers",
  arXiv:2205.15241 (2022).
- S. Reed et al., "A Generalist Agent" (Gato), arXiv:2205.06175
  (2022).
- "Scaling Offline Model-Based RL via Jointly-Optimized World-Action
  Model Pretraining" (JOWA), arXiv:2410.00564 (2024).
- "Mixture-of-World Models: Scaling Multi-Task Reinforcement Learning
  with Modular Latent Dynamics", arXiv:2602.01270 (2026).

Goal-conditioned RL and quasimetrics:

- T. Schaul et al., "Universal Value Function Approximators", ICML
  2015.
- T. Wang and P. Isola, "On the Learning and Learnability of
  Quasimetrics", arXiv:2206.15478 (2022).
- T. Wang et al., "Optimal Goal-Reaching Reinforcement Learning via
  Quasimetric Learning", arXiv:2304.01203 (2023).
- B. Eysenbach et al., "Contrastive Learning as Goal-Conditioned
  Reinforcement Learning", arXiv:2206.07568 (2022).

RL and language:

- P. Christiano et al., "Deep Reinforcement Learning from Human
  Preferences", arXiv:1706.03741 (2017).
- L. Ouyang et al., "Training Language Models to Follow Instructions
  with Human Feedback" (InstructGPT), arXiv:2203.02155 (2022).
- DeepSeek-AI, "DeepSeek-R1: Incentivizing Reasoning Capability in
  LLMs via Reinforcement Learning", arXiv:2501.12948 (2025).

Open-endedness:

- R. Wang et al., "Paired Open-Ended Trailblazer" (POET),
  arXiv:1901.01753 (2019).

## Pointers

- RL in the brain (breakthrough 2): [02-five-breakthroughs.md](02-five-breakthroughs.md).
- World models and JEPA: [03-jepa-and-world-models.md](03-jepa-and-world-models.md).
- Generative video and game world models: [07-video-world-models-survey.md](07-video-world-models-survey.md).
- RLHF and modern alignment: [06-classical-ml-and-rlhf.md](06-classical-ml-and-rlhf.md).
- The bootstrap track and ARC-AGI north star: [05-toy-problems-with-thvm.md](05-toy-problems-with-thvm.md).
- Consolidated bibliography: [references.md](references.md).
