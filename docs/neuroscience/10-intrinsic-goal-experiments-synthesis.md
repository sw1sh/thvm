# 10 - The intrinsic-goal exploration experiments (209-219): what worked, what didn't, and the ceiling

[Page 9](09-arc-agi3-exploration-survey.md) surveyed the literature on
exploration under absent reward and named the project's target,
ARC-AGI-3 (the Abstraction and Reasoning Corpus, agentic rung). This
page reports what happened when the project actually ran the
experiments that survey implied, in `brain/experiments/` numbers
209-219 (May 2026), and states the conclusion they reached: a positive
result, four clean negatives, a curriculum escape that turned out to have
no source and then, when one was authored, no transfer either, and a
precise account of the wall.

A note on scope, because it controls how to read every number. This arc
optimizes for a brain-inspired LEARNED agent and benchmarks against the
honest learned state-of-the-art, the official "Stochastic Goose" sample
(Kaggle public score about 0.25): a convolutional network that predicts
which action changes the frame, samples from it, and resets per level.
It does NOT chase the ARC-AGI-3 public leaderboard, which is won by
deep-copying the leaked per-game Python source and running offline
search to a win (simulator exploitation the project forbids). Against
honest learned agents, a single one of our explorers is at the Goose
frontier, not behind it.

## The setup

The unit of study is the per-game intrinsic-goal agent. For each game:
collect a few thousand random transitions, train a faithful per-game
forward model (a small convolutional next-frame predictor), then run
model-predictive control (MPC) where the agent imagines short action
rollouts over that model and executes the one whose imagined endpoint
scores best. The "goal" is intrinsic: count-based novelty, "go where you
have visited least." There is no reward to optimize until a rare level
completion, so the unlock signal is sparse and binary.

The test bed is three persistently stuck 64x64 (downsampled 16x16)
games: `sb26` (click-active), `ls20` and `tr87` (cardinal movement).
The honest deliverable quantity, established in experiment 208B, is a
per-game unlock RATE over 5 seeds, not an integer count: these unlocks
are real but low-rate stochastic events, so a single seed catching one
on a lucky draw over-claims. The whole arc reports rates out of 15
(3 games x 5 seeds).

## The progression

The arc is a sequence of attempts to raise that rate above the level a
single novelty explorer reaches (~3/15), each ruling out a hypothesis.

**209 -- is it breadth?** Scoring 8x as many random candidate rollouts
per plan step (32 to 256) left the rate flat (2/15 vs 3/15, within
noise). More uniform-random candidates do not reach the triggers more
reliably. The bottleneck is search DIRECTION, not breadth.

**210 -- is it sampler direction, and where do we stand vs Goose?** A
cross-entropy-method (CEM) planner that iteratively concentrates the
action distribution on high-novelty elites scored 2/15, while plain
random-shooting novelty-MPC scored 3/15 and Goose 2/15. Two findings.
Our model-based planner edges the honest learned SOTA (3 vs 2). And CEM
is complementary, not dominant: it is the only arm to crack `tr87`
(one seed), via a chance-chained precise sequence, even though it trails
in aggregate. Critically, the three learned arms (random-shooting,
CEM, Goose) unlocked 7 seeds sharing only 1, so their UNION is 6/15 --
double the best single arm.

**211 -- is it the novelty signal?** Swapping count-novelty over raw
frame bytes for distance-novelty in a learned representation (the
forward model's own convolutional features; and a quasimetric latent
trained so embedding-distance approximates temporal distance) did not
beat raw counts: the world-model-feature latent tied raw (3/15 each),
the learned quasimetric trailed (1/15). But again the three signals
unlocked different seeds (7 events, 1 shared), union 6/15. The same
complementarity structure, now across signals rather than samplers. A
secondary lesson: the FREE world-model feature beat the elaborate
learned metric, because temporal distance in random rollouts is a noisy
reachability proxy.

**212 -- can one agent realize the union?** An ensemble that
round-robins three diverse explorers per episode over a shared
experience buffer scored 4/15, beating the best single arm (3/15). But
it fell short of the cross-run union (6/15): round-robin splits the
budget three ways and STARVES the triggers each explorer hits only at
full budget. The surprise was super-additivity: the ensemble unlocked
two seeds NO single explorer reached, via cross-explorer trajectories on
the shared buffer, pushing the grand union (singles plus ensemble) to
8/15. Diversity does not just union known triggers; mixing explorers
reaches new ones.

