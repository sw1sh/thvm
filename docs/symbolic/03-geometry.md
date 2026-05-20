# 3. The geometry of learning

(GDL = geometric deep learning. CDL = categorical deep learning.
GNN = graph neural network. CNN = convolutional neural network.
RL = reinforcement learning. IQE = Interval Quasimetric Embedding.
MRN = Metric Residual Network. QRL = Quasimetric Reinforcement
Learning.)

The third thread in combining symbolists and connectionists is to
look for the *geometry* (or algebra, or topology) that both share --
a unifying mathematical structure from which architectures and
constraints can be derived rather than hand-designed.

## Geometric Deep Learning: the Erlangen Programme of ML

GDL (Bronstein, Bruna, Cohen, Velickovic; arXiv:2104.13478, 2021) is
pitched as an analogue of Felix Klein's 1872 Erlangen Programme,
which organized geometries by their symmetry groups. The thesis: most
successful architectures are recoverable from a small set of
geometric priors over a domain -- **symmetry** (group
invariance/equivariance), **stability** (to deformations), **scale
separation** (locality/hierarchy). From a symmetry group acting on
the domain you derive the right equivariant linear maps, and the
canonical architectures fall out as special cases: grids + translation
-> CNNs; sets/graphs + permutation -> GNNs and transformers; sphere +
SO(3) -> spherical CNNs; manifolds + gauge symmetry -> mesh CNNs.

Group-equivariant networks are the concrete payoff: G-CNNs (Cohen &
Welling, ICML 2016, arXiv:1602.07576), steerable CNNs
(arXiv:1612.08498), E(n)-equivariant GNNs (Satorras et al., ICML 2021,
arXiv:2102.09844). They are SOTA where symmetry is exact and data is
expensive -- molecular force fields (NequIP/MACE/Allegro), 3D molecule
generation.

## Categorical Deep Learning: the proposed successor

CDL (Gavranovic, Lessard, Dudzik, von Glehn, Araujo, Velickovic;
arXiv:2402.15332, ICML 2024 -- the Symbolica paper from
[01-the-divide.md](01-the-divide.md)) argues that group-theoretic GDL
captures *constraints* (what a layer must respect) but not
*implementations* (the actual maps, including stateful ones like
recurrent networks/automata). The proposal: use the universal algebra
of **monads valued in a 2-category of parametric maps** as a single
language for both, recovering group equivariance as a special case
and extending to structures groups cannot express. A 2026 follow-up
moves it toward executable tooling (Abbott & Zardini, *Weaves, Wires,
and Morphisms*, arXiv:2604.07242).

Honest status: CDL is the **most general and least empirically
validated** thread here. As of 2026 its value is conceptual
unification and code-generation/correctness, not benchmark wins -- see
the Symbolica trajectory in [01-the-divide.md](01-the-divide.md).

## The honest verdict: equivariance at scale

The key 2024-2026 finding is that hard-coded symmetry is a
data-efficiency prior that **scale + augmentation can match or beat**:

- *Does Equivariance Matter at Scale?* (Brehmer, Behrends, de Haan,
  Cohen; TMLR, arXiv:2410.23179): equivariance improves data
  efficiency, but non-equivariant + augmentation closes the gap given
  enough compute.
- **AlphaFold3 (2024) dropped explicit SE(3) equivariance** for a
  diffusion model + augmentation, and it worked at least as well --
  symmetry learned at scale beat symmetry hard-coded.
- *Learning (Approximately) Equivariant Networks via Constrained
  Optimization* (arXiv:2505.13631, 2025): even on symmetric data,
  strict equivariance can hurt optimization; relaxed/approximate
  equivariance is the pragmatic choice.

So geometry is a productive *prior-design* discipline in the
data-scarce, exact-symmetry regime (molecules, physics, permutation
structure), and a powerful *organizing* discipline everywhere else --
but at large data/compute the frontier has shifted toward approximate
equivariance and toward geometry as a *representational* rather than
strictly *architectural* constraint.

## The thread that matters most for this repo: quasimetric geometry for planning

(This is where the geometry survey touches running code -- our value
function. See [04-implications-for-thvm.md](04-implications-for-thvm.md).)

The optimal cost-to-go in a deterministic goal-reaching task is a
**quasimetric**: nonnegative, identity `d(x,x)=0`, triangle
inequality, but **asymmetric** (irreversible actions mean cost A->B
!= B->A). The right inductive bias for a learned distance/value head
is therefore a quasimetric -- not a symmetric L2 head (wrong by
construction) and not an unconstrained MLP (which violates the
triangle inequality and generalizes badly over composed paths). The
Tongzhou Wang & Phillip Isola line:

- **MRN -- Metric Residual Network** (Liu et al., 2022): the earlier
  latent-quasimetric head, with a large parameter count. **This is
  what this repo's `quasi_d` head is** (the metric-residual head
  introduced in experiment 155-159, used by `brain/qm_harness.py`).
- **IQE -- Interval Quasimetric Embedding** (Wang & Isola,
  arXiv:2211.15120, NeurReps 2022): reshape the latent to k x l, per
  row compute the **length of a union of intervals** on the real line
  `| union_j [u_ij, max(u_ij, v_ij)] |`, aggregate by sum or maxmean.
  Provably satisfies identity, nonnegativity, the triangle inequality,
  and positive homogeneity, with essentially **<= 1 parameter** in the
  distance head versus ~12,500 for MRN/Deep Norm, and reports large
  accuracy gains. A drop-in upgrade to our metric head.
- **QRL -- Quasimetric RL** (Wang, Torralba, Isola, Zhang; ICML 2023,
  arXiv:2304.01203): an objective designed for quasimetrics (maximize
  distances subject to a one-step transition-cost constraint), with
  optimal-value recovery guarantees.
- 2025-2026: **ProQ** (arXiv:2506.18847) uses the learned quasimetric
  as a planning *energy* (repulsion to spread keypoints, directional
  cost to subgoals); a NeurIPS 2025 paper (arXiv:2509.20478) unifies
  contrastive and temporal-distance representations under a quasimetric
  latent.

There is also a representational-geometry idea worth knowing: the
**Platonic Representation Hypothesis** (Huh, Cheung, Tongzhou Wang,
Isola; ICML 2024, arXiv:2405.07987) -- scaled models converge toward a
shared representational geometry. Same Tongzhou Wang as the
quasimetric line; the through-line is "geometry as a representational
constraint."
