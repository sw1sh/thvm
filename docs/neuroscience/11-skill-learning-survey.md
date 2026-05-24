# 11 - Connectionist skill learning: a survey, and a design for the brain-arc

This page surveys how to introduce SKILL LEARNING -- a set of learned,
improving multi-step policies that identify subgoals, form learnable
dependencies, and chain to solve long-horizon tasks -- in a connectionist
(neural, gradient-trained) way, and proposes a concrete architecture for
the next experiment in the `brain/experiments/` ARC-AGI-3 arc (226). It
was written May 2026 after the arc reached its diversity ceiling
(experiments 209-225, see [page 10](10-intrinsic-goal-experiments-synthesis.md)).

## Why skills, why now

The arc established two things and hit three walls. The lever that works
is explorer DIVERSITY (a set of differently-biased explorers unions to
roughly twice any single one, ~9/15 of the stuck-game seeds). The walls:
(1) REWARD STARVATION -- the games give no reward until a rare level
completion, so nothing shapes a representation toward the goal (experiment
216); (2) the RELATIONAL / MULTI-STEP wall -- completions are gated by a
precise sequence or by hidden state, which a one-step Markovian agent
cannot represent (214, 215, 221); (3) the object representation (222) is
the only thing that beat raw-pixel exploration and reached the hardest
game, so structure lives at the object level.