**213 -- is goal-directed reaching the missing mechanism?** Instead of
dispersing toward novelty, induce the action-to-effect AFFORDANCE
structure (which actions change which cells) and PLAN to reach an unmet
structural sub-goal: drive the controllable object to under-visited
positions (cardinal games), or exercise under-used effective click cells
(click games). This scored 0/15. Three failure modes: on `sb26` directed
single-cell coverage is the wrong primitive (the trigger needs a click
combination); on `ls20` the moving-object detector found no affordance,
so the agent was random; on `tr87` the spatial agent ran exactly as
designed, drove the object across the position space, and still missed.
The honest scope was stated up front: the LEVEL-COMPLETION rule cannot
be induced, because the stuck games almost never complete under
self-play, so there are no positive examples to mine its precondition
from.

**214 -- couple exploration and induction.** The capstone closed 213's
bind. Run the 212 ensemble (the best completion FINDER, grand union
8/15) to HARVEST the few completions it stumbles into, recording the
pre-completion frame and the completing action; induce the precondition;
then test whether a goal-directed agent that reaches that precondition
and fires the action TRANSFERS to seeds the ensemble missed. It scored
0/15, with zero transfer. The decisive detail: it failed to re-trigger
completion even on the seeds the positives were harvested FROM, where it
holds the exact frame and exact action. That can only mean the
completion is not a reachable (state, action) pair. It is gated by
HIDDEN STATE or history not visible in the single observed frame (a
look-alike frame is not the same game state), and/or by a precise
multi-step TRAJECTORY a one-step-accurate forward model cannot navigate
to.

## The conclusion

Two statements survive the arc.

**The positive result: explorer DIVERSITY is the lever.** It is the only
thing that ever raised the rate. Any single learned explorer reaches
~3/15 (Goose-level); a diverse set unions to 8/15; a single agent
mixing explorers realizes 4/15 and reaches genuinely new triggers by
cross-explorer interaction. This is a real, brain-resonant finding: a
population of differently-biased intrinsic drives covers a sparse
opportunity manifold that no single drive can.

**The negative result and the ceiling: the residual triggers are
trajectory- or hidden-state-gated.** Every attempt to REPLACE
exploration with a smarter learned objective or rule -- a better novelty
signal (211), an affordance goal (213), a harvested completion rule
(214) -- matched or underperformed plain diverse exploration. The 214
finding that even an exact harvested (frame, action) does not reproduce
the completion is the clincher: these games carry state the observation
does not expose, and trigger on precise trajectories, so a Markovian
single-frame agent cannot represent what the trigger requires, however
it is steered. These specific games are not solvable from 15k-step
self-play with a single-frame world model.

## Where this points

The 214 hidden-state finding was also a direction, and a model-
ARCHITECTURE one rather than another exploration or induction objective:
if dynamics and value depend on unobserved history, the world model must
be RECURRENT, so the agent can represent the trajectory the trigger
requires. Experiment 215 built exactly that -- a long short-term memory
(LSTM) world model carrying history, with novelty run over its hidden
state -- reconnecting the arc to the recurrent state-space models of
[page 3](03-jepa-and-world-models.md) and [page 7](07-video-world-models-survey.md)
and the partial-observability thread of [page 8](08-reinforcement-learning.md).

It scored 0/14, below even the Markovian frame-novelty baseline. The
result does not refute partial observability as the gate; it shows a
recurrent model is NECESSARY-NOT-SUFFICIENT. The LSTM is trained only by
self-supervised next-frame prediction, so its hidden state encodes what
helps predict frames, not the unobserved variable the completion gates
on; and without reward (which only fires on the completion we cannot
reach) nothing shapes its memory toward the trigger. That is the arc's
chicken-and-egg in its sharpest form: the representation needs the task
signal, and the task signal needs the representation. Separately,
history-novelty over a smooth learned latent proved a blurrier explorer
than exact raw-frame counts, the same lesson 211 taught.

