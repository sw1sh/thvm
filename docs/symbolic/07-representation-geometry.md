# 7. The geometry of learned representations and the loss landscape

Page [03-geometry.md](03-geometry.md) covered the geometry that is
*built into* models: equivariant architectures, the Geometric Deep
Learning blueprint, categorical structure, hyperbolic/sheaf
embeddings, the Platonic Representation Hypothesis, and quasimetric
heads for planning (which tie into this repo's `qm_harness`). This
page is the complement: the geometry that *emerges* inside a network
once you train it -- how features are laid out in activation space,
what shape the function and the loss surface take, and what (if
anything) information theory says about why training works. It is the
interpretability-and-theory slice that 03 deliberately left out.

(Abbreviations are page-local, expanded on first use here. LRH =
linear representation hypothesis. SAE = sparse autoencoder. MASO =
max-affine spline operator. NTK = neural tangent kernel. IB =
information bottleneck. K-FAC = Kronecker-factored approximate
curvature.)

## 1. The geometry of features: linear directions, superposition, and dictionary learning

The dominant working hypothesis in mechanistic interpretability is the
**Linear Representation Hypothesis (LRH)**: high-level concepts are
encoded as *directions* in a network's activation space, so that
semantic relations show up as vector arithmetic. The lineage runs from
the word2vec "king - man + woman ~= queen" offset regularities
([Mikolov, Yih, Zweig, "Linguistic Regularities in Continuous Space
Word Representations", NAACL 2013](https://aclanthology.org/N13-1090/))
to a modern formalization that ties linear directions to
counterfactual probing and steering, and identifies a non-Euclidean
"causal" inner product as the geometrically correct metric on concept
space ([Park, Choe, Veitch, "The Linear Representation Hypothesis and
the Geometry of Large Language Models", arXiv:2311.03658, ICML
2024](https://arxiv.org/abs/2311.03658)). The LRH is solid as an
empirical regularity and increasingly load-bearing, but it is a
hypothesis, not a theorem: some concepts appear to be encoded
non-linearly or multi-dimensionally, and the "right" inner product is
itself contested.

The reason features-as-directions is subtle is **superposition**. A
network can represent far more sparse features than it has neurons by
packing them into an overcomplete set of *almost*-orthogonal
directions, accepting small interference in exchange for capacity.
Anthropic's toy-model study makes this precise: small ReLU networks
trained on sparse synthetic features exhibit a phase change into
superposition and arrange features into surprisingly clean geometric
configurations (uniform polytopes such as digons, triangles, and
pentagons), with a direct link to polysemantic neurons and even to
adversarial examples ([Elhage et al., "Toy Models of Superposition",
Transformer Circuits Thread,
2022; arXiv:2209.10652](https://transformer-circuits.pub/2022/toy_model/index.html)).
Superposition is why you usually cannot read a feature off a single
neuron: the natural basis of the activation space is not the basis of
meaning.

The current best tool for *recovering* that meaningful basis is
**dictionary learning** via a **sparse autoencoder (SAE)**: train a
wide, sparsely-activating autoencoder to reconstruct a layer's
activations, and read each dictionary atom as a candidate monosemantic
feature. Anthropic showed this recovers thousands of largely
interpretable, monosemantic features from a one-layer transformer
([Bricken et al., "Towards Monosemanticity: Decomposing Language
Models With Dictionary Learning", Transformer Circuits Thread,
2023](https://transformer-circuits.pub/2023/monosemantic-features/index.html)),
with the same approach independently in academic work ([Cunningham,
Ewart, Riggs, Huben, Sharkey, "Sparse Autoencoders Find Highly
Interpretable Features in Language Models", arXiv:2309.08600,
2023](https://arxiv.org/abs/2309.08600)). It then scaled to a
production model, extracting millions of features (including multimodal
and safety-relevant ones) from Claude 3 Sonnet, with demonstrated
causal steering ([Templeton et al., "Scaling Monosemanticity:
Extracting Interpretable Features from Claude 3 Sonnet", Transformer
Circuits Thread,
2024](https://transformer-circuits.pub/2024/scaling-monosemanticity/index.html)).
Caveats are worth stating honestly (as of 2026): SAE features are not
a canonical decomposition (they depend on width, sparsity penalty, and
seed), reconstruction is lossy, and "feature splitting" means the same
concept fractures across atoms at different dictionary sizes. This is
the most active corner of "geometry of ML" right now, and also the
least settled.

## 2. The function's geometry: deep ReLU nets as piecewise-linear splines

A ReLU network with affine layers is *exactly* a continuous
piecewise-linear function, and that is not a metaphor. The **spline
theory of deep networks** writes a broad class of networks as
compositions of **max-affine spline operators (MASOs)**: conditioned on
an input, the whole network collapses to a single affine map, and the
network partitions input space into polyhedral regions, one affine
piece per region. Each region's affine map acts as a signal-dependent
matched filter, linking deep nets to classical template-matching
([Balestriero, Baraniuk, "A Spline Theory of Deep Networks", ICML
2018](https://proceedings.mlr.press/v80/balestriero18b.html); extended
as ["Mad Max: Affine Spline Insights into Deep Learning",
arXiv:1805.06576](https://arxiv.org/abs/1805.06576)). This view is
fully rigorous for piecewise-affine architectures and gives a concrete
handle on the input-space geometry a trained network induces. It
connects directly to a tensor VM like thvm: the partition is
determined entirely by which ReLU gates are active, i.e. the sign
pattern of the pre-activations the scheduler is computing anyway.

## 3. The loss landscape's geometry: NTK, mode connectivity, lottery tickets

Two complementary geometric stories describe training dynamics.

The **Neural Tangent Kernel (NTK)** describes the *function-space*
geometry near initialization. In the infinite-width limit a network
behaves like a linear model in its tangent features: gradient descent
on the parameters follows a kernel gradient flow whose kernel
converges to a fixed, deterministic NTK that stays constant through
training ([Jacot, Gabriel, Hongler, "Neural Tangent Kernel:
Convergence and Generalization in Neural Networks", NeurIPS 2018;
arXiv:1806.07572](https://arxiv.org/abs/1806.07572)). The NTK is a
clean, genuinely predictive theory in its regime; the standard caveat
(well established by 2026) is that the regime is the "lazy" /
wide-and-near-init one, and much of what makes finite, feature-learning
networks interesting happens precisely where the NTK approximation
breaks.

The **loss-surface** story is about the geometry of minima.
Independently trained solutions are not isolated basins: they are
connected by simple low-loss curves (**mode connectivity**), so the
set of good solutions is far more connected than a naive "rugged
landscape" picture suggests ([Garipov, Izmailov, Podoprikhin, Vetrov,
Wilson, "Loss Surfaces, Mode Connectivity, and Fast Ensembling of
DNNs", NeurIPS 2018;
arXiv:1802.10026](https://arxiv.org/abs/1802.10026)). A related
landscape phenomenon is the **lottery ticket hypothesis**: a dense
randomly-initialized network contains a sparse subnetwork ("winning
ticket") that, trained in isolation from the same initialization,
matches the full network's accuracy ([Frankle, Carbin, "The Lottery
Ticket Hypothesis: Finding Sparse, Trainable Neural Networks", ICLR
2019; arXiv:1803.03635](https://arxiv.org/abs/1803.03635)). Both are
robust empirical findings; the open question is *why* the landscape has
this structure, not whether it does.

## 4. The contested geometry: the Information Bottleneck theory

The **Information Bottleneck (IB)** principle frames a layer as a
representation T that should maximize information about the label Y
while compressing information about the input X, tracing a trajectory
in the "information plane" of mutual information I(X;T) vs I(T;Y)
([Tishby, Zaslavsky, "Deep Learning and the Information Bottleneck
Principle", arXiv:1503.02406,
2015](https://arxiv.org/abs/1503.02406)). The strong empirical claim
followed: training has two phases, a short *fitting* phase and a long
*compression* phase, with most epochs spent compressing, and
compression argued to drive generalization ([Shwartz-Ziv, Tishby,
"Opening the Black Box of Deep Neural Networks via Information",
arXiv:1703.00810, 2017](https://arxiv.org/abs/1703.00810)).

This is presented honestly as **contested**. A careful rebuttal showed
that none of the strong claims hold in general: the compression phase
is largely an artifact of double-saturating nonlinearities (tanh
saturating), it vanishes with ReLU, there is no clean causal link
between compression and generalization (networks generalize without
compressing and vice versa), and the apparent dynamics depend heavily
on how mutual information is *estimated* for a deterministic network
([Saxe et al., "On the Information Bottleneck Theory of Deep Learning",
ICLR 2018; J. Stat. Mech.
2019](https://openreview.net/forum?id=ry_WPG-A-)). The honest 2026
verdict: IB is a genuinely appealing lens and the information-plane
visualization is useful, but the specific "compression causes
generalization" story is unproven and the measurement is fragile.
Treat it as suggestive, not as settled mechanism.

## 5. The parameter manifold's geometry: information geometry and natural gradient

If you treat a parameterized model as a point on a Riemannian manifold
of probability distributions, the metric is the **Fisher information
matrix**, and the steepest-descent direction under that metric is the
**natural gradient**, which is invariant to reparameterization and
asymptotically Fisher-efficient ([Amari, "Natural Gradient Works
Efficiently in Learning", Neural Computation 10(2),
1998](https://direct.mit.edu/neco/article/10/2/251/6143/Natural-Gradient-Works-Efficiently-in-Learning)).
This is the cleanest geometric statement about *optimization*: the loss
landscape's relevant geometry is not Euclidean in parameter space. The
full Fisher is intractable at scale, so the practical descendant is
**K-FAC (Kronecker-Factored Approximate Curvature)**, which
approximates the per-layer Fisher blocks as Kronecker products of two
small matrices, making the natural gradient invertible and affordable
([Martens, Grosse, "Optimizing Neural Networks with Kronecker-factored
Approximate Curvature", ICML 2015;
arXiv:1503.05671](https://arxiv.org/abs/1503.05671)). Information
geometry also quietly underlies the "right inner product" theme from
Section 1 and the natural-gradient flavor of preconditioned optimizers.

## 6. The decision boundary's geometry: tropical geometry

There is an algebraic-geometry reframing of the same piecewise-linear
structure from Section 2. Over the tropical (min/max-plus) semiring, a
feedforward ReLU network is exactly a **tropical rational map** (a
difference of two tropical polynomials). Then the network's linear
regions correspond to vertices of the Newton polytope of that map, its
decision boundary is a **tropical hypersurface**, and a
one-hidden-layer network's regions are described by **zonotopes**; this
machinery also yields bounds showing deeper networks can have
exponentially more linear regions than shallow ones ([Zhang, Naitzat,
Lim, "Tropical Geometry of Deep Neural Networks", ICML 2018;
arXiv:1805.07091](https://arxiv.org/abs/1805.07091)). Tropical geometry
is mathematically solid and elegant; as of 2026 it is more an exact
structural lens than a source of large-scale practical tools, but it is
the cleanest bridge from "ReLU net" to combinatorial geometry.

## What is actually actionable

Of these threads, the **SAE / dictionary-learning** line (Section 1) is
the one paying practical dividends right now: it gives steerable,
auditable feature handles on production models and is where engineering
effort compounds. The **spline / tropical** views (Sections 2, 6) are
the most rigorous and the most directly relevant to a tensor VM, since
the partition geometry is a function of the same ReLU gate patterns the
scheduler already materializes, and they are a natural place to look
for representation diagnostics built on thvm primitives. **NTK, mode
connectivity, and natural gradient / K-FAC** (Sections 3, 5) are mature
theory with niche but real practical use (kernel-regime analysis,
ensembling, second-order optimization). The **Information Bottleneck**
(Section 4) is the one to cite with the most caution: intellectually
generative, empirically disputed, and not a reliable design principle.
For the planning-and-metric geometry that *is* actionable in this repo,
see [03-geometry.md](03-geometry.md); for the broader symbolic and
neuro-symbolic framing, see [01-the-divide.md](01-the-divide.md),
[02-integration.md](02-integration.md),
[04-implications-for-thvm.md](04-implications-for-thvm.md),
[05-reasoning-and-rule-induction.md](05-reasoning-and-rule-induction.md),
and [06-classical-foundations.md](06-classical-foundations.md).