Skill learning attacks all three at once, which is why it is the natural
next chapter. A long completion becomes a CHAIN of short, individually-
learnable subgoals (wall 2); HINDSIGHT relabeling makes every reached
state a training target, manufacturing dense signal with zero environment
reward (wall 1); and the subgoals can live in OBJECT space (wall 3 and
the 222 win). The brain does exactly this: cortico-basal-ganglia circuits
CHUNK isolated movements into reusable action sequences, and prefrontal-
basal-ganglia loops are hierarchically organized, the prefrontal cortex
encoding a "task space" of relationships between cues, actions, policies,
and outcomes ([Journal of Neuroscience 37:7893](https://www.jneurosci.org/content/37/33/7893);
[cortico-BG chunking review, PMC4523429](https://pmc.ncbi.nlm.nih.gov/articles/PMC4523429/);
[adaptive chunking + working memory, PMC11870651](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC11870651/)).

## The connectionist landscape

**A. The options framework (the substrate).** A skill is an OPTION: an
initiation set (where it can start), a policy that runs for several
steps, and a termination condition. Everything below is a way to
DISCOVER options without hand-coding them. Survey:
[Pateria et al., "Hierarchical Reinforcement Learning: A Comprehensive
Survey", ACM Computing Surveys 54(5), 2021](https://dl.acm.org/doi/fullHtml/10.1145/3453160).

**B. Unsupervised skill discovery (reward-free).** Learn a
skill-conditioned policy pi(a | s, z) plus a discriminator q(z | s),
trained so different skills z reach DISTINGUISHABLE states -- a mutual-
information / empowerment objective. DIAYN ("Diversity Is All You Need",
[arXiv:1802.06070]) learns separable skills but explores weakly; DADS
("Dynamics-Aware Discovery of Skills", [openreview HJgLZR4KvH](https://openreview.net/pdf?id=HJgLZR4KvH))
adds a learned dynamics model so skills are predictable and usable for
model-based control; recent work pushes coverage and disentanglement
(Behavior Contrastive Learning, [PMLR v202 yang23a](https://proceedings.mlr.press/v202/yang23a/yang23a.pdf);
Skill Regions Differentiation, [arXiv:2506.14420](https://arxiv.org/abs/2506.14420);
Disentangled skill discovery, [NeurIPS 2024](https://proceedings.neurips.cc/paper_files/paper/2024/file/8c263f70550cc7d69dba3fc170a23e77-Paper-Conference.pdf)).
Strength: a crisp reusable SET of skills with no reward. Weakness: skills
are emergent and uninterpretable, and MI objectives under-explore.

**C. Manager-worker hierarchy.** A MANAGER network emits a subgoal every
k steps; a WORKER reaches it. FeUdal Networks
([Vezhnevets et al. 2017, arXiv:1703.01161](https://arxiv.org/abs/1703.01161),
after Dayan & Hinton's feudal RL) sets directional goals in a learned
latent at a slower timescale. HIRO ([Nachum et al. 2018](https://openreview.net/references/pdf?id=ryQPmm9AX))
makes it data-efficient and off-policy with goal relabeling. Hierarchical
Actor-Critic / HAC ([Levy et al. 2019](https://www.researchgate.net/publication/321510889_Hierarchical_Actor-Critic))
trains every level with HINDSIGHT, relabeling proposed subgoals with the
state actually achieved -- the trick that tames the non-stationarity of a
moving worker. Strength: directly "identifies subgoals and solves them"
as one trained system. Weakness: two-timescale RL is notoriously finicky.

**D. Goal-conditioned RL + hindsight (the dense-signal engine).** Train
pi(a | s, g) and V(s, g) to reach ARBITRARY goals g. Hindsight Experience
Replay ([Andrychowicz et al. 2017]) relabels failed trajectories with the
goal that WAS reached, so every rollout is a success for some goal --
dense self-supervision with no reward. This is the single most relevant
mechanism for our reward-starvation wall. Recent extensions matter for
us: subgoals can be GENERATED from hindsight-relabeled achieved goals to
bootstrap long-horizon policies ([Goal-Conditioned RL with Subgoals from
Relabeling, OpenReview m7lBCyROPP](https://openreview.net/forum?id=m7lBCyROPP)),
and naive HER FAILS in object-centric / multi-entity domains unless
relabeling is interaction-aware (causal / null-counterfactual)
([Chuck et al. 2025, arXiv:2505.03172](https://arxiv.org/pdf/2505.03172);
hindsight regularization, [arXiv:2508.06108](https://arxiv.org/html/2508.06108v1)).

**E. Skill chaining and the learned SKILL GRAPH (the chosen route).**
Skill chaining ([Konidaris & Barto, "Skill Discovery in Continuous RL
Domains using Skill Chaining"](https://cs.brown.edu/people/gdk/pubs/skillchain-msrl.pdf))
grows options BACKWARD from the goal: learn an option that reaches the
goal, then an option that reaches the first option's initiation set, and
so on, until the start lies in some option's initiation set -- an
explicit chain where "skill A terminates inside skill B's initiation set"
is the learned dependency. Deep skill chaining scales this with neural
option policies ([Bagaria & Konidaris, ICLR 2020](https://openreview.net/pdf?id=B1gqipNYwH)).
Deep Skill Graphs ([Bagaria, Senthil & Konidaris, ICML 2021](http://proceedings.mlr.press/v139/bagaria21a/bagaria21a.pdf))
generalize the chain to a GRAPH: nodes are skill subgoals, edges are
skill policies that move between them, built in an unsupervised phase
that interleaves discovering skills and PLANNING over the graph to cover
ever more of the state space. This is exactly the "learnable dependencies
that identify subgoals" the design calls for. 2024 work improves
composability with pessimistic/optimistic initiation classifiers and
model-based option policies robust to non-stationary subgoal regions
([Robustly Learning Composable Options, NSF PAR 10293550](https://par.nsf.gov/biblio/10293550-robustly-learning-composable-options-deep-reinforcement-learning)).

**F. Object-centric and entity-centric hierarchy (the current frontier).**
The newest line fuses object representations with hierarchy: object-
centric world models extract per-object features from a segmentation
network and plan over them ([OC-STORM, arXiv:2501.16443](https://arxiv.org/pdf/2501.16443)),
object-based masks give dense reward and generalization in visual goal-
conditioned RL, and hierarchical ENTITY-centric agents factor subgoals
per object and generate them with diffusion ([Hierarchical Entity-centric
RL with Factored Subgoal Diffusion, arXiv:2602.02722](https://arxiv.org/html/2602.02722)).
This is precisely the intersection our arc arrived at independently: the
object representation that won (222) is the right space for subgoals.

## Mapping to the brain-arc, and the design for 226

The survey converges with the arc's own findings on one architecture.
Take the object representation (222) as the subgoal space; use hindsight
to escape reward starvation; use a Deep-Skill-Graph-style learned graph
for the dependencies (the chosen representation); chain subgoals to
assemble the multi-step completions that defeat single-step agents.

```
perception:  frame --> connected-component OBJECT set            (222)
goal space:  g = embedding of a target object-configuration
WORKER:      pi(a | enc(s), g), V(s,g)
             trained by HINDSIGHT: relabel g := object-config reached
             t' steps later; reward = reached(g)                  (HER/HAC)
             -> dense gradient with NO environment reward          (wall 1)
SKILL GRAPH: nodes  = visited object-configs (or cluster centroids)
             edges  = learned reachability e(gi -> gj) = "worker reaches
                      gj from gi in <= k steps" (a classifier)      (Deep Skill Graphs)
MANAGER:     pick the next subgoal on the frontier (novel + reachable);
             plan a PATH of subgoals over the graph and have the worker
             execute each hop                                      (walls 2, 3)
```

The worker is the "set of learned, improving multi-step policies" (a
continuum indexed by object-goal, improving as hindsight data
accumulates). The skill graph is the "learnable dependencies that
identify subgoals": its edge-classifier learns which object-configs
enable which, and the manager plans over it. A precise completion (for
example tr87's combination lock) is approached as a chain of short,
separately-learnable object-subgoals rather than stumbled into whole --
credit assignment by construction, the structure being the skill graph
rather than a search tree over a flaky learned model (which 221 showed
fails).

Recommended staging:
- 226a: build the worker -- object-goal-conditioned policy/value trained
  by hindsight on existing random + explorer rollouts (dense, offline).
  Metric: goal-REACH rate (can it reliably reach a specified object-
  configuration?), the prerequisite, not yet game unlocks.
- 226b: add the skill graph + manager -- self-proposed subgoals on the
  novel-reachable frontier, chained. Metric: stuck-game unlock rate vs
  the 9/15 diversity ceiling; the hypothesis is that directed skill-
  CHAINING reaches triggers undirected diverse exploration cannot.

Risks to watch, flagged by the survey: HER degrades in multi-object
domains without interaction-aware relabeling (Chuck 2025) -- our object
goals must relabel per-object, not over the whole frame; manager-worker
non-stationarity needs hindsight subgoal relabeling (HAC) or it diverges;
and unsupervised skills under-explore (DIAYN/DADS), so the manager's
novelty drive over the graph frontier is what supplies exploration. The
deepest open question is whether the worker's object-goals are
expressive enough to NAME the configuration a hidden-state-gated trigger
requires -- if the trigger depends on unobserved history (214/215), no
single-frame object-goal can specify it, and the recurrent-worker variant
becomes necessary.

## Update (May 2026, follow-up): the end-to-end variant -- a soft, differentiable, jointly-learned skill-dependency DAG over a latent skill partition

After the staged design above was built (experiments 226-229; see
[page 10 synthesis](10-intrinsic-goal-experiments-synthesis.md)), a
follow-up proposed a more end-to-end architecture and asked which
literature it best matches: a world model feeding a PARTITIONED
skill-space encoder where
each partition is initially an independent policy embedding, with a SOFT
DAG adjacency of dependency edges LEARNED between partitions, and a
sampler that learns to sequence policies over this substrate. Each of its
four pieces has a strong match, but the specific combination is a real gap
in the literature, not an existing system.

- **World model -> partitioned skill encoder, partition = policy
  embedding.** This is unsupervised skill discovery with a skill-
  conditioned policy pi(a | s, z). DADS (skill-conditioned dynamics model
  q(s' | s, z)) is the closest: skills partitioned by their EFFECT in a
  world model, which is literally "world model -> skill partition." DIAYN
  partitions state space by a discriminator. OPAL and CompILE learn a
  CONTINUOUS latent primitive space by segmenting trajectories into latent
  codes (partition + embedding learned from data); CompILE additionally
  segments AND recomposes, prefiguring the sampler.
- **Soft DAG of learned dependency edges -- the distinctive piece.**
  Subtask Graph Inference (Sohn, Oh, Lee, NeurIPS 2018; ICLR 2020) is the
  strongest direct match: it explicitly LEARNS a graph over subtasks with
  precondition/dependency edges (a DAG) and executes respecting it. Deep
  Skill Graphs (the route adopted for 226-229) build a graph too, but the
  edges are MEASURED reachability, not a soft differentiable adjacency.
  The differentiable-DAG mechanism itself is NOTEARS (a continuous DAG
  adjacency W with the acyclicity penalty h(W) = tr(e^{W o W}) - d). Nobody
  has bolted NOTEARS-style differentiable acyclic structure onto a latent
  skill space -- that fusion is the novel hinge. Classical ancestor:
  STRIPS/PDDL precondition-effect graphs; learned relaxations: policy
  sketches (Andreas 2017), Causal InfoGAN (latent transition graph).
- **Sampler that sequences policies -- the policy-over-skills.** Option-
  Critic learns options AND the policy-over-options end-to-end (the
  sampler co-trained with the skills, matching "learns a sequence").
  FeUdal/HIRO/HAC are the manager-emits-subgoal version. Successor
  Features + GPI / the Option Keyboard COMPOSE pre-learned skills as a
  vector-space combination -- a different flavor of sequencing.

**Best composite match:** DADS (world-model-partitioned latent skills) +
Subtask Graph Inference / Deep Skill Graphs (the dependency DAG) +
Option-Critic or FeUdal (the jointly-trained sampler). **The genuine gap
the intuition targets:** making the dependency graph a SOFT, DIFFERENTIABLE,
end-to-end-learned DAG (NOTEARS-style) over LATENT-PARTITION skill
embeddings, CO-TRAINED with the sampler -- rather than a hard reachability
graph built in a separate planning phase (Deep Skill Graphs) or a
discretely-inferred symbolic precondition graph (Subtask Graphs).

**Relation to 229 (what is built vs. this proposal).** 229 is the Deep-
Skill-Graph branch: nodes = enumerated object-config keys (222), edges =
measured worker reachability (hard), sampler = a hand-written novelty-
frontier manager. The proposed architecture is its end-to-end evolution
on all three axes: enumerated configs -> learned latent skill-partition
embeddings (DADS/OPAL); measured reachability -> a differentiable adjacency
co-trained with the policy (NOTEARS + Subtask Graph); hand-coded manager
-> a trained policy-over-skills (Option-Critic/FeUdal). That is the design
sketch for a future experiment if 229's hard skill graph shows the
composition lever is real but its enumerated/hard-coded form is the
bottleneck.

### Refinement (same follow-up): don't learn the DAG -- skill-coarsened MCTS, where the "DAG" is the traversal, not a trained object

A pushback on the learned-DAG emphasis above: do NOT fit an
explicit differentiable adjacency (NOTEARS). A better proxy is MCTS
COARSE-GRAINED BY LEARNED SKILLS -- and the "learned DAG" is properly
understood as the ALGORITHM that performs the traversal, not a separately-
trained matrix. The dependency structure should be EMERGENT in the search
tree; what you learn is the prior + value that guide the traversal
(AlphaZero/MuZero structure with skills as the macro-actions). The skill
graph is then just the realized search DAG, never materialized.

This specifically un-breaks experiment 221, which found primitive-action
MCTS over the learned forward model UNDERPERFORMED shallow shooting (the
~1-step-accurate model compounds error over a deep tree; the committed
path chases model artifacts). Coarsening the tree by skills fixes both
terms: branching collapses from 261 primitive actions to a handful of
worker hops (shallow-in-edges, deep-in-time), and per-edge model error is
amortized over a locally-competent worker hop to an object-config rather
than a raw per-pixel 1-step prediction -- the reliable-macro-transition
regime where deep search actually pays off, which 221 lacked at primitive
granularity.

Literature: MuZero (MCTS over a LEARNED latent model with a learned
prior/value -- the skill-coarsened version is MuZero with options as
abstract actions); Director / "Deep Hierarchical Planning from Pixels"
(manager picks latent goals in a world model, worker executes -- the
closest existing "plan over learned skills in a learned model");
hierarchical / options MCTS (Vien & Toussaint 2015, temporally-extended
actions in the tree -- the classic "coarse-grain the search by skills").

Slimmed design (candidate 230): keep the 227/228 object-goal worker as the
macro-action set; replace 229's hand-coded novelty-frontier manager with
skill-level PUCT/MCTS guided by a learned value over object-configs,
planning by composing worker hops over the learned forward model. No
explicit DAG learning. A better-motivated successor to 221 (skill-coarse
search) than re-running primitive MCTS.

### Follow-up (May 2026): subgoals from the world model's ATTENTION distribution

A follow-up asked whether a subgoal can be learned by reading the world
model's ATTENTION: when the model in a given state attends to MULTIPLE
coupled pieces -- two tiles that are rotationally identical, say -- the
attention matrix connects the related parts, and a hierarchical
architecture on top would learn deeper patterns from those couplings. Yes;
this is a real, grounded line, and it is the LEARNED, ADAPTIVE version of
the representation ladder that 222->223 climbed by hand (and that 223
botched by enumerating ALL pairwise relations, which was too fine).

- **Attention as a relation detector.** Self-attention over state elements
  (tiles / objects / patches) yields an attention matrix that is literally
  an adjacency over state parts, learned for free as a byproduct of
  prediction; high weight between two rotationally-identical tiles encodes
  "these are related." Relational RL (Zambaldi et al. 2019, "Deep RL with
  Relational Inductive Biases") runs multi-head self-attention over entity
  embeddings and shows the attention weights recover task-relevant
  relations and drive better policies; the general frame is relational
  inductive biases / graph networks (Battaglia et al. 2018).
- **Subgoals from the attention.** Two grounded routes: (1) attention as a
  saliency / bottleneck signal for WHAT to control -- if prediction
  depends heavily on a small set of parts, those are the controllable,
  outcome-relevant variables, hence good goal targets (feature-attainment
  goals over high-attention features rather than raw pixels); (2) attention
  coupling -> relation -> a RELATIONAL subgoal ("make these two coupled
  tiles satisfy the flagged relation"), the adaptive fix for 223's
  too-fine hand-coded pairwise relations.
- **The hierarchical part ("deeper patterns on top").** Stacking attention
  so a higher layer attends over lower-layer relation-groups is
  hierarchical / slot-based object-centric modeling: Slot Attention
  (Locatello et al. 2020) binds pixels to slots (the partitioned pieces),
  then a transformer over slots learns slot-slot relations -- the two-level
  structure exactly; object-centric / transformer world models (SLATE,
  STEVE, IRIS, OC-STORM) make the prediction attention itself the
  substrate to read subgoals off.

**Caveat (the arc's recurring gap).** Attention surfaces what is COUPLED,
not what to DO about it or whether it is REACHABLE -- the same gap that
qualified 226-229. A forward model trained only to predict next frames
surfaces PREDICTION-relevant relations, which need not be the
COMPLETION-relevant ones (the 214/215 hidden-state wall). The idea is
strongest where prediction-relevant and task-relevant structure coincide
-- and tr87 (a substitution puzzle whose win condition IS a tile-mapping/
identity relation) is the best case in our set. Complementary to 230: 230
searches over WHICH object-config to reach; an attention-relational world
model would change WHAT the configs/goals are MADE OF (relations, not just
object multisets), climbing the 222->223 ladder the learned/adaptive way.
Candidate for a future experiment (231+) if 230's skill-coarse search
shows the bottleneck is the goal REPRESENTATION rather than the search.