So across 209-215, FIVE attempts to beat plain diverse exploration with
a smarter objective, rule, or architecture (211 latent signal, 213
affordance goals, 214 harvested induction, 215 recurrent model) all
failed. The honest deliverable stands as written here: against the
honest learned SOTA, a diverse intrinsic-goal population doubles the
unlock rate (8/15 vs ~3/15), and the remaining triggers are gated by
structure a 15k-step self-play agent cannot align a representation to
without the reward it cannot earn.

The one lever the arc's negatives had not ruled out was a CURRICULUM:
easier games where completions DO occur under self-play, so reward can
shape the (recurrent) representation, then transfer it to the stuck
games. Experiment 216 tested the precondition that plan rests on -- is
there any such easy game? -- and found there is not. A scan of all 25
games shows NONE complete richly under self-play: under 4000 random
steps only three games (ft09, lp85, r11l) complete a level even once,
and the other twenty-two never; under the capable nov-raw explorer at
15000 steps those three still complete exactly once and stick at level
1. The set is UNIFORMLY reward-starved. The curriculum has no source,
because the bootstrap is circular on every game at once:
reward-shaped learning needs reward, reward needs reaching the triggers,
and reaching the triggers is the unsolved part everywhere.

That closes the OFFLINE SELF-PLAY arc (209-216) definitively. The
positive deliverable stands -- explorer diversity doubles the unlock
rate to 8/15 versus the Goose-level ~3/15 single explorer -- and no
further self-play experiment will move the number, because the regime
provides no reward to bootstrap from. The remaining genuine levers all
step OUTSIDE offline self-play, and each is a scope decision rather than
a tweak: in-context LLM agency (the RGB-Agent regime of page 9, reading
the interaction log and inducing rules in context, validated on the
preview games but excluded from the offline competition); cross-corpus
pretraining on a larger game set that does contain densely-completable
games (none of these 25 are); or a handful of demonstration / oracle
completion trajectories per game to supply the dense positive signal
self-play cannot produce. The honest brain-arc conclusion is the
diversity lever plus this precise map of why offline self-play cannot go
further on this set.

## Update (217): an authored in-engine curriculum was tried -- and the apparent win did NOT survive its control

The short version, recorded honestly because the first write-up of this
update over-claimed: authoring an easy in-engine level to bootstrap a
reward objective and transfer it to the stuck game produced an apparent
2/5 unlock on tr87 (217B), but a control run (217C) showed it was not the
learned objective and the effect does not hold up. The detailed account
follows; treat the optimistic framing two paragraphs down as the
hypothesis that was tested and largely rejected, not a result.

The "definitively closed" verdict above is about the STOCK set under
self-play. Experiment 217 took the curriculum lever's surviving form --
not finding an easy game but AUTHORING one in the real engine. tr87's
mechanic (a substitution puzzle: a cursor selects editable tiles, cycles
their values, wins when they match a reference) was decoded, and a
subclass `tr87e` was authored that swaps in an easy 1-to-4-tile ramp
while reusing tr87's own sprites and engine. Under random play tr87e
clears its easy levels (reaching levels_completed 3) where real tr87
never clears even level 0 -- so reward is now harvestable on a level
structurally identical to the target.

The split that keeps this scope-clean (no reading game internals, which
would be the forbidden simulator exploit): the forward model is
self-supervised and trained on the TARGET game's own random transitions
(free); only the OBJECTIVE -- a binary reward head predicting completion
from (frame, action) -- is trained on tr87e and transferred, with
model-predictive control planning toward summed predicted reward.

First it looked like a result: the transferred reward objective cleared
real tr87's level 0 on 2 of 5 seeds, versus 0/5 for random, novelty-MPC,
and a return-to-go value-head variant on the same harness. That was
written up, with caveats, as the arc's first positive beyond diversity --
prematurely.

Then the control run (217C) retracted it. Adding an UNTRAINED, random-
initialised reward head as a control, over eight seeds, gave: random
1/8, trained reward head 2/8, untrained-head control 3/8. The untrained
head unlocks tr87 as often as -- more often than -- the trained one, and
all three are within noise. So the LEARNED objective is not the cause.
The unlocks come from the directed-MPC machinery: committing to maximise
ANY fixed scalar (learned or random) over short forward-model rollouts
and executing the argmax beats novelty dispersal and uniform random by a
little (2-3/8 vs 0-1/8), but the content of the objective does not
matter. Curriculum-bootstrapped reward TRANSFER did not work.

Two things compounded it. The reward head trained on only three positive
(completing) examples, because tr87e walls at its four-tile level without
re-climbing and a forced-reset attempt to harvest more did not help (the
mid-game reset does not re-enable level clears); so the bootstrap was
never even fairly tested. And the "2/5" was a small single run that the
eight-seed control reveals as noise-level. The honest conclusion is that
the 209-216 verdict stands and is reinforced: no learned objective has
beaten the diversity / directed-machinery baseline, and the apparent 217
exception dissolved under its own control. The lesson for the project is
procedural as much as scientific: a surprising positive from a
3-positive-example model and a single 5-seed run needed its control
before it was worth claiming, and the control is what told the truth.

If the idea is revisited, the prerequisite is an authored source that is
genuinely reward-dense (all-trivial levels that fully win and auto-reset,
yielding hundreds of positives), so the learned objective can be trained
and then fairly compared against the untrained-head control.

Experiment 218 did exactly that. It authored tr87d -- three trivial
one-tile levels that fully win and auto-reset, verified to yield ~445
completion events per 8000 random steps versus tr87e's 3 -- and retrained
the reward head on 445 positives, 150 times the data, before the same
controlled transfer to tr87. The result settles it: on real tr87 the
trained head unlocks 3/8, exactly tying an untrained random-init control
(3/8), with random at 1/8. A properly-trained completion objective is
statistically indistinguishable from a random head, so
curriculum-bootstrapped reward transfer does not work on this mechanic,
and a dense source does not save it. 217's retraction is final, not a
data artifact.

What survives is a smaller, robust finding: a fixed-objective DIRECTED
model-predictive control -- commit to maximise SOME scalar (learned or
random) over short forward-model rollouts and execute the argmax -- clears
tr87's level 0 at about 3/8, where novelty-MPC was 0/5 and uniform random
about 1/8. The lever is COMMITMENT to a consistent directed objective
versus novelty's dispersal, not the content of the objective or whether
it was learned; and different arbitrary objectives unlock different seeds
(the trained and untrained heads share only one of their three each,
union five of eight), the arc's diversity result reappearing one level
down. The deep blocker behind this and the 211/213/214 negatives is the
same: a per-step convolutional objective cannot represent the matching
RELATION the completion requires, with or without reward; breaking it
needs an object-centric or relational representation, a real architecture
program. The brain-arc's standing deliverable remains the diversity lever,
now with a documented directedness corollary and a fairly-closed
curriculum-transfer negative.

Experiment 219 then tested that directedness corollary directly -- does
committing to a stable objective beat novelty's dispersal in general? --
and the answer sharpened into the arc's central result a third time. As a
single explorer, the best fixed-directed arm only ties novelty (3/15
each), and which one wins is mechanism-specific: novelty wins the click
puzzle sb26 (coverage matters), directed wins the combination-lock tr87
(a sustained committed sequence matters), they split ls20. So
"commitment beats dispersal" is not a law, and 218's gap was tr87-
specific. What 219 actually shows -- after 210 across samplers and 211
across novelty signals -- is that novelty and the directed objectives are
COMPLEMENTARY: eight unlock events sharing only one, a grand union of
7/15 versus 3/15 for any single arm, with even two arbitrary fixed
directions unioning above each alone. Directedness is not a better
explorer, just a different one. The arc's one durable, now triply
confirmed lever is that unioning a diverse SET of explorers (novelty plus
a few fixed directed objectives plus the cross-entropy sampler), not
perfecting any single objective, is what roughly doubles the unlock rate.

## Update (220-222): the planner is not the lever, but the REPRESENTATION partly is

Three follow-ups after the synthesis above tested whether a better
PLANNER or a smarter use of one budget could move the rate, and then
whether a better STATE REPRESENTATION could.

Experiment 220 asked whether one agent's budget can realize the diversity
union by allocating across explorers more cleverly than 212's
round-robin. It cannot: across single, round-robin, contiguous, and a
frontier-yield bandit, the best (3/14) only ties a single explorer, far
from the ~8/15 cross-run union -- diversity pays off across PARALLEL
full-budget runs, not within one shared budget. And round-robin is the
WORST allocation (0/14): interleaving starves every explorer below its
unlock threshold, and it degrades as you add explorers. The naive
"rotate through your explorers" ensemble is actively harmful.

Experiment 221 swapped the shallow random-shooting planner for Monte
Carlo Tree Search over the learned forward model (the AlphaGo / Eric Jang
inspiration, references.md), holding the novelty objective fixed. It
UNDERPERFORMED shallow shooting (1/15 vs 3/15, tr87 0/5). The AlphaGo
deep-search win relies on a PERFECT model; our learned forward model is
only approximately right one step ahead, so a deep tree compounds model
error and the committed most-visited path chases model artifacts, where
shallow shooting is robust. That relocates the bottleneck, once more,
upstream of the planner -- to model fidelity and representation.

Experiment 222 acted on that, and is the arc's first single-explorer win.
Holding the planner fixed and changing only the novelty KEY -- from raw
frame bytes to the MULTISET of connected-component OBJECTS (color,
size-bucket, coarse centroid) -- object novelty beat raw-pixel novelty
5/15 versus 3/15 on a clean control, and was the FIRST novelty signal in
the whole arc to reach tr87 (1/5, where raw is 0/5 everywhere, with the
same planner -- so the representation, not the search, did it). It works
by collapsing pixel-noise (a one-cell drift at the same coarse centroid
is not novel) and surfacing structural change (an object appearing,
moving, or recolouring is one event), concentrating exploration on
structurally-distinct states. A learned-feature latent (211) had only
tied raw; an explicitly SYMBOLIC object abstraction beats it. The margin
is modest (5 vs 3, single run) and object novelty is still complementary
to raw rather than a superset (union 6/15) -- a better AND different
explorer -- but it is the first crack in the "no single explorer beats
~3/15" wall, and it confirms that the binding constraint was a pixel
representation that hid objects and relations. Next (223): relational
(object-pair) novelty, since tr87's completion is itself a RELATION
between tiles -- attacking the relational wall directly.

## Update (223-225): the representation ladder and the diversity ceiling

Three more experiments mapped the representation and diversity frontiers.
223 climbed from the object multiset to objects-plus-pairwise-RELATIONS
and found it HURTS (3/15 < the object multiset's 5/15): pairwise
relations make the state key too fine, over-distinguishing configurations
so novelty saturates. There is a granularity sweet spot, an inverted-U --
raw pixels too fine (3/15), the object multiset just right (5/15),
object+relations too fine again (3/15). The 222 object win also
reproduced exactly (same five seeds), confirming it is robust.

224 and 225 then tied the object representation back to the diversity
lever by measuring the per-seed GRAND UNION of a set of explorers on one
forward model. With four explorers (raw, wm-feature, object, directed)
the union reached 9/15, fully covering ls20 (5/5) and bringing object's
unique tr87 seed. With the full six-explorer set (adding the
cross-entropy sampler and a second fixed direction) it reached 10/15 -- a
new high, because the two added explorers cracked a tr87 seed no prior
explorer touched. So diversity has not saturated, but the returns are
clearly diminishing (six initial-to-current unions trace 6, 7, 8, 9, 10),
an asymptote forming in the low-to-mid teens. And five of fifteen seeds
(sb26 s1/s3/s4, tr87 s0/s3) resist ALL six explorers across every
sampler, signal, representation, and direction -- the residual hard set,
gated by structure exploration breadth cannot reach. This bounds the
diverse-explorer approach: it tops out near 10/15, and the last third
needs directed skill COMPOSITION, not wider undirected search.

That is the bridge to the next chapter. [Page 11](11-skill-learning-survey.md)
surveys connectionist skill learning and proposes the agent for the
residual set: an object-goal-conditioned worker trained by hindsight
(dense signal without the reward the regime denies), a learned skill
graph for subgoal dependencies, and a manager that chains subgoals to
assemble the precise multi-step completions that single-step diverse
exploration cannot.

## Update (226-229): the skill-learning chapter -- a working worker, but composition did not break the ceiling

The skill chapter built that agent in four steps and reached a clear,
qualified conclusion: the prerequisite worker can be made to work, but
chaining it did NOT crack the residual hard set.

First the WORKER -- a goal-conditioned policy that should reach a
specified object-configuration. 226 trained it by hindsight BEHAVIOR
CLONING (relabel each random transition with a goal reached later, imitate
the action) and it LOST to random (42/371 goal-reaches vs 109): cloning
random rollouts merely imitates the random action distribution, and a
deterministic argmax of that covers less ground than stochastic random.
The textbook lesson is that hindsight needs REINFORCEMENT, not cloning.
227 swapped in hindsight goal-conditioned Q-learning and nearly DOUBLED
the worker (73/371), bringing the object-structured game tr87 to near-
parity, but it still did not cross random overall. 228 isolated why with a
one-variable sweep over the eval policy: the deterministic argmax was
UNDER-COVERING. An epsilon-greedy policy on the SAME Q-net lifted reaches
to 114/371 (+41) and beat random decisively on tr87 (54 vs 37) -- so the
Q-values carry real goal-directed signal where object structure matters,
though they mislead on the cardinal game ls20 where random dispersal
already reaches most goals. A qualified worker: a useful object-config
reacher on the structured game, counterproductive elsewhere.

Then the PAYOFF -- 229. A Go-Explore-style manager over an object-config
skill graph: return to the most-novel object-config by directing the
eps-greedy worker toward it, push past it (new configs enter the bank),
chaining short worker hops into a long path within an episode (the
environment cannot reset to mid-trajectory states, so "return" is worker
navigation, not save/restore). The control was pure object-novelty MPC
(the 222 winner) at the same budget on the same forward model. The result
was a clean negative on the hypothesis: skill composition did NOT break
the ceiling. Neither arm unlocked any residual-hard seed (sb26 s1/s3/s4,
tr87 s0/s3 stayed at zero), and standalone the manager unlocked FEWER than
plain novelty (2/15 vs 4/15). The mechanism is a budget trade -- every
step the worker spends RETURNING to a known novel config is a step not
spent on the novelty exploration that actually finds completions, and the
merely-qualified worker cannot repay that trade. Directed return is
exploitation; it starves the exploration that does the unlocking.

But the manager was COMPLEMENTARY: its union with the baseline was 6/15
(vs 4 alone), and both of its unlocks (ls20-s2, tr87-s1) were seeds the
baseline missed. Worker-mediated chaining reaches a DIFFERENT slice of the
seed space -- the exact diversity structure the arc has now shown five
times (across samplers, signals, directions, representations, and now
skill-composition). So the skill manager is best understood as yet another
complementary explorer, not the composition breakthrough the chapter
sought; and the failure is in the TRAVERSAL (a hand-coded greedy "return
to the most-novel config" heuristic that commits budget to exploitation
with no value estimate), not in the substrate (the worker and the
object-config graph are sound).

That diagnosis sets the next experiment, and it doubles back to 221. 221
found that MCTS over the learned forward model underperformed shallow
shooting, because the model is only ~1-step accurate so a deep tree at
PRIMITIVE-action granularity compounds error. The fix is to coarse-grain
the search by SKILLS: a skill-level MCTS where each tree edge is a worker
hop to an object-config, not a primitive action -- the branching factor
collapses and per-edge model error is amortized over a locally-competent
hop, the regime where deep search pays off and that 221 lacked. The
dependency structure is not a separately-learned object (an explicit DAG);
it is EMERGENT in the search tree, and the learned component is the
prior/value that guides the traversal -- AlphaZero/MuZero structure with
skills as the macro-actions. That is the design for the chapter's
continuation (see the page 11 refinement section): keep the worker as the
macro-action set, and replace the greedy manager with a learned-value
skill-level search.

## Update (230-234): the chapter's missing null check, and a substantial correction

The continuation went 230 (skill-coarsened MCTS) → 232 (saliency-augmented
goal key, on a new cached-substrate harness built mid-chapter in response
to "experiments too slow and narrow") → 233 (click-affordance prior),
each ruling out one "obvious" lever. 230 found that skill-coarse MCTS
in imagination unlocks zero of 14 cells vs object-novelty MPC's 3:
coarsening the search by skills did not tame model error, because the
worker's H_HOP-step rollout in imagination still compounds 1-step forward-
model prediction error, and the structured search amplifies it over
depth. 232 found that augmenting the goal key with the four most prediction-
salient cells (gradient saliency from the cached forward model, no new
training) made coverage 44% richer in mean and 49% richer on tr87 -- the
saliency signal is real -- but the residual hard set stayed 0/5 and the
231 skill-MCTS unlock of tr87-s3 did NOT replicate, so goal representation
isn't the bottleneck either. 233 trained a tiny classifier P(object_key
changes | frame, action) on the cached rollouts and used it to re-weight
MPC's action sampling toward effectful clicks (a sb26-targeted attack on
the 261-dim action space, where 3 of the 5 residual seeds live); the
classifier learned real signal but the lift was in the noise range (+3%
mean coverage), still 0/5 residual unlocks, and a greedy-on-affordance
arm sat at 3 configs on sb26 (the 228 deterministic-argmax pathology
again).

Three consecutive eliminations of "obvious" levers -- search structure,
goal representation, action-space prior -- left the residual hard set at
0/5 under everything tried since 226. Which is when the question that
should have been asked first finally got asked: how much of "0/5
unlocks" is just because the standard chapter budget (10000 env steps)
is too short? 234 runs pure random -- no model, no worker, no MPC, just
env.step on a uniformly sampled action -- at a budget sweep {5k, 15k,
50k, 150k} on the five residual hard seeds. The result rewrites the
chapter.

Pure random cracks **two of five residual seeds**: tr87-s3 unlocks at
step 47215 (budget 50k), tr87-s0 at step 135411 (budget 150k). The sb26
residual seeds (s1, s3, s4) stay at zero unlocks even at 150000 with
coverage growing healthily from 77-100 at 5k to 815-832 at 150k. So the
chapter's two distinct sub-problems behave differently. tr87 residual
seeds were **budget-limited, not technique-limited**: the elaborate
machinery built across 226-233 to crack tr87-s3 specifically (skill
chaining, skill-MCTS, salient_key, click-affordance) never surfaced
anything pure random couldn't reach given 5-15x more time. The
6-explorer diversity ensemble at 225 (budget 15k) was, on tr87 residual,
roughly equivalent to running random for 3-10x longer rather than a
fundamentally different explorer. And the 231 skill-MCTS unlock of
tr87-s3 at budget 5000 reads now as a ~10x budget acceleration over
random's 47215 step -- catalysis, not cracking. The sb26 residual seeds
are the genuine hard residual: the 261-dim action space (256 click cells)
needs combinatorial click sequences that pure random cannot cover by
chance even at 30x the standard budget. That is where technique would
actually be needed.

The methodology lesson is hard but clean: always run the cheap null
check first. A 90-minute pure-random budget sweep done in 226 would have
shown that 3 of the 5 residual seeds (the two tr87 ones, after
correction) are budget-limited and saved a chapter's worth of chasing.
The chapter's clearest deliverables in retrospect are the infrastructure
(the brain/concrete/arc_common.py shared cached-substrate harness built
mid-chapter, which made everything from 231 onward fast enough to
iterate) and the null check itself. The mechanistic experiments 226-233
sharpen to: skill-MCTS may catalyze on some seeds with maybe-10x budget
speedup; no single technique unlocks more than random plus enough budget
can; only sb26 residual is genuine. The realistic chapter ceiling
including budget extension is now 12/15 (the prior 10/15 plus tr87 s0
and tr87 s3), not the claimed 10/15.

The continuing question is what sb26 residual actually needs. The
candidates that 234 leaves open: even more random budget (500k, 1M)
to see whether sb26 is also budget-limited just at a higher tier; the
object-novelty MPC (the strongest single explorer from 222) at the
higher budgets to see whether technique closes a gap random cannot;
and if neither works, a structurally different mechanism -- cross-seed
transfer from any seed that ever solves sb26, online forward-model
learning during exploration, or click-pair combinatorial coverage
designed for the 261-dim action space. 235 will test the first two
before reaching for the third.
